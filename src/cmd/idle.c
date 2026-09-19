#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "kernel/config.h"

extern uint32_t g_idle_timeout_ticks;
extern int g_idle_enabled;

void cmd_idle(char* args, int* row) {
    char msg[64];
    char nb[16];

    if (args == NULL || args[0] == '\0') {
        if (!g_idle_enabled) {
            print_line_scroll("Idle screensaver: disabled", 0, row, 0x0F);
        } else {
            int minutes = (int)(g_idle_timeout_ticks / 6000);
            memset(msg, 0, 64);
            strcpy(msg, "Idle screensaver: after ");
            itoa(minutes, nb); strcat(msg, nb);
            strcat(msg, " min of no input");
            print_line_scroll(msg, 0, row, 0x0F);
        }
        return;
    }

    while (*args == ' ') args++;

    if (strcasecmp(args, "off") == 0) {
        g_idle_enabled = 0;
        config_set_idle(0, 0);
        print_line_scroll("Idle screensaver disabled.", 0, row, 0x0E);
        return;
    }
    if (strcasecmp(args, "on") == 0) {
        g_idle_enabled = 1;
        int m = (int)(g_idle_timeout_ticks / 6000);
        config_set_idle(1, (m > 0) ? m : 1);
        print_line_scroll("Idle screensaver enabled.", 0, row, 0x0E);
        return;
    }

    int minutes = atoi(args);
    if (minutes <= 0) {
        print_line_scroll("Usage: idle [minutes|on|off]", 0, row, 0x0E);
        return;
    }
    g_idle_enabled = 1;
    g_idle_timeout_ticks = (uint32_t)minutes * 60 * 100;
    config_set_idle(1, minutes);
    memset(msg, 0, 64);
    strcpy(msg, "Idle screensaver: after ");
    itoa(minutes, nb); strcat(msg, nb);
    strcat(msg, " min of no input");
    print_line_scroll(msg, 0, row, 0x0E);
}
REGISTER_COMMAND("idle", cmd_idle, 1);