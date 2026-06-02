#include "drivers/screen.h"
#include "drivers/io.h"
#include "libc/util.h"
#include "libc/font.h"

#define VGA_MEMORY ((char*)0xb8000)
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define CHAR_W 12
#define CHAR_H 14
#define SCALE_X_NUM 3
#define SCALE_X_DEN 2
#define SCALE_Y_NUM 7
#define SCALE_Y_DEN 4
#define CHAR_GAP_Y (CHAR_H / 7)
#define LOGO_OFFSET_X 6
#define CURSOR_BLINK_MS 500
#define SHADOW_BUFFER_ADDR 0x02000000

uint8_t* g_framebuffer = 0;
uint32_t g_width = 0, g_height = 0, g_pitch = 0;
int g_is_graphics = 0;
uint32_t g_bpp = 32;
uint32_t g_cols = 80;
uint32_t g_rows = 25;
static int cursor_visible = 1;
static uint32_t last_cursor_tick = 0;
int cursor_row = -1, cursor_col = -1;
static int cursor_drawn = 0;
uint8_t* g_shadow = 0;

unsigned char current_color = 0x0F;
static const char spinner_chars[] = {'|', '/', '-', '\\'};

uint32_t vga_to_rgb(unsigned char color) {
    color &= 0x0F;
    switch (color) {
        case 0x00: return 0x00000000; case 0x01: return 0x000000AA;
        case 0x02: return 0x0000AA00; case 0x03: return 0x0000AAAA;
        case 0x04: return 0x00AA0000; case 0x05: return 0x00AA00AA;
        case 0x07: return 0x00AAAAAA; case 0x08: return 0x00555555;
        case 0x09: return 0x005555FF; case 0x0A: return 0x0055FF55;
        case 0x0B: return 0x0055FFFF; case 0x0C: return 0x00FF5555;
        case 0x0D: return 0x00FF55FF; case 0x0E: return 0x00FFFF55;
        case 0x0F: return 0x00FFFFFF; default:   return 0x00FFFFFF;
    }
}

void screen_init_graphics(uint32_t framebuffer, uint32_t width, uint32_t height, uint32_t pitch) {
    g_framebuffer = (uint8_t*)framebuffer;
    g_width  = (width  > 0) ? width  : 1024;
    g_height = (height > 0) ? height : 768;
    g_pitch  = (pitch  > 0) ? pitch  : (g_width * 4);
    g_bpp = *((volatile uint32_t*)0x0530);
    if (g_bpp == 0) g_bpp = 32;
    g_shadow = (uint8_t*)SHADOW_BUFFER_ADDR;
    memset(g_shadow, 0, g_height * g_pitch);
    g_is_graphics = 1;
    g_cols = g_width  / CHAR_W;
    g_rows = g_height / (CHAR_H + CHAR_GAP_Y);
    cursor_visible = 1;
    cursor_drawn = 0;
    cursor_row = -1;
    cursor_col = -1;
    last_cursor_tick = 0;
}

int screen_get_cols() { return g_cols; }
int screen_get_rows() { return g_rows; }

static void gfx_flush_char(int col, int row) {
    int x = col * CHAR_W;
    int y = row * (CHAR_H + CHAR_GAP_Y);
    int h = CHAR_H + CHAR_GAP_Y;
    for (int dy = 0; dy < h; dy++) {
        uint32_t offset = (y + dy) * g_pitch + x * (g_bpp / 8);
        volatile uint32_t* dst = (volatile uint32_t*)(g_framebuffer + offset);
        uint32_t* src = (uint32_t*)(g_shadow + offset);
        int dwords = (CHAR_W * (g_bpp / 8)) / 4;
        for (int i = 0; i < dwords; i++) {
            dst[i] = src[i];
        }
    }
}

void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_width || y >= g_height) return;
    uint32_t offset = y * g_pitch + x * (g_bpp / 8);
    uint8_t* pixel = g_shadow + offset;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b = (color)       & 0xFF;
    if (g_bpp == 32) { pixel[0] = b; pixel[1] = g; pixel[2] = r; pixel[3] = 0; }
    else if (g_bpp == 24) { pixel[0] = b; pixel[1] = g; pixel[2] = r; }
    else if (g_bpp == 16) {
        uint16_t c16 = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
        pixel[0] = c16 & 0xFF; pixel[1] = c16 >> 8;
    }
}

