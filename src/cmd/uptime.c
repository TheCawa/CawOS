#include "commands.h"
#include "drivers/screen.h"
#include "libc/util.h"

extern volatile uint32_t system_ticks;

void cmd_uptime(char* args, int* row) {
    uint32_t total_seconds = system_ticks / 100;
    if (args != NULL && strcasecmp(args, "--seconds") == 0) {
        char buf[16];
        itoa(total_seconds, buf);
        print_line_scroll(buf, 0, row, 0x0F);
        return;
    }
    uint32_t days = total_seconds / 86400;
    uint32_t hours = (total_seconds % 86400) / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;
    char buffer[80];
    char num_buf[12];
    memset(buffer, 0, 80);
    if (args != NULL && strcasecmp(args, "--short") == 0) {
        if (days > 0) {
            itoa(days, num_buf);
            strcat(buffer, num_buf);
            strcat(buffer, "d ");
        }
        if (hours > 0 || days > 0) {
            itoa(hours, num_buf);
            strcat(buffer, num_buf);
            strcat(buffer, "h ");
        }
        itoa(minutes, num_buf);
        strcat(buffer, num_buf);
        strcat(buffer, "m");
        print_line_scroll(buffer, 0, row, 0x0F);
        return;
    }
    strcpy(buffer, "System uptime: ");
    if (days > 0) {
        itoa(days, num_buf);
        strcat(buffer, num_buf);
        strcat(buffer, days == 1 ? " day, " : " days, ");
    }
    if (hours > 0 || days > 0) {
        if (hours < 10) strcat(buffer, "0");
        itoa(hours, num_buf);
        strcat(buffer, num_buf);
        strcat(buffer, ":");
    }
    if (minutes < 10) strcat(buffer, "0");
    itoa(minutes, num_buf);
    strcat(buffer, num_buf);
    strcat(buffer, ":"); 
    if (seconds < 10) strcat(buffer, "0");
    itoa(seconds, num_buf);
    strcat(buffer, num_buf);
    print_line_scroll(buffer, 0, row, 0x0F);
}
REGISTER_COMMAND("uptime", cmd_uptime, 1);