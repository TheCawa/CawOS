#include "commands.h"
#include "fs.h"
#include "screen.h"

void cmd_ls(char* args, int* row) {
    (void)args; 
    print_at_color("Files:", *row, 0, 0x0B);
    (*row)++;
    
    fs_list(row);
}

REGISTER_COMMAND("ls", cmd_ls, 0);