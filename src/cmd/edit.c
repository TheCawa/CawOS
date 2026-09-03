#include "commands.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/memory.h"
#include "fs.h"
#include "kernel/idt.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"

#define EDIT_BUF_SIZE 16384
#define EDIT_FILENAME_MAX 64
#define EDIT_STATUS_MSG_MS 2000
#define EDIT_CURSOR_BLINK_MS 500

typedef struct {
    char filename[EDIT_FILENAME_MAX];
    char buffer[EDIT_BUF_SIZE];
    int len;
    int cursor;
    int top_line;
    int col_offset;
    int modified;
    int screen_rows;
    int screen_cols;
    int edit_rows;
    int edit_cols;
    char status_msg[80];
    uint32_t status_msg_timeout;
    int shift_active;
    int caps_active;
    int cursor_visible;
    uint32_t last_cursor_tick;
} editor_t;

static editor_t g_editor;

static void edit_set_status(editor_t* e, const char* msg) {
    safe_strcpy(e->status_msg, msg, sizeof(e->status_msg));
    e->status_msg_timeout = system_ticks + (EDIT_STATUS_MSG_MS / 10);
}

static int edit_get_line_count(editor_t* e) {
    int count = 1;
    for (int i = 0; i < e->len; i++) {
        if (e->buffer[i] == '\n') count++;
    }
    return count;
}

static int edit_get_line_start(editor_t* e, int line) {
    int current_line = 0;
    for (int i = 0; i <= e->len; i++) {
        if (current_line == line) return i;
        if (i < e->len && e->buffer[i] == '\n') current_line++;
    }
    return e->len;
}

static int edit_get_line_len(editor_t* e, int line) {
    int start = edit_get_line_start(e, line);
    int len = 0;
    while (start + len < e->len && e->buffer[start + len] != '\n') len++;
    return len;
}

static int edit_get_line_of(editor_t* e, int offset) {
    int line = 0;
    for (int i = 0; i < offset && i < e->len; i++) {
        if (e->buffer[i] == '\n') line++;
    }
    return line;
}

static int edit_get_col_in_line(editor_t* e, int offset) {
    int col = 0;
    int i = offset - 1;
    while (i >= 0 && e->buffer[i] != '\n') {
        col++;
        i--;
    }
    return col;
}

static void edit_clamp_cursor(editor_t* e) {
    if (e->cursor < 0) e->cursor = 0;
    if (e->cursor > e->len) e->cursor = e->len;
}

static void edit_ensure_cursor_visible(editor_t* e) {
    int line = edit_get_line_of(e, e->cursor);
    int col = edit_get_col_in_line(e, e->cursor);

    if (line < e->top_line) {
        e->top_line = line;
    } else if (line >= e->top_line + e->edit_rows) {
        e->top_line = line - e->edit_rows + 1;
    }

    if (col < e->col_offset) {
        e->col_offset = col;
    } else if (col >= e->col_offset + e->edit_cols) {
        e->col_offset = col - e->edit_cols + 1;
    }
}

static void edit_insert_char(editor_t* e, char c) {
    if (e->len >= EDIT_BUF_SIZE - 1) {
        edit_set_status(e, "Buffer full!");
        return;
    }
    for (int i = e->len; i > e->cursor; i--) {
        e->buffer[i] = e->buffer[i - 1];
    }
    e->buffer[e->cursor] = c;
    e->cursor++;
    e->len++;
    e->buffer[e->len] = '\0';
    e->modified = 1;
}

static void edit_delete_char(editor_t* e) {
    if (e->cursor <= 0) return;
    for (int i = e->cursor - 1; i < e->len - 1; i++) {
        e->buffer[i] = e->buffer[i + 1];
    }
    e->cursor--;
    e->len--;
    e->buffer[e->len] = '\0';
    e->modified = 1;
}

static void edit_split_line(editor_t* e) {
    if (e->len >= EDIT_BUF_SIZE - 1) {
        edit_set_status(e, "Buffer full!");
        return;
    }
    for (int i = e->len; i > e->cursor; i--) {
        e->buffer[i] = e->buffer[i - 1];
    }
    e->buffer[e->cursor] = '\n';
    e->cursor++;
    e->len++;
    e->buffer[e->len] = '\0';
    e->modified = 1;
}

