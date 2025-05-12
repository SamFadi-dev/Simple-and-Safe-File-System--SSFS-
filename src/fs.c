#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "include/vdisk.h"
#include "fs.h"
#include "ssfs.h"
#include "include/error.h"

#define INODE_VALID             1 // Valid inode status
#define INODE_STATUT            0 // Offset for inode status in the inode structure
#define INODE_SIZE_OFFSET       4 // Offset for file size in the inode structure
#define INODE_DIRECT_OFFSET     8 // Offset for direct pointers in the inode structure
#define INODE_INDIRECT1_OFFSET  24 // Offset for indirect1 pointer in the inode structure
#define INODE_INDIRECT2_OFFSET  28 // Offset for indirect2 pointer in the inode structure
#define BLOCK_POINTERS_SIZE     256 // Number of pointers in a block
#define NB_DIRECT_BLOCKS        4 // Number of direct blocks in an inode
#define BLOCK_PTR_SIZE          4 // Size of a block pointer

#define MAX_BLOCKS 10000
char block_used[MAX_BLOCKS] = {0}; // Array to track used blocks

static uint8_t* get_inode(uint32_t inode_num, uint8_t *block_out);
static int free_block(uint32_t block_num);
static uint32_t allocate_block();
static void clear_indirect_block(uint32_t block_num);
static void clear_double_indirect_block(uint32_t block_num);
static uint32_t get_vdisk_size(DISK *disk);
static void rebuild_block_usage_from_inodes();

//=============================================================================
//========================== SSFS API FUNCTIONS ===============================
//=============================================================================

/// @brief formats, 
/// that is, installs SSFS on, the virtual disk whose disk image is contained in file disk_name (as a c-style string). 
/// It will attempt to construct an SSFS instance with at least inodes i-nodes and a minimum of a single data block. 
/// inodes defaults to 1 if this argument is 0 or negative. Note that this function must refuse to format a mounted disk.
/// @param disk_name 
/// @param inodes 
/// @return 
int format(char *disk_name, int inodes)
{
    if (ssfs.is_mounted) return fs_EMOUNT;
    if (vdisk_on(disk_name, &ssfs.disk) != 0) return fs_EON;
    if (inodes <= 0) inodes = 1;

    // Calculate the number of blocks needed for inodes and data
    uint32_t inode_blocks = (inodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    uint32_t total_blocks = get_vdisk_size(&ssfs.disk);
    if (total_blocks <= 1 + inode_blocks)
        return fs_EWRITE;
    
    SuperBlock *sb = &ssfs.superblock;
    memcpy(sb->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE);
    sb->nb_blocks = total_blocks;
    sb->nb_inode_blocks = inode_blocks;
    sb->block_size = BLOCK_SIZE;

    // Write the superblock to the first block
    uint8_t block[BLOCK_SIZE] = {0};
    memcpy(block, sb, sizeof(SuperBlock));
    if (vdisk_write(&ssfs.disk, SUPERBLOCK_SECTOR, block) != 0)
        return fs_EWRITE;
    
    printf("format(): total_blocks = %u\n", total_blocks);

    for (uint32_t i = 1; i < total_blocks; ++i) {
        uint8_t check[BLOCK_SIZE];
        if (vdisk_read(&ssfs.disk, i, check) != 0) return fs_EREAD;
    
        for (int j = 0; j < BLOCK_SIZE; ++j) {
            if (check[j] != 0) {
                vdisk_off(&ssfs.disk);
                return fs_EWRITE; // Don't format non-empty disk
            }
        }
    }    

    // Erase the rest of the disk to 0
    memset(block, 0, BLOCK_SIZE);
    for (uint32_t i = 1; i < total_blocks; ++i) {
        if (vdisk_write(&ssfs.disk, i, block) != 0)
        {
            printf("Failed to write block %u\n", i);
            return fs_EWRITE;
        }
    }

    memset(block_used, 0, sizeof(block_used));

    if(vdisk_sync(&ssfs.disk) != 0) return fs_ESYNC;
    vdisk_off(&ssfs.disk);
    return 0;
}


/// @brief returns the file size on success.
/// @param inode_num 
/// @return 
int stat(int inode_num)
{
    if (!ssfs.is_mounted || inode_num < 0 || (uint32_t)inode_num >= ssfs.nb_inodes)
        return fs_EMOUNT;

    uint8_t inode_block[BLOCK_SIZE];
    uint8_t *inode = get_inode(inode_num, inode_block);
    if (!inode || inode[INODE_STATUT] != INODE_VALID) return fs_EREAD;

    uint32_t size;
    memcpy(&size, inode + INODE_SIZE_OFFSET, sizeof(uint32_t));
    return (int)size;
}


/// @brief mounts the virtual disk, whose disk image is contained in
/// file disk_name (as a c-style string), for use. Your implementation can safely assume that
/// maximum a single volume with SSFS will be mounted at any given time, that is, mount
/// should fail if it is called while another volume is already mounted.
/// @param disk_name 
/// @return 
int mount(char *disk_name)
{
    if (ssfs.is_mounted) return fs_EMOUNT;
    if (vdisk_on(disk_name, &ssfs.disk) != 0) return fs_EON;

    uint8_t block[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, SUPERBLOCK_SECTOR, block) != 0) {
        vdisk_off(&ssfs.disk);
        return fs_EREAD;
    }

    SuperBlock *sb = &ssfs.superblock;
    memcpy(sb, block, sizeof(SuperBlock));

    // Check the magic number to verify the file system
    if (memcmp(sb->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE) != 0) {
        vdisk_off(&ssfs.disk);
        return -1;
    }

    // Set all the parameters
    ssfs.nb_inodes = sb->nb_inode_blocks * INODES_PER_BLOCK;
    ssfs.inode_start_block = 1;
    ssfs.data_start_block  = ssfs.inode_start_block + sb->nb_inode_blocks;

    ssfs.is_mounted = 1;
    rebuild_block_usage_from_inodes();

    return 0;
}


