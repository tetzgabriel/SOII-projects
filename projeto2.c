#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BLOCKS 100
#define BLOCK_SIZE 1024

typedef struct {
    int magic;
    int num_blocks;
    int fat_block;
} Superblock;

typedef struct {
    char filename[256];
    int size;
    int blocks[MAX_BLOCKS];
} DirectoryEntry;

typedef struct {
    int table[MAX_BLOCKS];
} FAT;

Superblock sb;
DirectoryEntry dir[MAX_BLOCKS];
FAT fat;

void write_block(int block, const void *data) {
    // Simulated disk write operation
    printf("Writing block %d\n", block);
}

void read_block(int block, void *data) {
    // Simulated disk read operation
    printf("Reading block %d\n", block);
}

int fat_format() {
    // Check if the file system is already mounted
    if (sb.magic == 1234) {
        return -1; // File system is already mounted, return error
    }

    // Create superblock
    sb.magic = 1234;
    sb.num_blocks = MAX_BLOCKS;
    sb.fat_block = 1;

    // Clear directory entries
    memset(dir, 0, sizeof(DirectoryEntry) * MAX_BLOCKS);

    // Clear FAT entries
    memset(fat.table, 0, sizeof(int) * MAX_BLOCKS);

    // Write superblock to disk
    write_block(0, &sb);

    // Write FAT to disk
    write_block(sb.fat_block, &fat);

    // Write directory to disk
    write_block(2, &dir);

    return 0; // Success
}

void fat_debug() {
    // Print superblock information
    printf("superblock:\n");
    if (sb.magic == 1234) {
        printf("magic is ok\n");
        printf("%d blocks\n", sb.num_blocks);
        printf("1 block fat\n");
    } else {
        printf("magic is not ok\n");
    }

    // Print directory entries
    int i;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strlen(dir[i].filename) > 0) {
            printf("File \"%s\":\n", dir[i].filename);
            printf("size: %d bytes\n", dir[i].size);
            printf("Blocks:");
            int j;
            for (j = 0; j < MAX_BLOCKS && dir[i].blocks[j] != -1; j++) {
                printf(" %d", dir[i].blocks[j]);
            }
            printf("\n");
        }
    }
}

int fat_mount() {
    // Check if the file system is already mounted
    if (sb.magic == 1234) {
        return 0; // File system is already mounted, return success
    }

    // Read superblock from disk
    read_block(0, &sb);

    // Read FAT from disk
    read_block(sb.fat_block, &fat);

    // Read directory from disk
    read_block(2, &dir);

    // Verify if the file system is valid
    if (sb.magic != 1234) {
        return -1; // Invalid file system, return error
    }

    return 0; // Success
}

int fat_create(char *name) {
    // Check if the file system is mounted
    if (sb.magic != 1234) {
        return -1; // File system is not mounted, return error
    }

    // Check if the file already exists in the directory
    int i;
    for (i = 0; i < MAX_BLOCKS; i++) {
       if (strcmp(dir[i].filename, name) == 0) {
            return -1; // File already exists, return error
        }
    }

    // Find an empty directory entry
    int emptyEntry = -1;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strlen(dir[i].filename) == 0) {
            emptyEntry = i;
            break;
        }
    }

    if (emptyEntry == -1) {
        return -1; // Directory is full, return error
    }

    // Set the filename and initialize the file size and blocks
    strncpy(dir[emptyEntry].filename, name, sizeof(dir[emptyEntry].filename) - 1);
    dir[emptyEntry].size = 0;
    memset(dir[emptyEntry].blocks, -1, sizeof(int) * MAX_BLOCKS);

    // Write the updated directory to disk
    write_block(2, &dir);

    return 0; // Success
}

int fat_delete(char *name) {
    // Check if the file system is mounted
    if (sb.magic != 1234) {
        return -1; // File system is not mounted, return error
    }

    // Find the file in the directory
    int i;
    int fileIndex = -1;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(dir[i].filename, name) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1) {
        return -1; // File not found, return error
    }

    // Free the blocks associated with the file in the FAT
    int j;
    for (j = 0; j < MAX_BLOCKS && dir[fileIndex].blocks[j] != -1; j++) {
        int block = dir[fileIndex].blocks[j];
        fat.table[block] = 0; // Set the FAT entry to 0 (free)
    }

    // Clear the directory entry for the file
    memset(&dir[fileIndex], 0, sizeof(DirectoryEntry));

    // Write the updated FAT and directory to disk
    write_block(sb.fat_block, &fat);
    write_block(2, &dir);

    return 0; // Success
}

