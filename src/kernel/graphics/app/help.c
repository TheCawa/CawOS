#include "gui/window.h"
#include "gui/programs.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF

static void help_draw_cb(window_t* win, int cx, int cy, int cw, int ch) {
    window_draw_rect(win, 0, 0, cw, ch, 0x002B2D42);
    window_draw_text(win, "Quick Help Guide", 15, 15, 0x00EF233C, COLOR_TRANSPARENT);
    window_draw_line(win, 15, 28, cw - 15, 28, 0x008D99AE);
    window_draw_text(win, "- Mouse: Drag titles to move windows.", 15, 45, 0x00EDF2F4, COLOR_TRANSPARENT);
    window_draw_text(win, "- Start: Click CawOS button for menu.", 15, 65, 0x00EDF2F4, COLOR_TRANSPARENT);
    window_draw_text(win, "- Run:   Launch binaries by name.", 15, 85, 0x00EDF2F4, COLOR_TRANSPARENT);
    window_draw_text(win, "- Cross: Click [x] to close an app.", 15, 105, 0x00EDF2F4, COLOR_TRANSPARENT);
}

void help_launch(const char* args) {
    window_create("System Help", 80, 60, 340, 160, help_draw_cb);
}
REGISTER_PROGRAM("help", help_launch);