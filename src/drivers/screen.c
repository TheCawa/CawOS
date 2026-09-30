#include "drivers/screen.h"
#include "drivers/io.h"
#include "drivers/serial.h"
#include "libc/util.h"
#include "libc/font.h"
#include "kernel/memory.h"

#define VGA_MEMORY ((char*)0xb8000)
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define LOGO_OFFSET_X 6
#define CURSOR_BLINK_MS 500

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
uint32_t g_char_w = 8;
uint32_t g_char_h = 16;
uint32_t g_font_scale = 1;
uint32_t g_char_gap_y = 2;
uint32_t g_scale_num = 1;
uint32_t g_scale_den = 1;
static const font_t* push_saved_font = NULL;
static uint32_t push_saved_w, push_saved_h, push_saved_gap;
static uint32_t push_saved_cols, push_saved_rows;
static uint32_t push_saved_num, push_saved_den;
static int push_active = 0;

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
    g_font_scale = 1;
    g_char_w = 8;
    g_char_h = 16;
    g_char_gap_y = 2;
    g_cols = g_width / g_char_w;
    g_rows = g_height / (g_char_h + g_char_gap_y);
    if (g_bpp == 0) g_bpp = 32;
    size_t buffer_size = g_height * g_pitch;
    g_shadow = (uint8_t*)malloc(buffer_size);
    if (g_shadow == NULL) {
        g_shadow = (uint8_t*)0x02000000;
    }
    memset(g_shadow, 0, buffer_size);
    g_is_graphics = 1;
    g_cols = g_width  / g_char_w;
    g_rows = g_height / (g_char_h + g_char_gap_y);
    cursor_visible = 1;
    cursor_drawn = 0;
    cursor_row = -1;
    cursor_col = -1;
    last_cursor_tick = 0;
}

int screen_get_cols() { return g_cols; }
int screen_get_rows() { return g_rows; }

static void gfx_flush_char(int col, int row) {
    int x = col * g_char_w;
    int y = row * (g_char_h + g_char_gap_y);
    int h = g_char_h + g_char_gap_y;
    for (int dy = 0; dy < h; dy++) {
        uint32_t offset = (y + dy) * g_pitch + x * (g_bpp / 8);
        volatile uint32_t* dst = (volatile uint32_t*)(g_framebuffer + offset);
        uint32_t* src = (uint32_t*)(g_shadow + offset);
        int dwords = (g_char_w * (g_bpp / 8)) / 4;
        for (int i = 0; i < dwords; i++) {
            dst[i] = src[i];
        }
    }
}

void screen_push_font(font_t* f, uint32_t gap) {
    if (!g_is_graphics || push_active) return;
    push_saved_font = font_get_current();
    push_saved_w = g_char_w; push_saved_h = g_char_h; push_saved_gap = g_char_gap_y;
    push_saved_cols = g_cols; push_saved_rows = g_rows;
    push_saved_num = g_scale_num; push_saved_den = g_scale_den;
    push_active = 1;
    font_set_current(f);
    g_scale_num = 1; g_scale_den = 1;
    g_char_w = f->width;
    g_char_h = f->height;
    g_char_gap_y = gap;
    g_cols = g_width / g_char_w;
    g_rows = g_height / (g_char_h + g_char_gap_y);
}

void screen_pop_font(void) {
    if (!push_active) return;
    font_set_current((font_t*)push_saved_font);
    g_char_w = push_saved_w; g_char_h = push_saved_h; g_char_gap_y = push_saved_gap;
    g_cols = push_saved_cols; g_rows = push_saved_rows;
    g_scale_num = push_saved_num; g_scale_den = push_saved_den;
    push_active = 0;
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
        uint16_t c16 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        pixel[0] = c16 & 0xFF; pixel[1] = c16 >> 8;
    }
}

void gfx_fill_rect_px(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            gfx_putpixel((uint32_t)(x + dx), (uint32_t)(y + dy), color);
}

void screen_flip_rect(int x, int y, int w, int h) {
    if (!g_is_graphics || !g_framebuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= (int)g_width || y >= (int)g_height) return;
    if (x + w > (int)g_width)  w = (int)g_width  - x;
    if (y + h > (int)g_height) h = (int)g_height - y;
    uint32_t bpp = g_bpp / 8;
    for (int row = 0; row < h; row++) {
        memcpy(g_framebuffer + (uint32_t)(y + row) * g_pitch + (uint32_t)x * bpp,
               g_shadow      + (uint32_t)(y + row) * g_pitch + (uint32_t)x * bpp,
               (uint32_t)w * bpp);
    }
}

