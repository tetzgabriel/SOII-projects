// Marina Barbosa Américo 201152509
// Joao Victor Fleming 
// Gabriel Tetzlaf Mansano  201150956
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ds.h"

#define MAX_BLOCKS 100
#define IS_MOUNTED 1234

#define FREE 0

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

int fat_format() {
    if (sb.magic == IS_MOUNTED) {
        return -1;
    }

    // Cria superblock
    sb.magic = IS_MOUNTED;
    sb.num_blocks = MAX_BLOCKS;
    sb.fat_block = 1;

    // Libera entradas de diretorio
    memset(dir, 0, sizeof(DirectoryEntry) * MAX_BLOCKS);

    // Libera entradas FAT
    memset(fat.table, 0, sizeof(int) * MAX_BLOCKS);

    // Escreve superblock no disco
    ds_write(0, &sb);

    // Escreve fat no disco
    ds_write(sb.fat_block, &fat);

    // Escreve diretorio no disco
    ds_write(2, &dir);

    return 0;
}

void fat_debug() {
    // Print info do superblock
    printf("superblock:\n");
    if (sb.magic == IS_MOUNTED) {
        printf("magic is ok\n");
        printf("%d blocks\n", sb.num_blocks);
        printf("1 block fat\n");
    } else {
        printf("magic is not ok\n");
    }

    // Print diretorio
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
    if (sb.magic == IS_MOUNTED) {
        return 0;
    }

    // Le superblock do disco
    ds_read(0, &sb);

    // Le FAT do disco
    ds_read(sb.fat_block, &fat);

    // Le directory do disco
    ds_read(2, &dir);

    if (sb.magic != IS_MOUNTED) {
        return -1;
    }

    return 0;
}

int fat_create(char *name) {
    if (sb.magic != IS_MOUNTED) {
        return -1;
    }

    // Ve se o arquivo ja existe no diretorio
    int i;
    for (i = 0; i < MAX_BLOCKS; i++) {
       if (strcmp(dir[i].filename, name) == 0) {
            return -1; // Existe
        }
    }

    // Acha diretorio vazio
    int emptyEntry = -1;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strlen(dir[i].filename) == 0) {
            emptyEntry = i;
            break;
        }
    }

    if (emptyEntry == -1) {
        return -1; 
    }

    // Seta o filename e inicializa o tamanho do arquivo e os blocos
    strncpy(dir[emptyEntry].filename, name, sizeof(dir[emptyEntry].filename) - 1);
    dir[emptyEntry].size = 0;
    memset(dir[emptyEntry].blocks, -1, sizeof(int) * MAX_BLOCKS);

    // Escreve o diretorio updatado no disco
    ds_write(2, &dir);

    return 0;
}

int fat_delete( char *name) {
    if (sb.magic != IS_MOUNTED) {
        return -1;
    }

    // Acha o arquivo no diretorio
    int i;
    int fileIndex = -1;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(dir[i].filename, name) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1) {
        return -1; // N achou
    }

    // Libera os blocos assciados com o arquivo na FAT
    int j;
    for (j = 0; j < MAX_BLOCKS && dir[fileIndex].blocks[j] != -1; j++) {
        int block = dir[fileIndex].blocks[j];
        fat.table[block] = FREE; 
    }

    // Limpa o diretorio para o arquivo
    memset(&dir[fileIndex], 0, sizeof(DirectoryEntry));

    // Escreve a FAT updatada e o diretorio no disco
    ds_write(sb.fat_block, &fat);
    ds_write(2, &dir);

    return 0;
}

int fat_getsize( char *name) { 
    if (sb.magic != IS_MOUNTED) {
        return -1;
    }

    int i;
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(dir[i].filename, name) == 0) {
            return dir[i].size;
        }
    }

    return -1;
}

//Retorna a quantidade de caracteres lidos
int fat_read( char *name, char *buff, int length, int offset) {
    if (sb.magic != IS_MOUNTED) {
        return -1;
    }

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

    // Offset
    if (offset >= dir[fileIndex].size) {
        return 0;
    }

    // Calcula os bytes pra ler do offset
    int bytesToRead = length;
    if (offset + bytesToRead > dir[fileIndex].size) {
        // Ajusta o numero de bytes pra nao estourar o tamanho do arquivo
        bytesToRead = dir[fileIndex].size - offset;
    }

    int bytesRead = 0;
    int blockIndex = offset / BLOCK_SIZE;
    int blockOffset = offset % BLOCK_SIZE;
    int remainingBytes = bytesToRead;

    while (remainingBytes > 0) {
        int block = dir[fileIndex].blocks[blockIndex];
        int blockSize = BLOCK_SIZE - blockOffset;
        if (blockSize > remainingBytes) {
            //Ajusta o tamanho do bloco se ele for maior do que os bytes restantes
            blockSize = remainingBytes;
        }

        ds_read(block, &buff[bytesRead]);

        bytesRead += blockSize;
        remainingBytes -= blockSize;
        blockIndex++;
        blockOffset = 0;
    }

    return bytesRead;
}

//Retorna a quantidade de caracteres escritos
int fat_write( char *name, const char *buff, int length, int offset) {
    if (sb.magic != IS_MOUNTED) {
        return -1; // Sistema de arquivo nao esta montado, erro
    }

    // Encontra o arquivo no diretorio
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

    // Offset
    if (offset > dir[fileIndex].size) {
        return -1;
    }

    // Ajusta os bytes a serem escritos do offset
    int bytesToWrite = length;
    if (offset + bytesToWrite > dir[fileIndex].size) {
        // Ajusta os bytes pra nao estourar o tamanho do arquivo
        bytesToWrite = dir[fileIndex].size - offset;
    }

    int bytesWritten = 0;
    int blockIndex = offset / BLOCK_SIZE;
    int blockOffset = offset % BLOCK_SIZE;
    int remainingBytes = bytesToWrite;

    while (remainingBytes > 0) {
        int block = dir[fileIndex].blocks[blockIndex];
        int blockSize = BLOCK_SIZE - blockOffset;
        if (blockSize > remainingBytes) {
            // Ajusta o tamanho do bloco de acordo com os bytes restantes
            blockSize = remainingBytes;
        }

        ds_write(block, &buff[bytesWritten]);

        bytesWritten += blockSize;
        remainingBytes -= blockSize;
        blockIndex++;
        blockOffset = 0;
   
 }

    // Ajusta o tamanho do arquivo se necessario
    if (offset + bytesToWrite > dir[fileIndex].size) {
        dir[fileIndex].size = offset + bytesToWrite;
    }

    // Escreve o diretorio novo no disco
    ds_write(2, &dir);

    return bytesWritten;
}