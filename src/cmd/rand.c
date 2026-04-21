#include "commands.h"
#include "util.h"
#include "screen.h"

#ifndef NULL
#define NULL ((void*)0)
#endif


void cmd_rand(char* args, int* row) {
    int r = rand();
    char buf[32];
    strcpy(buf, "Random number: ");
    char num[16];
    itoa(r, num);
    strcat(buf, num);
    print_line_scroll(buf, 0, row, 0x0A);
}

REGISTER_COMMAND("rand", cmd_rand, 0);