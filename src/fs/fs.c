#include "fs.h"
#include "ata.h"
#include "util.h"
#include "screen.h"
#include "idt.h"

file_t fs[MAX_FILES]; 
ata_device_t* main_dev;

#define FS_TABLE_LBA 65
#define FS_TABLE_SECTORS 2
#define DATA_REGION_START 200

void fs_flush() {
    bios_write_sector(FS_TABLE_LBA,     (uint8_t*)fs);
    bios_write_sector(FS_TABLE_LBA + 1, (uint8_t*)fs + 512);
}

void fs_init() {
    extern void thunk_init(uint8_t drive);
    thunk_init(0x80);
    bios_read_sector(FS_TABLE_LBA, (uint8_t*)fs);
    bios_read_sector(FS_TABLE_LBA + 1, (uint8_t*)fs + 512);
    idt_reload();
}

void fs_list(int* row) {
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists) {
            char line[64];
            memset(line, 0, 64);
            strcpy(line, "FILE: ");
            strcat(line, fs[i].name);
            print_line_scroll(line, 0, row, 0x0F);
            char size_line[32];
            memset(size_line, 0, 32);
            strcpy(size_line, "SIZE: ");
            char s_buf[16];
            itoa(fs[i].size_bytes, s_buf);
            strcat(size_line, s_buf);
            strcat(size_line, " bytes");
            print_line_scroll(size_line, 6, row, 0x0F);
            found = 1;
        }
    }
    if (!found) {
        print_line_scroll("Drive is empty.", 0, row, 0x0F);
    }
}

uint32_t find_free_lba() {
    uint32_t last_lba = DATA_REGION_START;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists) {
            uint32_t end_of_file = fs[i].start_lba + (fs[i].size_bytes / 512) + 1;
            if (end_of_file > last_lba) last_lba = end_of_file;
        }
    }
    return last_lba;
}

int fs_create(char* name) {
    for (int i = 0; i < MAX_FILES; i++)
        if (fs[i].exists && strcmp(fs[i].name, name) == 0) return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) {
            memset(fs[i].name, 0, 32);
            strcpy(fs[i].name, name);
            fs[i].start_lba = find_free_lba();
            fs[i].size_bytes = 0;
            fs[i].exists = 1;
            fs_flush();
            return 1;
        }
    }
    return 0;
}

int fs_write(char* name, uint8_t* data, uint32_t len) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0) {
            uint8_t sector_buffer[512];
            memset(sector_buffer, 0, 512);
            uint32_t to_copy = (len > 512) ? 512 : len;
            memcpy(sector_buffer, data, to_copy);
            fs[i].size_bytes = len;
            fs_flush();
            bios_write_sector(fs[i].start_lba, sector_buffer);
            return 1;
        }
    }
    return 0;
}

int fs_delete(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0) {
            fs[i].exists = 0;
            fs_flush();
            return 1;
        }
    }
    return 0;
}

int fs_load_to_memory(char* name, uint8_t* address) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0) {
            uint32_t sectors = (fs[i].size_bytes / 512) + 1;
            for (uint32_t s = 0; s < sectors; s++) {
                bios_read_sector(fs[i].start_lba + s, address + (s * 512));
            }
            return 1;
        }
    }
    return 0;
}