/// @brief unmounts the mounted volume. This can only fail if it is called when no
/// volume has been mounted.
/// @return 
int unmount()
{
    if (!ssfs.is_mounted) return fs_EMOUNT;
    if(vdisk_sync(&ssfs.disk) != 0) return fs_ESYNC;

    vdisk_off(&ssfs.disk);
    ssfs.is_mounted = 0;
    memset(block_used, 0, sizeof(block_used)); // Reset block usage information

    return 0;
}

/// @brief deletes the file identified by inode_num.
/// @param inode_num
/// @return
int delete(int inode_num) 
{
    if (!ssfs.is_mounted || inode_num < 0 || (uint32_t)inode_num >= ssfs.nb_inodes)
        return fs_EMOUNT;

    uint8_t inode_block[BLOCK_SIZE];
    uint8_t *inode = get_inode(inode_num, inode_block);
    if (!inode || inode[0] == 0) return fs_EREAD;

    // Direct pointers
    for (int i = 0; i < NB_DIRECT_BLOCKS; i++) {
        uint32_t ptr;
        memcpy(&ptr, inode + INODE_DIRECT_OFFSET + i * BLOCK_PTR_SIZE, sizeof(uint32_t));
        if (ptr) free_block(ptr);
    }

    // Indirect 1
    uint32_t indirect1;
    memcpy(&indirect1, inode + INODE_INDIRECT1_OFFSET, sizeof(uint32_t));
    if (indirect1) clear_indirect_block(indirect1);

    // Indirect 2
    uint32_t indirect2;
    memcpy(&indirect2, inode + INODE_INDIRECT2_OFFSET, sizeof(uint32_t));
    if (indirect2) clear_double_indirect_block(indirect2);

    // Clear inode
    memset(inode, 0, INODE_SIZE);

    // Save inode block
    int inode_block_index = inode_num / INODES_PER_BLOCK;
    int block_num = ssfs.inode_start_block + inode_block_index;
    return vdisk_write(&ssfs.disk, block_num, inode_block);
}

