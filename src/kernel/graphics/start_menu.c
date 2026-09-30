#include "gui/start_menu.h"
#include "gui/desktop.h"
#include "gui/gui.h"
#include "drivers/screen.h"
#include "gui/programs.h"

static menu_item_t menu_items[] = {
    { "Programs",    MENU_ITEM_SUBMENU,   0x000000FF, menu_run_programs },
    { "Documents",   MENU_ITEM_SUBMENU,   0x00FF8000, menu_run_documents },
    { "Settings",    MENU_ITEM_SUBMENU,   0x00808080, menu_run_settings },
    { "Find",        MENU_ITEM_SUBMENU,   0x0000FF00, menu_run_find },
    { "Explorer",    MENU_ITEM_NORMAL,    0x00FF80FF, menu_run_explorer },
    { "Help",        MENU_ITEM_NORMAL,    0x00FFFF00, menu_run_help },
    { "Run...",      MENU_ITEM_NORMAL,    0x00FFFFFF, menu_run_run },
    { "Log Off",       MENU_ITEM_NORMAL,    0x00FF00FF, menu_logoff },
    { "Shutdown...",  MENU_ITEM_NORMAL,    0x00FF0000, menu_shutdown },
};
#define MENU_ITEM_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

static int menu_x = 0, menu_y = 0;
static int hovered_item = -1;

static void set_pixel_menu(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)g_width || y < 0 || y >= (int)g_height) return;
    uint32_t bpp = g_bpp / 8;
    uint32_t off = y * g_pitch + x * bpp;
    if (bpp == 4) {
        *((uint32_t*)(g_shadow + off)) = color;
    } else if (bpp == 3) {
        g_shadow[off] = (color >> 0) & 0xFF;
        g_shadow[off+1] = (color >> 8) & 0xFF;
        g_shadow[off+2] = (color >> 16) & 0xFF;
    } else if (bpp == 2) {
        // RGB555: RRRRRGGGGGGBBBBB
        uint8_t b = (color >> 0) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint16_t rgb555 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *((uint16_t*)(g_shadow + off)) = rgb555;
    }
}

