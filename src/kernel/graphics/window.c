#include "gui/window.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "gui/gui.h"
#include "kernel/memory.h"
#include "gui/desktop.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF
#define WIN_ABS(x) ((x) < 0 ? -(x) : (x))
#define WIN_TASKBTN_W 110
#define WIN_TASKBTN_X0 (START_BUTTON_WIDTH + 6)
#define WIN_CLOCK_RESERVE 70

static window_t window_pool[MAX_WINDOWS];
static int next_window_id = 1;
static window_t* g_dragged_window = NULL;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;
static window_t* g_last_click_win = NULL;
static uint32_t g_last_click_tick = 0;
static int g_last_click_x = 0, g_last_click_y = 0;

static inline void win_put_pixel_internal(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)g_width || y < 0 || y >= (int)g_height) return;
    uint32_t bpp = g_bpp / 8;
    uint32_t off = y * g_pitch + x * bpp;
    if (!gui_in_clip(x, y)) return;

    if (bpp == 4) {
        *((uint32_t*)(g_shadow + off)) = color;
    } else if (bpp == 3) {
        g_shadow[off]   = (color >> 0)  & 0xFF;
        g_shadow[off+1] = (color >> 8)  & 0xFF;
        g_shadow[off+2] = (color >> 16) & 0xFF;
    } else if (bpp == 2) {
        uint8_t b = (color >> 0) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint16_t rgb555 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *((uint16_t*)(g_shadow + off)) = rgb555;
    }
}

static void win_draw_rect(int x, int y, int width, int height, uint32_t color) {
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            win_put_pixel_internal(x + dx, y + dy, color);
        }
    }
}

static void focus_window(window_t* win) {
    for (int j = 0; j < MAX_WINDOWS; j++) window_pool[j].is_focused = 0;
    if (win) win->is_focused = 1;
}

static window_t* topmost_visible(void) {
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        if (window_pool[i].is_visible && !window_pool[i].is_minimized) return &window_pool[i];
    }
    return NULL;
}

int window_manager_handle_scancode(unsigned char scancode) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (window_pool[i].is_visible && window_pool[i].is_focused) {
            if (window_pool[i].handle_scancode) {
                return window_pool[i].handle_scancode(&window_pool[i], scancode);
            }
            return 0;
        }
    }
    return 0;
}

static void win_draw_text(const char* text, int x, int y, uint32_t fg, uint32_t bg) {
    int tx = x;
    for (int i = 0; text[i]; i++) {
        unsigned char c = text[i];
        if (c >= 128) c = '?';
        for (int row = 0; row < 8; row++) {
            unsigned char line = font8x8_basic[c][row];
            for (int col = 0; col < 8; col++) {
                int px = tx + col, py = y + row;
                int is_fg = (line & (0x80 >> col));
                if (!is_fg && bg == COLOR_TRANSPARENT) continue;
                uint32_t color = is_fg ? fg : bg;
                win_put_pixel_internal(px, py, color);
            }
        }
        tx += 8;
    }
}

void window_get_client_rect(window_t* win, int* x, int* y, int* w, int* h) {
    if (!win) return;
    if (x) *x = win->x + 2;
    if (y) *y = win->y + 2 + WINDOW_TITLEBAR_HEIGHT + 2;
    if (w) *w = win->width - 4;
    if (h) *h = win->height - 4 - WINDOW_TITLEBAR_HEIGHT;
}

void window_draw_rect(window_t* win, int local_x, int local_y, int w, int h, uint32_t color) {
    if (!win || !win->is_visible) return;
    int cx, cy, cw, ch;
    window_get_client_rect(win, &cx, &cy, &cw, &ch);

    for (int dy = 0; dy < h; dy++) {
        int py = cy + local_y + dy;
        if (py < cy || py >= cy + ch) continue;
        
        for (int dx = 0; dx < w; dx++) {
            int px = cx + local_x + dx;
            if (px < cx || px >= cx + cw) continue;
            
            win_put_pixel_internal(px, py, color);
        }
    }
}

