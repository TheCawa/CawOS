#include "commands.h"
#include "util.h"
#include "screen.h"

#ifndef NULL
#define NULL ((void*)0)
#endif


void cmd_rand(char* args, int* row) {
    int min = 0;
    int max = 100;
    if (args && args[0] != '\0') {
        char* space = 0;
        for (char* p = args; *p != '\0'; p++) {
            if (*p == ' ') { space = p; break; }
        }
        if (space) {
            *space = '\0';
            min = atoi(args);
            max = atoi(space + 1);
            *space = ' ';
        } else {
            max = atoi(args);
        }
    }
    int r = rand(min, max);
    char buf[32];
    char num[16];
    itoa(r, num);
    strcpy(buf, "Random number: ");
    strcat(buf, num);
    print_line_scroll(buf, 0, row, 0x0A);
}

REGISTER_COMMAND("rand", cmd_rand, 1);