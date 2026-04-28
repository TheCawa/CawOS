#include "fs.h"
#include "util.h"
#include "screen.h"
#include "commands.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

void cmd_cat(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_line_scroll("Usage: cat <filename>", 0, row, 0x0E);
        return;
    }
    static char file_buffer[2048];
    memset(file_buffer, 0, 2048);

    if (fs_load_to_memory(args, (uint8_t*)file_buffer)) {
        print_line_scroll("", 0, row, 0x0E); 

        char* ptr = file_buffer;
        int col = 0;
        
        while (*ptr != '\0') {
            if (col >= 80) {
                col = 0;
                (*row)++;
                if (*row >= 25) { scroll(); *row = 24; }
            }
            if (*ptr == '\n') {
                col = 0;
                (*row)++;
                if (*row >= 25) { scroll(); *row = 24; }
            } else {
                print_char_at(*ptr, *row, col, 0x0E);
                col++;
            }
            ptr++;
        }
        if (col > 0) {
            (*row)++;
        }
        print_line_scroll("", 0, row, 0x0E);

    } else {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("cat", cmd_cat, 1);