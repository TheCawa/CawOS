#include "commands.h"
#include "util.h"
#include "screen.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

void cmd_dice(char* args, int* row) {
    int roll = rand(1, 6);
    char buf[16];
    char num[4];
    strcpy(buf, "You rolled: ");
    num[0] = roll + '0';
    num[1] = '\0';
    strcat(buf, num);
    print_line_scroll(buf, 0, row, 0x0E);
}

REGISTER_COMMAND("dice", cmd_dice, 0);