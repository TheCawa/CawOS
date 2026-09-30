#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>
#include "libc/font.h"

extern unsigned char current_color;
extern int g_is_graphics;
extern int cursor_row;
extern int cursor_col;
extern uint8_t* g_framebuffer;
extern uint32_t g_width, g_height, g_pitch, g_bpp;
extern uint32_t g_cols;
extern uint32_t g_rows;
extern uint8_t* g_shadow;
extern uint32_t g_font_scale;
int screen_get_cols();
int screen_get_rows();
typedef enum {
    VIDEO_VGA,
    VIDEO_FB
} video_mode_t;

void screen_init_graphics(uint32_t framebuffer, uint32_t width, uint32_t height, uint32_t pitch);
void screen_set_font_scale(uint32_t num, uint32_t den);
void screen_pop_font(void);
void screen_push_font(font_t* f, uint32_t gap);
void set_video_mode(video_mode_t mode);
void clear_screen();
uint32_t vga_to_rgb(unsigned char color);
void gfx_toggle_cursor(int row, int col, int draw);
void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color);
void gfx_fill_rect_px(int x, int y, int w, int h, uint32_t color);
void screen_flip_rect(int x, int y, int w, int h);
void gfx_draw_char_ex(char c, int x, int y, uint32_t fg, uint32_t bg,
                      const font_t* f, uint32_t num, uint32_t den);
void gfx_draw_char_px(char c, int px, int py, uint32_t fg, uint32_t bg,
                      const font_t* f, uint32_t scale);
void print_at_color(const char* message, int row, int col, unsigned char color);
void print_at(char* message, int row, int col);
void print_char_at(char c, int row, int col, unsigned char color);
void disable_cursor();
void print_line_scroll(const char* msg, int col, int* row, unsigned char color);
void enable_cursor(unsigned char start, unsigned char end);
void update_cursor(int row, int col);
void scroll();
void draw_logo();

#endif