#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "libc/keyboard.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

#define DVD_W 7
#define DVD_H 3
static const unsigned char dvd_pal[6] = {0x0C, 0x0A, 0x09, 0x0E, 0x0D, 0x0F};

void cmd_dvd(char* args, int* row) {
    (void)args;
    int cols = screen_get_cols();
    int rows = screen_get_rows();

    int x = 1, y = 1, dx = 1, dy = 1;
    int ci = 0;
    int hits = 0;

    clear_screen();
    disable_cursor();

    while (!is_interrupt_requested()) {
        for (int yy = 0; yy < DVD_H; yy++)
            for (int xx = 0; xx < DVD_W; xx++)
                print_char_at(' ', y + yy, x + xx, 0x00);

        x += dx; y += dy;
        int bx = 0, by = 0;
        if (x <= 0)            { x = 0;            dx = 1;  bx = 1; }
        if (x >= cols - DVD_W) { x = cols - DVD_W; dx = -1; bx = 1; }
        if (y <= 0)            { y = 0;            dy = 1;  by = 1; }
        if (y >= rows - DVD_H) { y = rows - DVD_H; dy = -1; by = 1; }
        if (bx || by) ci = (ci + 1) % 6;
        if (bx && by) hits++;

        unsigned char col = dvd_pal[ci];
        for (int xx = 0; xx < DVD_W; xx++) {
            print_char_at('#', y, x + xx, col);
            print_char_at('#', y + DVD_H - 1, x + xx, col);
        }
        print_char_at('#', y + 1, x, col);
        print_char_at('D', y + 1, x + 2, col);
        print_char_at('V', y + 1, x + 3, col);
        print_char_at('D', y + 1, x + 4, col);
        print_char_at('#', y + 1, x + DVD_W - 1, col);

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            if (!(scancode & 0x80) && scancode == ESC) {
                break;
            }
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        }
        sleep_ms(100);
    }

    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);

    *row = 0;
    char msg[48];
    snprintf(msg, 48, "DVD finished. Corner hits: %d", hits);
    print_line_scroll(msg, 0, row, 0x0E);
}
REGISTER_COMMAND("dvd", cmd_dvd, 1);