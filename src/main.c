#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "fs.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <disk_image>\n", argv[0]);
        return 1;
    }
    const char *disk_name = argv[1];

    printf("Formatting disk...\n");
    if (format((char *)disk_name, 10) != 0) {
        printf("Format error.\n");
        return 1;
    }

    printf("Mounting disk...\n");
    if (mount((char *)disk_name) != 0) {
        printf("Mouting error.\n");
        return 1;
    }

    printf("Creation of file...\n");
    int inode = create();
    if (inode < 0) {
        printf("File creation error.\n");
        return 1;
    }

    printf("Writting in the file (inode %d)\n", inode);
    const char *message = "Hello, SSFS!";
    if (write(inode, (uint8_t *)message, strlen(message), 0) != (int)strlen(message)) {
        printf("Writting error.\n");
        return 1;
    }

    printf("Reading file...\n");
    uint8_t buffer[100] = {0};
    int read_bytes = read(inode, buffer, sizeof(buffer), 0);
    if (read_bytes < 0) {
        printf("Reading error.\n");
        return 1;
    }

    printf("Read content : %.*s\n", read_bytes, buffer);

    printf("Check size with stat()...\n");
    int size = stat(inode);
    if (size < 0) {
        printf("Erreur lors du stat.\n");
        return 1;
    }

    printf("File size : %d octets\n", size);

    printf("Deleting file...\n");
    if (delete(inode) != 0) {
        printf("File deletion error.\n");
        return 1;
    }

    printf("Unmounting disk...\n");
    if (unmount() != 0) {
        printf("Unmounting error.\n");
        return 1;
    }

    printf("Succes ✅\n");
    return 0;
}