void gfx_draw_char(char c, int x, int y, uint32_t fg, uint32_t bg) {
    if (!g_framebuffer) return;
    unsigned char* font_row = font8x8_basic[(unsigned char)c];
    int dst_y = y;
    for (int dy = 0; dy < 8; dy++) {
        int row_height = (dy % SCALE_Y_DEN < (SCALE_Y_NUM % SCALE_Y_DEN)) ? 
                         (SCALE_Y_NUM / SCALE_Y_DEN + 1) : (SCALE_Y_NUM / SCALE_Y_DEN);
        if (row_height == 0) row_height = 1;
        int dst_x = x;
        for (int dx = 0; dx < 8; dx++) {
            uint32_t px_color = (font_row[dy] & (1 << (7 - dx))) ? fg : bg;
            int col_width = (dx % SCALE_X_DEN < (SCALE_X_NUM % SCALE_X_DEN)) ? 
                            (SCALE_X_NUM / SCALE_X_DEN + 1) : (SCALE_X_NUM / SCALE_X_DEN);
            if (col_width == 0) col_width = 1;
            for (int sy = 0; sy < row_height; sy++) {
                for (int sx = 0; sx < col_width; sx++) {
                    gfx_putpixel(dst_x + sx, dst_y + sy, px_color);
                }
            }
            dst_x += col_width;
        }
        dst_y += row_height;
    }
}

void clear_screen() {
    uint32_t flags;
    __asm__ volatile("pushfl; pop %0; cli" : "=r"(flags) :: "memory");

    if (g_is_graphics) {
        memset(g_shadow, 0, g_height * g_pitch);
        volatile uint32_t* fb = (volatile uint32_t*)g_framebuffer;
        uint32_t* sh = (uint32_t*)g_shadow;
        uint32_t total_dwords = (g_height * g_pitch) / 4;
        for (uint32_t i = 0; i < total_dwords; i++) {
            fb[i] = sh[i];
        }
    } else {
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT * 2; i += 2) {
            VGA_MEMORY[i] = ' ';
            VGA_MEMORY[i + 1] = 0x0F;
        }
    }

    __asm__ volatile("push %0; popfl" :: "r"(flags) : "memory", "cc");
}

void print_char_at(char c, int row, int col, unsigned char color) {
    if (g_is_graphics) {
        uint32_t fg = vga_to_rgb(color);
        uint32_t bg = vga_to_rgb(color >> 4);
        gfx_draw_char(c, col*CHAR_W, row*(CHAR_H + CHAR_GAP_Y), fg, bg);
        gfx_flush_char(col, row);
    } else {
        if (row >= SCREEN_HEIGHT || col >= SCREEN_WIDTH) return;
        int offset = (row * SCREEN_WIDTH + col) * 2;
        VGA_MEMORY[offset] = c; VGA_MEMORY[offset + 1] = color;
    }
}

void print_at_color(const char* message, int row, int col, unsigned char color) {
    for (int i = 0; message[i] != 0; i++) print_char_at(message[i], row, col + i, color);
}
void print_at(char* message, int row, int col) { print_at_color(message, row, col, current_color); }

void disable_cursor() {
    if (!g_is_graphics) { port_byte_out(0x3D4, 0x0A); port_byte_out(0x3D5, 0x20); }
    else {
        if (cursor_drawn && cursor_visible) { gfx_toggle_cursor(cursor_row, cursor_col, 0); cursor_drawn = 0; }
        cursor_visible = 0;
    }
}

void enable_cursor(unsigned char start, unsigned char end) {
    if (!g_is_graphics) {
        port_byte_out(0x3D4, 0x0A); port_byte_out(0x3D5, (port_byte_in(0x3D5) & 0xC0) | start);
        port_byte_out(0x3D4, 0x0B); port_byte_out(0x3D5, (port_byte_in(0x3D5) & 0xE0) | end);
    } else { cursor_visible = 1; last_cursor_tick = 0; }
}

void gfx_toggle_cursor(int row, int col, int draw) {
    if (!g_is_graphics || !g_framebuffer) return;
    int x = col * CHAR_W;
    int y = row * (CHAR_H + CHAR_GAP_Y);
    int cursor_height = 2;
    int cursor_y = y + CHAR_H + CHAR_GAP_Y - cursor_height;
    for (int cy = 0; cy < cursor_height; cy++) {
        for (int cx = 0; cx < CHAR_W; cx++) {
            int px = x + cx;
            int py = cursor_y + cy;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint8_t* pixel = g_framebuffer + py * g_pitch + px * (g_bpp / 8);
                if (draw) {
                    if (g_bpp == 32) { pixel[0] ^= 0xFF; pixel[1] ^= 0xFF; pixel[2] ^= 0xFF; }
                    else if (g_bpp == 24) { pixel[0] ^= 0xFF; pixel[1] ^= 0xFF; pixel[2] ^= 0xFF; }
                    else if (g_bpp == 16) { pixel[0] ^= 0xFF; pixel[1] ^= 0xFF; }
                } else {
                    if (g_bpp == 32) { pixel[0] = 0; pixel[1] = 0; pixel[2] = 0; pixel[3] = 0; }
                    else if (g_bpp == 24) { pixel[0] = 0; pixel[1] = 0; pixel[2] = 0; }
                    else if (g_bpp == 16) { pixel[0] = 0; pixel[1] = 0; }
                }
            }
        }
    }
}