/// @brief reads len bytes, from
/// offset into file inode_num, into data. On success, it returns the number of bytes actually read.
/// @param inode_num 
/// @param data 
/// @param len 
/// @param offset 
/// @return 
int read(int inode_num, uint8_t *data, int len, int offset)
{
    if (!ssfs.is_mounted || inode_num < 0 || (uint32_t)inode_num >= ssfs.nb_inodes)
        return fs_EMOUNT;

    // Read the inode block
    uint8_t block[BLOCK_SIZE];
    uint8_t *inode = get_inode(inode_num, block);

    uint32_t size;
    memcpy(&size, inode + INODE_SIZE_OFFSET, sizeof(uint32_t));
    if (offset >= (int)size) return 0;

    int bytes_to_read = (len < (int)(size - offset)) ? len : (int)(size - offset);
    int bytes_read = 0;
    int current_offset = offset;

    while (bytes_read < bytes_to_read) {
        int file_block_index = current_offset / BLOCK_SIZE;
        int inner_offset = current_offset % BLOCK_SIZE;

        uint32_t data_block_num = 0;

        // Direct blocks
        if (file_block_index < NB_DIRECT_BLOCKS) {
            memcpy(&data_block_num, inode + INODE_DIRECT_OFFSET + BLOCK_PTR_SIZE * file_block_index, sizeof(uint32_t));
        } 

        // Indirect 1 blocks
        else if (file_block_index < BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS) {
            uint32_t indirect1;
            memcpy(&indirect1, inode + INODE_INDIRECT1_OFFSET, sizeof(uint32_t));
            if (indirect1 == 0) break;

            uint8_t indirect_block[BLOCK_SIZE];
            if (vdisk_read(&ssfs.disk, indirect1, indirect_block) != 0)
                return fs_EREAD;

            memcpy(&data_block_num, indirect_block + BLOCK_PTR_SIZE * (file_block_index - NB_DIRECT_BLOCKS), BLOCK_PTR_SIZE);
        
        } 
        
        // Indirect 2 blocks
        else {
            uint32_t indirect2;
            memcpy(&indirect2, inode + INODE_INDIRECT2_OFFSET, sizeof(uint32_t));
            if (indirect2 == 0) break;

            uint8_t indirect2_block[BLOCK_SIZE];
            if (vdisk_read(&ssfs.disk, indirect2, indirect2_block) != 0)
                return fs_EREAD;

            int idx = file_block_index - (BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS);
            int first_level = idx / BLOCK_POINTERS_SIZE;
            int second_level = idx % BLOCK_POINTERS_SIZE;

            uint32_t intermediate_block_num;
            memcpy(&intermediate_block_num, indirect2_block + BLOCK_PTR_SIZE * first_level, sizeof(uint32_t));
            if (intermediate_block_num == 0) break;

            uint8_t intermediate_block[BLOCK_SIZE];
            if (vdisk_read(&ssfs.disk, intermediate_block_num, intermediate_block) != 0)
                return fs_EREAD;

            memcpy(&data_block_num, intermediate_block + BLOCK_PTR_SIZE * second_level, sizeof(uint32_t));
        }

        if (data_block_num == 0) {
            int chunk = (BLOCK_SIZE - inner_offset < bytes_to_read - bytes_read)
                      ? BLOCK_SIZE - inner_offset
                      : bytes_to_read - bytes_read;
            memset(data + bytes_read, 0, chunk); // simulate sparse
            bytes_read += chunk;
            current_offset += chunk;
            continue;
        }
        
        uint8_t data_block[BLOCK_SIZE];
        if (vdisk_read(&ssfs.disk, data_block_num, data_block) != 0)
            break;

        int bytes_available = BLOCK_SIZE - inner_offset;
        int bytes_remaining = bytes_to_read - bytes_read;
        int chunk = (bytes_available < bytes_remaining) ? bytes_available : bytes_remaining;

        memcpy(data + bytes_read, data_block + inner_offset, chunk);

        bytes_read += chunk;
        current_offset += chunk;
    }

    return bytes_read;
}

