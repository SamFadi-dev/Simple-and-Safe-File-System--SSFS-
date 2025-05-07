#ifndef SSFS_H
#define SSFS_H

#include <stdint.h>
#include "vdisk.h"

#define BLOCK_SIZE 1024
#define INODE_SIZE 32
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define SUPERBLOCK_SECTOR 0
#define MAGIC_NUMBER_SIZE 16

typedef struct {
    DISK disk;
    int is_mounted;

    SuperBlock superblock;

    uint32_t nb_inodes;
    uint32_t inode_start_block;
    uint32_t data_start_block; 
} SSFS;

extern SSFS ssfs;


typedef struct {
    uint8_t magic[MAGIC_NUMBER_SIZE]; // 0–15
    uint32_t nb_blocks;               // 16–19
    uint32_t nb_inode_blocks;         // 20–23
    uint32_t block_size;              // 24–27
} SuperBlock;

/// @brief Magic number used to identify the file system. It is stored in the superblock
extern const uint8_t MAGIC_NUMBER[MAGIC_NUMBER_SIZE];
/// @brief Offset of the number of blocks in the superblock
extern const uint8_t OFFSET_NB_BLOCKS = 16;
/// @brief Offset of the number of inode blocks in the superblock
extern const uint8_t OFFSET_NB_INODE_BLOCKS = 20;
/// @brief Offset of the block size in the superblock
extern const uint8_t OFFSET_BLOCK_SIZE = 24;

#endif
