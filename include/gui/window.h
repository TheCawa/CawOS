#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stddef.h>

#define MAX_WINDOWS 16
#define WIN_TITLE_MAX 32

#define WINDOW_TITLEBAR_HEIGHT 20
#define WINDOW_CLOSE_BTN_SIZE  16

#define COLOR_WIN_BG            0x00D4D0C8
#define COLOR_WIN_BORDER        0x00000000
#define COLOR_WIN_TITLE_ACTIVE  0x000A246A
#define COLOR_WIN_TITLE_TEXT    0x00FFFFFF
#define COLOR_WIN_BTN_BG        0x00D4D0C8

typedef struct window {
    int id;
    int x, y;
    int width, height;
    void* user_data;
    void (*handle_key)(struct window* win, char ascii);
    int (*handle_scancode)(struct window* win, unsigned char scancode);
    void (*handle_mouse)(struct window* self, int local_x, int local_y, int is_double);
    char title[WIN_TITLE_MAX];
    int is_visible;
    int is_focused;
    int is_minimized;
    void (*draw_content)(struct window* self, int x, int y, int w, int h);
} window_t;

void window_manager_init();
window_t* window_create(const char* title, int x, int y, int w, int h, void (*draw_cb)(window_t*, int, int, int, int));
void window_close(window_t* win);
void window_manager_draw();
window_t* window_get_focused(void);
window_t* window_find_at(int mx, int my);
int window_manager_handle_click(int mx, int my);
void window_manager_handle_move(int mx, int my);
void window_manager_handle_release();
void window_get_client_rect(window_t* win, int* x, int* y, int* w, int* h);
void window_draw_rect(window_t* win, int local_x, int local_y, int w, int h, uint32_t color);
void window_draw_text(window_t* win, const char* text, int local_x, int local_y, uint32_t fg, uint32_t bg);
void window_draw_line(window_t* win, int local_x1, int local_y1, int local_x2, int local_y2, uint32_t color);
void window_minimize(window_t* win);
void window_restore(window_t* win);
void window_manager_draw_taskbar_buttons(void);
int  window_manager_handle_taskbar_click(int x, int y);
int window_manager_handle_scancode(unsigned char scancode);
int window_manager_handle_key(char ascii);

#endif