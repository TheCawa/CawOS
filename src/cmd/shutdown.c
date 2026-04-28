#include "commands.h"
#include "acpi.h"
#include "screen.h"
#include "util.h"
#include "io.h"

void cmd_shutdown(char* args, int* row) {
    clear_screen();
    disable_cursor();
    const char* msg1 = "CawOS has been shut down.";
    const char* msg2 = "It is now safe to turn off your computer.";
    int col1 = (80 - strlen(msg1)) / 2;
    int col2 = (80 - strlen(msg2)) / 2;
    print_at_color((char*)msg1, 11, col1, 0x0F);
    print_at_color((char*)msg2, 12, col2, 0x0B);
    __asm__ __volatile__("cli");
    acpi_shutdown();
    for(;;) __asm__ __volatile__("hlt");
}

REGISTER_COMMAND("shutdown", cmd_shutdown, 0);