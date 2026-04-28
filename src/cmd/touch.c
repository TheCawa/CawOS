#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

void cmd_touch(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: touch <filename>", 0, row, 0x0E);
        return;
    }
    if (fs_create(args, row)) {
        print_line_scroll("File created.", 0, row, 0x0A);
    } else {
        print_line_scroll("Error: Could not create file.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("touch", cmd_touch, 1);