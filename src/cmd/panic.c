#include "commands.h"
#include "util.h"
#include "screen.h"

void cmd_panic(char* args, int* row) {
    print_at_color("Initiating system crash...", *row, 0, 0x0E);
    (*row)++;
    sleep_ms(20);
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c; 
}

REGISTER_COMMAND("panic", cmd_panic, 0);