static void draw_icon(int x, int y, uint32_t color) {
    uint32_t bpp = g_bpp / 8;
    for (int dy = 0; dy < MENU_ICON_SIZE; dy++) {
        for (int dx = 0; dx < MENU_ICON_SIZE; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                if (!gui_in_clip(x, y)) return;
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

static void menu_draw_text(const char* text, int x, int y, uint32_t color) {
    uint32_t bpp = g_bpp / 8;
    int tx = x;
    for (int i = 0; text[i]; i++) {
        unsigned char c = text[i];
        if (c >= 128) c = '?';
        for (int row = 0; row < 8; row++) {
            unsigned char line = font8x8_basic[c][row];
            for (int col = 0; col < 8; col++) {
                int px = tx + col, py = y + row;
                if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                    if (!gui_in_clip(x, y)) return;
                    if (line & (0x80 >> col)) {
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
        tx += 8;
    }
}

static void draw_separator(int x, int y, int width) {
    uint32_t dark = 0x00808080;
    uint32_t light = 0x00FFFFFF;
    for (int dx = 4; dx < width - 4; dx++) {
        set_pixel_menu(x + dx, y, dark);
        set_pixel_menu(x + dx, y + 1, light);
    }
}

static int menu_item_at_rel(int rel_y) {
    if (rel_y < 0) return -1;
    int current_y = 0;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int h = (menu_items[i].type == MENU_ITEM_SEPARATOR) ? 12 : MENU_ITEM_HEIGHT;
        if (rel_y < current_y + h) {
            if (menu_items[i].type == MENU_ITEM_SEPARATOR) return -1;
            return i;
        }
        current_y += h;
    }
    return -1;
}

void start_menu_init(void) {
    hovered_item = -1;
}

void start_menu_navigate_down(void) {
    if (hovered_item == -1) {
        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            if (menu_items[i].type != MENU_ITEM_SEPARATOR) {
                hovered_item = i;
                return;
            }
        }
        return;
    }

    for (int i = hovered_item + 1; i < MENU_ITEM_COUNT; i++) {
        if (menu_items[i].type != MENU_ITEM_SEPARATOR) {
            hovered_item = i;
            return;
        }
    }

    for (int i = 0; i < hovered_item; i++) {
        if (menu_items[i].type != MENU_ITEM_SEPARATOR) {
            hovered_item = i;
            return;
        }
    }
}

void start_menu_navigate_up(void) {
    if (hovered_item == -1) {
        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            if (menu_items[i].type != MENU_ITEM_SEPARATOR) {
                hovered_item = i;
                return;
            }
        }
        return;
    }
    for (int i = hovered_item - 1; i >= 0; i--) {
        if (menu_items[i].type != MENU_ITEM_SEPARATOR) {
            hovered_item = i;
            return;
        }
    }
    for (int i = MENU_ITEM_COUNT - 1; i > hovered_item; i--) {
        if (menu_items[i].type != MENU_ITEM_SEPARATOR) {
            hovered_item = i;
            return;
        }
    }
}

void start_menu_activate_selected(void) {
    if (hovered_item >= 0 && hovered_item < MENU_ITEM_COUNT) {
        if (menu_items[hovered_item].callback &&
            menu_items[hovered_item].type != MENU_ITEM_SEPARATOR &&
            menu_items[hovered_item].type != MENU_ITEM_DISABLED) {
            menu_items[hovered_item].callback();
        }
    }
}

int start_menu_get_hovered(void) {
    return hovered_item;
}

void start_menu_handle_key(unsigned char scancode) {
    if (scancode & 0x80) {
        return;
    }
    if (scancode == 0x50) {
        start_menu_navigate_down();
    }
    else if (scancode == 0x48) {
        start_menu_navigate_up();
    }
    else if (scancode == 0x1C) {
        start_menu_activate_selected();
        start_menu_hide();
    }
}

void start_menu_draw(int x, int y) {
    menu_x = x; menu_y = y;
    uint32_t bpp = g_bpp / 8;
    for (int dy = 0; dy < START_MENU_HEIGHT; dy++) {
        for (int dx = 0; dx < START_MENU_WIDTH; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint32_t off = py * g_pitch + px * bpp;
                if (bpp == 4) {
                    *((uint32_t*)(g_shadow + off)) = COLOR_MENU_BG;
                } else if (bpp == 3) {
                    g_shadow[off] = 0xC0;
                    g_shadow[off+1] = 0xC0;
                    g_shadow[off+2] = 0xC0;
                } else if (bpp == 2) {
                    // COLOR_MENU_BG = 0x00C0C0C0 -> rgb555
                    uint8_t b = 0xC0;
                    uint8_t g = 0xC0;
                    uint8_t r = 0xC0;
                    uint16_t rgb555 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                    *((uint16_t*)(g_shadow + off)) = rgb555;
                }
            }
        }
    }
    for (int dx = 0; dx < START_MENU_WIDTH; dx++) {
        set_pixel_menu(x + dx, y, 0x00FFFFFF);
        set_pixel_menu(x + dx, y + START_MENU_HEIGHT - 1, 0x00808080);
    }
    for (int dy = 0; dy < START_MENU_HEIGHT; dy++) {
        set_pixel_menu(x, y + dy, 0x00FFFFFF);
        set_pixel_menu(x + START_MENU_WIDTH - 1, y + dy, 0x00808080);
    }
    for (int dy = 2; dy < START_MENU_HEIGHT - 2; dy++) {
        for (int dx = 2; dx < 22; dx++) {
            set_pixel_menu(x + dx, y + dy, 0x00000080);
        }
    }
    int item_y = y + 4;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int item_x = x + 24;
        if (menu_items[i].type == MENU_ITEM_SEPARATOR) {
            draw_separator(item_x, item_y + 8, START_MENU_WIDTH - 28);
            item_y += 12;
            continue;
        }
        if (i == hovered_item) {
            for (int dy = 0; dy < MENU_ITEM_HEIGHT; dy++) {
                for (int dx = 2; dx < START_MENU_WIDTH - 2; dx++) {
                    set_pixel_menu(item_x - 22 + dx, item_y + dy, COLOR_MENU_HILITE);
                }
            }
        }
        draw_icon(item_x, item_y + 2, menu_items[i].icon_color);
        uint32_t text_color = (menu_items[i].type == MENU_ITEM_DISABLED) ? 
                              COLOR_MENU_GRAY : 
                              (i == hovered_item ? 0x00FFFFFF : COLOR_MENU_TEXT);
        menu_draw_text(menu_items[i].text, item_x + 20,
                       item_y + (MENU_ITEM_HEIGHT - font_height()) / 2, text_color);
        if (menu_items[i].type == MENU_ITEM_SUBMENU) {
            menu_draw_text(">>", item_x + START_MENU_WIDTH - 44,
                           item_y + (MENU_ITEM_HEIGHT - font_height()) / 2, text_color);
        }
        item_y += MENU_ITEM_HEIGHT;
    }
}

void start_menu_handle_click(int x, int y) {
    if (!start_menu_hit_test(x, y)) return;
    int idx = menu_item_at_rel(y - menu_y - 4);
    if (idx < 0) return;
    if (menu_items[idx].type == MENU_ITEM_DISABLED) return;
    if (menu_items[idx].callback) {
        menu_items[idx].callback();
    }
}

void start_menu_handle_hover(int x, int y) {
    hovered_item = -1;
    if (start_menu_hit_test(x, y)) {
        hovered_item = menu_item_at_rel(y - menu_y - 4);
    }
}

int start_menu_hit_test(int x, int y) {
    return (x >= menu_x && x < menu_x + START_MENU_WIDTH &&
            y >= menu_y && y < menu_y + START_MENU_HEIGHT);
}

void menu_run_programs(void) {
    execute_program("list");
}

void menu_run_documents(void) {
    execute_program("help");
}

void menu_run_settings(void) {
    execute_program("about");
}

void menu_run_find(void) {
    execute_program("find");
}

void menu_run_help(void) {
    execute_program("help");
}

void menu_run_run(void) {
    execute_program("run");
}

void menu_shutdown(void) {
    g_desktop_exit_requested = 1;
}

void menu_logoff(void) {
    g_desktop_exit_reason = 1;
    g_desktop_exit_requested = 1;
}

void menu_run_explorer(void) {
    execute_program("explorer");
}