#include "commands.h"
#include "fs.h"
#include "screen.h"

void cmd_ls(char* args, int* row) {
    (void)args;
    print_line_scroll("Files:", 0, row, 0x0B);
    fs_list(row);
}

REGISTER_COMMAND("ls", cmd_ls, 0);