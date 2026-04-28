#include "commands.h"
#include "fs.h"
#include "screen.h"
#include "util.h"

void cmd_format(char* args, int* row) {
    if (args == 0 || strcmp(args, "confirm") != 0) {
        print_line_scroll("WARNING: This will delete ALL files!", 0, row, 0x0E);
        print_line_scroll("Type 'format confirm' to proceed.", 0, row, 0x0E);
        return;
    }

    fs_format(0xCA705);
    print_line_scroll("Drive formatted successfully.", 0, row, 0x0A);
}

REGISTER_COMMAND("format", cmd_format, 1);