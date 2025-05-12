#ifndef ERROR_H
#define ERROR_H

extern const int vdisk_ENODISK ; // Disk not found
extern const int vdisk_EACCESS ; // Disk access error
extern const int vdisk_ENOEXIST; // Disk does not exist
extern const int vdisk_EEXCEED ; // Disk size exceeded
extern const int vdisk_ESECTOR ; // Sector read error
extern const int fs_EWRITE     ; // Write error
extern const int fs_ESYNC      ; // Sync error
extern const int fs_EREAD      ; // Read error
extern const int fs_EON        ; // Disk on error
extern const int fs_EMOUNT     ; // Disk related mount error
#endif
