#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

void cmd_rm(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_at("Usage: rm <filename>", *row, 0);
        (*row)++; return;
    }

    if (fs_delete(args)) {
        print_at_color("File removed.", *row, 0, 0x0A);
    } else {
        print_at_color("Error: File not found.", *row, 0, 0x0C);
    }
    (*row)++;
}

REGISTER_COMMAND("rm", cmd_rm, 1);