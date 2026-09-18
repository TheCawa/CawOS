#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "fs.h"

extern file_t fs[MAX_FILES];

static int name_matches(const char* filename, const char* pattern) {
    return (strcasestr(filename, pattern) != NULL);
}

static void print_found(int idx, int* row, int show_size) {
    char line[96];
    memset(line, 0, 96);
    
    if (strcmp(fs[idx].dir, "/") == 0) {
        strcpy(line, "/");
    } else {
        strcpy(line, fs[idx].dir);
        strcat(line, "/");
    }
    strcat(line, fs[idx].name);
    
    unsigned char color = fs[idx].is_dir ? 0x0B : 0x0F;
    print_line_scroll(line, 0, row, color);

    if (show_size && !fs[idx].is_dir) {
        char size_line[32];
        char num_buf[16];
        memset(size_line, 0, 32);
        strcpy(size_line, "    ");
        itoa(fs[idx].size_bytes, num_buf);
        strcat(size_line, num_buf);
        strcat(size_line, " bytes");
        print_line_scroll(size_line, 0, row, 0x07);
    }
}

static void find_help(int* row) {
    print_line_scroll("Usage: find [OPTIONS] <pattern>", 0, row, 0x0B);
    print_line_scroll("", 0, row, 0x0F);
    print_line_scroll("Options:", 0, row, 0x0E);
    print_line_scroll("  (no args)        List all files in all directories", 0, row, 0x0F);
    print_line_scroll("  <pattern>        Search files by name (case-insensitive)", 0, row, 0x0F);
    print_line_scroll("  --dir <pattern>  Search only directories", 0, row, 0x0F);
    print_line_scroll("  --file <pattern> Search only files", 0, row, 0x0F);
    print_line_scroll("  --size           Show file sizes", 0, row, 0x0F);
    print_line_scroll("  --help           Show this help", 0, row, 0x0F);
}

void cmd_find(char* args, int* row) {
    int found_count = 0;
    int show_size = 0;
    int only_dirs = 0;
    int only_files = 0;
    char pattern[64];
    memset(pattern, 0, 64);
    if (args == NULL || args[0] == '\0') {
        pattern[0] = '\0';
    } else {
        char* p = args;
        while (*p == ' ') p++;
        if (strcasecmp(p, "--help") == 0 || strcasecmp(p, "-h") == 0) {
            find_help(row);
            return;
        }
        if (strncasecmp(p, "--dir", 5) == 0 && (p[5] == ' ' || p[5] == '\0')) {
            only_dirs = 1;
            p += 5;
            while (*p == ' ') p++;
        } else if (strncasecmp(p, "--file", 6) == 0 && (p[6] == ' ' || p[6] == '\0')) {
            only_files = 1;
            p += 6;
            while (*p == ' ') p++;
        }
        if (strstr(args, "--size") != NULL) {
            show_size = 1;
            char* size_pos = strstr(p, "--size");
            if (size_pos != NULL) {
                int len_before = size_pos - p;
                if (len_before > 0 && len_before < 63) {
                    strncpy(pattern, p, len_before);
                    pattern[len_before] = '\0';
                }
            } else {
                safe_strcpy(pattern, p, 63);
            }
        } else {
            safe_strcpy(pattern, p, 63);
        }
        int plen = strlen(pattern);
        while (plen > 0 && pattern[plen - 1] == ' ') {
            pattern[--plen] = '\0';
        }
    }
    char header[80];
    memset(header, 0, 80);
    if (pattern[0] != '\0') {
        strcpy(header, "Searching for: \"");
        strcat(header, pattern);
        strcat(header, "\"...");
    } else {
        strcpy(header, "All files:");
    }
    print_line_scroll(header, 0, row, 0x0E);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) continue;
        if (strcmp(fs[i].name, "boot_sound_cawos") == 0) continue;
        if (only_dirs && !fs[i].is_dir) continue;
        if (only_files && fs[i].is_dir) continue;
        if (pattern[0] != '\0') {
            if (!name_matches(fs[i].name, pattern)) continue;
        }
        
        print_found(i, row, show_size);
        found_count++;
    }

    char result[48];
    char num_buf[12];
    memset(result, 0, 48);
    itoa(found_count, num_buf);
    strcpy(result, num_buf);
    strcat(result, found_count == 1 ? " item found." : " items found.");
    print_line_scroll(result, 0, row, found_count > 0 ? 0x0A : 0x0C);
}

REGISTER_COMMAND("find", cmd_find, 1);