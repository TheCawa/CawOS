#include "commands.h"
#include "fs.h"
#include "screen.h"

void cmd_cd(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: cd <dirname>", 0, row, 0x0E);
        return;
    }
    fs_cd(args, row);
}

REGISTER_COMMAND("cd", cmd_cd, 1);