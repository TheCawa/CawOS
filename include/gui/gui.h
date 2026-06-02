#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define TASKBAR_HEIGHT      22
#define START_BUTTON_WIDTH  80
#define COLOR_DESKTOP_BG    0x00191920
#define COLOR_TASKBAR_BG    0x00808080
#define COLOR_TASKBAR_TEXT  0x00FFFFFF
#define COLOR_BUTTON_BG     0x00A0A0A0
#define COLOR_BUTTON_HOVER  0x00000080

typedef struct {
    int x, y;
    int width, height;
    const char* text;
    int hovered;
    int pressed;
} button_t;

extern button_t start_button;

void taskbar_init(void);
void taskbar_draw(void);
void taskbar_handle_click(int x, int y);
void taskbar_handle_mouse_move(int x, int y);
void start_menu_show(void);
void start_menu_hide(void);
int start_menu_is_visible(void);

#endif