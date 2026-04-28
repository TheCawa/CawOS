#include "fs.h"
#include "util.h"
#include "screen.h"
#include "io.h"
#include "idt.h"
#include "ata.h"

file_t fs[MAX_FILES]; 
char current_dir[32];
static uint8_t fs_io_buf[512 * 4];
static int fs_use_ata = 0;
extern void thunk_init(uint8_t drive);

#define FS_TABLE_LBA 65
#define FS_TABLE_SECTORS 4
#define DATA_REGION_START 200

static void fs_write_sector(uint32_t lba, uint8_t* buf) {
    if (fs_use_ata) {
        ata_write_sectors(0, lba, 1, buf);
    } else {
        bios_write_sector(lba, buf);
    }
}

static void fs_read_sector(uint32_t lba, uint8_t* buf) {
    if (fs_use_ata) {
        ata_read_sectors(0, lba, 1, buf);
    } else {
        bios_read_sector(lba, buf);
    }
}

void fs_flush() {
    memset(fs_io_buf, 0, sizeof(fs_io_buf));
    memcpy(fs_io_buf, fs, sizeof(file_t) * MAX_FILES);
    if (fs_use_ata) {
        ata_write_sectors(0, FS_TABLE_LBA, FS_TABLE_SECTORS, fs_io_buf);
    } else {
        bios_write_sector(FS_TABLE_LBA + 0, fs_io_buf + 0 * 512);
        bios_write_sector(FS_TABLE_LBA + 1, fs_io_buf + 1 * 512);
        bios_write_sector(FS_TABLE_LBA + 2, fs_io_buf + 2 * 512);
        bios_write_sector(FS_TABLE_LBA + 3, fs_io_buf + 3 * 512);
        pic_init();
        idt_reload(); 
    }
}

void fs_init() {
    memset(current_dir, 0, 32);
    current_dir[0] = '/';
    uint8_t status = port_byte_in(0x1F7);
    if (status != 0xFF && status != 0x00) {
        fs_use_ata = 1;
    } else {
        fs_use_ata = 0;
        thunk_init(0x80);
    }

    memset(fs_io_buf, 0, sizeof(fs_io_buf));
    if (fs_use_ata) {
        ata_read_sectors(0, FS_TABLE_LBA, FS_TABLE_SECTORS, fs_io_buf);
    } else {
        bios_read_sector(FS_TABLE_LBA + 0, fs_io_buf + 0 * 512);
        bios_read_sector(FS_TABLE_LBA + 1, fs_io_buf + 1 * 512);
        bios_read_sector(FS_TABLE_LBA + 2, fs_io_buf + 2 * 512);
        bios_read_sector(FS_TABLE_LBA + 3, fs_io_buf + 3 * 512);
        pic_init();
        idt_reload(); 
    }
    memcpy(fs, fs_io_buf, sizeof(file_t) * MAX_FILES);
}

int fs_exists(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            return 1;
        }
    }
    return 0;
}

void fs_list(int* row) {
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].dir, current_dir) == 0) {
            char line[72];
            memset(line, 0, 72);
            if (fs[i].is_dir) strcpy(line, "DIR:  ");
            else strcpy(line, "FILE: ");
            strcat(line, fs[i].name);
            print_line_scroll(line, 0, row, fs[i].is_dir ? 0x0B : 0x0F);
            if (!fs[i].is_dir) {
                char size_line[32];
                memset(size_line, 0, 32);
                strcpy(size_line, "SIZE: ");
                char s_buf[16];
                itoa(fs[i].size_bytes, s_buf);
                strcat(size_line, s_buf);
                strcat(size_line, " bytes");
                print_line_scroll(size_line, 6, row, 0x07);
            }
            found = 1;
        }
    }
    if (!found) print_line_scroll("Directory is empty.", 0, row, 0x07);
}

uint32_t find_free_lba() {
    uint32_t last_lba = DATA_REGION_START;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists) {
            uint32_t end = fs[i].start_lba + (fs[i].size_bytes / 512) + 1;
            if (end > last_lba) last_lba = end;
        }
    }
    return last_lba;
}

int fs_create(char* name, int* row) {
    if (strlen(name) >= 32) {
        print_line_scroll("Error: Name too long!", 0, row, 0x0C);
        return 0;
    }
    for (int i = 0; i < MAX_FILES; i++)
        if (fs[i].exists && strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) return 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) {
            memset(&fs[i], 0, sizeof(file_t));
            strcpy(fs[i].name, name);
            strcpy(fs[i].dir, current_dir);
            fs[i].start_lba = find_free_lba();
            fs[i].size_bytes = 0;
            fs[i].exists = 1;
            fs[i].is_dir = 0;
            fs_flush();
            return 1;
        }
    }
    return 0;
}