void update_cursor(int row, int col) {
    if (!g_is_graphics) {
        if (row < 0) row = 0;
        unsigned short pos = row * SCREEN_WIDTH + col;
        port_byte_out(0x3D4, 14); port_byte_out(0x3D5, (unsigned char)(pos >> 8));
        port_byte_out(0x3D4, 15); port_byte_out(0x3D5, (unsigned char)(pos & 0xFF));
        return;
    }
    if (cursor_drawn && cursor_visible) { gfx_toggle_cursor(cursor_row, cursor_col, 0); cursor_drawn = 0; }
    cursor_row = (row < 0) ? 0 : row;
    cursor_col = col;
    extern volatile uint32_t system_ticks;
    if (system_ticks - last_cursor_tick >= CURSOR_BLINK_MS / 10) {
        cursor_visible = !cursor_visible; last_cursor_tick = system_ticks;
    }
    if (cursor_visible) { gfx_toggle_cursor(cursor_row, cursor_col, 1); cursor_drawn = 1; }
}

void draw_logo() {
    clear_screen();
    int rows = screen_get_rows(); int cols = screen_get_cols();
    const int LOGO_CHAR_WIDTH = 75; const int LOGO_CHAR_HEIGHT = 5;
    int start_col = (cols - LOGO_CHAR_WIDTH) / 2 + LOGO_OFFSET_X;
    int start_row = (rows - LOGO_CHAR_HEIGHT - 5) / 2;
    if (start_col < 2) start_col = 2; if (start_row < 2) start_row = 2;
    unsigned char color = 0x0B;
    print_at_color("  ______      ______      __     __      ______      ______   ", start_row,     start_col, color);
    print_at_color(" /\\  ___\\    /\\  __ \\    /\\ \\  _ \\ \\    /\\  __ \\    /\\  ___\\  ", start_row+1, start_col, color);
    print_at_color(" \\ \\ \\____   \\ \\  __ \\   \\ \\ \\/ \".\\ \\   \\ \\ \\/\\ \\   \\ \\___  \\ ", start_row+2, start_col, color);
    print_at_color("  \\ \\_____\\   \\ \\_\\ \\_\\   \\ \\__/\".~\\_\\   \\ \\_____\\   \\/\\_____\\", start_row+3, start_col, color);
    print_at_color("   \\/_____/    \\/_/\\/_/    \\/_/   \\/_/    \\/_____/    \\/_____/", start_row+4, start_col, color);
    print_at_color(">> CawOS is loading your dreams... <<", start_row+7, start_col + 18, 0x0E);
    int spinner_row = start_row + 10; int spinner_col = cols / 2;
    for (int i = 0; i < 20; i++) {
        char c = spinner_chars[i % 4];
        print_char_at(c, spinner_row, spinner_col, 0x0F);
        sleep_ms(100);
        print_char_at(' ', spinner_row, spinner_col, 0x0F);
    }
}

__attribute__((force_align_arg_pointer))
void scroll() {
    uint32_t flags;
    __asm__ volatile("pushfl; pop %0; cli" : "=r"(flags) :: "memory");

    if (g_is_graphics) {
        if (cursor_drawn) { gfx_toggle_cursor(cursor_row, cursor_col, 0); cursor_drawn = 0; }
        
        uint32_t line_pixels  = CHAR_H + CHAR_GAP_Y;
        uint32_t bytes_per_line = g_pitch * line_pixels;
        uint32_t total_bytes    = g_height * g_pitch;
        
        memmove(g_shadow, g_shadow + bytes_per_line, total_bytes - bytes_per_line);
        memset(g_shadow + (g_height - line_pixels) * g_pitch, 0, bytes_per_line);
        
        volatile uint32_t* fb = (volatile uint32_t*)g_framebuffer;
        uint32_t* sh = (uint32_t*)g_shadow;
        uint32_t total_dwords = total_bytes / 4;
        for (uint32_t i = 0; i < total_dwords; i++) {
            fb[i] = sh[i];
        }

        if (cursor_row > 0) cursor_row--;
    } else {
        char* vm = VGA_MEMORY;
        for (int i = 0; i < SCREEN_WIDTH * (SCREEN_HEIGHT - 1) * 2; i++) vm[i] = vm[i + SCREEN_WIDTH * 2];
        for (int i = SCREEN_WIDTH * (SCREEN_HEIGHT - 1) * 2; i < SCREEN_WIDTH * SCREEN_HEIGHT * 2; i += 2) {
            vm[i] = ' '; vm[i + 1] = 0x0F;
        }
    }

    __asm__ volatile("push %0; popfl" :: "r"(flags) : "memory", "cc");
}

void print_line_scroll(const char* msg, int col, int* row, unsigned char color) {
    int max_rows = screen_get_rows();
    if (*row >= max_rows) {
        scroll();
        *row = max_rows - 1;
        if (g_is_graphics) cursor_row = max_rows - 1;
    }
    if (*row < 0) *row = 0;
    if (*row >= max_rows) *row = max_rows - 1;
    print_at_color(msg, *row, col, color);
    if (g_is_graphics) cursor_col = col + strlen(msg);
    (*row)++;
}