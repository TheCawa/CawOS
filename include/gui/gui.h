#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define TASKBAR_HEIGHT      24
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
extern int gui_clip_enabled;
extern int gui_clip_x0, gui_clip_y0, gui_clip_x1, gui_clip_y1;

static inline int gui_in_clip(int x, int y) {
    if (!gui_clip_enabled) return 1;
    return x >= gui_clip_x0 && x < gui_clip_x1 && y >= gui_clip_y0 && y < gui_clip_y1;
}

void taskbar_init(void);
void taskbar_draw(void);
void taskbar_handle_click(int x, int y);
void taskbar_handle_mouse_move(int x, int y);
void start_menu_show(void);
void start_menu_hide(void);
int start_menu_is_visible(void);

#endif