void gfx_draw_char_ex(char c, int x, int y, uint32_t fg, uint32_t bg,
                      const font_t* f, uint32_t num, uint32_t den) {
    if (!g_framebuffer || !f) return;
    if (den == 0) den = 1;
    if (num == 0) num = 1;
    unsigned char uc = (unsigned char)c;
    if (uc >= f->num_glyphs) uc = '?';
    const uint8_t* glyph = f->glyphs + (uint32_t)uc * f->height;
    int dst_h = (int)(((uint32_t)f->height * num + den - 1) / den);
    int dst_w = (int)(((uint32_t)f->width  * num + den - 1) / den);
    int up_y = (num >= den);
    int up_x = (num >= den);

    for (int ty = 0; ty < dst_h; ty++) {
        uint8_t combined;
        if (up_y) {
            int sy = (int)((uint32_t)ty * den / num);
            if (sy >= (int)f->height) sy = (int)f->height - 1;
            combined = glyph[sy];
        } else {
            int sy0 = (int)((uint32_t)ty * den / num);
            int sy1 = (int)((((uint32_t)ty + 1) * den + num - 1) / num);
            if (sy1 <= sy0) sy1 = sy0 + 1;
            combined = 0;
            for (int sy = sy0; sy < sy1 && sy < (int)f->height; sy++) combined |= glyph[sy];
        }
        for (int tx = 0; tx < dst_w; tx++) {
            int bit;
            if (up_x) {
                int sx = (int)((uint32_t)tx * den / num);
                if (sx >= (int)f->width) sx = (int)f->width - 1;
                bit = (combined >> (7 - sx)) & 1;
            } else {
                int sx0 = (int)((uint32_t)tx * den / num);
                int sx1 = (int)((((uint32_t)tx + 1) * den + num - 1) / num);
                if (sx1 <= sx0) sx1 = sx0 + 1;
                bit = 0;
                for (int sx = sx0; sx < sx1 && sx < (int)f->width; sx++) {
                    if (combined & (0x80 >> sx)) { bit = 1; break; }
                }
            }
            gfx_putpixel(x + tx, y + ty, bit ? fg : bg);
        }
    }
}

void gfx_draw_char_px(char c, int px, int py, uint32_t fg, uint32_t bg,
                      const font_t* f, uint32_t scale) {
    gfx_draw_char_ex(c, px, py, fg, bg, f, scale, 1);
}

void gfx_draw_char(char c, int x, int y, uint32_t fg, uint32_t bg) {
    gfx_draw_char_ex(c, x, y, fg, bg, font_get_current(), g_scale_num, g_scale_den);
}

