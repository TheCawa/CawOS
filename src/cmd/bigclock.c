#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "drivers/rtc.h"
#include "libc/keyboard.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

static const char* digit_font[10][7] = {
    { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " },
    { "  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " },
    { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" },
    { "#### ", "    #", "    #", " ### ", "    #", "    #", "#### " },
    { "   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # " },
    { "#####", "#    ", "#    ", "#### ", "    #", "    #", "#### " },
    { " ### ", "#   #", "#    ", "#### ", "#   #", "#   #", " ### " },
    { "#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   " },
    { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " },
    { " ### ", "#   #", "#   #", " ####", "    #", "#   #", " ### " },
};

static const char* weekday_names[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static int weekday_of(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void draw_big_digit(int digit, int row0, int col0, unsigned char color) {
    if (digit < 0 || digit > 9) digit = 0;
    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 5; c++) {
            int on = (digit_font[digit][r][c] == '#');
            print_char_at(on ? '#' : ' ', row0 + r, col0 + c, on ? color : 0x00);
        }
    }
}

static void draw_colon(int row0, int col0, int on, unsigned char color) {
    for (int r = 0; r < 7; r++) {
        int dot = on && (r == 2 || r == 4);
        print_char_at(dot ? '#' : ' ', row0 + r, col0 + 1, dot ? color : 0x00);
    }
}

void cmd_bigclock(char* args, int* row) {
    (void)args;
    (void)row;

    int cols = screen_get_cols();
    int rows = screen_get_rows();

    int col0 = (cols - 27) / 2;
    int row0 = (rows - 7) / 2 - 2;
    if (col0 < 0) col0 = 0;
    if (row0 < 0) row0 = 0;

    clear_screen();
    disable_cursor();

    while (!is_interrupt_requested()) {
        rtc_time_t t;
        rtc_get_time(&t);

        unsigned char dcol = 0x0B;
        draw_big_digit(t.hour / 10,   row0, col0,      dcol);
        draw_big_digit(t.hour % 10,   row0, col0 + 6,  dcol);
        draw_colon(row0, col0 + 12, (t.second % 2) == 0, 0x0E);
        draw_big_digit(t.minute / 10, row0, col0 + 16, dcol);
        draw_big_digit(t.minute % 10, row0, col0 + 22, dcol);

        char line[64];
        char nb[8];
        memset(line, 0, 64);

        if (t.day < 10) strcat(line, "0");
        itoa(t.day, nb);   strcat(line, nb); strcat(line, ".");
        if (t.month < 10) strcat(line, "0");
        itoa(t.month, nb); strcat(line, nb); strcat(line, ".");
        itoa(t.year, nb);  strcat(line, nb);
        strcat(line, " ");
        strcat(line, weekday_names[weekday_of(t.year, t.month, t.day)]);
        strcat(line, "  ");
        if (t.hour < 10) strcat(line, "0");
        itoa(t.hour, nb);   strcat(line, nb); strcat(line, ":");
        if (t.minute < 10) strcat(line, "0");
        itoa(t.minute, nb); strcat(line, nb); strcat(line, ":");
        if (t.second < 10) strcat(line, "0");
        itoa(t.second, nb); strcat(line, nb);

        int len = strlen(line);
        int lcol = (cols - len) / 2;
        if (lcol < 0) lcol = 0;
        print_at_color(line, row0 + 9, lcol, 0x0F);

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            if (!(scancode & 0x80) && scancode == ESC) {
                break;
            }
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        }
        sleep_ms(250);
    }

    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
}
REGISTER_COMMAND("bigclock", cmd_bigclock, 0);