void window_draw_text(window_t* win, const char* text, int local_x, int local_y, uint32_t fg, uint32_t bg) {
    if (!win || !win->is_visible || !text) return;
    int cx, cy, cw, ch;
    window_get_client_rect(win, &cx, &cy, &cw, &ch);
    int tx = cx + local_x;
    for (int i = 0; text[i]; i++) {
        unsigned char c = text[i];
        if (c >= 128) c = '?';
        for (int row = 0; row < 8; row++) {
            int py = cy + local_y + row;
            if (py < cy || py >= cy + ch) continue;
            unsigned char line = font8x8_basic[c][row];
            for (int col = 0; col < 8; col++) {
                int px = tx + col;
                if (px < cx || px >= cx + cw) continue;   
                int is_fg = (line & (0x80 >> col));
                if (!is_fg && bg == COLOR_TRANSPARENT) continue;
                uint32_t color = is_fg ? fg : bg;
                win_put_pixel_internal(px, py, color);
            }
        }
        tx += 8;
    }
}

void window_draw_line(window_t* win, int local_x1, int local_y1, int local_x2, int local_y2, uint32_t color) {
    if (!win || !win->is_visible) return;
    int cx, cy, cw, ch;
    window_get_client_rect(win, &cx, &cy, &cw, &ch);
    int x1 = cx + local_x1;
    int y1 = cy + local_y1;
    int x2 = cx + local_x2;
    int y2 = cy + local_y2;
    int dx = WIN_ABS(x2 - x1);
    int dy = WIN_ABS(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        if (x1 >= cx && x1 < cx + cw && y1 >= cy && y1 < cy + ch) {
            win_put_pixel_internal(x1, y1, color);
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void window_manager_init() {
    memset(window_pool, 0, sizeof(window_pool));
    next_window_id = 1;
}

window_t* window_create(const char* title, int x, int y, int w, int h, void (*draw_cb)(window_t*, int, int, int, int)) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!window_pool[i].is_visible) {
            window_pool[i].id = next_window_id++;
            window_pool[i].x = x;
            window_pool[i].y = y;
            window_pool[i].width = w;
            window_pool[i].height = h;
            window_pool[i].is_visible = 1;
            window_pool[i].is_focused = 1;
            window_pool[i].is_minimized = 0;
            window_pool[i].draw_content = draw_cb;
            window_pool[i].user_data = NULL;
            strncpy(window_pool[i].title, title, WIN_TITLE_MAX - 1);
            window_pool[i].title[WIN_TITLE_MAX - 1] = '\0';
            for (int j = 0; j < MAX_WINDOWS; j++) {
                if (i != j) window_pool[j].is_focused = 0;
            }

            window_pool[i].handle_key = NULL;
            window_pool[i].handle_mouse = NULL;
            window_pool[i].handle_scancode = NULL;
            return &window_pool[i];
        }
    }
    return NULL;
}

void window_close(window_t* win) {
    if (win) {
        int was_focused = win->is_focused;
        if (win->user_data) {
            free(win->user_data);
            win->user_data = NULL;
        }
        win->is_visible = 0;
        win->id = 0;
        if (was_focused) focus_window(topmost_visible());
    }
}