int fat_getsize(char *name) {
    // Check if the file system is mounted
    if (sb.magic != 1234) {
        return -1; // File system is not mounted, return error
    }

    // Find the file in the directory
    int i;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(dir[i].filename, name) == 0) {
            return dir[i].size; // Return the file size
        }
    }

    return -1; // File not found, return error
}

int fat_read(char *name, char *buff, int length, int offset) {
    // Check if the file system is mounted
    if (sb.magic != 1234) {
        return -1; // File system is not mounted, return error
    }

    // Find the file in the directory
    int i;
    int fileIndex = -1;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(dir[i].filename, name) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1) {
        return -1;
        }

    // Verify the offset
    if (offset >= dir[fileIndex].size) {
        return 0; // Offset is beyond the file size, no bytes to read
    }

    // Calculate the number of bytes to read from the offset
    int bytesToRead = length;
    if (offset + bytesToRead > dir[fileIndex].size) {
        bytesToRead = dir[fileIndex].size - offset; // Adjust the number of bytes to avoid reading beyond the file size
    }

    // Read the data from the file
    int bytesRead = 0;
    int blockIndex = offset / BLOCK_SIZE; // Index of the starting block to read
    int blockOffset = offset % BLOCK_SIZE; // Offset within the starting block
    int remainingBytes = bytesToRead; // Number of remaining bytes to read

    while (remainingBytes > 0) {
        int block = dir[fileIndex].blocks[blockIndex];
        int blockSize = BLOCK_SIZE - blockOffset;
        if (blockSize > remainingBytes) {
            blockSize = remainingBytes; // Adjust the block size if it's larger than the remaining bytes to be read
        }

        // Read the block from the disk (simulate disk read operation)
        // Example: read_block(block, &buff[bytesRead]);

        bytesRead += blockSize;
        remainingBytes -= blockSize;
        blockIndex++;
        blockOffset = 0;
    }

    return bytesRead; // Total number of bytes read
}

int fat_write(char *name, const char *buff, int length, int offset) {
    // Check if the file system is mounted
    if (sb.magic != 1234) {
        return -1; // File system is not mounted, return error
    }

    // Find the file in the directory
    int i;
    int fileIndex = -1;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(dir[i].filename, name) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1) {
        return -1; // File not found, return error
    }

    // Verify the offset
    if (offset > dir[fileIndex].size) {
        return -1; // Offset is beyond the file size, return error
    }

    // Determine the number of bytes to be written from the offset
    int bytesToWrite = length;
    if (offset + bytesToWrite > dir[fileIndex].size) {
        bytesToWrite = dir[fileIndex].size - offset; // Adjust the number of bytes to avoid writing beyond the file size
    }

    // Write the data to the file
    int bytesWritten = 0;
    int blockIndex = offset / BLOCK_SIZE; // Index of the starting block to write
    int blockOffset = offset % BLOCK_SIZE; // Offset within the starting block
    int remainingBytes = bytesToWrite; // Number of remaining bytes to write

    while (remainingBytes > 0) {
        int block = dir[fileIndex].blocks[blockIndex];
        int blockSize = BLOCK_SIZE - blockOffset;
        if (blockSize > remainingBytes) {
            blockSize = remainingBytes; // Adjust the block size if it's larger than the remaining bytes to be written
        }

        // Write the block to the disk (simulate disk write operation)
        // Example: write_block(block, &buff[bytesWritten]);

        bytesWritten += blockSize;
        remainingBytes -= blockSize;
        blockIndex++;
        blockOffset = 0;
   
 }

    // Update the file size if necessary
    if (offset + bytesToWrite > dir[fileIndex].size) {
        dir[fileIndex].size = offset + bytesToWrite;
    }

    // Write the updated directory to disk
    write_block(2, &dir);

    return bytesWritten; // Total number of bytes written
}