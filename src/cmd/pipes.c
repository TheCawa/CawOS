#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "libc/keyboard.h"
#include "kernel/interrupt.h"
#include "kernel/memory.h"
#include "libc/keyboard_map.h"

#define PIPE_COUNT 3

#define FADE_DIM  30
#define FADE_GRAY 60
#define FADE_DEAD 90

static const unsigned char pipe_pal[6] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E};

void cmd_pipes(char* args, int* row) {
    (void)args;
    (void)row;
    int cols = screen_get_cols();
    int rows = screen_get_rows();
    int cells = cols * rows;

    char* grid_ch = (char*)malloc(cells);
    unsigned char* grid_col = (unsigned char*)malloc(cells);
    unsigned char* grid_age = (unsigned char*)malloc(cells);
    if (!grid_ch || !grid_col || !grid_age) {
        if (grid_ch) free(grid_ch);
        if (grid_col) free(grid_col);
        if (grid_age) free(grid_age);
        print_line_scroll("Error: Not enough memory.", 0, row, 0x0C);
        return;
    }

    int px[PIPE_COUNT], py[PIPE_COUNT], pdir[PIPE_COUNT];
    unsigned char pcol[PIPE_COUNT];

    for (int p = 0; p < PIPE_COUNT; p++) {
        px[p] = rand(0, cols - 1);
        py[p] = rand(0, rows - 1);
        pdir[p] = rand(0, 3);
        pcol[p] = pipe_pal[rand(0, 5)];
    }

    clear_screen();
    disable_cursor();

    for (int i = 0; i < cells; i++) {
        grid_ch[i] = ' ';
        grid_col[i] = 0;
        grid_age[i] = 0;
    }

    while (!is_interrupt_requested()) {
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                int i = y * cols + x;
                if (grid_ch[i] == ' ') continue;

                grid_age[i]++;
                if (grid_age[i] >= FADE_DEAD) {
                    grid_ch[i] = ' ';
                    print_char_at(' ', y, x, 0x00);
                } else if (grid_age[i] >= FADE_GRAY) {
                    print_char_at(grid_ch[i], y, x, 0x08);
                } else if (grid_age[i] >= FADE_DIM) {
                    unsigned char base = grid_col[i];
                    print_char_at(grid_ch[i], y, x, (base & 0x08) ? (base - 8) : base);
                }
            }
        }

        for (int p = 0; p < PIPE_COUNT; p++) {
            int turned = 0;

            if (rand(0, 99) < 20) {
                pdir[p] = (pdir[p] + (rand(0, 1) ? 1 : 3)) & 3;
                pcol[p] = pipe_pal[rand(0, 5)];
                turned = 1;
            }
            if (rand(0, 150) == 0) {
                px[p] = rand(0, cols - 1);
                py[p] = rand(0, rows - 1);
                pdir[p] = rand(0, 3);
                pcol[p] = pipe_pal[rand(0, 5)];
                turned = 1;
            }

            char c;
            if (turned) c = '+';
            else c = (pdir[p] == 0 || pdir[p] == 2) ? '|' : '-';

            int idx = py[p] * cols + px[p];
            grid_ch[idx] = c;
            grid_col[idx] = pcol[p];
            grid_age[idx] = 0;
            print_char_at(c, py[p], px[p], pcol[p]);

            switch (pdir[p]) {
                case 0: py[p]--; break;
                case 1: px[p]++; break;
                case 2: py[p]++; break;
                case 3: px[p]--; break;
            }

            if (px[p] < 0 || px[p] >= cols || py[p] < 0 || py[p] >= rows) {
                px[p] = rand(0, cols - 1);
                py[p] = rand(0, rows - 1);
                pdir[p] = rand(0, 3);
                pcol[p] = pipe_pal[rand(0, 5)];
            }
        }

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            if (!(scancode & 0x80) && scancode == ESC) {
                break;
            }
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        }
        sleep_ms(33);
    }

    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
    free(grid_ch);
    free(grid_col);
    free(grid_age);
}
REGISTER_COMMAND("pipes", cmd_pipes, 1);