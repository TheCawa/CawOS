#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "drivers/rtc.h"
#include "libc/keyboard.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

#define CLOCK_CELL 12

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

static void draw_big_digit_px(int digit, int x0, int y0, int cell, uint32_t color) {
    if (digit < 0 || digit > 9) digit = 0;
    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 5; c++) {
            if (digit_font[digit][r][c] == '#') {
                gfx_fill_rect_px(x0 + c * cell, y0 + r * cell, cell - 1, cell - 1, color);
            }
        }
    }
}

static void draw_colon_px(int x0, int y0, int cell, int on, uint32_t color) {
    for (int r = 0; r < 7; r++) {
        if (on && (r == 2 || r == 4)) {
            gfx_fill_rect_px(x0 + cell, y0 + r * cell, cell - 1, cell - 1, color);
        }
    }
}

void cmd_bigclock(char* args, int* row) {
    int idle_mode = 0;
    if (args != NULL && strstr(args, "--idle") != NULL) idle_mode = 1;
    (void)row;
    if (!g_is_graphics) {
        static const char* digit_text[10][7] = {
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
        static const int offsets[4] = { 0, 6, 16, 22 };
        clear_screen();
        disable_cursor();
        int block_w = 27, block_h = 7;
        int x0 = (80 - block_w) / 2;
        int y0 = (25 - block_h) / 2 - 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        while (!is_interrupt_requested()) {
            rtc_time_t t;
            rtc_get_time(&t);

            for (int r = 0; r < 25; r++)
                for (int c = 0; c < 80; c++)
                    print_char_at(' ', r, c, 0x00);

            int digits[4] = { t.hour / 10, t.hour % 10, t.minute / 10, t.minute % 10 };
            for (int d = 0; d < 4; d++) {
                for (int r = 0; r < 7; r++) {
                    for (int c = 0; c < 5; c++) {
                        if (digit_text[digits[d]][r][c] == '#') {
                            char buf[2] = { '#', 0 };
                            print_at_color(buf, y0 + r, x0 + offsets[d] + c, 0x0B);
                        }
                    }
                }
            }

            if ((t.second % 2) == 0) {
                char buf[2] = { '#', 0 };
                print_at_color(buf, y0 + 2, x0 + 13, 0x0E);
                print_at_color(buf, y0 + 4, x0 + 13, 0x0E);
            }
            char line[64]; char nb[8];
            memset(line, 0, 64);
            if (t.day   < 10) { strcat(line, "0"); }
            itoa(t.day, nb);   strcat(line, nb); strcat(line, ".");
            if (t.month < 10) { strcat(line, "0"); }
            itoa(t.month, nb); strcat(line, nb); strcat(line, ".");
            itoa(t.year, nb); strcat(line, nb);
            strcat(line, " ");
            strcat(line, weekday_names[weekday_of(t.year, t.month, t.day)]);
            strcat(line, "  ");
            if (t.hour   < 10) { strcat(line, "0"); }
            itoa(t.hour, nb);   strcat(line, nb); strcat(line, ":");
            if (t.minute < 10) { strcat(line, "0"); }
            itoa(t.minute, nb); strcat(line, nb); strcat(line, ":");
            if (t.second < 10) { strcat(line, "0"); }
            itoa(t.second, nb); strcat(line, nb);
            int lx = (80 - (int)strlen(line)) / 2;
            int ly = y0 + block_h + 1;
            print_at_color(line, ly, lx, 0x0F);
            if (key_queue_head != key_queue_tail) {
                unsigned char scancode = key_queue[key_queue_head];
                key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
                if (!(scancode & 0x80)) {
                    if (idle_mode || scancode == ESC) break;
                }
            }
            sleep_ms(250);
        }

        clear_interrupt();
        clear_screen();
        enable_cursor(13, 15);
        return;
    }
    const font_t* f8 = font_get_8x8();
    const int cell = CLOCK_CELL;
    int block_w = 27 * cell;
    int block_h = 7 * cell;
    int x0 = ((int)g_width - block_w) / 2;
    int y0 = ((int)g_height - block_h) / 2 - cell * 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    uint32_t dcol = vga_to_rgb(0x0B);
    uint32_t ccol = vga_to_rgb(0x0E);
    uint32_t tcol = vga_to_rgb(0x0F);
    uint32_t bgc  = vga_to_rgb(0x00);

    clear_screen();
    disable_cursor();

    while (!is_interrupt_requested()) {
        rtc_time_t t;
        rtc_get_time(&t);

        gfx_fill_rect_px(0, 0, (int)g_width, (int)g_height, bgc);

        draw_big_digit_px(t.hour / 10,   x0,             y0, cell, dcol);
        draw_big_digit_px(t.hour % 10,   x0 + 6 * cell,  y0, cell, dcol);
        draw_colon_px(                   x0 + 12 * cell, y0, cell, (t.second % 2) == 0, ccol);
        draw_big_digit_px(t.minute / 10, x0 + 16 * cell, y0, cell, dcol);
        draw_big_digit_px(t.minute % 10, x0 + 22 * cell, y0, cell, dcol);

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

        int len = (int)strlen(line);
        int lx = ((int)g_width - len * 16) / 2;
        int ly = y0 + block_h + cell;
        for (int i = 0; i < len; i++) {
            gfx_draw_char_px(line[i], lx + i * 16, ly, tcol, bgc, f8, 2);
        }

        screen_flip_rect(0, 0, (int)g_width, (int)g_height);

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
            if (!(scancode & 0x80)) {
                if (idle_mode || scancode == ESC) {
                    break;
                }
            }
        }
        sleep_ms(250);
    }

    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
}
REGISTER_COMMAND("bigclock", cmd_bigclock, 0);