#include <stdint.h>

#define VBE_FB_PTR      ((unsigned int*)0x0520)
#define VBE_PITCH_PTR   ((unsigned int*)0x0524)
#define VBE_WIDTH_PTR   ((unsigned int*)0x0528)
#define VBE_HEIGHT_PTR  ((unsigned int*)0x052C)
#define VBE_BPP_PTR     ((unsigned int*)0x0530)
extern unsigned char font8x16_basic[256][16];

__attribute__((always_inline)) static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

__attribute__((always_inline)) static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

static volatile uint8_t* const VGA_TEXT = (volatile uint8_t*)0xb8000;

__attribute__((always_inline)) static inline void text_clear(uint8_t attr) {
    for (int i = 0; i < 80 * 25; i++) {
        VGA_TEXT[i * 2] = ' ';
        VGA_TEXT[i * 2 + 1] = attr;
    }
}

__attribute__((always_inline)) static inline void text_put(int row, int col, char c, uint8_t attr) {
    if (row < 0 || row >= 25 || col < 0 || col >= 80) return;
    VGA_TEXT[(row * 80 + col) * 2] = (uint8_t)c;
    VGA_TEXT[(row * 80 + col) * 2 + 1] = attr;
}

__attribute__((always_inline)) static inline void text_print_scaled(int row, int col, const char* s, uint8_t attr) {
    for (int i = 0; s[i]; i++) {
        text_put(row, col + i, s[i], attr);
    }
}

__attribute__((always_inline)) static inline int run_ram_test(void) {
    volatile unsigned int* mem_start = (volatile unsigned int*)0x500000;
    unsigned int total_words = (6 * 1024 * 1024) / 4;
    for (unsigned int i = 0; i < total_words; i++) mem_start[i] = 0x55555555;
    for (unsigned int i = 0; i < total_words; i++)
        if (mem_start[i] != 0x55555555) return 0;
    for (unsigned int i = 0; i < total_words; i++) mem_start[i] = 0xAAAAAAAA;
    for (unsigned int i = 0; i < total_words; i++)
        if (mem_start[i] != 0xAAAAAAAA) return 0;
    return 1;
}

__attribute__((always_inline)) static inline void drain_keyboard(void) {
    for (int i = 0; i < 10; i++) {
        if ((inb(0x64) & 1) == 0) break;
        (void)inb(0x60);
    }
}

__attribute__((always_inline)) static inline void wait_for_reboot(void) {
    drain_keyboard();
    while (1) {
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (scancode == 0x13) {  /* 'R' */
                outb(0x64, 0xFE);
                uint8_t r92 = inb(0x92);
                if (!(r92 & 1)) outb(0x92, r92 | 1);
                volatile uint16_t idt_stub[3] = {0, 0, 0};
                __asm__ volatile("lidt (%0); int $3" : : "r"(idt_stub));
            }
        }
        for (volatile int i = 0; i < 1000000; i++);
    }
}

__attribute__((always_inline)) static inline void putpixel_fb(uint8_t* fb, unsigned int pitch, unsigned int bpp,
                               unsigned int x, unsigned int y, unsigned int color) {
    unsigned int offset = y * pitch + x * (bpp / 8);
    if (bpp == 32) {
        fb[offset]     = color & 0xFF;
        fb[offset + 1] = (color >> 8) & 0xFF;
        fb[offset + 2] = (color >> 16) & 0xFF;
        fb[offset + 3] = 0;
    } else if (bpp == 24) {
        fb[offset]     = color & 0xFF;
        fb[offset + 1] = (color >> 8) & 0xFF;
        fb[offset + 2] = (color >> 16) & 0xFF;
    } else if (bpp == 16) {
        unsigned int r = (color >> 16) & 0xFF;
        unsigned int g = (color >> 8) & 0xFF;
        unsigned int b = color & 0xFF;
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *((volatile uint16_t*)(fb + offset)) = rgb565;
    }
}

__attribute__((always_inline)) static inline void fill_rect_fb(uint8_t* fb, unsigned int pitch, unsigned int bpp,
                         unsigned int width, unsigned int height,
                         unsigned int x, unsigned int y, unsigned int w, unsigned int h,
                         unsigned int color) {
    for (unsigned int dy = 0; dy < h && (y + dy) < height; dy++) {
        for (unsigned int dx = 0; dx < w && (x + dx) < width; dx++) {
            putpixel_fb(fb, pitch, bpp, x + dx, y + dy, color);
        }
    }
}

__attribute__((always_inline)) static inline void draw_char_fb(uint8_t* fb, unsigned int pitch, unsigned int bpp,
                         unsigned int width, unsigned int height,
                         char c, int x, int y, unsigned int fg, unsigned int bg, int scale) {
    unsigned char uc = (unsigned char)c;
    if (uc >= 128) uc = '?';
    const uint8_t* glyph = font8x16_basic[uc];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            int is_fg = (glyph[row] >> (7 - col)) & 1;
            unsigned int px_color = is_fg ? fg : bg;
            fill_rect_fb(fb, pitch, bpp, width, height,
                        x + col * scale, y + row * scale, scale, scale, px_color);
        }
    }
}

