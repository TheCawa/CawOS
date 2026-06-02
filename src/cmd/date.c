#include "commands.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "drivers/rtc.h"

void cmd_date(char* args, int* row) {
    rtc_time_t now;
    rtc_get_time(&now);
    char buffer[64];
    char d[3], m[3], y[5], h[3], min[3], s[3];
    itoa(now.day, d);
    itoa(now.month, m);
    itoa(now.year, y);
    itoa(now.hour, h);
    itoa(now.minute, min);
    itoa(now.second, s);
    strcpy(buffer, "Current Date/Time: ");
    strcat(buffer, d); strcat(buffer, ".");
    strcat(buffer, m); strcat(buffer, ".");
    strcat(buffer, y); strcat(buffer, " ");
    if (now.hour < 10) strcat(buffer, "0");
    strcat(buffer, h); strcat(buffer, ":");
    if (now.minute < 10) strcat(buffer, "0");
    strcat(buffer, min); strcat(buffer, ":");
    if (now.second < 10) strcat(buffer, "0");
    strcat(buffer, s);
    print_line_scroll(buffer, 0, row, 0x0F);
}
REGISTER_COMMAND("date", cmd_date, 0);