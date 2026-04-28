#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

char* strchr(const char* s, int c); 

void cmd_rename(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: rename <old_name> <new_name>", 0, row, 0x0E);
        return;
    }
    char* space_ptr = strchr(args, ' ');
    if (!space_ptr) {
        print_line_scroll("Error: Two arguments required.", 0, row, 0x0C);
        return;
    }
    *space_ptr = '\0';
    char* old_name = args;
    char* new_name = space_ptr + 1;
    while (*new_name == ' ') new_name++;
    if (strlen(new_name) == 0) {
        print_line_scroll("Error: New name cannot be empty.", 0, row, 0x0C);
        return;
    }
    if (fs_rename(old_name, new_name)) {
        print_line_scroll("File renamed successfully.", 0, row, 0x0A);
    } else {
        print_line_scroll("Error: File not found or name already exists.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("rename", cmd_rename, 1);