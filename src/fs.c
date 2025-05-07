#include <stdint.h>

/// @brief formats, 
/// that is, installs SSFS on, the virtual disk whose disk image is contained in file disk_name (as a c-style string). 
/// It will attempt to construct an SSFS instance with at least inodes i-nodes and a minimum of a single data block. 
/// inodes defaults to 1 if this argument is 0 or negative. Note that this function must refuse to format a mounted disk.
/// @param disk_name 
/// @param inodes 
/// @return 
int format(char *disk_name, int inodes)
{
	
}

/// @brief returns the file size on success.
/// @param inode_num 
/// @return 
int stat(int inode_num)
{

}

/// @brief mounts the virtual disk, whose disk image is contained in
/// file disk_name (as a c-style string), for use. Your implementation can safely assume that
/// maximum a single volume with SSFS will be mounted at any given time, that is, mount
/// should fail if it is called while another volume is already mounted.
/// @param disk_name 
/// @return 
int mount(char *disk_name)
{

}

/// @brief unmounts the mounted volume. This can only fail if it is called when no
/// volume has been mounted.
/// @return 
int unmount()
{

}

/// @brief creates a file and, on success, returns the i-node number that identifies the file.
/// @return 
int create()
{

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