#include "commands.h"
#include "acpi.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "drivers/io.h"
#include "kernel/config.h"

void cmd_shutdown(char* args, int* row) {
    clear_screen();
    disable_cursor();
    const char* msg1 = "CawOS has been shut down.";
    const char* msg2 = "It is now safe to turn off your computer.";
    int cols = screen_get_cols();
    int rows = screen_get_rows();
    int col1 = (cols - (int)strlen(msg1)) / 2;
    int col2 = (cols - (int)strlen(msg2)) / 2;
    int row1 = rows / 2;
    int row2 = rows / 2 + 1;
    if (col1 < 0) col1 = 0;
    if (col2 < 0) col2 = 0;
    print_at_color((char*)msg1, row1, col1, 0x0F);
    print_at_color((char*)msg2, row2, col2, 0x0B);
    config_set_shutdown_clean(1);
    __asm__ __volatile__("cli");
    acpi_shutdown();
    for(;;) __asm__ __volatile__("hlt");
}

REGISTER_COMMAND("shutdown", cmd_shutdown, 0);