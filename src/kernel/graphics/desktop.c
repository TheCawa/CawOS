#include "drivers/screen.h"
#include "drivers/io.h"
#include "gui/desktop.h"
#include "libc/util.h"
#include "gui/gui.h"
#include "gui/mouse.h"
#include "libc/keyboard.h"
#include "libc/keyboard_map.h"
#include "gui/start_menu.h"
#include "gui/window.h"
#include "gui/programs.h"
#include "kernel/scheduler.h"
#include "kernel/memory.h"
#include "drivers/rtc.h"
#include "gui/bmp.h"
#include "libc/logger.h"
#include "fs.h"

#define DESKTOP_MAX_ICONS 32
#define WALLPAPER_TILE_SCALE 4
#define ICON_CELL_W 96
#define ICON_CELL_H 84
#define ICON_SQ 32
#define CURSOR_W 8
#define CURSOR_H 16

typedef enum { DICON_PROGRAM, DICON_FOLDER, DICON_FILE } dicon_type_t;

typedef struct {
    dicon_type_t type;
    const char* label;
    uint32_t color;
    int selected;
    bmp_image_t img;
    int has_img;
} dicon_t;

static dicon_t g_icons[DESKTOP_MAX_ICONS];
static int g_icon_count = 0;
extern program_t __start_prog;
extern program_t __stop_prog;
extern void watchdog_reset();
extern volatile uint32_t system_ticks;
extern file_t fs[MAX_FILES];
volatile int g_desktop_exit_requested = 0;
volatile int g_desktop_exit_reason = 0;
int gui_clip_enabled = 0;
int gui_clip_x0 = 0, gui_clip_y0 = 0, gui_clip_x1 = 0, gui_clip_y1 = 0;
static uint8_t* g_bg_buffer = NULL;
static int dmg_x0, dmg_y0, dmg_x1, dmg_y1;
static int dmg_active = 0;
static int band_on_screen = 0;

void desktop_damage_rect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (!dmg_active) {
        dmg_x0 = x; dmg_y0 = y; dmg_x1 = x + w; dmg_y1 = y + h;
        dmg_active = 1;
        return;
    }
    if (x < dmg_x0) dmg_x0 = x;
    if (y < dmg_y0) dmg_y0 = y;
    if (x + w > dmg_x1) dmg_x1 = x + w;
    if (y + h > dmg_y1) dmg_y1 = y + h;
}

static void damage_whole(void) {
    desktop_damage_rect(0, 0, (int)g_width, (int)g_height);
}

void desktop_damage_taskbar(void) {
    desktop_damage_rect(0, 0, (int)g_width, TASKBAR_HEIGHT);
    if (start_menu_is_visible()) {
        desktop_damage_rect(0, TASKBAR_HEIGHT, START_MENU_WIDTH, START_MENU_HEIGHT);
    }
}

static void dset_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)g_width || y < 0 || y >= (int)g_height) return;
    if (!gui_in_clip(x, y)) return;
    uint32_t bpp = g_bpp / 8;
    uint32_t off = (uint32_t)y * g_pitch + (uint32_t)x * bpp;
    if (bpp == 4) *((uint32_t*)(g_shadow + off)) = color;
    else if (bpp == 3) { g_shadow[off] = color & 0xFF; g_shadow[off+1] = (color >> 8) & 0xFF; g_shadow[off+2] = (color >> 16) & 0xFF; }
    else if (bpp == 2) {
        uint8_t b = color & 0xFF, g = (color >> 8) & 0xFF, r = (color >> 16) & 0xFF;
        *((uint16_t*)(g_shadow + off)) = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

static void drect(int x, int y, int w, int h, uint32_t c) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            dset_pixel(x + dx, y + dy, c);
}

static void dtext(const char* t, int x, int y, uint32_t fg) {
    const font_t* f = font_get_current();
    if (!f) return;
    int tx = x;
    int cw = (int)f->width, ch = (int)f->height;
    for (int i = 0; t[i]; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c >= f->num_glyphs) c = '?';
        const uint8_t* glyph = f->glyphs + (uint32_t)c * f->height;
        for (int row = 0; row < ch; row++) {
            unsigned char line = glyph[row];
            for (int col = 0; col < cw && col < 8; col++) {
                if (line & (0x80 >> col)) dset_pixel(tx + col, y + row, fg);
            }
        }
        tx += cw;
    }
}

