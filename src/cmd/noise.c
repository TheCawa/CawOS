#include "commands.h"
#include "drivers/io.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/interrupt.h"
#include "kernel/idt.h"

void render_noise() {
    if (!g_is_graphics) {
        return;
    }
    extern volatile uint32_t system_ticks;
    for (uint32_t y = 0; y < g_height; y++) {
        uint8_t* row = g_shadow + y * g_pitch;
        for (uint32_t x = 0; x < g_width; x++) {
            uint8_t* pixel = row + x * (g_bpp / 8);
            feed_entropy((unsigned char)(x ^ y ^ system_ticks));
            uint8_t r = rand(0, 255);
            uint8_t g = rand(0, 255);
            uint8_t b = rand(0, 255);
            if (g_bpp == 32) {
                pixel[0] = b;
                pixel[1] = g;
                pixel[2] = r;
                pixel[3] = 0;
            } else if (g_bpp == 24) {
                pixel[0] = b;
                pixel[1] = g;
                pixel[2] = r;
            } else if (g_bpp == 16) {
                uint16_t c16 = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
                pixel[0] = c16 & 0xFF;
                pixel[1] = c16 >> 8;
            }
        }
    }
    memcpy(g_framebuffer, g_shadow, g_height * g_pitch);
}

void cmd_noise(char* args, int* row) {
    (void)args;
    (void)row;
    if (!g_is_graphics) {
        print_line_scroll("Error: noise requires graphics mode", 0, row, 0x0C);
        return;
    }
    disable_cursor();
    while (!is_interrupt_requested()) {
        render_noise();
        sleep_ms(30);
    }
    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
}

REGISTER_COMMAND("noise", cmd_noise, 1);