static void window_draw_single(window_t* win) {
    if (!win->is_visible) return;
    win_draw_rect(win->x, win->y, win->width, win->height, COLOR_WIN_BG);
    win_draw_rect(win->x, win->y, win->width, 1, COLOR_WIN_BORDER);
    win_draw_rect(win->x, win->y + win->height - 1, win->width, 1, COLOR_WIN_BORDER);
    win_draw_rect(win->x, win->y, 1, win->height, COLOR_WIN_BORDER);
    win_draw_rect(win->x + win->width - 1, win->y, 1, win->height, COLOR_WIN_BORDER);
    uint32_t title_bg = win->is_focused ? COLOR_WIN_TITLE_ACTIVE : 0x00808080;
    win_draw_rect(win->x + 2, win->y + 2, win->width - 4, WINDOW_TITLEBAR_HEIGHT, title_bg);
    int text_y = win->y + 2 + (WINDOW_TITLEBAR_HEIGHT - font_height()) / 2 + 2;
    win_draw_text(win->title, win->x + 6, text_y, COLOR_WIN_TITLE_TEXT, COLOR_TRANSPARENT);
    int btn_x = win->x + win->width - WINDOW_CLOSE_BTN_SIZE - 4;
    int btn_y = win->y + 2 + (WINDOW_TITLEBAR_HEIGHT - WINDOW_CLOSE_BTN_SIZE) / 2;
    win_draw_rect(btn_x, btn_y, WINDOW_CLOSE_BTN_SIZE, WINDOW_CLOSE_BTN_SIZE, COLOR_WIN_BTN_BG);
    win_draw_rect(btn_x, btn_y, WINDOW_CLOSE_BTN_SIZE, 1, 0);
    win_draw_rect(btn_x, btn_y + WINDOW_CLOSE_BTN_SIZE - 1, WINDOW_CLOSE_BTN_SIZE, 1, 0);
    win_draw_rect(btn_x, btn_y, 1, WINDOW_CLOSE_BTN_SIZE, 0);
    win_draw_rect(btn_x + WINDOW_CLOSE_BTN_SIZE - 1, btn_y, 1, WINDOW_CLOSE_BTN_SIZE, 0);
    int cross_x = btn_x + (WINDOW_CLOSE_BTN_SIZE - font_width()) / 2;
    int cross_y = btn_y + (WINDOW_CLOSE_BTN_SIZE - font_height()) / 2 + 2;
    win_draw_text("x", cross_x, cross_y, 0x00000000, COLOR_TRANSPARENT);
    if (win->draw_content) {
        int client_x = win->x + 2;
        int client_y = win->y + 2 + WINDOW_TITLEBAR_HEIGHT + 2;
        int client_w = win->width - 4;
        int client_h = win->height - 4 - WINDOW_TITLEBAR_HEIGHT;
        
        win->draw_content(win, client_x, client_y, client_w, client_h);
    }
}

void window_manager_draw() {
    window_t* focused_window = NULL;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (window_pool[i].is_visible) {
            if (window_pool[i].is_focused) {
                focused_window = &window_pool[i];
            } else {
                window_draw_single(&window_pool[i]);
            }
        }
    }
    if (focused_window) {
        window_draw_single(focused_window);
    }
}

window_t* window_find_at(int mx, int my) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (window_pool[i].is_visible && window_pool[i].is_focused) {
            if (mx >= window_pool[i].x && mx < window_pool[i].x + window_pool[i].width &&
                my >= window_pool[i].y && my < window_pool[i].y + window_pool[i].height) {
                return &window_pool[i];
            }
        }
    }
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        if (window_pool[i].is_visible && !window_pool[i].is_focused) {
            if (mx >= window_pool[i].x && mx < window_pool[i].x + window_pool[i].width &&
                my >= window_pool[i].y && my < window_pool[i].y + window_pool[i].height) {
                return &window_pool[i];
            }
        }
    }
    return NULL;
}

int window_manager_handle_click(int mx, int my) {
    window_t* win = window_find_at(mx, my);
    if (!win) return 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (&window_pool[i] != win) {
            window_pool[i].is_focused = 0;
        }
    }
    win->is_focused = 1;
    int btn_x = win->x + win->width - WINDOW_CLOSE_BTN_SIZE - 4;
    int btn_y = win->y + 2 + (WINDOW_TITLEBAR_HEIGHT - WINDOW_CLOSE_BTN_SIZE) / 2;
    if (mx >= btn_x && mx < btn_x + WINDOW_CLOSE_BTN_SIZE &&
        my >= btn_y && my < btn_y + WINDOW_CLOSE_BTN_SIZE) {
        window_close(win);
        return 1;
    }
    if (my >= win->y + 2 && my < win->y + 2 + WINDOW_TITLEBAR_HEIGHT &&
        mx >= win->x && mx < win->x + win->width) {
        g_dragged_window = win;
        g_drag_off_x = mx - win->x;
        g_drag_off_y = my - win->y;
        return 1; 
    }
    int cx, cy, cw, chh;
    window_get_client_rect(win, &cx, &cy, &cw, &chh);
    extern volatile uint32_t system_ticks;
    int is_double = 0;
    if (win == g_last_click_win &&
        (uint32_t)(system_ticks - g_last_click_tick) <= 40 &&
        WIN_ABS(mx - g_last_click_x) <= 4 && WIN_ABS(my - g_last_click_y) <= 4) {
        is_double = 1;
        g_last_click_win = NULL;
    } else {
        g_last_click_win = win;
        g_last_click_tick = (uint32_t)system_ticks;
        g_last_click_x = mx;
        g_last_click_y = my;
    }
    if (mx >= cx && mx < cx + cw && my >= cy && my < cy + chh && win->handle_mouse) {
        win->handle_mouse(win, mx - cx, my - cy, is_double);
    }
    return 1;
}