static const uint32_t icon_palette[8] = {
    0x000000AA, 0x0000AA00, 0x00AA0000, 0x0000AAAA,
    0x00AA00AA, 0x00AAAA00, 0x00AA5500, 0x005555AA
};

static void desktop_add_icon(dicon_type_t type, const char* label) {
    if (g_icon_count >= DESKTOP_MAX_ICONS) return;
    int idx = g_icon_count;
    g_icons[idx].type = type;
    g_icons[idx].label = label;
    g_icons[idx].color = icon_palette[idx % 8];
    g_icons[idx].selected = 0;
    char path[48];
    safe_strcpy(path, "/icons/", sizeof(path));
    if (strlen(path) + strlen(label) < sizeof(path)) {
        strcat(path, label);
        g_icons[idx].has_img = bmp_load(path, &g_icons[idx].img);
    } else {
        g_icons[idx].has_img = 0;
    }
    g_icon_count++;
}

static void desktop_build_icons(void) {
    for (int i = 0; i < g_icon_count; i++) {
        if (g_icons[i].has_img) bmp_free(&g_icons[i].img);
    }
    g_icon_count = 0;
    desktop_add_icon(DICON_PROGRAM, "explorer");
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].exists || !fs[i].is_dir) continue;
        if (strcmp(fs[i].dir, "/") != 0) continue;
        desktop_add_icon(DICON_FOLDER, fs[i].name);
    }
}

static void icon_pos(int i, int* ix, int* iy) {
    int area_h = (int)g_height - TASKBAR_HEIGHT - 16;
    int rows = area_h / ICON_CELL_H;
    if (rows < 1) rows = 1;
    int col = i / rows;
    int row = i % rows;
    *ix = 8 + col * ICON_CELL_W + (ICON_CELL_W - ICON_SQ) / 2;
    *iy = TASKBAR_HEIGHT + 8 + row * ICON_CELL_H;
}

static int icon_rect(int i, int* x, int* y, int* w, int* h) {
    if (i < 0 || i >= g_icon_count) return 0;
    int ix, iy;
    icon_pos(i, &ix, &iy);
    int lw = (int)strlen(g_icons[i].label) * font_width();
    int rw = ICON_SQ + 8;
    if (lw + 8 > rw) rw = lw + 8;
    *x = ix + ICON_SQ / 2 - rw / 2;
    *y = iy - 4;
    *w = rw;
    *h = ICON_SQ + 24;
    return 1;
}

static void desktop_draw_icons(void) {
    for (int i = 0; i < g_icon_count; i++) {
        int ix, iy;
        icon_pos(i, &ix, &iy);

        if (g_icons[i].has_img) {
            bmp_blit_scaled(&g_icons[i].img, ix, iy, ICON_SQ, ICON_SQ, dset_pixel);
        } else if (g_icons[i].type == DICON_FOLDER) {
            drect(ix, iy + 8, ICON_SQ, ICON_SQ - 8, 0x0000FFFF);
            drect(ix, iy + 4, 14, 5, 0x0000FFFF);
            drect(ix, iy + 4, 14, 1, 0x00008080);
            drect(ix + 13, iy + 4, 1, 5, 0x00008080);
            drect(ix + 14, iy + 8, ICON_SQ - 14, 1, 0x00008080);
            drect(ix, iy + 4, 1, ICON_SQ - 4, 0x00008080);
            drect(ix + ICON_SQ - 1, iy + 8, 1, ICON_SQ - 8, 0x00008080);
            drect(ix, iy + ICON_SQ - 1, ICON_SQ, 1, 0x00008080);
        } else if (g_icons[i].type == DICON_PROGRAM && strcmp(g_icons[i].label, "explorer") == 0) {
            drect(ix, iy + 4, ICON_SQ, ICON_SQ - 6, 0x00D4D0C8);
            drect(ix, iy + 4, ICON_SQ, 1, 0x00000000);
            drect(ix, iy + ICON_SQ - 3, ICON_SQ, 1, 0x00000000);
            drect(ix, iy + 4, 1, ICON_SQ - 6, 0x00000000);
            drect(ix + ICON_SQ - 1, iy + 4, 1, ICON_SQ - 6, 0x00000000);
            drect(ix + 2, iy + 6, ICON_SQ - 4, 6, 0x000A246A);
            drect(ix + 2, iy + 16, ICON_SQ - 4, 2, 0x00808080);
            drect(ix + 2, iy + 20, ICON_SQ - 4, 2, 0x00808080);
        } else {
            drect(ix, iy, ICON_SQ, ICON_SQ, g_icons[i].color);
            drect(ix, iy, ICON_SQ, 1, 0x00FFFFFF);
            drect(ix, iy, 1, ICON_SQ, 0x00FFFFFF);
            drect(ix, iy + ICON_SQ - 1, ICON_SQ, 1, 0x00404040);
            drect(ix + ICON_SQ - 1, iy, 1, ICON_SQ, 0x00404040);
            char init[2] = { g_icons[i].label[0], 0 };
            if (init[0] >= 'a' && init[0] <= 'z') init[0] -= 32;
            dtext(init, ix + ICON_SQ / 2 - 4, iy + ICON_SQ / 2 - 4, 0x00FFFFFF);
        }

        int lw = (int)strlen(g_icons[i].label) * font_width();
        int lx = ix + ICON_SQ / 2 - lw / 2;
        int ly = iy + ICON_SQ + 4;
        if (g_icons[i].selected) {
            drect(lx - 2, ly - 1, lw + 4, font_height() + 2, 0x00000080);
            dtext(g_icons[i].label, lx, ly, 0x00FFFFFF);
        } else {
            dtext(g_icons[i].label, lx + 1, ly + 1, 0x00000000);
            dtext(g_icons[i].label, lx, ly, 0x00FFFFFF);
        }
    }
}

