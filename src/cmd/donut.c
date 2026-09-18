#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "libc/keyboard.h"
#include "kernel/memory.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

static float d_sin(float x) {
    const float PI = 3.14159265f;
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    float term = x, sum = x, x2 = x * x;
    for (int n = 1; n <= 5; n++) {
        term *= -x2 / ((float)(2 * n) * (float)(2 * n + 1));
        sum += term;
    }
    return sum;
}
static float d_cos(float x) { return d_sin(x + 1.5707963f); }

void cmd_donut(char* args, int* row) {
    (void)args;
    int cols = screen_get_cols();
    int rows = screen_get_rows();

    char* dbuf = (char*)malloc(cols * rows);
    float* zbuf = (float*)malloc(cols * rows * sizeof(float));
    if (!dbuf || !zbuf) {
        if (dbuf) free(dbuf);
        if (zbuf) free(zbuf);
        print_line_scroll("Error: Not enough memory.", 0, row, 0x0C);
        return;
    }

    float sx = (float)cols / 4.0f;
    float sy = (float)rows / 3.0f;
    float A = 0, B = 0;

    clear_screen();
    disable_cursor();

    while (!is_interrupt_requested()) {
        memset(dbuf, ' ', cols * rows);
        for (int i = 0; i < cols * rows; i++) zbuf[i] = 0;

        A += 0.04f; B += 0.02f;
        float cA = d_cos(A), sA = d_sin(A), cB = d_cos(B), sB = d_sin(B);

        for (float j = 0; j < 6.28f; j += 0.07f) {
            float ct = d_cos(j), st = d_sin(j);
            for (float i = 0; i < 6.28f; i += 0.02f) {
                float sp = d_sin(i), cp = d_cos(i);
                float h = ct + 2.0f;
                float D = 1.0f / (sp * h * sA + st * cA + 5.0f);
                float t = sp * h * cA - st * sA;
                int x = (int)(cols / 2 + sx * D * (cp * h * cB - t * sB));
                int y = (int)(rows / 2 + sy * D * (cp * h * sB + t * cB));
                if (x < 0 || x >= cols || y < 0 || y >= rows) continue;
                int o = x + y * cols;
                if (D > zbuf[o]) {
                    zbuf[o] = D;
                    int N = (int)(8.0f * ((st * sA - sp * ct * cA) * cB
                              - sp * ct * sA - st * cA - cp * ct * sB));
                    if (N < 0) N = 0;
                    if (N > 11) N = 11;
                    dbuf[o] = ".,-~:;=!*#$@"[N];
                }
            }
        }

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                char c = dbuf[x + y * cols];
                print_char_at(c, y, x, (c != ' ') ? 0x0F : 0x00);
            }
        }

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            if (!(scancode & 0x80) && scancode == ESC) {
                break;
            }
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        }
        sleep_ms(40);
    }

    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
    free(dbuf);
    free(zbuf);
}
REGISTER_COMMAND("donut", cmd_donut, 1);