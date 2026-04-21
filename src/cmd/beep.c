#include "commands.h"
#include "screen.h"

extern void beep(); 

void cmd_beep(char* args, int* row) {
    (void)args;
    beep();
    print_line_scroll("BEEEP!", 0, row, 0x0F);
}

REGISTER_COMMAND("beep", cmd_beep, 0);