#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "libc/keyboard.h"
#include "kernel/memory.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

void cmd_life(char* args, int* row) {
    (void)args;
    int cols = screen_get_cols();
    int rows = screen_get_rows();

    unsigned char* ga = (unsigned char*)malloc(cols * rows);
    unsigned char* gb = (unsigned char*)malloc(cols * rows);
    if (!ga || !gb) {
        if (ga) free(ga);
        if (gb) free(gb);
        print_line_scroll("Error: Not enough memory.", 0, row, 0x0C);
        return;
    }

    for (int i = 0; i < cols * rows; i++) {
        ga[i] = (rand(0, 9) < 3) ? 1 : 0;
    }

    clear_screen();
    disable_cursor();

    while (!is_interrupt_requested()) {
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                int i = x + y * cols;
                print_char_at(ga[i] ? '#' : ' ', y, x, ga[i] ? 0x0A : 0x00);
            }
        }

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                int n = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || ny >= rows || nx < 0 || nx >= cols) continue;
                        n += ga[nx + ny * cols];
                    }
                }
                int i = x + y * cols;
                gb[i] = ga[i] ? ((n == 2 || n == 3) ? 1 : 0)
                              : ((n == 3) ? 1 : 0);
            }
        }
        memcpy(ga, gb, cols * rows);

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
    free(ga);
    free(gb);
}
REGISTER_COMMAND("life", cmd_life, 1);