/// @brief writes len bytes from
/// data, at offset into file inode_num. If need be, any gap inside the file is filled with zeros.
/// On success, it returns the number of bytes actually written from data (i.e. filling bytes are
/// not counted in the return value).
/// @param inode_num 
/// @param data 
/// @param len 
/// @param offset 
/// @return 
int write(int inode_num, uint8_t *data, int len, int offset)
{
    if (!ssfs.is_mounted || inode_num < 0 || (uint32_t)inode_num >= ssfs.nb_inodes)
        return fs_EMOUNT;

    // Get inode and containing block
    uint8_t inode_block[BLOCK_SIZE];
    uint8_t *inode = get_inode(inode_num, inode_block);
    if (!inode || inode[0] != INODE_VALID)
        return fs_EREAD;

    uint32_t file_size;
    memcpy(&file_size, inode + INODE_SIZE_OFFSET, sizeof(uint32_t));

    int bytes_written = 0;
    int current_offset = offset;

    while (bytes_written < len) {
        int file_block_index = current_offset / BLOCK_SIZE;
        int inner_offset = current_offset % BLOCK_SIZE;

        uint32_t *data_block_ptr = NULL;
        uint8_t indirect_block[BLOCK_SIZE];
        uint8_t dbl_indirect_block[BLOCK_SIZE];
        uint8_t inner_indirect_block[BLOCK_SIZE];

        // Case 1: Direct blocks
        if (file_block_index < NB_DIRECT_BLOCKS) {
            data_block_ptr = (uint32_t *)(inode + INODE_DIRECT_OFFSET + BLOCK_PTR_SIZE * file_block_index);
        }

        // Case 2: Indirect 1
        else if (file_block_index < BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS) {
            uint32_t *indirect_ptr = (uint32_t *)(inode + INODE_INDIRECT1_OFFSET);
            if (*indirect_ptr == 0) {
                *indirect_ptr = allocate_block();
                if (*indirect_ptr == 0) return fs_EWRITE;
            }

            if (vdisk_read(&ssfs.disk, *indirect_ptr, indirect_block) != 0)
                return fs_EREAD;

            data_block_ptr = (uint32_t *)(indirect_block + BLOCK_PTR_SIZE * (file_block_index - NB_DIRECT_BLOCKS));
        }

        // Case 3: Indirect 2
        else {
            int idx = file_block_index - (BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS);
            int outer = idx / BLOCK_POINTERS_SIZE;
            int inner = idx % BLOCK_POINTERS_SIZE;

            uint32_t *indirect2_ptr = (uint32_t *)(inode + INODE_INDIRECT2_OFFSET);
            if (*indirect2_ptr == 0) {
                *indirect2_ptr = allocate_block();
                if (*indirect2_ptr == 0) return -1;
            }

            if (vdisk_read(&ssfs.disk, *indirect2_ptr, dbl_indirect_block) != 0)
                return fs_EREAD;

            uint32_t *intermediate_ptr = (uint32_t *)(dbl_indirect_block + BLOCK_PTR_SIZE * outer);
            if (*intermediate_ptr == 0) {
                *intermediate_ptr = allocate_block();
                if (*intermediate_ptr == 0) return -1;
                if(vdisk_write(&ssfs.disk, *indirect2_ptr, dbl_indirect_block) != 0)
                    return fs_EWRITE;
            }

            if (vdisk_read(&ssfs.disk, *intermediate_ptr, inner_indirect_block) != 0)
                return fs_EREAD;

            data_block_ptr = (uint32_t *)(inner_indirect_block + BLOCK_PTR_SIZE * inner);
        }

        // Allocate data block if needed
        if (*data_block_ptr == 0) {
            *data_block_ptr = allocate_block();
            if (*data_block_ptr == 0) return fs_EWRITE;
        }

        // Write actual data
        uint8_t data_block[BLOCK_SIZE];
        if(vdisk_read(&ssfs.disk, *data_block_ptr, data_block) != 0)
            return -1;

        int bytes_available = BLOCK_SIZE - inner_offset;
        int bytes_remaining = len - bytes_written;
        int chunk = (bytes_available < bytes_remaining) ? bytes_available : bytes_remaining;

        memcpy(data_block + inner_offset, data + bytes_written, chunk);
        if(vdisk_write(&ssfs.disk, *data_block_ptr, data_block) != 0)
            return fs_EWRITE;
        
        // Write back modified pointer block if indirect
        if (file_block_index >= NB_DIRECT_BLOCKS && file_block_index < BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS) {
            // Indirect1
            uint32_t indirect1 = *(uint32_t *)(inode + INODE_INDIRECT1_OFFSET);
            if (vdisk_write(&ssfs.disk, indirect1, indirect_block) != 0)
                return fs_EWRITE;
        }
        else if (file_block_index >= BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS) {
            // Indirect2
            int idx = file_block_index - (BLOCK_POINTERS_SIZE + NB_DIRECT_BLOCKS);
            int outer = idx / BLOCK_POINTERS_SIZE;

            uint32_t *indirect2_ptr = (uint32_t *)(inode + INODE_INDIRECT2_OFFSET);
            uint32_t *intermediate_ptr = (uint32_t *)(dbl_indirect_block + BLOCK_PTR_SIZE * outer);

            if (vdisk_write(&ssfs.disk, *intermediate_ptr, inner_indirect_block) != 0)
                return fs_EWRITE;
            if (vdisk_write(&ssfs.disk, *indirect2_ptr, dbl_indirect_block) != 0)
                return fs_EWRITE;
        }

        bytes_written += chunk;
        current_offset += chunk;
    }

    // Update file size if needed
    uint32_t new_size = offset + bytes_written;
    if (new_size > file_size) {
        memcpy(inode + INODE_SIZE_OFFSET, &new_size, sizeof(uint32_t));
    }

    // Save updated inode block
    int block_num = ssfs.inode_start_block + (inode_num / INODES_PER_BLOCK);
    return vdisk_write(&ssfs.disk, block_num, inode_block) == 0 ? bytes_written : -1;
}

