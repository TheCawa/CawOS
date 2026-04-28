#include "commands.h"
#include "fs.h"
#include "screen.h"

void cmd_mkdir(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: mkdir <dirname>", 0, row, 0x0E);
        return;
    }
    if (fs_mkdir(args, row)) {
        print_line_scroll("Directory created.", 0, row, 0x0A);
    } else {
        print_line_scroll("Error: Could not create directory.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("mkdir", cmd_mkdir, 1);