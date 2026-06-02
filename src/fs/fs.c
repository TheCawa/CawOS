#include "fs.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "drivers/io.h"
#include "kernel/idt.h"
#include "drivers/ata.h"
#include "kernel/memory.h"

file_t fs[MAX_FILES];
uint16_t cawfat[MAX_CLUSTERS];
char current_dir[32];
static uint8_t* fs_io_buf = NULL;
static int fs_use_ata = 0;
extern void thunk_init(uint8_t drive);

static void fs_write_sector(uint32_t lba, uint8_t* buf) {
    if (fs_use_ata) {
        ata_write_sectors(0, lba, 1, buf);
    } else {
        bios_write_sector(lba, buf);
        pic_init();
        idt_reload();
        while (port_byte_in(0x64) & 1) {
            port_byte_in(0x60);
        }
        
        __asm__ volatile("sti");
    }
}

static void fs_read_sector(uint32_t lba, uint8_t* buf) {
    if (fs_use_ata) {
        ata_read_sectors(0, lba, 1, buf);
    } else {
        bios_read_sector(lba, buf);
        pic_init();
        idt_reload();
        while (port_byte_in(0x64) & 1) {
            port_byte_in(0x60);
        }
        __asm__ volatile("sti");
    }
}

static int find_free_cluster() {
    for (int i = 1; i < MAX_CLUSTERS; i++) {
        if (cawfat[i] == CLUSTER_EMPTY) {
            return i;
        }
    }
    return -1;
}

static void free_cluster_chain(uint16_t start_cluster) {
    if (start_cluster == CLUSTER_EOF || start_cluster == 0) return;
    uint16_t current = start_cluster;
    while (current != CLUSTER_EOF && current < MAX_CLUSTERS) {
        uint16_t next = cawfat[current];
        cawfat[current] = CLUSTER_EMPTY;
        current = next;
    }
}

void fs_flush() {
    if (!fs_io_buf) return;
    memset(fs_io_buf, 0, FS_TABLE_SECTORS * 512);
    memcpy(fs_io_buf, fs, sizeof(file_t) * MAX_FILES);
    if (fs_use_ata) {
        ata_write_sectors(0, FS_TABLE_LBA, FS_TABLE_SECTORS, fs_io_buf);
        ata_write_sectors(0, CAWFAT_LBA, CAWFAT_SECTORS, (uint8_t*)cawfat);
    } else {
        for (int i = 0; i < FS_TABLE_SECTORS; i++) {
            fs_write_sector(FS_TABLE_LBA + i, fs_io_buf + i * 512);
        }
        for (int i = 0; i < CAWFAT_SECTORS; i++) {
            fs_write_sector(CAWFAT_LBA + i, ((uint8_t*)cawfat) + i * 512);
        }
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
    fs_io_buf = (uint8_t*)malloc(512 * FS_TABLE_SECTORS);
    if (!fs_io_buf) return;
    memset(fs_io_buf, 0, 512 * FS_TABLE_SECTORS);
    if (fs_use_ata) {
        ata_read_sectors(0, FS_TABLE_LBA, FS_TABLE_SECTORS, fs_io_buf);
        ata_read_sectors(0, CAWFAT_LBA, CAWFAT_SECTORS, (uint8_t*)cawfat);
    } else {
        for (int i = 0; i < FS_TABLE_SECTORS; i++) {
            fs_read_sector(FS_TABLE_LBA + i, fs_io_buf + i * 512);
        }
        for (int i = 0; i < CAWFAT_SECTORS; i++) {
            fs_read_sector(CAWFAT_LBA + i, ((uint8_t*)cawfat) + i * 512);
        }
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

uint32_t fs_get_size(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            return fs[i].size_bytes;
        }
    }
    return 0;
}

void fs_list(int* row) {
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].dir, current_dir) == 0) {
            if (strcmp(fs[i].name, "boot_sound_cawos") == 0) continue; 
            
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

int fs_create(char* name, int* row) {
    if (strcmp(name, "boot_sound_cawos") == 0) return 0;
    if (strlen(name) >= 32) {
        print_line_scroll("Error: Name too long!", 0, row, 0x0C);
        return 0;
    }
    if (fs_exists(name)) return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) {
            memset(&fs[i], 0, sizeof(file_t));
            strcpy(fs[i].name, name);
            strcpy(fs[i].dir, current_dir);
            fs[i].first_cluster = CLUSTER_EOF;
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
    if (strcmp(name, "boot_sound_cawos") == 0) return 0;
    if (strlen(name) >= 32) {
        print_line_scroll("Error: Name too long!", 0, row, 0x0C);
        return 0;
    }
    if (fs_exists(name)) return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) {
            memset(&fs[i], 0, sizeof(file_t));
            strcpy(fs[i].name, name);
            strcpy(fs[i].dir, current_dir);
            fs[i].first_cluster = CLUSTER_EOF;
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
    if (strcmp(path, "boot_sound_cawos") == 0) return 0;
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

    if (strlen(current_dir) + strlen(path) + 2 > 32) {
        print_line_scroll("Error: Path too long!", 0, row, 0x0C);
        return 0;
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
    if (strcmp(name, "boot_sound_cawos") == 0) return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && !fs[i].is_dir &&
            strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
            free_cluster_chain(fs[i].first_cluster);
            fs[i].first_cluster = CLUSTER_EOF;
            if (len == 0) {
                fs[i].size_bytes = 0;
                fs_flush();
                return 1;
            }
            uint32_t sectors_needed = (len + 511) / 512;
            uint16_t prev_cluster = CLUSTER_EOF;
            for (uint32_t s = 0; s < sectors_needed; s++) {
                int free_idx = find_free_cluster();
                if (free_idx == -1) break;
                cawfat[free_idx] = CLUSTER_EOF;
                if (s == 0) {
                    fs[i].first_cluster = free_idx;
                } else {
                    cawfat[prev_cluster] = free_idx;
                }
                uint8_t buf[512];
                memset(buf, 0, 512);
                uint32_t offset = s * 512;
                uint32_t to_copy = (len - offset > 512) ? 512 : (len - offset);
                memcpy(buf, data + offset, to_copy);
                fs_write_sector(DATA_REGION_START + free_idx, buf);
                prev_cluster = free_idx;
            }

            fs[i].size_bytes = len;
            fs_flush();
            return 1;
        }
    }
    return 0;
}

