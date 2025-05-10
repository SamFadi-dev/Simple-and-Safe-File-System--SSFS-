#include "ssfs.h"

SSFS ssfs = { .is_mounted = 0 };

const uint8_t MAGIC_NUMBER[MAGIC_NUMBER_SIZE] = {
    0xf0, 0x55, 0x4c, 0x49,
    0x45, 0x47, 0x45, 0x49,
    0x4e, 0x46, 0x4f, 0x30,
    0x39, 0x34, 0x30, 0x0f
};

/// @brief Offset of the number of blocks in the superblock
const uint8_t OFFSET_NB_BLOCKS = 16;
/// @brief Offset of the number of inode blocks in the superblock
const uint8_t OFFSET_NB_INODE_BLOCKS = 20;
/// @brief Offset of the block size in the superblock
const uint8_t OFFSET_BLOCK_SIZE = 24;
