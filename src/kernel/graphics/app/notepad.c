#include "gui/window.h"
#include "gui/programs.h"
#include "libc/util.h"
#include "kernel/memory.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF
#define BUFFER_MAX 256

typedef struct {
    char text_buffer[BUFFER_MAX];
    int text_len;
} notepad_data_t;

static void notepad_draw_cb(window_t* win, int cx, int cy, int cw, int ch) {
    notepad_data_t* data = (notepad_data_t*)win->user_data;
    if (!data) return;
    window_draw_rect(win, 0, 0, cw, ch, 0x00FFFFFF);
    window_draw_line(win, 0, 0, cw, 0, 0x00888888);
    int line_y = 10;
    char line_buf[BUFFER_MAX];
    int line_len = 0;
    int cursor_x = 10;
    int cursor_y = 10;
    for (int i = 0; i <= data->text_len; i++) {
        char c = data->text_buffer[i];
        if (c == '\n' || c == '\0') {
            line_buf[line_len] = '\0';
            if (line_len > 0) {
                window_draw_text(win, line_buf, 10, line_y, 0x00000000, COLOR_TRANSPARENT);
            }
            if (i == data->text_len) {
                cursor_x = 10 + (line_len * 8);
                cursor_y = line_y;
            }
            if (c == '\n') {
                line_y += 12;
                line_len = 0;
            }
        } else {
            if (line_len < (int)sizeof(line_buf) - 1) {
                line_buf[line_len++] = c;
            }
        }
    }
    if (cursor_x < cw - 15 && cursor_y < ch - 25) {
        window_draw_text(win, "|", cursor_x, cursor_y, 0x00FF0000, COLOR_TRANSPARENT);
    }
    window_draw_rect(win, 0, ch - 20, cw, 20, 0x00EEEEEE);
    window_draw_line(win, 0, ch - 20, cw, ch - 20, 0x00CCCCCC);
    window_draw_text(win, "Type text... Enter for newline. Backspace to delete.", 10, ch - 14, 0x00555555, COLOR_TRANSPARENT);
}

static void notepad_key_cb(window_t* win, char ascii) {
    notepad_data_t* data = (notepad_data_t*)win->user_data;
    if (!data) return;
    
    if (ascii == '\b' || ascii == 8) {
        if (data->text_len > 0) {
            data->text_len--;
            data->text_buffer[data->text_len] = '\0';
        }
        return;
    }
    if (ascii == '\r') {
        ascii = '\n';
    }
    if ((ascii >= 32 && ascii <= 126) || ascii == '\n') {
        if (data->text_len < BUFFER_MAX - 1) {
            data->text_buffer[data->text_len] = ascii;
            data->text_len++;
            data->text_buffer[data->text_len] = '\0';
        }
    }
}

void notepad_launch(const char* args) {
    window_t* win = window_create("CawPad Text Editor", 100, 80, 360, 200, notepad_draw_cb);
    if (win) {
        notepad_data_t* data = (notepad_data_t*)malloc(sizeof(notepad_data_t));
        if (data) {
            data->text_len = 0;
            data->text_buffer[0] = '\0';
            win->user_data = data;
            win->handle_key = notepad_key_cb;
        }
    }
}

REGISTER_PROGRAM("notepad", notepad_launch);