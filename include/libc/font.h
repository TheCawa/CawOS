#ifndef FONT_H
#define FONT_H

#include <stdint.h>

extern unsigned char font8x8_basic[128][8];
extern unsigned char font8x16_basic[256][16];

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t num_glyphs;
    const uint8_t* glyphs;
    const char* name;
} font_t;

extern font_t* current_font;

void font_init(void);
int  font_load_psf(const char* path);
void font_unload_loaded(void);
int  font_save_config(const char* name);
font_t* font_get_current(void);
void    font_set_current(font_t* f);
font_t* font_get_8x8(void);
font_t* font_get_8x16(void);

static inline int font_height(void) {
    const font_t* f = font_get_current();
    return f ? (int)f->height : 8;
}
static inline int font_width(void) {
    const font_t* f = font_get_current();
    return f ? (int)f->width : 8;
}

#endif