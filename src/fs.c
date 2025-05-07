#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vdisk.h"
#include "fs.h"
#include "ssfs.h"
#include "error.h"

#define INODE_VALID 1 // Valid inode status
#define INODE_STATUT 0 // Offset for inode status in the inode structure

/// @brief formats, 
/// that is, installs SSFS on, the virtual disk whose disk image is contained in file disk_name (as a c-style string). 
/// It will attempt to construct an SSFS instance with at least inodes i-nodes and a minimum of a single data block. 
/// inodes defaults to 1 if this argument is 0 or negative. Note that this function must refuse to format a mounted disk.
/// @param disk_name 
/// @param inodes 
/// @return 
int format(char *disk_name, int inodes)
{
    if (ssfs.is_mounted)
        return -1;

    if (vdisk_on(disk_name, &ssfs.disk) != 0)
        return -1;

    if (inodes <= 0)
        inodes = 1;

    // Calculate the number of blocks needed for inodes and data
    uint32_t inode_blocks = (inodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    uint32_t total_blocks = 1 + inode_blocks + 1;  // 1 SuperBlock + i-nodes + 1 DataBlock

    SuperBlock *sb = &ssfs.superblock;
    memcpy(sb->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE);
    sb->nb_blocks = total_blocks;
    sb->nb_inode_blocks = inode_blocks;
    sb->block_size = BLOCK_SIZE;

    // Write the superblock to the first block
    uint8_t block[BLOCK_SIZE] = {0};
    memcpy(block, sb, sizeof(SuperBlock));
    if (vdisk_write(&ssfs.disk, SUPERBLOCK_SECTOR, block) != 0)
        return -1;

    // Erase the rest of the disk to 0
    memset(block, 0, BLOCK_SIZE);
    for (uint32_t i = 1; i < total_blocks; ++i) {
        if (vdisk_write(&ssfs.disk, i, block) != 0)
            return -1;
    }

    vdisk_sync(&ssfs.disk);
    vdisk_off(&ssfs.disk);

    return 0;
}


/// @brief returns the file size on success.
/// @param inode_num 
/// @return 
int stat(int inode_num)
{
    if (!ssfs.is_mounted)
        return -1;

    if (inode_num < 0 || (uint32_t)inode_num >= ssfs.nb_inodes)
        return -1;

    // Get the block index and offset for the inode
    int block_index = inode_num / INODES_PER_BLOCK;
    int offset = inode_num % INODES_PER_BLOCK;

    int block_num = ssfs.inode_start_block + block_index;

    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, block_num, block) != 0)
        return -1;

    // Get the inode from the block
    uint8_t *inode = block + (offset * INODE_SIZE);

    // Check if inode is valid
    if (inode[INODE_STATUT] == !INODE_VALID)
        return -1;

    uint32_t size;
    memcpy(&size, inode + 4, sizeof(uint32_t)); // "size" is at offset 4 in the inode

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
    if (ssfs.is_mounted)
        return -1;

    if (vdisk_on(disk_name, &ssfs.disk) != 0)
        return -1;

    uint8_t block[BLOCK_SIZE];
    if (vdisk_read(&ssfs.disk, SUPERBLOCK_SECTOR, block) != 0) {
        vdisk_off(&ssfs.disk);
        return -1;
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
    return 0;
}


/// @brief unmounts the mounted volume. This can only fail if it is called when no
/// volume has been mounted.
/// @return 
int unmount()
{
    if (!ssfs.is_mounted)
        return -1;

    vdisk_sync(&ssfs.disk);
    vdisk_off(&ssfs.disk);
    ssfs.is_mounted = 0;

    return 0;
}

/// @brief deletes the file identified by inode_num.
/// @param inode_num
/// @return
int delete(int inode_num)
{

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

}