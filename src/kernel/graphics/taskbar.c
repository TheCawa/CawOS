#include "gui/gui.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "gui/start_menu.h"

button_t start_button;
static int start_menu_visible = 0;
#define COLOR_TRANSPARENT 0xFFFFFFFF


void taskbar_init() {
    start_button.x = 0;
    start_button.y = 0;
    start_button.width = START_BUTTON_WIDTH;
    start_button.height = TASKBAR_HEIGHT;
    start_button.text = "CawOS";
    start_button.hovered = 0;
    start_button.pressed = 0;
    start_menu_visible = 0;
}

static void draw_rect(int x, int y, int width, int height, uint32_t color) {
    uint32_t bpp = g_bpp / 8;
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint32_t off = py * g_pitch + px * bpp;
                if (bpp == 4) {
                    *((uint32_t*)(g_shadow + off)) = color;
                } else if (bpp == 3) {
                    g_shadow[off] = (color >> 0) & 0xFF;
                    g_shadow[off+1] = (color >> 8) & 0xFF;
                    g_shadow[off+2] = (color >> 16) & 0xFF;
                } else if (bpp == 2) {
                    // rgb555: RRRRRGGGGGGBBBBB
                    uint8_t b = (color >> 0) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t r = (color >> 16) & 0xFF;
                    uint16_t rgb555 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                    *((uint16_t*)(g_shadow + off)) = rgb555;
                }
            }
        }
    }
}

static void draw_text(const char* text, int x, int y, uint32_t fg, uint32_t bg) {
    uint32_t bpp = g_bpp / 8;
    extern unsigned char font8x8_basic[128][8];
    int tx = x;
    for (int i = 0; text[i]; i++) {
        unsigned char c = text[i];
        if (c >= 128) c = '?';
        for (int row = 0; row < 8; row++) {
            unsigned char line = font8x8_basic[c][row];
            for (int col = 0; col < 8; col++) {
                int px = tx + col, py = y + row;
                if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                    int is_foreground = (line & (0x80 >> col));
                    if (!is_foreground && bg == COLOR_TRANSPARENT) {
                        continue;
                    }
                    uint32_t off = py * g_pitch + px * bpp;
                    uint32_t color = is_foreground ? fg : bg;
                    
                    if (bpp == 4) {
                        *((uint32_t*)(g_shadow + off)) = color;
                    } else if (bpp == 3) {
                        g_shadow[off] = (color >> 0) & 0xFF;
                        g_shadow[off+1] = (color >> 8) & 0xFF;
                        g_shadow[off+2] = (color >> 16) & 0xFF;
                    } else if (bpp == 2) {
                        uint8_t b = (color >> 0) & 0xFF;
                        uint8_t g = (color >> 8) & 0xFF;
                        uint8_t r = (color >> 16) & 0xFF;
                        uint16_t rgb555 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                        *((uint16_t*)(g_shadow + off)) = rgb555;
                    }
                }
            }
        }
        tx += 8;
    }
}

void taskbar_draw() {
    int taskbar_y = 0;
    draw_rect(0, taskbar_y, g_width, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);

    uint32_t btn_color = start_button.pressed ? COLOR_BUTTON_HOVER :
                        (start_button.hovered ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG);

    draw_rect(start_button.x + 2, start_button.y + 2,
              start_button.width - 4, start_button.height - 4, btn_color);

    draw_rect(start_button.x, start_button.y, start_button.width, 1, COLOR_TASKBAR_TEXT);
    draw_rect(start_button.x, start_button.y + start_button.height - 1, start_button.width, 1, COLOR_TASKBAR_TEXT);
    draw_rect(start_button.x, start_button.y, 1, start_button.height, COLOR_TASKBAR_TEXT);
    draw_rect(start_button.x + start_button.width - 1, start_button.y, 1, start_button.height, COLOR_TASKBAR_TEXT);

    int text_y = start_button.y + (start_button.height - 8) / 2;
    draw_text(start_button.text, start_button.x + 8, text_y, COLOR_TASKBAR_TEXT, COLOR_TRANSPARENT);

    if (start_menu_visible) {
        start_menu_draw(0, TASKBAR_HEIGHT);
    }
}

void taskbar_handle_click(int x, int y) {
    if (x >= start_button.x && x < start_button.x + start_button.width &&
        y >= start_button.y && y < start_button.y + start_button.height) {

        start_button.pressed = 1;
        if (start_menu_visible) {
            start_menu_hide();
        } else {
            start_menu_show();
            start_menu_init();
        }
        start_button.pressed = 0;
        return;
    }

    if (start_menu_visible) {
        if (start_menu_hit_test(x, y)) {
            start_menu_handle_click(x, y);
            return;
        }
        if (!(x >= 0 && x < START_MENU_WIDTH &&
              y >= TASKBAR_HEIGHT && y < TASKBAR_HEIGHT + START_MENU_HEIGHT)) {
            start_menu_hide();
        }
    }
}

void taskbar_handle_mouse_move(int x, int y) {
    int was_hovered = start_button.hovered;
    start_button.hovered = (x >= start_button.x && x < start_button.x + start_button.width &&
                           y >= start_button.y && y < start_button.y + start_button.height);
    
    if (start_menu_visible) {
        start_menu_handle_hover(x, y);
    }
}

void start_menu_show() {
    start_menu_visible = 1;
}

void start_menu_hide() {
    start_menu_visible = 0;
}

int start_menu_is_visible() {
    return start_menu_visible;
}