#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

void cmd_write(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: write <file> <text>", 0, row, 0x0E);
        return;
    }
    char fname[32];
    int i = 0;
    while (args[i] != ' ' && args[i] != '\0' && i < 31) {
        fname[i] = args[i];
        i++;
    }
    fname[i] = '\0';

    if (args[i] == ' ') {
        char* text = args + i + 1;
        uint32_t len = strlen(text);
        if (fs_write(fname, (uint8_t*)text, len)) {
            print_line_scroll("Data written.", 0, row, 0x0A);
        } else {
            print_line_scroll("Error: File not found.", 0, row, 0x0C);
        }
    } else {
        print_line_scroll("Error: No text provided.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("write", cmd_write, 1);