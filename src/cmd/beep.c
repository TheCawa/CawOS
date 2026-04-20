#include "commands.h"
#include "screen.h"

extern void beep(); 

void cmd_beep(char* args, int* row) {
    (void)args;
    beep();
    print_at("BEEEP!", *row, 0);
}

REGISTER_COMMAND("beep", cmd_beep, 0);