static void edit_move_cursor(editor_t* e, int dx, int dy) {
    int line = edit_get_line_of(e, e->cursor);
    int col = edit_get_col_in_line(e, e->cursor);

    if (dy != 0) {
        int target_line = line + dy;
        int line_count = edit_get_line_count(e);
        if (target_line < 0) target_line = 0;
        if (target_line >= line_count) target_line = line_count - 1;
        int target_line_len = edit_get_line_len(e, target_line);
        if (col > target_line_len) col = target_line_len;
        e->cursor = edit_get_line_start(e, target_line) + col;
    } else if (dx != 0) {
        e->cursor += dx;
    }

    edit_clamp_cursor(e);
    edit_ensure_cursor_visible(e);
}

static void edit_move_home(editor_t* e) {
    int line = edit_get_line_of(e, e->cursor);
    e->cursor = edit_get_line_start(e, line);
    e->col_offset = 0;
}

static void edit_move_end(editor_t* e) {
    int line = edit_get_line_of(e, e->cursor);
    e->cursor = edit_get_line_start(e, line) + edit_get_line_len(e, line);
}

static int edit_load(editor_t* e) {
    e->len = 0;
    e->cursor = 0;
    e->top_line = 0;
    e->col_offset = 0;
    e->modified = 0;
    e->buffer[0] = '\0';

    uint32_t size = fs_get_size(e->filename);
    if (size == 0) {
        if (fs_exists(e->filename)) {
            edit_set_status(e, "Empty file loaded");
        } else {
            edit_set_status(e, "New file");
        }
        return 1;
    }

    if (size >= EDIT_BUF_SIZE) {
        edit_set_status(e, "File too large!");
        return 0;
    }

    uint32_t sectors = (size / 512) + 1;
    uint32_t buffer_size = sectors * 512;
    uint8_t* temp = (uint8_t*)malloc(buffer_size);
    if (!temp) {
        edit_set_status(e, "Out of memory!");
        return 0;
    }

    int ok = fs_load_to_memory(e->filename, temp);
    if (ok) {
        memcpy(e->buffer, temp, size);
        e->len = size;
        e->buffer[e->len] = '\0';
        edit_set_status(e, "File loaded");
    } else {
        edit_set_status(e, "Load failed!");
    }

    free(temp);
    return ok;
}

static int edit_save(editor_t* e) {
    if (!fs_exists(e->filename)) {
        int dummy_row = 0;
        if (!fs_create(e->filename, &dummy_row)) {
            edit_set_status(e, "Create failed!");
            return 0;
        }
    }

    if (fs_write(e->filename, (uint8_t*)e->buffer, e->len)) {
        e->modified = 0;
        edit_set_status(e, "Saved");
        return 1;
    } else {
        edit_set_status(e, "Save failed!");
        return 0;
    }
}

static void edit_draw_hline(int row, int col, int width, unsigned char color) {
    for (int i = 0; i < width; i++) {
        print_char_at('-', row, col + i, color);
    }
}

static void edit_draw_vline(int row, int col, int height, unsigned char color) {
    for (int i = 0; i < height; i++) {
        print_char_at('|', row + i, col, color);
    }
}

static void edit_draw_frame(editor_t* e) {
    unsigned char frame_color = 0x0B;

    print_char_at('+', 0, 0, frame_color);
    print_char_at('+', 0, e->screen_cols - 1, frame_color);
    print_char_at('+', e->screen_rows - 2, 0, frame_color);
    print_char_at('+', e->screen_rows - 2, e->screen_cols - 1, frame_color);

    edit_draw_hline(0, 1, e->screen_cols - 2, frame_color);
    edit_draw_hline(e->screen_rows - 2, 1, e->screen_cols - 2, frame_color);
    edit_draw_vline(1, 0, e->screen_rows - 3, frame_color);
    edit_draw_vline(1, e->screen_cols - 1, e->screen_rows - 3, frame_color);

    char title[64];
    snprintf(title, sizeof(title), " Edit: %s ", e->filename);
    int title_len = strlen(title);
    int title_col = (e->screen_cols - title_len) / 2;
    if (title_col < 1) title_col = 1;
    print_at_color(title, 0, title_col, 0x0E);
}

static void edit_draw_status(editor_t* e) {
    unsigned char status_color = 0x70;
    char status[128];
    char line_str[12], col_str[12];
    int line = edit_get_line_of(e, e->cursor) + 1;
    int col = edit_get_col_in_line(e, e->cursor) + 1;
    itoa(line, line_str);
    itoa(col, col_str);

    const char* msg = e->status_msg;
    if (system_ticks > e->status_msg_timeout) {
        msg = "";
    }

    snprintf(status, sizeof(status),
        " %s | L:%s C:%s | %s | Ctrl+S=Save Esc=Exit ",
        e->modified ? "MODIFIED" : "SAVED",
        line_str, col_str,
        msg);

    int status_len = strlen(status);
    for (int i = 0; i < e->screen_cols; i++) {
        print_char_at(' ', e->screen_rows - 1, i, status_color);
    }
    if (status_len > e->screen_cols) status_len = e->screen_cols;
    print_at_color(status, e->screen_rows - 1, 0, status_color);
}