int create()
{
    if (!ssfs.is_mounted) return fs_EMOUNT;

    for (uint32_t inode_num = 0; inode_num < ssfs.nb_inodes; ++inode_num) {
        uint8_t block[BLOCK_SIZE];
        uint8_t *inode = get_inode(inode_num, block);
        if (!inode)
            return fs_EREAD;

        if (inode[INODE_STATUT] != INODE_VALID) {
            inode[INODE_STATUT] = (uint8_t)INODE_VALID;
            memset(inode + 1, 0, INODE_SIZE - 1);

            int block_index = inode_num / INODES_PER_BLOCK;
            int block_num = ssfs.inode_start_block + block_index;
            if (vdisk_write(&ssfs.disk, block_num, block) != 0)
                return fs_EWRITE;

            return inode_num;
        }
    }

    return -1; // No free inode found
}

//=============================================================================
//========================== SSFS STATIC FUNCTIONS ============================
//=============================================================================

/// @brief Gets the inode of a file.
/// @param inode_num 
/// @param block_out 
/// @return pointer to the inode in the block_out buffer
static uint8_t* get_inode(uint32_t inode_num, uint8_t *block_out) 
{
    int block_index = inode_num / INODES_PER_BLOCK;
    int offset = inode_num % INODES_PER_BLOCK;
    int block_num = ssfs.inode_start_block + block_index;

    if (vdisk_read(&ssfs.disk, block_num, block_out) != 0)
        return NULL;

    return block_out + (offset * INODE_SIZE);
}

/// @brief Frees a block by writing zeros to it.
/// @param block_num 
/// @return 0 on success, -1 on error
static int free_block(uint32_t block_num) 
{
    uint8_t zero[BLOCK_SIZE] = {0};
    return vdisk_write(&ssfs.disk, block_num, zero);
}

/// @brief Allocates a free block by writing zeros to it.
/// @return The block number of the allocated block, or 0 if no free block is found.
uint32_t allocate_block() 
{
    uint8_t block[BLOCK_SIZE];

    for (uint32_t i = ssfs.data_start_block; i < ssfs.superblock.nb_blocks; ++i) {
        if (block_used[i]) continue;

        if (vdisk_read(&ssfs.disk, i, block) != 0) {
            fprintf(stderr, "vdisk_read failed on block %u\n", i);
            return 0;
        }

        int is_free = 1;
        for (int j = 0; j < BLOCK_SIZE; ++j) {
            if (block[j] != 0) {
                is_free = 0;
                break;
            }
        }

        if (is_free) {
            block_used[i] = 1;
            return i;
        }
    }

    printf("No free block available!\n");
    return 0;
}

/// @brief Clears an indirect1 block by freeing all its data blocks.
/// @param block_num 
static void clear_indirect_block(uint32_t block_num) 
{
    uint8_t block[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, block_num, block) != 0) return;
    for (int i = 0; i < BLOCK_POINTERS_SIZE; i++) {
        uint32_t ptr;
        memcpy(&ptr, block + i * BLOCK_PTR_SIZE, sizeof(uint32_t));
        if (ptr != 0) {
            free_block(ptr);
        }
    }
    free_block(block_num);
}

