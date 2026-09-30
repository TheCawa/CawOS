#include "gui/programs.h"
#include "gui/window.h"
#include "gui/gui.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "fs.h"
#include "kernel/memory.h"
#include "libc/keyboard_map.h"

#define EXPL_ROW_H 20
#define EXPL_PATHBAR_H 22
#define EXPL_MAX_ENTRIES 64
#define EXPL_BTN_UP_W 40
#define COLOR_TRANSPARENT 0xFFFFFFFF

typedef struct {
    char name[32];
    int is_dir;
    int is_virtual;
    uint32_t size;
} expl_entry_t;

typedef struct {
    char path[64];
    int is_programs;
    expl_entry_t entries[EXPL_MAX_ENTRIES];
    int entry_count;
    int selected;
    int scroll_top;
} explorer_state_t;

extern file_t fs[MAX_FILES];
extern program_t __start_prog;
extern program_t __stop_prog;

static void explorer_build_entries(explorer_state_t* st) {
    st->entry_count = 0;
    if (st->is_programs) {
        for (const program_t* p = &__start_prog; p < &__stop_prog; p++) {
            if (st->entry_count >= EXPL_MAX_ENTRIES) break;
            expl_entry_t* e = &st->entries[st->entry_count++];
            safe_strcpy(e->name, p->name, 32);
            e->is_dir = 0; e->is_virtual = 0; e->size = 0;
        }
        return;
    }
    if (strcmp(st->path, "/") == 0) {
        expl_entry_t* e = &st->entries[st->entry_count++];
        safe_strcpy(e->name, "Programs", 32);
        e->is_dir = 1; e->is_virtual = 1; e->size = 0;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists) continue;
        if (strcmp(fs[i].dir, st->path) != 0) continue;
        if (st->entry_count >= EXPL_MAX_ENTRIES) break;
        expl_entry_t* e = &st->entries[st->entry_count++];
        safe_strcpy(e->name, fs[i].name, 32);
        e->is_dir = fs[i].is_dir;
        e->is_virtual = 0;
        e->size = fs[i].size_bytes;
    }
}

static void explorer_go_up(explorer_state_t* st) {
    if (st->is_programs) {
        st->is_programs = 0;
        safe_strcpy(st->path, "/", sizeof(st->path));
        return;
    }
    if (strcmp(st->path, "/") == 0) return;
    char* slash = NULL;
    for (char* p = st->path; *p; p++) if (*p == '/') slash = p;
    if (!slash) return;
    if (slash == st->path) safe_strcpy(st->path, "/", sizeof(st->path));
    else *slash = '\0';
}

static void explorer_open_entry(window_t* win, explorer_state_t* st, int idx) {
    (void)win;
    expl_entry_t* e = &st->entries[idx];
    if (st->is_programs) {
        execute_program(e->name);
        return;
    }
    if (e->is_virtual) {
        st->is_programs = 1;
        safe_strcpy(st->path, "/Programs", sizeof(st->path));
        explorer_build_entries(st);
        st->selected = -1; st->scroll_top = 0;
        return;
    }
    if (e->is_dir) {
        size_t need = strlen(st->path) + 1 + strlen(e->name);
        if (need >= sizeof(st->path)) return;
        if (strcmp(st->path, "/") != 0) strcat(st->path, "/");
        strcat(st->path, e->name);
        explorer_build_entries(st);
        st->selected = -1; st->scroll_top = 0;
        return;
    }
    char cmd[48];
    safe_strcpy(cmd, "notepad ", sizeof(cmd));
    if (strlen(cmd) + strlen(e->name) < sizeof(cmd)) {
        strcat(cmd, e->name);
        execute_program(cmd);
    }
}

static int explorer_visible_rows(window_t* win) {
    int ch = win->height - 4 - WINDOW_TITLEBAR_HEIGHT;
    int list_h = ch - EXPL_PATHBAR_H;
    int visible = list_h / EXPL_ROW_H;
    return (visible < 1) ? 1 : visible;
}

static int explorer_key(window_t* win, unsigned char scancode) {
    explorer_state_t* st = (explorer_state_t*)win->user_data;
    if (!st) return 0;
    int visible = explorer_visible_rows(win);

    if (scancode == ARROW_UP) {
        if (st->entry_count == 0) return 1;
        if (st->selected < 0) st->selected = 0;
        else if (st->selected > 0) st->selected--;
        if (st->selected < st->scroll_top) st->scroll_top = st->selected;
    } else if (scancode == ARROW_DOWN) {
        if (st->entry_count == 0) return 1;
        if (st->selected < 0) st->selected = 0;
        else if (st->selected < st->entry_count - 1) st->selected++;
        if (st->selected >= st->scroll_top + visible) st->scroll_top = st->selected - visible + 1;
    } else if (scancode == ENTER) {
        if (st->selected >= 0 && st->selected < st->entry_count) {
            explorer_open_entry(win, st, st->selected);
        }
    } else if (scancode == BACKSPACE) {
        explorer_go_up(st);
        explorer_build_entries(st);
        st->selected = -1;
        st->scroll_top = 0;
    } else {
        return 0;
    }
    return 1;
}

