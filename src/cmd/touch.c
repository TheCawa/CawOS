#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

void cmd_touch(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_at_color("Usage: touch <filename>", *row, 0, 0x0E);
        (*row)++; return;
    }

    if (fs_create(args)) {
        print_at_color("File created.", *row, 0, 0x0A);
    } else {
        print_at_color("Error: Could not create file.", *row, 0, 0x0C);
    }
    (*row)++;
}

REGISTER_COMMAND("touch", cmd_touch, 1);