static void desktop_damage_icon(int i) {
    int x, y, w, h;
    if (icon_rect(i, &x, &y, &w, &h)) desktop_damage_rect(x, y, w, h);
}

static int icon_at(int mx, int my) {
    for (int i = 0; i < g_icon_count; i++) {
        int x, y, w, h;
        if (!icon_rect(i, &x, &y, &w, &h)) continue;
        if (mx >= x && mx < x + w && my >= y && my < y + h) return i;
    }
    return -1;
}

static void desktop_select_icon(int ic) {
    for (int i = 0; i < g_icon_count; i++) {
        int want = (i == ic) ? 1 : 0;
        if (g_icons[i].selected != want) {
            g_icons[i].selected = want;
            desktop_damage_icon(i);
        }
    }
}

static void desktop_clear_selection(void) {
    for (int i = 0; i < g_icon_count; i++) {
        if (g_icons[i].selected) {
            g_icons[i].selected = 0;
            desktop_damage_icon(i);
        }
    }
}

static void desktop_select_band(int x1, int y1, int x2, int y2) {
    int l = x1 < x2 ? x1 : x2, r = x1 > x2 ? x1 : x2;
    int t = y1 < y2 ? y1 : y2, b = y1 > y2 ? y1 : y2;
    for (int i = 0; i < g_icon_count; i++) {
        int x, y, w, h;
        if (!icon_rect(i, &x, &y, &w, &h)) continue;
        int hit = !(x + w < l || x > r || y + h < t || y > b);
        if (g_icons[i].selected != hit) {
            g_icons[i].selected = hit;
            desktop_damage_icon(i);
        }
    }
}

static void desktop_launch_icon(int i) {
    if (i < 0 || i >= g_icon_count) return;
    if (g_icons[i].type == DICON_PROGRAM) {
        execute_program(g_icons[i].label);
        return;
    }
    if (g_icons[i].type == DICON_FOLDER) {
        char cmd[48];
        safe_strcpy(cmd, "explorer /", sizeof(cmd));
        if (strlen(cmd) + strlen(g_icons[i].label) < sizeof(cmd)) {
            strcat(cmd, g_icons[i].label);
            execute_program(cmd);
        }
    }
}

static void desktop_build_bg(void) {
    if (!g_bg_buffer) {
        g_bg_buffer = (uint8_t*)malloc((size_t)g_height * g_pitch);
    }
    if (!g_bg_buffer) return;
    uint32_t bpp = g_bpp / 8;
    uint8_t* row0 = g_bg_buffer;
    for (uint32_t x = 0; x < g_width; x++) {
        uint32_t off = x * bpp;
        if (bpp == 4) *((uint32_t*)(row0 + off)) = COLOR_DESKTOP_BG;
        else if (bpp == 3) { row0[off]=0x19; row0[off+1]=0x19; row0[off+2]=0x20; }
        else if (bpp == 2) *((uint16_t*)(row0 + off)) = 0x1082;
    }
    for (uint32_t y = 1; y < g_height; y++) {
        memcpy(g_bg_buffer + (size_t)y * g_pitch, row0, (size_t)g_width * bpp);
    }
}

