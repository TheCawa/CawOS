#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"

void cmd_write(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_at("Usage: write <file> <text>", *row, 0);
        (*row)++; return;
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
            print_at_color("Data written.", *row, 0, 0x0A);
        } else {
            print_at_color("Error: File not found.", *row, 0, 0x0C);
        }
    } else {
        print_at("Error: No text provided.", *row, 0);
    }
    (*row)++;
}

REGISTER_COMMAND("write", cmd_write, 1);