/// @brief Clears a indirect2 block by freeing all its data blocks.
/// @param block_num 
static void clear_double_indirect_block(uint32_t block_num) 
{
    uint8_t outer[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, block_num, outer) != 0) return;
    for (int i = 0; i < BLOCK_POINTERS_SIZE; i++) {
        uint32_t indirect_block_num;
        memcpy(&indirect_block_num, outer + i * BLOCK_PTR_SIZE, sizeof(uint32_t));
        if (indirect_block_num != 0) {
            clear_indirect_block(indirect_block_num);
        }
    }
    free_block(block_num);
}

/// @brief Gets the size of the virtual disk.
/// @param disk 
/// @return The size of the disk in blocks.
static uint32_t get_vdisk_size(DISK *disk) 
{
    long current = ftell(disk->fp);
    fseek(disk->fp, 0, SEEK_END);
    long size_bytes = ftell(disk->fp);
    fseek(disk->fp, current, SEEK_SET); // Restore original position
    return (uint32_t)(size_bytes / BLOCK_SIZE);
}

/// @brief Marks a block as used.
/// @param block_num 
static void mark_block_used(uint32_t block_num) 
{
    if (block_num < MAX_BLOCKS)
        block_used[block_num] = 1;
}

/// @brief Marks all blocks in an indirect block as used.
/// @param block_num 
static void mark_indirect_blocks(uint32_t block_num) 
{
    mark_block_used(block_num);

    uint8_t block[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, block_num, block) != 0)
        return;

    for (int i = 0; i < BLOCK_POINTERS_SIZE; ++i) {
        uint32_t ptr;
        memcpy(&ptr, block + i * BLOCK_PTR_SIZE, sizeof(uint32_t));
        if (ptr != 0)
            mark_block_used(ptr);
    }
}

/// @brief Marks all blocks in a double indirect block as used.
/// @param block_num 
static void mark_double_indirect_blocks(uint32_t block_num) 
{
    mark_block_used(block_num);

    uint8_t outer[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, block_num, outer) != 0)
        return;

    for (int i = 0; i < BLOCK_POINTERS_SIZE; ++i) {
        uint32_t intermediate;
        memcpy(&intermediate, outer + i * BLOCK_PTR_SIZE, sizeof(uint32_t));
        if (intermediate == 0) continue;

        mark_block_used(intermediate);

        uint8_t inner[BLOCK_SIZE];
        if (vdisk_read(&ssfs.disk, intermediate, inner) != 0)
            continue;

        for (int j = 0; j < BLOCK_POINTERS_SIZE; ++j) {
            uint32_t data_ptr;
            memcpy(&data_ptr, inner + j * BLOCK_PTR_SIZE, sizeof(uint32_t));
            if (data_ptr != 0)
                mark_block_used(data_ptr);
        }
    }
}

/// @brief Rebuilds the block usage information from inodes.
/// @note This should be called after mounting the disk to ensure that all blocks are marked as used or free.
static void rebuild_block_usage_from_inodes() 
{
    uint8_t inode_block[BLOCK_SIZE];
    for (uint32_t inode_num = 0; inode_num < ssfs.nb_inodes; ++inode_num) {
        uint8_t *inode = get_inode(inode_num, inode_block);
        if (!inode || inode[INODE_STATUT] != INODE_VALID)
            continue;

        // Direct
        for (int i = 0; i < NB_DIRECT_BLOCKS; ++i) {
            uint32_t ptr;
            memcpy(&ptr, inode + INODE_DIRECT_OFFSET + i * BLOCK_PTR_SIZE, sizeof(uint32_t));
            if (ptr != 0)
                mark_block_used(ptr);
        }

        // Indirect1
        uint32_t indirect1;
        memcpy(&indirect1, inode + INODE_INDIRECT1_OFFSET, sizeof(uint32_t));
        if (indirect1 != 0)
            mark_indirect_blocks(indirect1);

        // Indirect2
        uint32_t indirect2;
        memcpy(&indirect2, inode + INODE_INDIRECT2_OFFSET, sizeof(uint32_t));
        if (indirect2 != 0)
            mark_double_indirect_blocks(indirect2);
    }
}