static void desktop_wait_vsync(void) {
    for (int i = 0; i < 200000; i++) {
        if (port_byte_in(0x3DA) & 0x08) return;
    }
}

static void desktop_load_wallpaper(void) {
    if (!g_bg_buffer) return;
    bmp_image_t img;
    if (!bmp_load("/core/res/bmp/wallpaper", &img)) return;
    if (img.width <= 0 || img.height <= 0) { bmp_free(&img); return; }

    uint32_t bpp = g_bpp / 8;
    uint32_t tw = (uint32_t)img.width  * WALLPAPER_TILE_SCALE;
    uint32_t th = (uint32_t)img.height * WALLPAPER_TILE_SCALE;

    uint8_t* tile_row = (uint8_t*)malloc((size_t)tw * bpp);
    if (!tile_row) { bmp_free(&img); return; }

    for (uint32_t y = 0; y < g_height; y++) {
        uint32_t ty = y % th;
        uint32_t sy = ty / WALLPAPER_TILE_SCALE;
        const uint8_t* srow = img.rgb + sy * (uint32_t)img.width * 3;

        for (uint32_t tx = 0; tx < tw; tx++) {
            const uint8_t* p = srow + (tx / WALLPAPER_TILE_SCALE) * 3;
            uint8_t* d = tile_row + (size_t)tx * bpp;
            if (bpp == 4) { d[0] = p[2]; d[1] = p[1]; d[2] = p[0]; d[3] = 0; }
            else if (bpp == 3) { d[0] = p[2]; d[1] = p[1]; d[2] = p[0]; }
            else if (bpp == 2) {
                uint8_t b = p[2], g = p[1], r = p[0];
                *((uint16_t*)d) = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
        }

        uint8_t* drow = g_bg_buffer + y * g_pitch;
        uint32_t x = 0;
        while (x < g_width) {
            uint32_t n = g_width - x;
            if (n > tw) n = tw;
            memcpy(drow + (size_t)x * bpp, tile_row, (size_t)n * bpp);
            x += n;
        }
    }

    free(tile_row);
    bmp_free(&img);
}

static void desktop_commit(void) {
    if (!dmg_active) return;
    uint32_t bpp = g_bpp / 8;
    if (dmg_x0 < 0) dmg_x0 = 0;
    if (dmg_y0 < 0) dmg_y0 = 0;
    if (dmg_x1 > (int)g_width)  dmg_x1 = (int)g_width;
    if (dmg_y1 > (int)g_height) dmg_y1 = (int)g_height;
    if (dmg_x1 <= dmg_x0 || dmg_y1 <= dmg_y0) { dmg_active = 0; return; }

    if (g_bg_buffer) {
        size_t row_bytes = (size_t)(dmg_x1 - dmg_x0) * bpp;
        for (int y = dmg_y0; y < dmg_y1; y++) {
            memcpy(g_shadow + (size_t)y * g_pitch + (size_t)dmg_x0 * bpp,
                   g_bg_buffer + (size_t)y * g_pitch + (size_t)dmg_x0 * bpp,
                   row_bytes);
        }
    }

    gui_clip_enabled = 1;
    gui_clip_x0 = dmg_x0; gui_clip_y0 = dmg_y0;
    gui_clip_x1 = dmg_x1; gui_clip_y1 = dmg_y1;
    desktop_draw_icons();
    window_manager_draw();
    taskbar_draw();
    gui_clip_enabled = 0;

    desktop_wait_vsync();

    size_t row_bytes = (size_t)(dmg_x1 - dmg_x0) * bpp;
    for (int y = dmg_y0; y < dmg_y1; y++) {
        memcpy(g_framebuffer + (size_t)y * g_pitch + (size_t)dmg_x0 * bpp,
               g_shadow + (size_t)y * g_pitch + (size_t)dmg_x0 * bpp,
               row_bytes);
    }
    dmg_active = 0;
}

static void full_redraw(void) {
    damage_whole();
    desktop_commit();
}

static const uint8_t cursor_shape[16] = {
    0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF,
    0xF8,0xD8,0x8C,0x0C,0x06,0x06,0x03,0x00
};

static int cursor_shape_bit(int row, int col) {
    if (row < 0 || row >= CURSOR_H || col < 0 || col >= CURSOR_W) return 0;
    return (cursor_shape[row] >> (7 - col)) & 1;
}

static uint8_t cursor_saved_bg[(CURSOR_H + 2) * (CURSOR_W + 2) * 4];
static int cursor_saved_x = -1, cursor_saved_y = -1;
static int cursor_x = -1, cursor_y = -1;

static void cursor_put_pixel(int px, int py, uint32_t color) {
    if (px < 0 || px >= (int)g_width || py < 0 || py >= (int)g_height) return;
    uint32_t bpp = g_bpp / 8;
    uint32_t off = (uint32_t)py * g_pitch + (uint32_t)px * bpp;
    if (bpp == 4) *((uint32_t*)(g_framebuffer + off)) = color;
    else if (bpp == 3) {
        g_framebuffer[off]     = color & 0xFF;
        g_framebuffer[off + 1] = (color >> 8) & 0xFF;
        g_framebuffer[off + 2] = (color >> 16) & 0xFF;
    } else if (bpp == 2) {
        uint8_t b = color & 0xFF, g = (color >> 8) & 0xFF, r = (color >> 16) & 0xFF;
        *((uint16_t*)(g_framebuffer + off)) = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

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
    uint32_t bpp = g_bpp / 8;

    for (int row = -1; row <= CURSOR_H; row++) {
        for (int col = -1; col <= CURSOR_W; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint32_t off = (uint32_t)py * g_pitch + (uint32_t)px * bpp;
                uint8_t* slot = &cursor_saved_bg[((row + 1) * (CURSOR_W + 2) + (col + 1)) * bpp];
                for (uint32_t i = 0; i < bpp; i++) slot[i] = g_framebuffer[off + i];
            }
        }
    }
    cursor_saved_x = x;
    cursor_saved_y = y;

    for (int row = -1; row <= CURSOR_H; row++) {
        for (int col = -1; col <= CURSOR_W; col++) {
            if (cursor_shape_bit(row, col)) continue;
            if (cursor_shape_bit(row - 1, col) || cursor_shape_bit(row + 1, col) ||
                cursor_shape_bit(row, col - 1) || cursor_shape_bit(row, col + 1)) {
                cursor_put_pixel(x + col, y + row, 0x00000000);
            }
        }
    }

    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            if (cursor_shape[row] & (0x80 >> col)) {
                cursor_put_pixel(x + col, y + row, 0x00FFFFFF);
            }
        }
    }

    cursor_x = x;
    cursor_y = y;
}