static void explorer_draw_file_icon(window_t* win, int x, int y) {
    window_draw_rect(win, x, y, 11, 14, 0x00FFFFFF);
    window_draw_rect(win, x, y, 11, 1, 0x00404040);
    window_draw_rect(win, x, y + 13, 11, 1, 0x00404040);
    window_draw_rect(win, x, y, 1, 14, 0x00404040);
    window_draw_rect(win, x + 10, y, 1, 14, 0x00404040);
    window_draw_rect(win, x + 8, y + 1, 2, 2, 0x00C0C0C0);
    window_draw_rect(win, x + 2, y + 4, 6, 1, 0x00808080);
    window_draw_rect(win, x + 2, y + 6, 6, 1, 0x00808080);
    window_draw_rect(win, x + 2, y + 8, 6, 1, 0x00808080);
    window_draw_rect(win, x + 2, y + 10, 4, 1, 0x00808080);
}

static void explorer_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)cx; (void)cy;
    explorer_state_t* st = (explorer_state_t*)win->user_data;
    if (!st) return;

    window_draw_rect(win, 0, 0, cw, ch, 0x00FFFFFF);
    window_draw_rect(win, 0, 0, cw, EXPL_PATHBAR_H, 0x00D4D0C8);
    const char* ptext = st->is_programs ? "/Programs (virtual)" : st->path;
    window_draw_text(win, ptext, 4, (EXPL_PATHBAR_H - font_height()) / 2, 0x00000000, COLOR_TRANSPARENT);

    window_draw_rect(win, cw - EXPL_BTN_UP_W - 4, 3, EXPL_BTN_UP_W, 14, 0x00C0C0C0);
    window_draw_text(win, "Up", cw - EXPL_BTN_UP_W + 12, 3 + (16 - font_height()) / 2, 0x00000000, COLOR_TRANSPARENT);

    int list_y = EXPL_PATHBAR_H;
    int list_w = cw - 16;
    int list_h = ch - list_y;
    int visible = list_h / EXPL_ROW_H;
    if (visible < 1) visible = 1;
    if (st->scroll_top > st->entry_count - visible) {
        st->scroll_top = st->entry_count - visible;
        if (st->scroll_top < 0) st->scroll_top = 0;
    }

    for (int i = 0; i < visible; i++) {
        int idx = st->scroll_top + i;
        if (idx >= st->entry_count) break;
        int ry = list_y + i * EXPL_ROW_H;
        if (idx == st->selected) {
            window_draw_rect(win, 0, ry, list_w, EXPL_ROW_H, 0x00000080);
        }
        int ix = 6, iy = ry + 3;
        if (st->entries[idx].is_dir) {
            window_draw_rect(win, ix, iy + 2, 14, 10, 0x0000AAAA);
            window_draw_rect(win, ix, iy, 6, 3, 0x0000AAAA);
        } else {
            explorer_draw_file_icon(win, ix + 2, iy);
        }
        uint32_t tcol = (idx == st->selected) ? 0x00FFFFFF : 0x00000000;
        window_draw_text(win, st->entries[idx].name, 26, ry + (EXPL_ROW_H - font_height()) / 2, tcol, COLOR_TRANSPARENT);
        if (!st->entries[idx].is_dir) {
            char nb[12];
            itoa((int)st->entries[idx].size, nb);
            window_draw_text(win, nb, list_w - 60, ry + (EXPL_ROW_H - font_height()) / 2, tcol, COLOR_TRANSPARENT);
        }
    }

    window_draw_rect(win, cw - 16, list_y, 16, 16, 0x00C0C0C0);
    window_draw_text(win, "^", cw - 12, list_y + (16 - font_height()) / 2, 0x00000000, COLOR_TRANSPARENT);
    window_draw_rect(win, cw - 16, list_y + 16, 16, 16, 0x00C0C0C0);
    window_draw_text(win, "v", cw - 12, list_y + 16 + (16 - font_height()) / 2, 0x00000000, COLOR_TRANSPARENT);
}

static void explorer_mouse(window_t* win, int lx, int ly, int is_double) {
    explorer_state_t* st = (explorer_state_t*)win->user_data;
    if (!st) return;
    int cw = win->width - 4;

    if (ly >= 3 && ly < 17 && lx >= cw - EXPL_BTN_UP_W - 4 && lx < cw - 4) {
        explorer_go_up(st);
        explorer_build_entries(st);
        st->selected = -1; st->scroll_top = 0;
        return;
    }

    int list_y = EXPL_PATHBAR_H;
    if (lx >= cw - 16) {
        if (ly >= list_y && ly < list_y + 16) {
            st->scroll_top--;
            if (st->scroll_top < 0) st->scroll_top = 0;
        } else if (ly >= list_y + 16 && ly < list_y + 32) {
            st->scroll_top++;
        }
        return;
    }

    if (ly >= list_y && lx < cw - 16) {
        int row = (ly - list_y) / EXPL_ROW_H;
        int idx = st->scroll_top + row;
        if (idx >= 0 && idx < st->entry_count) {
            st->selected = idx;
            if (is_double) explorer_open_entry(win, st, idx);
        }
    }
}

static void explorer_launch(const char* args) {
    explorer_state_t* st = (explorer_state_t*)malloc(sizeof(explorer_state_t));
    if (!st) return;
    memset(st, 0, sizeof(explorer_state_t));
    if (args && args[0] != '\0') {
        safe_strcpy(st->path, args, sizeof(st->path));
    } else {
        safe_strcpy(st->path, "/", sizeof(st->path));
    }
    st->selected = -1;
    st->scroll_top = 0;
    explorer_build_entries(st);
    window_t* win = window_create("Explorer", 120, 60, 420, 300, explorer_draw);
    if (!win) { free(st); return; }
    win->user_data = st;
    win->handle_mouse = explorer_mouse;
    win->handle_scancode = explorer_key;
    st->selected = (st->entry_count > 0) ? 0 : -1; 
}

REGISTER_PROGRAM("explorer", explorer_launch);