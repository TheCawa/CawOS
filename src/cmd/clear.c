#include "commands.h"
#include "screen.h"

void cmd_clear(char* args, int* row) {
    clear_screen();
    *row = 0;
}

REGISTER_COMMAND("clear", cmd_clear, 0);