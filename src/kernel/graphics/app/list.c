#include "gui/window.h"
#include "gui/programs.h"
#include "libc/util.h"

extern program_t __start_prog;
extern program_t __stop_prog;

#define COLOR_TRANSPARENT 0xFFFFFFFF

static void list_draw_cb(window_t* win, int cx, int cy, int cw, int ch) {
    window_draw_rect(win, 0, 0, cw, ch, 0x00FFFFFF);
    int y_offset = 10;
    int count = 0;
    for (const program_t* prog = &__start_prog; prog < &__stop_prog; prog++) {
        if (prog->name && prog->name[0] != '\0') {
            window_draw_text(win, prog->name, 10, y_offset, 0x00000000, COLOR_TRANSPARENT);
            y_offset += 15;
            count++;
        }
    }
    if (count == 0) {
        window_draw_text(win, "No programs registered.", 10, 10, 0x00FF0000, COLOR_TRANSPARENT);
    }
}

void list_launch(const char* args) {
    int count = 0;
    for (const program_t* prog = &__start_prog; prog < &__stop_prog; prog++) {
        if (prog->name && prog->name[0] != '\0') {
            count++;
        }
    }
    int height = (count > 0) ? (count * 15 + 40) : 100;
    if (height > 400) {
        height = 400; 
    }
    window_t* win = window_create("Installed Programs", 50, 50, 200, height, list_draw_cb);
}

REGISTER_PROGRAM("list", list_launch);