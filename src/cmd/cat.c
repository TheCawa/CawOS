#include "fs.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "commands.h"
#include "kernel/memory.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

void cmd_cat(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_line_scroll("Usage: cat <filename>", 0, row, 0x0E);
        return;
    }
    if (strcmp(args, "boot_sound_cawos") == 0) {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
        return;
    }

    uint32_t file_size = fs_get_size(args);
    if (file_size == 0) {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
        return;
    }

    uint32_t sectors = (file_size / 512) + 1;
    uint32_t buffer_size = sectors * 512;
    char* file_buffer = (char*)malloc(buffer_size);
    if (!file_buffer) {
        print_line_scroll("Error: Out of memory.", 0, row, 0x0C);
        return;
    }

    memset(file_buffer, 0, buffer_size);

    if (fs_load_to_memory(args, (uint8_t*)file_buffer)) {
        char line[256];
        int line_idx = 0;

        for (uint32_t i = 0; i < file_size; i++) {
            char c = file_buffer[i];

            if (c == '\n' || line_idx >= 255) {
                line[line_idx] = '\0';
                print_line_scroll(line, 0, row, 0x0E);
                line_idx = 0;
            } else if (c != '\r') {
                line[line_idx++] = c;
            }
        }

        if (line_idx > 0) {
            line[line_idx] = '\0';
            print_line_scroll(line, 0, row, 0x0E);
        }

    } else {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
    }

    free(file_buffer);
}

REGISTER_COMMAND("cat", cmd_cat, 1);