void window_manager_handle_move(int mx, int my) {
    if (g_dragged_window) {
        desktop_damage_rect(g_dragged_window->x, g_dragged_window->y,
                            g_dragged_window->width, g_dragged_window->height);
        g_dragged_window->x = mx - g_drag_off_x;
        g_dragged_window->y = my - g_drag_off_y;
        if (g_dragged_window->y < TASKBAR_HEIGHT) {
            g_dragged_window->y = TASKBAR_HEIGHT;
        }
        desktop_damage_rect(g_dragged_window->x, g_dragged_window->y,
                            g_dragged_window->width, g_dragged_window->height);
    }
}

void window_manager_handle_release() {
    g_dragged_window = NULL;
}

int window_manager_handle_key(char ascii) {
    if (ascii == 0) return 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (window_pool[i].is_visible && window_pool[i].is_focused) {
            if (window_pool[i].handle_key) {
                window_pool[i].handle_key(&window_pool[i], ascii);
                return 1;
            }
            break; 
        }
    }
    return 0;
}

window_t* window_get_focused(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (window_pool[i].is_visible && window_pool[i].is_focused) return &window_pool[i];
    }
    return NULL;
}

void window_minimize(window_t* win) {
    if (!win) return;
    win->is_minimized = 1;
    if (win->is_focused) focus_window(topmost_visible());
}

void window_restore(window_t* win) {
    if (!win) return;
    win->is_minimized = 0;
    focus_window(win);
}

static int taskbar_slot_rect(int slot, int* x, int* y, int* w, int* h) {
    int avail = (int)g_width - WIN_CLOCK_RESERVE;
    int max_slots = (avail - WIN_TASKBTN_X0) / (WIN_TASKBTN_W + 4);
    if (max_slots < 0) max_slots = 0;
    if (slot >= max_slots) return 0;
    *x = WIN_TASKBTN_X0 + slot * (WIN_TASKBTN_W + 4);
    *y = 3;
    *w = WIN_TASKBTN_W;
    *h = TASKBAR_HEIGHT - 6;
    return 1;
}

void window_manager_draw_taskbar_buttons(void) {
    int slot = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!window_pool[i].is_visible) continue;
        int x, y, w, h;
        if (!taskbar_slot_rect(slot, &x, &y, &w, &h)) break;

        int active = window_pool[i].is_focused && !window_pool[i].is_minimized;
        win_draw_rect(x, y, w, h, active ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG);
        win_draw_rect(x, y, w, 1, 0x00FFFFFF);
        win_draw_rect(x, y + h - 1, w, 1, 0x00404040);
        win_draw_rect(x, y, 1, h, 0x00FFFFFF);
        win_draw_rect(x + w - 1, y, 1, h, 0x00404040);

        char tmp[WIN_TITLE_MAX];
        int max_chars = (w - 12) / font_width();
        if (max_chars < 3) max_chars = 3;
        int tl = (int)strlen(window_pool[i].title);
        if (tl > max_chars) {
            int keep = max_chars - 2;
            memcpy(tmp, window_pool[i].title, keep);
            tmp[keep] = '.'; tmp[keep + 1] = '.'; tmp[keep + 2] = '\0';
        } else {
            strcpy(tmp, window_pool[i].title);
        }
        win_draw_text(tmp, x + 6, y + (h - font_height()) / 2 + 2,
              active ? 0x00FFFFFF : 0x00000000, COLOR_TRANSPARENT);
        slot++;
    }
}

int window_manager_handle_taskbar_click(int x, int y) {
    if (y >= TASKBAR_HEIGHT) return 0;
    int slot = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!window_pool[i].is_visible) continue;
        int bx, by, bw, bh;
        if (!taskbar_slot_rect(slot, &bx, &by, &bw, &bh)) break;
        if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
            window_t* win = &window_pool[i];
            if (win->is_minimized) {
                window_restore(win);
            } else if (win->is_focused) {
                window_minimize(win);
            } else {
                focus_window(win);
            }
            return 1;
        }
        slot++;
    }
    return 0;
}