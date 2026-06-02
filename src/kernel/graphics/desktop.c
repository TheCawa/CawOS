#include "drivers/screen.h"
#include "gui/desktop.h"
#include "libc/util.h"
#include "gui/gui.h"
#include "gui/mouse.h"
#include "libc/keyboard.h"
#include "libc/keyboard_map.h"
#include "gui/start_menu.h"
#include "gui/window.h"
#include "gui/programs.h"

volatile int g_desktop_exit_requested = 0;
static uint8_t cursor_saved_bg[16 * 8 * 4]; 
static int cursor_saved_x = -1, cursor_saved_y = -1;

void desktop_init() {
    if (!g_is_graphics) return;

    uint32_t bpp = g_bpp / 8;
    for (uint32_t y = 0; y < g_height; y++) {
        for (uint32_t x = 0; x < g_width; x++) {
            uint32_t off = y * g_pitch + x * bpp;
            if (bpp == 4) *((uint32_t*)(g_shadow + off)) = COLOR_DESKTOP_BG;
            else if (bpp == 3) { g_shadow[off]=0x19; g_shadow[off+1]=0x19; g_shadow[off+2]=0x20; }
            else if (bpp == 2) *((uint16_t*)(g_shadow + off)) = 0x1082;
        }
    }
    taskbar_init();
    taskbar_draw();
    mouse_init();
    execute_program("about");
}

static int cursor_x = -1, cursor_y = -1;

static inline void xor_pixel_fb(int x, int y) {
    if (x < 0 || x >= (int)g_width || y < 0 || y >= (int)g_height) return;
    uint32_t bpp = g_bpp / 8;
    uint32_t off = y * g_pitch + x * bpp;
    if (bpp == 4) {
        uint32_t p = *((uint32_t*)(g_framebuffer + off));
        *((uint32_t*)(g_framebuffer + off)) = p ^ 0x00FFFFFF;
    } else if (bpp == 3) {
        g_framebuffer[off] ^= 0xFF;
        g_framebuffer[off+1] ^= 0xFF;
        g_framebuffer[off+2] ^= 0xFF;
    } else if (bpp == 2) {
        uint16_t p = *((uint16_t*)(g_framebuffer + off));
        *((uint16_t*)(g_framebuffer + off)) = p ^ 0xFFFF;
    }
}

static void draw_cursor_fb(int x, int y) {
    static const uint8_t shape[16] = {
        0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF,
        0xF8,0xD8,0x8C,0x0C,0x06,0x06,0x03,0x00
    };
    uint32_t bpp = g_bpp / 8;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint32_t off = py * g_pitch + px * bpp;
                for (uint32_t i = 0; i < bpp; i++) {
                    cursor_saved_bg[(row * 8 + col) * bpp + i] = g_framebuffer[off + i];
                }
            }
        }
    }
    cursor_saved_x = x; 
    cursor_saved_y = y;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (shape[row] & (0x80 >> col)) {
                int px = x + col, py = y + row;
                if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                    uint32_t off = py * g_pitch + px * bpp;
                    if (bpp == 4) {
                        *((uint32_t*)(g_framebuffer + off)) = 0x00FFFFFF;
                    } else if (bpp == 3) {
                        g_framebuffer[off] = 0xFF; 
                        g_framebuffer[off+1] = 0xFF; 
                        g_framebuffer[off+2] = 0xFF;
                    } else if (bpp == 2) {
                        *((uint16_t*)(g_framebuffer + off)) = 0xFFFF;
                    }
                }
            }
        }
    }
    cursor_x = x; 
    cursor_y = y;
}

static void erase_cursor_fb() {
    if (cursor_saved_x < 0) return;
    uint32_t bpp = g_bpp / 8;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            int px = cursor_saved_x + col, py = cursor_saved_y + row;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint32_t off = py * g_pitch + px * bpp;
                for (uint32_t i = 0; i < bpp; i++) {
                    g_framebuffer[off + i] = cursor_saved_bg[(row * 8 + col) * bpp + i];
                }
            }
        }
    }
    cursor_saved_x = -1;
    cursor_x = -1;
}

static int sel_active = 0;
static int sel_x1 = 0, sel_y1 = 0, sel_x2 = 0, sel_y2 = 0;
static int sel_old_x2 = 0, sel_old_y2 = 0;
static void xor_rect_fb(int x1, int y1, int x2, int y2) {
    int l = x1 < x2 ? x1 : x2, r = x1 > x2 ? x1 : x2;
    int t = y1 < y2 ? y1 : y2, b = y1 > y2 ? y1 : y2;
    
    for (int x = l; x <= r; x++) {
        if (t >= 0 && t < (int)g_height) xor_pixel_fb(x, t);
        if (b != t && b >= 0 && b < (int)g_height) xor_pixel_fb(x, b);
    }
    for (int y = t + 1; y < b; y++) {
        if (y >= 0 && y < (int)g_height) {
            if (l >= 0 && l < (int)g_width) xor_pixel_fb(l, y);
            if (r != l && r >= 0 && r < (int)g_width) xor_pixel_fb(r, y);
        }
    }
}

