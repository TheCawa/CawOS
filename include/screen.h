#ifndef SCREEN_H
#define SCREEN_H

extern unsigned char current_color;

void clear_screen();
void print_at_color(const char* message, int row, int col, unsigned char color);
void print_at(char* message, int row, int col);
void disable_cursor();
void enable_cursor(unsigned char cursor_start, unsigned char cursor_end);
void update_cursor(int row, int col);
void draw_logo();
void scroll();
void print_line_scroll(const char* msg, int col, int* row, unsigned char color);
void print_char_at(char c, int row, int col, unsigned char color);

#endif