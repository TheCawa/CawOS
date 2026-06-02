#include "commands.h"
#include "drivers/screen.h"
#include "libc/util.h"

extern volatile uint32_t system_ticks;

void cmd_uptime(char* args, int* row) {
    uint32_t total_seconds = system_ticks / 100;
    uint32_t days = total_seconds / 86400;
    uint32_t hours = (total_seconds % 86400) / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;
    char buffer[64];
    char num_buf[12];
    strcpy(buffer, "System uptime: ");
    itoa(days, num_buf);
    strcat(buffer, num_buf);
    strcat(buffer, " days, ");
    if (hours < 10) strcat(buffer, "0");
    itoa(hours, num_buf);
    strcat(buffer, num_buf);
    strcat(buffer, ":");
    if (minutes < 10) strcat(buffer, "0");
    itoa(minutes, num_buf);
    strcat(buffer, num_buf);
    strcat(buffer, ":");
    if (seconds < 10) strcat(buffer, "0");
    itoa(seconds, num_buf);
    strcat(buffer, num_buf);
    print_line_scroll(buffer, 0, row, 0x0F);
}
REGISTER_COMMAND("uptime", cmd_uptime, 0);