#ifndef FS_H
#define FS_H

#include <stdint.h>

#define CAWFS_MAGIC 0xCA705    // "CAWOS" magic number
#define MAX_FILES 24
#define FILES_START_LBA 130
#define FS_TABLE_SECTORS 4

typedef struct {
    uint32_t magic;
    uint32_t total_files;
} cawfs_superblock_t;

typedef struct __attribute__((packed)) {
    char name[32];
    char dir[32];
    uint32_t start_lba;
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
int fs_delete(char* name);
int fs_rename(char* old_name, char* new_name);
void fs_format(uint32_t magic);
int fs_load_to_memory(char* name, uint8_t* address);
int fs_read_content(char* name, uint8_t* address);
int fs_mkdir(char* name, int* row);
int fs_cd(char* path, int* row);
void bios_write_sector(uint32_t lba, uint8_t* data);
extern void bios_read_sector(uint32_t lba, uint8_t* buffer);
void pic_init();

#endif