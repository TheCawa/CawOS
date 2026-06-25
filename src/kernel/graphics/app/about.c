#include "gui/window.h"
#include "gui/programs.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF

static void about_draw_cb(window_t* win, int cx, int cy, int cw, int ch) {
    window_draw_rect(win, 0, 0, cw, ch, 0x001E1E24);
    window_draw_line(win, 2, 2, cw - 3, 2, 0x0000E5FF);
    window_draw_line(win, 2, ch - 3, cw - 3, ch - 3, 0x0000E5FF);
    window_draw_text(win, "--- CawOS Microkernel OS ---", 15, 15, 0x0000E5FF, COLOR_TRANSPARENT);
    window_draw_text(win, "Kernel Version:", 15, 45, 0x00AAAAAA, COLOR_TRANSPARENT);
    window_draw_text(win, "v0.3.1", 140, 45, 0x0000FF00, COLOR_TRANSPARENT);
    window_draw_text(win, "Architecture:", 15, 65, 0x00AAAAAA, COLOR_TRANSPARENT);
    window_draw_text(win, "x86_32 Protected", 140, 65, 0x00FFFFFF, COLOR_TRANSPARENT);
    window_draw_text(win, "Environment:", 15, 85, 0x00AAAAAA, COLOR_TRANSPARENT);
    window_draw_text(win, "GUI Subsystem OK", 140, 85, 0x00FFFF00, COLOR_TRANSPARENT);
    window_draw_text(win, "Stay close to the metal.", 15, ch - 22, 0x00555555, COLOR_TRANSPARENT);
}

void about_launch(const char* args) {
    window_create("About CawOS", 180, 100, 310, 160, about_draw_cb);
}

REGISTER_PROGRAM("about", about_launch);