int fs_write_file(char* name, uint8_t* data, uint32_t size) {
    return fs_write(name, data, size);
}

int fs_delete(char* name) {
    if (strcmp(name, "boot_sound_cawos") == 0) return 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {   
            if (!fs[i].is_dir) {
                free_cluster_chain(fs[i].first_cluster);
            } else {
                char target_dir[32];
                memset(target_dir, 0, 32);
                if (strcmp(current_dir, "/") == 0) {
                    target_dir[0] = '/';
                    strcpy(target_dir + 1, name);
                } else {
                    strcpy(target_dir, current_dir);
                    int len = strlen(target_dir);
                    if (len < 30) {
                        target_dir[len] = '/';
                        strcpy(target_dir + len + 1, name);
                    }
                }
                for (int j = 0; j < MAX_FILES; j++) {
                    if (fs[j].exists) {
                        int match = 0;
                        if (strcmp(fs[j].dir, target_dir) == 0) {
                            match = 1;
                        } else {
                            int target_len = strlen(target_dir);
                            int k = 0;
                            while (k < target_len && fs[j].dir[k] != '\0' && fs[j].dir[k] == target_dir[k]) {
                                k++;
                            }
                            if (k == target_len && fs[j].dir[k] == '/') {
                                match = 1;
                            }
                        }
                        if (match) {
                            if (!fs[j].is_dir) {
                                free_cluster_chain(fs[j].first_cluster);
                            }
                            fs[j].exists = 0;
                        }
                    }
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
    file_t boot_backup;
    int boot_found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, "boot_sound_cawos") == 0) {
            memcpy(&boot_backup, &fs[i], sizeof(file_t));
            boot_found = 1;
            break;
        }
    }
    uint8_t protected_map[MAX_CLUSTERS];
    memset(protected_map, 0, MAX_CLUSTERS);
    if (boot_found && boot_backup.first_cluster != CLUSTER_EOF) {
        uint16_t curr = boot_backup.first_cluster;
        while (curr != CLUSTER_EOF && curr < MAX_CLUSTERS) {
            protected_map[curr] = 1;
            curr = cawfat[curr];
        }
    }
    uint8_t zero[512];
    memset(zero, 0, 512);
    for (int i = 0; i < MAX_CLUSTERS; i++) {
        if (!protected_map[i]) {
            cawfat[i] = CLUSTER_EMPTY;
        }
    }
    memset(fs, 0, sizeof(file_t) * MAX_FILES);
    memset(current_dir, 0, 32);
    current_dir[0] = '/';
    if (boot_found) {
        fs[0] = boot_backup;
    }
    fs_flush();
}

int fs_rename(char* old_name, char* new_name) {
    if (strlen(new_name) >= 32) return 0;
    if (fs_exists(new_name)) return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].name, old_name) == 0 &&
            strcmp(fs[i].dir, current_dir) == 0) {
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
            uint16_t current_cluster = fs[i].first_cluster;
            uint32_t sector_offset = 0;
            while (current_cluster != CLUSTER_EOF && current_cluster < MAX_CLUSTERS) {
                uint32_t lba = DATA_REGION_START + current_cluster;
                fs_read_sector(lba, address + (sector_offset * 512));
                
                current_cluster = cawfat[current_cluster];
                sector_offset++;
            }
            return 1;
        }
    }
    return 0;
}