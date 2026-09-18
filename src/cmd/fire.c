#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "libc/keyboard.h"
#include "kernel/memory.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

#define FIRE_MAX 36

void cmd_fire(char* args, int* row) {
    (void)args;
    int cols = screen_get_cols();
    int rows = screen_get_rows();

    unsigned char* heat = (unsigned char*)malloc(cols * rows);
    if (!heat) {
        print_line_scroll("Error: Not enough memory.", 0, row, 0x0C);
        return;
    }

    memset(heat, 0, cols * rows);
    for (int x = 0; x < cols; x++) heat[(rows - 1) * cols + x] = FIRE_MAX;

    clear_screen();
    disable_cursor();

    while (!is_interrupt_requested()) {
        for (int x = 0; x < cols; x++) heat[(rows - 1) * cols + x] = FIRE_MAX;

        for (int y = 1; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                int src = heat[y * cols + x];
                if (src == 0) continue;
                int decay = rand(0, 2);
                int nx = x + decay - 1;
                if (nx < 0) nx = 0;
                if (nx >= cols) nx = cols - 1;
                int val = src - (decay & 1);
                if (val < 0) val = 0;
                heat[(y - 1) * cols + nx] = (unsigned char)val;
            }
        }

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                int h = heat[y * cols + x];
                char c; unsigned char col;
                if (h == 0)       { c = ' '; col = 0x00; }
                else if (h < 8)  { c = '.'; col = 0x04; }
                else if (h < 16) { c = ':'; col = 0x04; }
                else if (h < 24) { c = '*'; col = 0x0C; }
                else if (h < 30) { c = '#'; col = 0x0E; }
                else             { c = '@'; col = 0x0F; }
                print_char_at(c, y, x, col);
            }
        }

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            if (!(scancode & 0x80) && scancode == ESC) {
                break;
            }
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        }
        sleep_ms(50);
    }

    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
    free(heat);
}
REGISTER_COMMAND("fire", cmd_fire, 1);