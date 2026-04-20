#include "fs.h"
#include "util.h"
#include "screen.h"
#include "commands.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

void cmd_cat(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_at_color("Usage: cat <filename>", *row, 0, 0x0E);
        (*row)++;
        return;
    }
    static char file_buffer[2048];
    memset(file_buffer, 0, 2048);

    if (fs_load_to_memory(args, (uint8_t*)file_buffer)) {
        print_at_color(file_buffer, *row, 0, 0x0E);
        (*row)++;
    } else {
        print_at_color("Error: File not found.", *row, 0, 0x0C);
        (*row)++;
    }
}

REGISTER_COMMAND("cat", cmd_cat, 1);