static void edit_draw_content(editor_t* e) {
    int line_count = edit_get_line_count(e);
    int cursor_line = edit_get_line_of(e, e->cursor);
    int cursor_col = edit_get_col_in_line(e, e->cursor);

    for (int screen_row = 0; screen_row < e->edit_rows; screen_row++) {
        int file_line = e->top_line + screen_row;
        int draw_row = screen_row + 1;

        for (int col = 0; col < e->edit_cols; col++) {
            print_char_at(' ', draw_row, col + 1, 0x0F);
        }

        if (file_line >= line_count) {
            if (e->cursor_visible && file_line == cursor_line) {
                int cursor_screen_col = cursor_col - e->col_offset;
                if (cursor_screen_col >= 0 && cursor_screen_col < e->edit_cols) {
                    print_char_at(' ', draw_row, cursor_screen_col + 1, 0xF0);
                }
            }
            continue;
        }

        int line_start = edit_get_line_start(e, file_line);
        int line_len = edit_get_line_len(e, file_line);

        int draw_len = line_len - e->col_offset;
        if (draw_len < 0) draw_len = 0;
        if (draw_len > e->edit_cols) draw_len = e->edit_cols;

        for (int i = 0; i < draw_len; i++) {
            char c = e->buffer[line_start + e->col_offset + i];
            if (c == '\t') c = ' ';
            unsigned char color = 0x0F;
            if (e->cursor_visible && file_line == cursor_line &&
                (e->col_offset + i) == cursor_col) {
                color = 0xF0;
            }
            print_char_at(c, draw_row, i + 1, color);
        }

        if (e->cursor_visible && file_line == cursor_line &&
            cursor_col >= e->col_offset + draw_len &&
            cursor_col - e->col_offset < e->edit_cols) {
            int cursor_screen_col = cursor_col - e->col_offset;
            print_char_at(' ', draw_row, cursor_screen_col + 1, 0xF0);
        }
    }
}

static void edit_draw(editor_t* e) {
    clear_screen();
    edit_draw_frame(e);
    edit_draw_content(e);
    edit_draw_status(e);
}

static int edit_confirm_exit(editor_t* e) {
    if (!e->modified) return 1;

    int row = e->screen_rows / 2;
    int col = (e->screen_cols - 30) / 2;
    if (col < 0) col = 0;

    print_at_color(" File modified. Save? [Y/N/C] ", row, col, 0x4F);

    while (1) {
        __asm__ volatile("hlt");
        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
            if (!(scancode & 0x80)) {
                if (scancode == 0x15) { // Y
                    edit_save(e);
                    return 1;
                } else if (scancode == 0x31) { // N
                    return 1;
                } else if (scancode == 0x2E) { // C
                    return 0;
                }
            }
        }
    }
}