static void full_redraw() {
    uint32_t bpp = g_bpp / 8;
    uint32_t bg = COLOR_DESKTOP_BG;
    for (uint32_t y = TASKBAR_HEIGHT; y < g_height; y++) {
        for (uint32_t x = 0; x < g_width; x++) {
            uint32_t off = y * g_pitch + x * bpp;
            if (bpp == 4) *((uint32_t*)(g_shadow + off)) = bg;
            else if (bpp == 3) { g_shadow[off]=0x19; g_shadow[off+1]=0x19; g_shadow[off+2]=0x20; }
            else if (bpp == 2) *((uint16_t*)(g_shadow + off)) = 0x1082;
        }
    }
    window_manager_draw();
    taskbar_draw();
    memcpy(g_framebuffer, g_shadow, g_height * g_pitch);
}

void desktop_run() {
    desktop_init();
    mouse_state_t* mouse = mouse_get_state();
    int last_mx = -1, last_my = -1;
    uint8_t last_buttons = 0;
    int dragging = 0;
    int cursor_visible = 0;
    int shift_state = 0;
    int caps_lock_state = 0;
    full_redraw();
    draw_cursor_fb(mouse->x, mouse->y);
    cursor_visible = 1;
    while (!g_desktop_exit_requested) {
        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
            int need_menu_redraw = 0;
            if (scancode == LSHIFT || scancode == RSHIFT) {
                shift_state = 1;
            } else if (scancode == (LSHIFT | 0x80) || scancode == (RSHIFT | 0x80)) {
                shift_state = 0; 
            } else if (scancode == CAPSLOCK) {
                caps_lock_state = !caps_lock_state; 
            }
            if (scancode == LWIN || scancode == RWIN) {
                if (start_menu_is_visible()) {
                    start_menu_hide();
                } else {
                    start_menu_show();
                    start_menu_init();
                }
                need_menu_redraw = 1;
            }
            else if (scancode == ESC && start_menu_is_visible()) {
                start_menu_hide();
                need_menu_redraw = 1;
            }
            else if (start_menu_is_visible()) {
                int old_hovered = start_menu_get_hovered();
                start_menu_handle_key(scancode);
                int new_hovered = start_menu_get_hovered();
                if (old_hovered != new_hovered || !start_menu_is_visible()) {
                    need_menu_redraw = 1;
                }
            }
            else {
                if (!(scancode & 0x80) && scancode < 128) {
                    char ascii = 0;
                    char base_char = ascii_map[scancode];
                    
                    int is_letter = (base_char >= 'a' && base_char <= 'z');
                    int use_shift = shift_state;
                    if (is_letter && caps_lock_state) {
                        use_shift = !use_shift;
                    }

                    ascii = use_shift ? shift_map[scancode] : ascii_map[scancode];

                    if (ascii == BACKSPACE) ascii = '\b'; 
                    if (ascii == ENTER)     ascii = '\n'; 

                    if (ascii != 0) {
                        extern int window_manager_handle_key(char ascii);
                        if (window_manager_handle_key(ascii)) {
                            need_menu_redraw = 1; 
                        }
                    }
                }
            }
            if (need_menu_redraw) {
                if (cursor_visible) {
                    erase_cursor_fb();
                    cursor_visible = 0;
                }
                full_redraw();
                draw_cursor_fb(mouse->x, mouse->y);
                cursor_visible = 1;
            }
        }
        uint8_t left = mouse->buttons & 0x01;
        uint8_t was_left = last_buttons & 0x01;
        int need_redraw = 0;
        if (left && !was_left) {
            int on_taskbar = mouse->y < TASKBAR_HEIGHT;
            if (start_menu_is_visible() || on_taskbar) {
                taskbar_handle_click(mouse->x, mouse->y);
            } else {
                if (window_manager_handle_click(mouse->x, mouse->y)) {
                    dragging = 0;
                } else {
                    dragging = 1;
                    sel_active = 1;
                    sel_x1 = sel_x2 = sel_old_x2 = mouse->x;
                    sel_y1 = sel_y2 = sel_old_y2 = mouse->y;
                }
            }
            
            need_redraw = 1;
        }
        else if (!left && was_left) {
            window_manager_handle_release();
            if (dragging) {
                dragging = 0;
                sel_active = 0;
            }
            need_redraw = 1;
        }
        else if (left && was_left) {
            if (dragging) {
                if (mouse->x != sel_old_x2 || mouse->y != sel_old_y2) {
                    sel_x2 = mouse->x; sel_y2 = mouse->y;
                    sel_old_x2 = sel_x2; sel_old_y2 = sel_y2;
                    need_redraw = 1;
                }
            } else {
                window_manager_handle_move(mouse->x, mouse->y);
                if (mouse->x != last_mx || mouse->y != last_my) {
                    need_redraw = 1;
                }
            }
        }
        int was_hovered = start_button.hovered;
        taskbar_handle_mouse_move(mouse->x, mouse->y);
        if (was_hovered != start_button.hovered) {
            need_redraw = 1;
        }
        if (need_redraw) {
            if (cursor_visible) {
                erase_cursor_fb();
                cursor_visible = 0;
            }
            full_redraw();
            if (sel_active) {
                xor_rect_fb(sel_x1, sel_y1, sel_x2, sel_y2);
            }
            draw_cursor_fb(mouse->x, mouse->y);
            cursor_visible = 1;
        }
        else if (mouse->x != last_mx || mouse->y != last_my) {
            if (cursor_visible) erase_cursor_fb();
            draw_cursor_fb(mouse->x, mouse->y);
            cursor_visible = 1;
        }

        last_mx = mouse->x; last_my = mouse->y;
        last_buttons = mouse->buttons;

        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (cursor_visible) erase_cursor_fb();
}