#ifndef FS_H
#define FS_H

#include <stdint.h>
#include "fs_config.h"

#ifndef MAX_FILES
    #define MAX_FILES 24
#endif
#ifndef MAX_CLUSTERS
    #define MAX_CLUSTERS 256
#endif
#ifndef FS_TABLE_LBA
    #define FS_TABLE_LBA 591
#endif
#ifndef FS_TABLE_SECTORS
    #define FS_TABLE_SECTORS 4
#endif
#ifndef CAWFAT_SECTORS
    #define CAWFAT_SECTORS 1
#endif
#ifndef DATA_REGION_START
    #define DATA_REGION_START (FS_TABLE_LBA + FS_TABLE_SECTORS + CAWFAT_SECTORS)
#endif

#define CAWFS_MAGIC 0xCA705    // "CAWOS" magic number
#define CAWFAT_LBA           (FS_TABLE_LBA + FS_TABLE_SECTORS)
#define CLUSTER_EMPTY        0x0000
#define CLUSTER_EOF          0xFFFF

typedef struct {
    uint32_t magic;
    uint32_t total_files;
} cawfs_superblock_t;

typedef struct __attribute__((packed)) {
    char name[32];
    char dir[32];
    uint16_t first_cluster;
    uint16_t reserved_pad;
    uint32_t size_bytes;
    uint8_t is_executable;
    uint8_t exists;
    uint8_t is_dir;
    uint8_t reserved;
} file_t;

extern char current_dir[32];

// API
void fs_init();
int fs_exists(char* name);
void fs_list(int* row);
void fs_flush();
int fs_create(char* name, int* row);
int fs_write(char* name, uint8_t* data, uint32_t len);
int fs_write_file(char* name, uint8_t* data, uint32_t size);
int fs_delete(char* name);
int fs_rename(char* old_name, char* new_name);
void fs_format(uint32_t magic);
int fs_load_to_memory(char* name, uint8_t* address);
uint32_t fs_get_size(char* name);
int fs_read_content(char* name, uint8_t* address);
int fs_mkdir(char* name, int* row);
int fs_cd(char* path, int* row);
void bios_write_sector(uint32_t lba, uint8_t* data);
void bios_read_sector(uint32_t lba, uint8_t* buffer);
void pic_init();
void idt_reload();

#endif