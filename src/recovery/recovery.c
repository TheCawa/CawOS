#include <stdint.h>

#define VBE_FB_PTR      ((unsigned int*)0x0520)
#define VBE_PITCH_PTR   ((unsigned int*)0x0524)
#define VBE_WIDTH_PTR   ((unsigned int*)0x0528)
#define VBE_HEIGHT_PTR  ((unsigned int*)0x052C)
#define VBE_BPP_PTR     ((unsigned int*)0x0530)
#define CHAR_W 12
#define CHAR_H 14
#define SCALE_X_NUM 3
#define SCALE_X_DEN 2
#define SCALE_Y_NUM 7
#define SCALE_Y_DEN 4
#define CHAR_GAP_Y 2
extern unsigned char font8x8_basic[128][8];

__attribute__((always_inline)) static inline void putpixel_local(
    uint8_t* fb, unsigned int width, unsigned int height, 
    unsigned int pitch, unsigned int bpp, 
    unsigned int x, unsigned int y, unsigned int color) 
{
    if (x >= width || y >= height) return;
    unsigned int offset = y * pitch + x * (bpp / 8);
    if (bpp == 32) {
        fb[offset]     = color & 0xFF;         // B
        fb[offset + 1] = (color >> 8) & 0xFF;  // G
        fb[offset + 2] = (color >> 16) & 0xFF; // R
        fb[offset + 3] = 0;                    // A
    } else if (bpp == 24) {
        fb[offset]     = color & 0xFF;         // B
        fb[offset + 1] = (color >> 8) & 0xFF;  // G
        fb[offset + 2] = (color >> 16) & 0xFF; // R
    } else if (bpp == 16) {
        unsigned int r = (color >> 16) & 0xFF;
        unsigned int g = (color >> 8) & 0xFF;
        unsigned int b = color & 0xFF;
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *(volatile uint16_t*)(fb + offset) = rgb565;
    }
}

__attribute__((always_inline)) static inline void clear_row_local(
    uint8_t* fb, unsigned int width, unsigned int height, unsigned int pitch, unsigned int bpp,
    int row, int start_col, int num_cols, unsigned int bg_color) 
{
    int start_x = start_col * CHAR_W;
    int end_x = start_x + (num_cols * CHAR_W);
    int start_y = row * (CHAR_H + CHAR_GAP_Y);
    int end_y = start_y + CHAR_H;

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            putpixel_local(fb, width, height, pitch, bpp, x, y, bg_color);
        }
    }
}

__attribute__((always_inline)) static inline void draw_char_local(
    uint8_t* fb, unsigned int width, unsigned int height, unsigned int pitch, unsigned int bpp,
    char c, int x, int y, unsigned int fg, unsigned int bg) 
{
    unsigned char* font_row = font8x8_basic[(unsigned char)c];
    int dst_y = y;
    for (int dy = 0; dy < 8; dy++) {
        int row_height = (dy % SCALE_Y_DEN < (SCALE_Y_NUM % SCALE_Y_DEN)) ? 
                         (SCALE_Y_NUM / SCALE_Y_DEN + 1) : (SCALE_Y_NUM / SCALE_Y_DEN);
        if (row_height == 0) row_height = 1;
        int dst_x = x;
        for (int dx = 0; dx < 8; dx++) {
            unsigned int px_color = (font_row[dy] & (1 << (7 - dx))) ? fg : bg;
            int col_width = (dx % SCALE_X_DEN < (SCALE_X_NUM % SCALE_X_DEN)) ? 
                            (SCALE_X_NUM / SCALE_X_DEN + 1) : (SCALE_X_NUM / SCALE_X_DEN);
            if (col_width == 0) col_width = 1;
            for (int sy = 0; sy < row_height; sy++) {
                for (int sx = 0; sx < col_width; sx++) {
                    putpixel_local(fb, width, height, pitch, bpp, dst_x + sx, dst_y + sy, px_color);
                }
            }
            dst_x += col_width;
        }
        dst_y += row_height;
    }
}

__attribute__((always_inline)) static inline void print_str_local(
    uint8_t* fb, unsigned int width, unsigned int height, unsigned int pitch, unsigned int bpp,
    const char* str, int row, int col, unsigned int fg, unsigned int bg) 
{
    for (int i = 0; str[i] != 0; i++) {
        draw_char_local(fb, width, height, pitch, bpp, str[i], col * CHAR_W + (i * CHAR_W), row * (CHAR_H + CHAR_GAP_Y), fg, bg);
    }
}

__attribute__((force_align_arg_pointer))
void _start() {
    __asm__ volatile("cli");
    uint8_t* fb         = (uint8_t*)*VBE_FB_PTR;
    unsigned int pitch  = *VBE_PITCH_PTR;
    unsigned int width  = *VBE_WIDTH_PTR;
    unsigned int height = *VBE_HEIGHT_PTR;
    unsigned int bpp    = *VBE_BPP_PTR;
    if (fb == 0) {
        while(1);
    }
    if (width == 0)  width = 1024;
    if (height == 0) height = 768;
    if (bpp == 0)    bpp = 32;
    if (pitch == 0)  pitch = width * (bpp / 8);
    unsigned int bg_color = 0x000000AA;
    unsigned int fg_color = 0x00FFFFFF;
    for (unsigned int y = 0; y < height; y++) {
        for (unsigned int x = 0; x < width; x++) {
            putpixel_local(fb, width, height, pitch, bpp, x, y, bg_color);
        }
    }
    print_str_local(fb, width, height, pitch, bpp, "--- CawOS Recovery Mode ---", 2, 5, 0x00FFFF00, bg_color); 
    print_str_local(fb, width, height, pitch, bpp, "System diagnostics and rescue environment", 3, 5, fg_color, bg_color);
    print_str_local(fb, width, height, pitch, bpp, "Running hardware integrity checks...", 6, 5, fg_color, bg_color);
    for (volatile int d = 0; d < 30000000; d++);
    print_str_local(fb, width, height, pitch, bpp, "Testing RAM (0x500000 - 0x900000): Testing...", 8, 5, fg_color, bg_color);
    for (volatile int d = 0; d < 50000000; d++);
    int mem_ok = 1;
    volatile unsigned int* mem_start = (volatile unsigned int*)0x500000;
    unsigned int total_words = (4 * 1024 * 1024) / 4; 
    for (unsigned int i = 0; i < total_words; i++) mem_start[i] = 0x55555555;
    for (unsigned int i = 0; i < total_words; i++) {
        if (mem_start[i] != 0x55555555) { mem_ok = 0; break; }
    }
    if (mem_ok) {
        for (unsigned int i = 0; i < total_words; i++) mem_start[i] = 0xAAAAAAAA;
        for (unsigned int i = 0; i < total_words; i++) {
            if (mem_start[i] != 0xAAAAAAAA) { mem_ok = 0; break; }
        }
    }
    clear_row_local(fb, width, height, pitch, bpp, 8, 40, 15, bg_color);
    if (mem_ok) {
        print_str_local(fb, width, height, pitch, bpp, "[OK]", 8, 40, 0x0000FF00, bg_color); 
    } else {
        print_str_local(fb, width, height, pitch, bpp, "[FAIL]", 8, 40, 0x00FF0000, bg_color); 
    }
    print_str_local(fb, width, height, pitch, bpp, "Diagnostics completed.", 11, 5, fg_color, bg_color);
    print_str_local(fb, width, height, pitch, bpp, "Please restart your computer.", 13, 5, 0x0000FFFF, bg_color);
    while(1);
}