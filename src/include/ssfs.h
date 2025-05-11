#ifndef SSFS_H
#define SSFS_H

#include <stdint.h>
#include "vdisk.h"

#define BLOCK_SIZE 1024 // Size of a block in bytes
#define INODE_SIZE 32 // Size of an inode in bytes
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE) // Number of inodes per block
#define SUPERBLOCK_SECTOR 0 // The superblock is stored in the first block of the disk
#define MAGIC_NUMBER_SIZE 16 // Size of the magic number

/// @brief SuperBlock structure (inside the first block of the SSFS disk)
typedef struct {
    uint8_t magic[MAGIC_NUMBER_SIZE]; // 0–15
    uint32_t nb_blocks;               // 16–19
    uint32_t nb_inode_blocks;         // 20–23
    uint32_t block_size;              // 24–27
} SuperBlock;

/// @brief SSFS file system structure
typedef struct {
    DISK disk; // The virtual disk
    int is_mounted; // 1 if the disk is mounted, 0 otherwise
    SuperBlock superblock; // The superblock
    uint32_t nb_inodes; // Number of inodes
    uint32_t inode_start_block; // The block number where the inodes start
    uint32_t data_start_block;  // The block number where the data starts
} SSFS;

extern SSFS ssfs;

/// @brief Magic number used to identify the file system. It is stored in the superblock
extern const uint8_t MAGIC_NUMBER[MAGIC_NUMBER_SIZE];
/// @brief Offset of the number of blocks in the superblock
extern const uint8_t OFFSET_NB_BLOCKS;
/// @brief Offset of the number of inode blocks in the superblock
extern const uint8_t OFFSET_NB_INODE_BLOCKS;
/// @brief Offset of the block size in the superblock
extern const uint8_t OFFSET_BLOCK_SIZE;

#endif