static void gfx_draw_char_xy(char c, int x, int y, uint32_t fg, uint32_t bg,
                             const font_t* f,
                             uint32_t num_x, uint32_t den_x,
                             uint32_t num_y, uint32_t den_y) {
    if (!g_framebuffer || !f) return;
    if (den_x == 0) den_x = 1;
    if (den_y == 0) den_y = 1;
    if (num_x == 0) num_x = 1;
    if (num_y == 0) num_y = 1;
    unsigned char uc = (unsigned char)c;
    if (uc >= f->num_glyphs) uc = '?';
    const uint8_t* glyph = f->glyphs + (uint32_t)uc * f->height;
    int dst_h = (int)(((uint32_t)f->height * num_y + den_y - 1) / den_y);
    int dst_w = (int)(((uint32_t)f->width  * num_x + den_x - 1) / den_x);
    int up_y = (num_y >= den_y);
    int up_x = (num_x >= den_x);

    for (int ty = 0; ty < dst_h; ty++) {
        uint8_t combined;
        if (up_y) {
            int sy = (int)((uint32_t)ty * den_y / num_y);
            if (sy >= (int)f->height) sy = (int)f->height - 1;
            combined = glyph[sy];
        } else {
            int sy0 = (int)((uint32_t)ty * den_y / num_y);
            int sy1 = (int)((((uint32_t)ty + 1) * den_y + num_y - 1) / num_y);
            if (sy1 <= sy0) sy1 = sy0 + 1;
            combined = 0;
            for (int sy = sy0; sy < sy1 && sy < (int)f->height; sy++) combined |= glyph[sy];
        }
        for (int tx = 0; tx < dst_w; tx++) {
            int bit;
            if (up_x) {
                int sx = (int)((uint32_t)tx * den_x / num_x);
                if (sx >= (int)f->width) sx = (int)f->width - 1;
                bit = (combined >> (7 - sx)) & 1;
            } else {
                int sx0 = (int)((uint32_t)tx * den_x / num_x);
                int sx1 = (int)((((uint32_t)tx + 1) * den_x + num_x - 1) / num_x);
                if (sx1 <= sx0) sx1 = sx0 + 1;
                bit = 0;
                for (int sx = sx0; sx < sx1 && sx < (int)f->width; sx++) {
                    if (combined & (0x80 >> sx)) { bit = 1; break; }
                }
            }
            gfx_putpixel((uint32_t)(x + tx), (uint32_t)(y + ty), bit ? fg : bg);
        }
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
        gfx_draw_char(c, col*g_char_w, row*(g_char_h + g_char_gap_y), fg, bg);
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
    int x = col * g_char_w;
    int y = row * (g_char_h + g_char_gap_y);
    int cursor_height = 2;
    int cursor_y = y + g_char_h;
    
    for (int cy = 0; cy < cursor_height; cy++) {
        for (int cx = 0; cx < (int)g_char_w; cx++) {
            int px = x + cx;
            int py = cursor_y + cy;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint8_t* pixel = g_framebuffer + py * g_pitch + px * (g_bpp / 8);
                if (g_bpp == 32) {
                    pixel[0] ^= 0xFF; pixel[1] ^= 0xFF; pixel[2] ^= 0xFF;
                } else if (g_bpp == 24) {
                    pixel[0] ^= 0xFF; pixel[1] ^= 0xFF; pixel[2] ^= 0xFF;
                } else if (g_bpp == 16) {
                    pixel[0] ^= 0xFF; pixel[1] ^= 0xFF;
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

void draw_logo(void) {
    clear_screen();
    if (!g_is_graphics) {
        static const char* art[5] = {
            "  ______      ______      __     __      ______      ______   ",
            " /\\  ___\\    /\\  __ \\    /\\ \\  _ \\ \\    /\\  __ \\    /\\  ___\\  ",
            " \\ \\ \\____   \\ \\  __ \\   \\ \\ \\/ \".\\ \\   \\ \\ \\/\\ \\   \\ \\___  \\ ",
            "  \\ \\_____\\   \\ \\_\\ \\_\\   \\ \\__/\".~\\_\\   \\ \\_____\\   \\/\\_____\\",
            "   \\/_____/    \\/_/\\/_/    \\/_/   \\/_/    \\/_____/    \\/_____/"
        };
        for (int r = 0; r < 25; r++)
            for (int c = 0; c < 80; c++)
                print_char_at(' ', r, c, 0x00);
        int art_cols = (int)strlen(art[0]);
        int x0 = (80 - art_cols) / 2;
        int y0 = (25 - 5) / 2 - 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        for (int r = 0; r < 5; r++)
            print_at_color(art[r], y0 + r, x0, 0x0B);
        const char* tag = ">> CawOS is loading your dreams... <<";
        int tx = (80 - (int)strlen(tag)) / 2;
        int ty = y0 + 5 + 1;
        print_at_color(tag, ty, tx, 0x0E);
        int sx = 40, sy = ty + 2;
        for (int i = 0; i < 20; i++) {
            char c = spinner_chars[i % 4];
            char buf[2] = { c, 0 };
            print_at_color(buf, sy, sx, 0x0F);
            sleep_ms(100);
            print_at_color(" ", sy, sx, 0x00);
        }
        return;
    }
    const font_t* f = font_get_8x8();
    const uint32_t nx = 3, dx = 2;
    const uint32_t ny = 2, dy = 1;
    const int cell_w = (int)((8 * nx + dx - 1) / dx);
    const int cell_h = (int)((8 * ny + dy - 1) / dy);

    uint32_t fg = vga_to_rgb(0x0B);
    uint32_t bg = vga_to_rgb(0x00);
    static const char* art[5] = {
        "  ______      ______      __     __      ______      ______   ",
        " /\\  ___\\    /\\  __ \\    /\\ \\  _ \\ \\    /\\  __ \\    /\\  ___\\  ",
        " \\ \\ \\____   \\ \\  __ \\   \\ \\ \\/ \".\\ \\   \\ \\ \\/\\ \\   \\ \\___  \\ ",
        "  \\ \\_____\\   \\ \\_\\ \\_\\   \\ \\__/\".~\\_\\   \\ \\_____\\   \\/\\_____\\",
        "   \\/_____/    \\/_/\\/_/    \\/_/   \\/_/    \\/_____/    \\/_____/"
    };
    int art_cols = (int)strlen(art[0]);
    int x0 = ((int)g_width - art_cols * cell_w) / 2;
    int y0 = ((int)g_height - 5 * cell_h) / 2 - cell_h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; art[r][c]; c++) {
            gfx_draw_char_xy(art[r][c], x0 + c * cell_w, y0 + r * cell_h,
                             fg, bg, f, nx, dx, ny, dy);
        }
    }

    const char* tag = ">> CawOS is loading your dreams... <<";
    int tx = ((int)g_width - (int)strlen(tag) * cell_w) / 2;
    int ty = y0 + 5 * cell_h + cell_h / 2;
    for (int c = 0; tag[c]; c++) {
        gfx_draw_char_xy(tag[c], tx + c * cell_w, ty,
                         vga_to_rgb(0x0E), bg, f, nx, dx, ny, dy);
    }
    screen_flip_rect(0, 0, (int)g_width, (int)g_height);

    int sx = (int)g_width / 2;
    int sy = ty + cell_h * 2;
    for (int i = 0; i < 20; i++) {
        char c = spinner_chars[i % 4];
        gfx_draw_char_xy(c, sx, sy, vga_to_rgb(0x0F), bg, f, nx, dx, ny, dy);
        screen_flip_rect(sx, sy, cell_w, cell_h);
        sleep_ms(100);
        gfx_draw_char_xy(' ', sx, sy, vga_to_rgb(0x0F), bg, f, nx, dx, ny, dy);
        screen_flip_rect(sx, sy, cell_w, cell_h);
    }
}

__attribute__((force_align_arg_pointer))
void scroll() {
    uint32_t flags;
    __asm__ volatile("pushfl; pop %0; cli" : "=r"(flags) :: "memory");

    if (g_is_graphics) {
        if (cursor_drawn) { gfx_toggle_cursor(cursor_row, cursor_col, 0); cursor_drawn = 0; }
        uint32_t line_pixels  = g_char_h + g_char_gap_y;
        uint32_t bytes_per_line = g_pitch * line_pixels;
        uint32_t total_bytes    = g_height * g_pitch;
        memmove(g_shadow, g_shadow + bytes_per_line, total_bytes - bytes_per_line);
        memset(g_shadow + (g_height - line_pixels) * g_pitch, 0, bytes_per_line);
        volatile uint32_t* dst = (volatile uint32_t*)g_framebuffer;
        uint32_t* src = (uint32_t*)g_shadow;
        uint32_t total_dwords = total_bytes / 4;
        
        for (uint32_t i = 0; i < total_dwords; i++) {
            dst[i] = src[i];
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
    int max_cols = screen_get_cols();
    if (max_cols < 1) max_cols = 1;
    if (col < 0) col = 0;
    if (col >= max_cols) col = max_cols - 1;
    int len = strlen(msg);
    if (len == 0) {
        while (*row >= max_rows) { scroll(); *row = max_rows - 1; }
        if (*row < 0) *row = 0;
        if (g_is_graphics) { cursor_row = *row; cursor_col = col; }
        serial_mirror_line(msg);
        (*row)++;
        return;
    }
    int off = 0;
    int cur_col = col;
    while (off < len) {
        int width = max_cols - cur_col;
        if (width < 1) width = 1;
        int chunk = len - off;
        if (chunk > width) chunk = width;
        while (*row >= max_rows) { scroll(); *row = max_rows - 1; }
        if (*row < 0) *row = 0;
        for (int i = 0; i < chunk; i++) {
            print_char_at(msg[off + i], *row, cur_col + i, color);
        }
        if (g_is_graphics) {
            cursor_row = *row;
            cursor_col = cur_col + chunk;
        }
        off += chunk;
        cur_col = 0;
        if (off < len) (*row)++;
    }
    serial_mirror_line(msg);
    (*row)++;
}

void screen_set_font_scale(uint32_t num, uint32_t den) {
    if (!g_is_graphics) return;
    if (den == 0) den = 1;
    if (num == 0) num = 1;
    g_scale_num = num;
    g_scale_den = den;
    const font_t* f = font_get_current();
    g_char_w = ((uint32_t)f->width  * num + den - 1) / den;
    g_char_h = ((uint32_t)f->height * num + den - 1) / den;
    if (g_char_w == 0) g_char_w = 1;
    if (g_char_h == 0) g_char_h = 1;
    g_char_gap_y = (g_char_h / f->height) / 2;
    g_cols = g_width  / g_char_w;
    g_rows = g_height / (g_char_h + g_char_gap_y);
    cursor_row = 0;
    cursor_col = 0;
    clear_screen();
}