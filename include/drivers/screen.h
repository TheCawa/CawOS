#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

extern unsigned char current_color;
extern int g_is_graphics;
extern int cursor_row;
extern int cursor_col;
extern uint8_t* g_framebuffer;
extern uint32_t g_width, g_height, g_pitch, g_bpp;
extern uint32_t g_cols;
extern uint32_t g_rows;
extern uint8_t* g_shadow;
int screen_get_cols();
int screen_get_rows();
typedef enum {
    VIDEO_VGA,
    VIDEO_FB
} video_mode_t;

void screen_init_graphics(uint32_t framebuffer, uint32_t width, uint32_t height, uint32_t pitch);
void screen_set_font_scale(uint32_t scale_x_num, uint32_t scale_x_den, uint32_t scale_y_num, uint32_t scale_y_den);
void set_video_mode(video_mode_t mode);
void clear_screen();
void gfx_toggle_cursor(int row, int col, int draw);
void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color);
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