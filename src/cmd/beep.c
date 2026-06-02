#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"

extern void beep(int duration, int frequency); 

void cmd_beep(char* args, int* row) {
    int frequency = 1000;
    int duration = 200;
    if (args && args[0] != '\0') {
        char* space_ptr = strchr(args, ' ');    
        if (space_ptr) {
            *space_ptr = '\0';
            frequency = atoi(args);
            duration = atoi(space_ptr + 1);
        } else {
            frequency = atoi(args);
        }
    }
    print_line_scroll("BEEEP!", 0, row, 0x0F);
    beep(duration, frequency);
    
}

REGISTER_COMMAND("beep", cmd_beep, 1);