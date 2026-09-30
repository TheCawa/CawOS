#include "commands.h"
#include "drivers/screen.h"
#include "gui/desktop.h"
#include "acpi.h"
#include "kernel/config.h"

void cmd_gfx(char* args, int* row) {
    if (!g_is_graphics) {
        print_line_scroll("Error: Graphics mode not available", 0, row, 0x0C);
        return;
    }
    print_line_scroll("Entering desktop mode...", 0, row, 0x0A);
    g_desktop_exit_requested = 0;
    g_desktop_exit_reason = 0;
    desktop_run();
    clear_screen();
    *row = 0;
    if (g_desktop_exit_reason == 1) {
        print_line_scroll("Exited desktop mode", 0, row, 0x0F);
        enable_cursor(13, 15);
        return;
    }
    print_line_scroll("Exited desktop mode", 0, row, 0x0F);
    print_line_scroll("Shutdown system...", 0, row, 0x0F);
    config_set_shutdown_clean(1);
    acpi_shutdown();
    for (;;) __asm__ volatile("hlt");
}

REGISTER_COMMAND("gfx", cmd_gfx, 0);