__attribute__((always_inline)) static inline void print_str_fb(uint8_t* fb, unsigned int pitch, unsigned int bpp,
                         unsigned int width, unsigned int height,
                         const char* s, int x, int y, unsigned int fg, unsigned int bg, int scale) {
    int cx = x;
    for (int i = 0; s[i]; i++) {
        draw_char_fb(fb, pitch, bpp, width, height, s[i], cx, y, fg, bg, scale);
        cx += 8 * scale;
    }
}

__attribute__((force_align_arg_pointer))
void _start() {
    __asm__ volatile("cli");
    uint32_t fb_ptr     = *((volatile uint32_t*)VBE_FB_PTR);
    uint32_t pitch_val  = *((volatile uint32_t*)VBE_PITCH_PTR);
    uint32_t width_val  = *((volatile uint32_t*)VBE_WIDTH_PTR);
    uint32_t height_val = *((volatile uint32_t*)VBE_HEIGHT_PTR);
    uint32_t bpp_val    = *((volatile uint32_t*)VBE_BPP_PTR);
    int vbe_valid = (fb_ptr != 0 && fb_ptr >= 0x100000 && 
                     width_val > 0 && width_val <= 4096 &&
                     height_val > 0 && height_val <= 4096 &&
                     bpp_val >= 8 && bpp_val <= 32);
    
    if (!vbe_valid) {
        text_clear(0x1F);
        text_print_scaled(2,  5, "--- CawOS Recovery Mode ---", 0x1E);
        text_print_scaled(3,  5, "System diagnostics and rescue environment", 0x1F);
        text_print_scaled(6,  5, "Running hardware integrity checks...", 0x1F);
        for (volatile int d = 0; d < 30000000; d++);
        text_print_scaled(8,  5, "Testing RAM (0x500000 - 0xB00000):", 0x1F);
        text_print_scaled(8, 41, "Testing...", 0x1F);
        for (volatile int d = 0; d < 50000000; d++);
        for (int i = 41; i < 55; i++) text_put(8, i, ' ', 0x1F);
        int mem_ok = run_ram_test();
        text_print_scaled(8, 41, mem_ok ? "[OK]" : "[FAIL]", mem_ok ? 0x1A : 0x1C);
        text_print_scaled(11, 5, "Diagnostics completed.", 0x1F);
        text_print_scaled(13, 5, "Press 'R' key to restart your computer.", 0x1B);
        wait_for_reboot();
        return;
    }
    uint8_t* fb = (uint8_t*)fb_ptr;
    unsigned int pitch = pitch_val;
    unsigned int width = width_val;
    unsigned int height = height_val;
    unsigned int bpp = bpp_val;
    if (pitch == 0) pitch = width * (bpp / 8);
    unsigned int bg_color = 0x000000AA;
    unsigned int fg_color = 0x00FFFFFF;
    unsigned int title_color = 0x00FFFF00;
    fill_rect_fb(fb, pitch, bpp, width, height, 0, 0, width, height, bg_color);
    int scale = 1;
    int char_w = 8 * scale;
    int char_h = 16 * scale;
    print_str_fb(fb, pitch, bpp, width, height,
                 "--- CawOS Recovery Mode ---", 
                 5 * char_w, 2 * char_h, title_color, bg_color, scale);
    print_str_fb(fb, pitch, bpp, width, height,
                 "System diagnostics and rescue environment",
                 5 * char_w, 3 * char_h, fg_color, bg_color, scale);
    print_str_fb(fb, pitch, bpp, width, height,
                 "Running hardware integrity checks...",
                 5 * char_w, 6 * char_h, fg_color, bg_color, scale);
    print_str_fb(fb, pitch, bpp, width, height,
                 "Testing RAM (0x500000 - 0xB00000): Testing...",
                 5 * char_w, 8 * char_h, fg_color, bg_color, scale);
    fill_rect_fb(fb, pitch, bpp, width, height, 
                 40 * char_w, 8 * char_h, 15 * char_w, char_h, bg_color);
    int mem_ok = run_ram_test();
    print_str_fb(fb, pitch, bpp, width, height,
                 mem_ok ? "[OK]" : "[FAIL]",
                 41 * char_w, 8 * char_h,
                 mem_ok ? 0x0000FF00 : 0x00FF0000, bg_color, scale);
    print_str_fb(fb, pitch, bpp, width, height,
                 "Diagnostics completed.",
                 5 * char_w, 11 * char_h, fg_color, bg_color, scale);
    print_str_fb(fb, pitch, bpp, width, height,
                 "Press 'R' key to restart your computer.",
                 5 * char_w, 13 * char_h, 0x0000FFFF, bg_color, scale);
    wait_for_reboot();
}