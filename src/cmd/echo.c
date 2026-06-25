#include "commands.h"
#include "drivers/screen.h"
#include "libc/util.h"

void cmd_echo(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: echo <text>", 0, row, 0x0E);
        return;
    }
    if (!args || args[0] == '\0') {
        print_line_scroll("", 0, row, current_color);
        return;
    }
    print_line_scroll(args, 0, row, current_color);
}

REGISTER_COMMAND("echo", cmd_echo, 1);