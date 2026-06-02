#include "commands.h"
#include "drivers/screen.h"
#include "gui/desktop.h"
#include "acpi.h"

void cmd_gfx(char* args, int* row) {
    if (!g_is_graphics) {
        print_line_scroll("Error: Graphics mode not available", 0, row, 0x0C);
        return;
    }
    print_line_scroll("Entering desktop mode...", 0, row, 0x0A);
    g_desktop_exit_requested = 0;
    desktop_run();
    clear_screen();
    *row = 0;
    print_line_scroll("Exited desktop mode", 0, row, 0x0F);
    print_line_scroll("Shutdown system...", 0, row, 0x0F);
    acpi_shutdown();
}

REGISTER_COMMAND("gfx", cmd_gfx, 0);