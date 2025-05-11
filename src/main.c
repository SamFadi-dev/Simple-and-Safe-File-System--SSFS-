#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "fs.h"

void print_file_preview(int inode) {
    int size = stat(inode);
    if (size < 0) {
        printf("stat(inode %d) failed\n", inode);
        return;
    }

    printf("inode %d: %d bytes\n", inode, size);

    int to_read = size > 64 ? 64 : size;
    uint8_t buffer[65] = {0}; // +1 for null-terminator

    int r = read(inode, buffer, to_read, 0);
    if (r < 0) {
        printf("read(inode %d) failed\n", inode);
    } else {
        printf("First %d bytes: ", r);
        for (int i = 0; i < r; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126)
                putchar(buffer[i]);
            else
                printf("\\x%02x", buffer[i]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <disk_image>\n", argv[0]);
        return 1;
    }
    char *disk_name = argv[1];

    printf("\n-------------- Test 1: Read files from outer disk --------------\n");

    printf("Mounting outer disk: disk_img.2\n");
    if (mount(disk_name) != 0) {
        printf("Failed to mount disk_img.2\n");
        return 1;
    }
    printf("Outer disk mounted.\n");

    for (int i = 0; i < 3; i++) {
        print_file_preview(i);
    }

    int size = stat(0);
    if (size <= 0) {
        printf("Failed to stat inode 0 (expected nested image)\n");
        unmount();
        return 1;
    }

    uint8_t *nested_data = malloc(size);
    if (!nested_data) {
        printf("Failed to allocate buffer\n");
        unmount();
        return 1;
    }

    if (read(0, nested_data, size, 0) != size) {
        printf("Failed to read full nested disk\n");
        free(nested_data);
        unmount();
        return 1;
    }

    const char *nested_filename = "disk_img.3.extracted";
    FILE *f = fopen(nested_filename, "wb");
    if (!f || fwrite(nested_data, 1, size, f) != (size_t)size) {
        printf("Failed to write extracted disk image\n");
        free(nested_data);
        if (f) fclose(f);
        unmount();
        return 1;
    }
    fclose(f);
    free(nested_data);
    printf("Extracted nested disk to %s\n", nested_filename);
    unmount();

    printf("Mounting nested disk: %s\n", nested_filename);
    if (mount((char *)nested_filename) != 0) {
        printf("Failed to mount nested disk image\n");
        return 1;
    }
    printf("Nested disk mounted.\n");

    printf("\n-------------- Test 2: Read files from nested disk --------------\n");
    for (int i = 1; i <= 4; i++) {
        print_file_preview(i);
    }

    printf("\n-------------- Test 3: Create, write, read and delete --------------\n");

    int inode = create();
    if (inode < 0) { 
        printf("Failed to create new file\n"); 
        unmount();
        return 1; 
    }

    printf("Created new file with inode %d\n", inode);

    const char *text = "Hello from SSFS!\n";
    if (write(inode, (uint8_t *)text, strlen(text), 0) != (int)strlen(text)) {
        printf("Failed to write to inode %d\n", inode); 
        unmount();
        return 1;
    }
    printf("Wrote text to inode %d\n", inode);

    print_file_preview(inode);

    if (delete(inode) != 0) {
        printf("Failed to delete inode %d\n", inode);
        unmount();
        return 1;
    } else {
        printf("Deleted inode %d\n", inode);
    }

    printf("\n-------------- Test 4: Appending and stat checks --------------\n");

    inode = create();
    if (inode < 0) { 
        printf("[T4.1] create() failed\n"); 
        unmount();
        return 1; 
    }

    const char *msg1 = "First part.";
    if (write(inode, (uint8_t *)msg1, strlen(msg1), 0) != (int)strlen(msg1)) {
        printf("write() failed\n"); 
        unmount();
        return 1;
    }

    const char *msg2 = " Second part.";
    if (write(inode, (uint8_t *)msg2, strlen(msg2), strlen(msg1)) != (int)strlen(msg2)) {
        printf("append write() failed\n"); 
        unmount(); 
        return 1;
    }

    int expected_size = strlen(msg1) + strlen(msg2);
    int actual_size = stat(inode);
    if (actual_size != expected_size) {
        printf("Expected size %d, got %d\n", expected_size, actual_size);
        unmount();
        return 1;
    } else {
        printf("Correct file size: %d bytes\n", actual_size);
    }

    print_file_preview(inode);

    if (delete(inode) != 0) {
        printf("delete() failed\n");
        unmount();
        return 1;
    } else {
        printf("Deleted inode %d\n", inode);
    }

    printf("\n-------------- Test 5: Stress test - fill all inodes --------------\n");

    int count = 0;
    int inodes_created[1024];
    while ((inode = create()) >= 0 && count < 1024) {
        inodes_created[count] = inode;
        char name[32];
        snprintf(name, sizeof(name), "File #%d\n", inode);
        if (write(inode, (uint8_t *)name, strlen(name), 0) != (int)strlen(name)) {
            printf("Failed to write to inode %d (excepted)\n", inode);
            break;
        }
        count++;
    }
    printf("Created and wrote to %d files until no inodes left.\n", count);

    for (int i = 0; i < count; i++) {
        if (delete(inodes_created[i]) != 0) {
            printf("Failed to delete inode %d\n", inodes_created[i]);
        }
    }
    printf("Deleted all created inodes.\n");

    unmount();
    printf("Nested disk unmounted.\n");
    remove(nested_filename);

    printf("\nAll tests passed.\n");

    return 0;
}
