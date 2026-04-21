#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

void cmd_rm(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: rm <filename>", 0, row, 0x0E);
        return;
    }
    if (fs_delete(args)) {
        print_line_scroll("File removed.", 0, row, 0x0A);
    } else {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("rm", cmd_rm, 1);