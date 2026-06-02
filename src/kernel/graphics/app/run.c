#include "gui/window.h"
#include "gui/programs.h"
#include "libc/util.h"
#include "kernel/memory.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF
#define INPUT_MAX 64

typedef struct {
    char input_buffer[INPUT_MAX];
    int input_len;
} run_data_t;

static void run_draw_cb(window_t* win, int cx, int cy, int cw, int ch) {
    run_data_t* data = (run_data_t*)win->user_data;
    if (!data) return;
    window_draw_rect(win, 0, 0, cw, ch, 0x00F4F4F9);
    window_draw_text(win, "Type the name of a program to launch:", 15, 15, 0x00000000, COLOR_TRANSPARENT);
    window_draw_rect(win, 15, 35, cw - 30, 20, 0x00FFFFFF);
    window_draw_line(win, 15, 35, cw - 15, 35, 0x00888888);
    window_draw_line(win, 15, 35, 15, 55, 0x00888888);
    window_draw_line(win, cw - 15, 35, cw - 15, 55, 0x00888888);
    window_draw_line(win, 15, 55, cw - 15, 55, 0x00888888);
    if (data->input_len > 0) {
        window_draw_text(win, data->input_buffer, 22, 41, 0x00000000, COLOR_TRANSPARENT);
    }
    int cursor_x = 22 + (data->input_len * 8);
    if (cursor_x < cw - 20) {
        window_draw_text(win, "|", cursor_x, 41, 0x00000000, COLOR_TRANSPARENT);
    }
}

static void run_key_cb(window_t* win, char ascii) {
    run_data_t* data = (run_data_t*)win->user_data;
    if (!data) return;
    if (ascii == '\b' || ascii == 8) {
        if (data->input_len > 0) {
            data->input_len--;
            data->input_buffer[data->input_len] = '\0';
        }
    } 
    else if (ascii == '\r' || ascii == '\n') {
        data->input_buffer[data->input_len] = '\0';
        if (data->input_len > 0) {
            execute_program(data->input_buffer);
        }
        window_close(win);
    } 
    else if (ascii == 27) {
        window_close(win);
    } 
    else if ((ascii >= 32 && ascii <= 126) && data->input_len < INPUT_MAX - 1) {
        data->input_buffer[data->input_len] = ascii;
        data->input_len++;
        data->input_buffer[data->input_len] = '\0';
    }
}

void run_launch(const char* args) {
    window_t* win = window_create("Run...", 150, 150, 320, 90, run_draw_cb);
    if (win) {
        run_data_t* data = (run_data_t*)malloc(sizeof(run_data_t));
        if (data) {
            data->input_len = 0;
            data->input_buffer[0] = '\0';
            win->user_data = data;
            win->handle_key = run_key_cb;
        }
    }
}

REGISTER_PROGRAM("run", run_launch);