static void erase_cursor_fb() {
    if (cursor_saved_x < 0) return;
    uint32_t bpp = g_bpp / 8;
    for (int row = -1; row <= CURSOR_H; row++) {
        for (int col = -1; col <= CURSOR_W; col++) {
            int px = cursor_saved_x + col, py = cursor_saved_y + row;
            if (px >= 0 && px < (int)g_width && py >= 0 && py < (int)g_height) {
                uint32_t off = (uint32_t)py * g_pitch + (uint32_t)px * bpp;
                uint8_t* slot = &cursor_saved_bg[((row + 1) * (CURSOR_W + 2) + (col + 1)) * bpp];
                for (uint32_t i = 0; i < bpp; i++) g_framebuffer[off + i] = slot[i];
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

void desktop_init() {
    if (!g_is_graphics) return;
    desktop_build_bg();
    desktop_load_wallpaper();
    taskbar_init();
    mouse_init();
    desktop_build_icons();
    execute_program("about");
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
    int last_h = -1, last_m = -1;
    int last_click_icon = -1;
    uint32_t last_click_tick = 0;

    full_redraw();
    draw_cursor_fb(mouse->x, mouse->y);
    cursor_visible = 1;

    while (!g_desktop_exit_requested) {
        schedule();
        scheduler_reap_zombies();
        watchdog_reset();
        {
            rtc_time_t rt;
            rtc_get_time(&rt);
            if (rt.hour != last_h || rt.minute != last_m) {
                last_h = rt.hour;
                last_m = rt.minute;
                desktop_damage_taskbar();
            }
        }

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;

            if (scancode == LSHIFT || scancode == RSHIFT) {
                shift_state = 1;
            } else if (scancode == (LSHIFT | 0x80) || scancode == (RSHIFT | 0x80)) {
                shift_state = 0;
            } else if (scancode == CAPSLOCK) {
                caps_lock_state = !caps_lock_state;
            }

            if (scancode == LWIN || scancode == RWIN) {
                if (start_menu_is_visible()) start_menu_hide();
                else { start_menu_show(); start_menu_init(); }
                desktop_damage_taskbar();
            }
            else if (scancode == ESC && start_menu_is_visible()) {
                start_menu_hide();
                desktop_damage_taskbar();
            }
            else if (start_menu_is_visible()) {
                int old_hovered = start_menu_get_hovered();
                start_menu_handle_key(scancode);
                int new_hovered = start_menu_get_hovered();
                if (old_hovered != new_hovered || !start_menu_is_visible()) {
                    desktop_damage_taskbar();
                }
            }
            else {
                if (!(scancode & 0x80)) {
                    if (window_manager_handle_scancode(scancode)) {
                        window_t* fw = window_get_focused();
                        if (fw) desktop_damage_rect(fw->x, fw->y, fw->width, fw->height);
                    } else if (scancode < 128) {
                        char base_char = ascii_map[scancode];
                        int is_letter = (base_char >= 'a' && base_char <= 'z');
                        int use_shift = shift_state;
                        if (is_letter && caps_lock_state) use_shift = !use_shift;
                        char ascii = use_shift ? shift_map[scancode] : ascii_map[scancode];
                        if (ascii == BACKSPACE) ascii = '\b';
                        if (ascii == ENTER)     ascii = '\n';
                        if (ascii != 0) {
                            if (window_manager_handle_key(ascii)) {
                                window_t* fw = window_get_focused();
                                if (fw) desktop_damage_rect(fw->x, fw->y, fw->width, fw->height);
                            }
                        }
                    }
                }
            }
        }

        uint8_t left = mouse->buttons & 0x01;
        uint8_t was_left = last_buttons & 0x01;
        int force_full = 0;
        int refresh = 0;

        if (left && !was_left) {
            int on_taskbar = mouse->y < TASKBAR_HEIGHT;
            if (start_menu_is_visible() || on_taskbar) {
                taskbar_handle_click(mouse->x, mouse->y);
                force_full = 1;
            } else if (window_manager_handle_click(mouse->x, mouse->y)) {
                dragging = 0;
                force_full = 1;
            } else {
                int ic = icon_at(mouse->x, mouse->y);
                if (ic >= 0) {
                    uint32_t now = (uint32_t)system_ticks;
                    if (ic == last_click_icon && (now - last_click_tick) <= 40) {
                        desktop_launch_icon(ic);
                        last_click_icon = -1;
                    } else {
                        last_click_icon = ic;
                        last_click_tick = now;
                    }
                    desktop_select_icon(ic);
                } else {
                    last_click_icon = -1;
                    desktop_clear_selection();
                    dragging = 1;
                    sel_active = 1;
                    sel_x1 = sel_x2 = sel_old_x2 = mouse->x;
                    sel_y1 = sel_y2 = sel_old_y2 = mouse->y;
                }
            }
        }
        else if (!left && was_left) {
            window_manager_handle_release();
            if (dragging) {
                dragging = 0;
                if (sel_active) {
                    desktop_select_band(sel_x1, sel_y1, sel_x2, sel_y2);
                    sel_active = 0;
                }
            }
            force_full = 1;
        }
        else if (left && was_left) {
            if (dragging) {
                if (mouse->x != sel_x2 || mouse->y != sel_y2) {
                    sel_x2 = mouse->x;
                    sel_y2 = mouse->y;
                    refresh = 1;
                }
            } else {
                window_manager_handle_move(mouse->x, mouse->y);
            }
        }

        int was_hovered = start_button.hovered;
        int was_menu_hover = start_menu_is_visible() ? start_menu_get_hovered() : -1;
        taskbar_handle_mouse_move(mouse->x, mouse->y);
        if (was_hovered != start_button.hovered ||
            (start_menu_is_visible() && was_menu_hover != start_menu_get_hovered())) {
            desktop_damage_taskbar();
        }

        if (force_full) damage_whole();

        if (dmg_active || refresh) {
            if (cursor_visible) { erase_cursor_fb(); cursor_visible = 0; }
            if (band_on_screen) {
                xor_rect_fb(sel_x1, sel_y1, sel_old_x2, sel_old_y2);
                band_on_screen = 0;
            }
            if (dmg_active) desktop_commit();
            if (sel_active) {
                xor_rect_fb(sel_x1, sel_y1, sel_x2, sel_y2);
                band_on_screen = 1;
                sel_old_x2 = sel_x2;
                sel_old_y2 = sel_y2;
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
        sleep_ms(5);
    }

    if (cursor_visible) erase_cursor_fb();
}