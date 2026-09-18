#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "fs.h"
#include "kernel/memory.h"

extern file_t fs[MAX_FILES];
extern char current_dir[32];

void cmd_cp(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_line_scroll("Usage: cp <source> <dest>", 0, row, 0x0C);
        return;
    }
    char source[32];
    char dest[32];
    memset(source, 0, 32);
    memset(dest, 0, 32);
    while (*args == ' ') args++;
    int i = 0;
    while (*args && *args != ' ' && i < 31) {
        source[i++] = *args++;
    }
    source[i] = '\0';
    while (*args == ' ') args++;
    i = 0;
    while (*args && *args != ' ' && i < 31) {
        dest[i++] = *args++;
    }
    dest[i] = '\0';
    if (source[0] == '\0' || dest[0] == '\0') {
        print_line_scroll("Usage: cp <source> <dest>", 0, row, 0x0C);
        return;
    }
    if (strcmp(source, dest) == 0) {
        print_line_scroll("Error: Source and dest are the same file.", 0, row, 0x0C);
        return;
    }
    if (!fs_exists(source)) {
        print_line_scroll("Error: Source file not found.", 0, row, 0x0C);
        return;
    }
    for (int j = 0; j < MAX_FILES; j++) {
        if (fs[j].exists && strcmp(fs[j].name, source) == 0 &&
            strcmp(fs[j].dir, current_dir) == 0) {
            if (fs[j].is_dir) {
                print_line_scroll("Error: Cannot copy directories.", 0, row, 0x0C);
                return;
            }
            break;
        }
    }
    if (strcmp(source, "boot_sound_cawos") == 0 ||
        strcmp(dest, "boot_sound_cawos") == 0) {
        return;
    }
    uint32_t size = fs_get_size(source);
    uint8_t* buffer = (uint8_t*)malloc(size > 0 ? size : 1);
    if (!buffer) {
        print_line_scroll("Error: Not enough memory.", 0, row, 0x0C);
        return;
    }
    memset(buffer, 0, size > 0 ? size : 1);
    if (!fs_load_to_memory(source, buffer)) {
        free(buffer);
        print_line_scroll("Error: Failed to read source.", 0, row, 0x0C);
        return;
    }
    if (!fs_exists(dest)) {
        if (!fs_create(dest, row)) {
            free(buffer);
            print_line_scroll("Error: Failed to create destination.", 0, row, 0x0C);
            return;
        }
    }
    if (!fs_write(dest, buffer, size)) {
        free(buffer);
        print_line_scroll("Error: Failed to write destination.", 0, row, 0x0C);
        return;
    }

    free(buffer);
    char msg[96];
    char size_str[16];
    memset(msg, 0, 96);
    strcpy(msg, "Copied '");
    strcat(msg, source);
    strcat(msg, "' -> '");
    strcat(msg, dest);
    strcat(msg, "' (");
    itoa(size, size_str);
    strcat(msg, size_str);
    strcat(msg, " bytes)");
    print_line_scroll(msg, 0, row, 0x0A);
}

REGISTER_COMMAND("cp", cmd_cp, 1);