void cmd_edit(char* args, int* row) {
    (void)row;

    if (!args || args[0] == '\0') {
        print_line_scroll("Usage: edit <filename>", 0, row, 0x0C);
        return;
    }

    if (strlen(args) >= EDIT_FILENAME_MAX) {
        print_line_scroll("Error: Filename too long!", 0, row, 0x0C);
        return;
    }

    memset(&g_editor, 0, sizeof(g_editor));
    safe_strcpy(g_editor.filename, args, sizeof(g_editor.filename));

    g_editor.screen_rows = screen_get_rows();
    g_editor.screen_cols = screen_get_cols();

    if (g_editor.screen_rows < 6 || g_editor.screen_cols < 20) {
        print_line_scroll("Error: Screen too small for editor!", 0, row, 0x0C);
        return;
    }

    g_editor.edit_rows = g_editor.screen_rows - 3;
    g_editor.edit_cols = g_editor.screen_cols - 2;

    edit_load(&g_editor);
    edit_ensure_cursor_visible(&g_editor);

    clear_screen();
    disable_cursor();
    edit_draw(&g_editor);
    g_editor.cursor_visible = 1;
    g_editor.last_cursor_tick = system_ticks;

    int ctrl_pressed = 0;
    int running = 1;
    int redraw_status = 1;
    int redraw_content = 0;

    while (running) {
        if (system_ticks - g_editor.last_cursor_tick >= EDIT_CURSOR_BLINK_MS / 10) {
            g_editor.cursor_visible = !g_editor.cursor_visible;
            g_editor.last_cursor_tick = system_ticks;
            redraw_content = 1;
        }

        if (redraw_content) {
            edit_draw_content(&g_editor);
            redraw_content = 0;
        }
        if (redraw_status) {
            edit_draw_status(&g_editor);
            redraw_status = 0;
        }

        __asm__ volatile("hlt");

        if (is_interrupt_requested()) {
            clear_interrupt();
            break;
        }

        if (key_queue_head == key_queue_tail) continue;

        unsigned char scancode = key_queue[key_queue_head];
        key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;

        if (scancode & 0x80) {
            unsigned char released = scancode & 0x7F;
            if (released == LSHIFT || released == RSHIFT) g_editor.shift_active = 0;
            if (released == 0x1D) ctrl_pressed = 0;
            continue;
        }

        if (scancode == LSHIFT || scancode == RSHIFT) {
            g_editor.shift_active = 1;
            continue;
        }
        if (scancode == CAPSLOCK) {
            g_editor.caps_active = !g_editor.caps_active;
            continue;
        }
        if (scancode == 0x1D) {
            ctrl_pressed = 1;
            continue;
        }

        if (scancode == ESC) {
            if (edit_confirm_exit(&g_editor)) {
                running = 0;
            } else {
                edit_draw(&g_editor);
            }
            continue;
        }

        if (ctrl_pressed) {
            if (scancode == 0x1F) { // Ctrl+S
                edit_save(&g_editor);
                redraw_status = 1;
            }
            continue;
        }

        int need_full_redraw = 0;

        if (scancode == BACKSPACE) {
            edit_delete_char(&g_editor);
            edit_ensure_cursor_visible(&g_editor);
            need_full_redraw = 1;
        } else if (scancode == ENTER) {
            edit_split_line(&g_editor);
            edit_ensure_cursor_visible(&g_editor);
            need_full_redraw = 1;
        } else if (scancode == ARROW_UP) {
            edit_move_cursor(&g_editor, 0, -1);
        } else if (scancode == ARROW_DOWN) {
            edit_move_cursor(&g_editor, 0, 1);
        } else if (scancode == ARROW_LEFT) {
            edit_move_cursor(&g_editor, -1, 0);
        } else if (scancode == ARROW_RIGHT) {
            edit_move_cursor(&g_editor, 1, 0);
        } else if (scancode == 0x47) { // Home
            edit_move_home(&g_editor);
        } else if (scancode == 0x4F) { // End
            edit_move_end(&g_editor);
        } else if (scancode == 0x49) { // Page Up
            g_editor.top_line -= g_editor.edit_rows;
            if (g_editor.top_line < 0) g_editor.top_line = 0;
            edit_ensure_cursor_visible(&g_editor);
            need_full_redraw = 1;
        } else if (scancode == 0x51) { // Page Down
            int line_count = edit_get_line_count(&g_editor);
            g_editor.top_line += g_editor.edit_rows;
            if (g_editor.top_line >= line_count) g_editor.top_line = line_count - 1;
            if (g_editor.top_line < 0) g_editor.top_line = 0;
            edit_ensure_cursor_visible(&g_editor);
            need_full_redraw = 1;
        } else {
            char key = g_editor.shift_active ? shift_map[scancode] : ascii_map[scancode];
            if (key >= 'a' && key <= 'z') {
                if (g_editor.caps_active && !g_editor.shift_active) key -= 32;
                if (g_editor.caps_active && g_editor.shift_active) key += 32;
            } else if (key >= 'A' && key <= 'Z') {
                if (g_editor.caps_active && !g_editor.shift_active) key += 32;
                if (g_editor.caps_active && g_editor.shift_active) key -= 32;
            }
            if (key >= 32 && key <= 126) {
                edit_insert_char(&g_editor, key);
                edit_ensure_cursor_visible(&g_editor);
                need_full_redraw = 1;
            }
        }

        if (need_full_redraw) {
            edit_draw_content(&g_editor);
            g_editor.cursor_visible = 1;
            g_editor.last_cursor_tick = system_ticks;
        }
        redraw_status = 1;
    }

    clear_screen();
    enable_cursor(13, 15);
}

REGISTER_COMMAND("edit", cmd_edit, 1);
