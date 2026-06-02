#include "gui/window.h"
#include "gui/programs.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF

static void find_draw_cb(window_t* win, int cx, int cy, int cw, int ch) {
    window_draw_rect(win, 0, 0, cw, ch, 0x00111111);
    window_draw_text(win, "Search Companion", 15, 15, 0x0000FF00, COLOR_TRANSPARENT);
    window_draw_rect(win, 15, 35, cw - 30, 18, 0x00222222);
    window_draw_text(win, "Look for: *.c", 20, 40, 0x0000AA00, COLOR_TRANSPARENT);
    window_draw_text(win, "Searching virtual storage...", 15, 70, 0x00888888, COLOR_TRANSPARENT);
    window_draw_text(win, "> kernel.c           (FOUND)", 15, 90, 0x00FFFFFF, COLOR_TRANSPARENT);
    window_draw_text(win, "> gui/window.c     (FOUND)", 15, 105, 0x00FFFFFF, COLOR_TRANSPARENT);
}

void find_launch(const char* args) {
    window_create("Find Files", 220, 120, 280, 150, find_draw_cb);
}
REGISTER_PROGRAM("find", find_launch);