int fs_mkdir(char* name, int* row) {
    if (strlen(name) >= 32) {
        print_line_scroll("Error: Name too long!", 0, row, 0x0C);
        return 0;
    }
    for (int i = 0; i < MAX_FILES; i++)
        if (fs[i].exists && strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) return 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) {
            memset(&fs[i], 0, sizeof(file_t));
            strcpy(fs[i].name, name);
            strcpy(fs[i].dir, current_dir);
            fs[i].start_lba = 0;
            fs[i].size_bytes = 0;
            fs[i].exists = 1;
            fs[i].is_dir = 1;
            fs_flush();
            return 1;
        }
    }
    return 0;
}

int fs_cd(char* path, int* row) {
    if (strcmp(path, "/") == 0) {
        memset(current_dir, 0, 32);
        current_dir[0] = '/';
        return 1;
    }
    if (strcmp(path, "..") == 0) {
        if (strcmp(current_dir, "/") == 0) return 1;
        int len = strlen(current_dir);
        int last_slash_idx = -1;
        for (int i = len - 1; i >= 0; i--) {
            if (current_dir[i] == '/') { last_slash_idx = i; break; }
        }
        if (last_slash_idx <= 0) {
            memset(current_dir, 0, 32);
            current_dir[0] = '/';
        } else {
            current_dir[last_slash_idx] = '\0';
        }
        return 1;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && fs[i].is_dir &&
            strcmp(fs[i].name, path) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            char new_dir[32];
            memset(new_dir, 0, 32);
            if (strcmp(current_dir, "/") == 0) {
                new_dir[0] = '/';
                strcpy(new_dir + 1, path);
            } else {
                strcpy(new_dir, current_dir);
                int len = strlen(new_dir);
                new_dir[len] = '/';
                strcpy(new_dir + len + 1, path);
            }
            strcpy(current_dir, new_dir);
            return 1;
        }
    }
    print_line_scroll("Error: Directory not found.", 0, row, 0x0C);
    return 0;
}

int fs_write(char* name, uint8_t* data, uint32_t len) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && !fs[i].is_dir &&
            strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            fs[i].size_bytes = len;
            fs_flush();
            uint32_t sectors = (len / 512) + 1;
            for (uint32_t s = 0; s < sectors; s++) {
                uint8_t buf[512];
                memset(buf, 0, 512);
                uint32_t offset = s * 512;
                uint32_t to_copy = (len - offset > 512) ? 512 : (len - offset);
                memcpy(buf, data + offset, to_copy);
                fs_write_sector(fs[i].start_lba + s, buf);
            }
            return 1;
        }
    }
    return 0;
}

int fs_delete(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            if (!fs[i].is_dir && fs[i].start_lba >= DATA_REGION_START) {
                uint32_t sectors = (fs[i].size_bytes / 512) + 1;
                uint8_t zero[512];
                memset(zero, 0, 512);
                for (uint32_t s = 0; s < sectors; s++) {
                    fs_write_sector(fs[i].start_lba + s, zero);
                }
            }
            fs[i].exists = 0;
            fs_flush();
            return 1;
        }
    }
    return 0;
}

void fs_format(uint32_t magic) {
    if (magic != 0xCA705) return;
    uint8_t zero[512];
    memset(zero, 0, 512);
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && !fs[i].is_dir &&
            fs[i].start_lba >= DATA_REGION_START) {
            uint32_t sectors = (fs[i].size_bytes / 512) + 1;
            for (uint32_t s = 0; s < sectors; s++) {
                fs_write_sector(fs[i].start_lba + s, zero);
            }
        }
    }
    memset(fs, 0, sizeof(file_t) * MAX_FILES);
    memset(current_dir, 0, 32);
    current_dir[0] = '/';
    fs_flush();
}

int fs_rename(char* old_name, char* new_name) {
    if (strlen(new_name) >= 32) {
        return 0;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, old_name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            for (int j = 0; j < MAX_FILES; j++) {
                if (fs[j].exists && strcmp(fs[j].name, new_name) == 0 &&
                    strcmp(fs[j].dir, current_dir) == 0) {
                    return 0;
                }
            }
            strcpy(fs[i].name, new_name);
            fs_flush();
            return 1;
        }
    }
    return 0;
}

int fs_load_to_memory(char* name, uint8_t* address) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && !fs[i].is_dir &&
            strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            uint32_t sectors = (fs[i].size_bytes / 512) + 1;
            for (uint32_t s = 0; s < sectors; s++) {
                fs_read_sector(fs[i].start_lba + s, address + s * 512);
            }
            return 1;
        }
    }
    return 0;
}