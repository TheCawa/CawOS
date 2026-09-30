#include "libc/font.h"
#include "fs.h"
#include "libc/util.h"
#include "kernel/memory.h"

static font_t font_loaded = { 0, 0, 0, NULL, NULL };
static char font_loaded_name[48];

void font_unload_loaded(void) {
    if (font_loaded.glyphs) {
        free((void*)font_loaded.glyphs);
        font_loaded.glyphs = NULL;
    }
    font_loaded.width = 0;
    font_loaded.height = 0;
    font_loaded.num_glyphs = 0;
}

int font_load_psf(const char* path) {
    uint32_t size = fs_get_size_abs(path);
    if (size < 4) return 0;
    
    uint8_t* buf = (uint8_t*)malloc(size);
    if (!buf) return 0;
    
    if (!fs_load_to_memory_abs(path, buf)) {
        free(buf);
        return 0;
    }

    if (buf[0] != 0x36 || buf[1] != 0x04) {
        free(buf);
        return 0;
    }
    
    uint8_t mode = buf[2];
    uint8_t charsize = buf[3];
    uint32_t num_glyphs = (mode & 0x01) ? 512 : 256;
    uint32_t expected_size = 4 + num_glyphs * charsize;
    
    if (size < expected_size) {
        free(buf);
        return 0;
    }
    
    font_unload_loaded();
    uint8_t* glyph_data = (uint8_t*)malloc(num_glyphs * charsize);
    if (!glyph_data) {
        free(buf);
        return 0;
    }
    memcpy(glyph_data, buf + 4, num_glyphs * charsize);
    free(buf);
    
    font_loaded.width = 8;
    font_loaded.height = charsize;
    font_loaded.num_glyphs = num_glyphs;
    font_loaded.glyphs = glyph_data;
    const char* base = path;
    for (const char* p = path; *p; p++) if (*p == '/') base = p + 1;
    safe_strcpy(font_loaded_name, base, sizeof(font_loaded_name));
    font_loaded.name = font_loaded_name;
    font_set_current(&font_loaded);
    return 1;
}

int font_save_config(const char* name) {
    int dummy_row = 0;
    if (!fs_cd_abs("/core/config")) return 0;
    if (!fs_exists("font")) {
        if (!fs_create("font", &dummy_row)) {
            fs_cd_abs("/");
            return 0;
        }
    }
    uint32_t len = strlen(name) + 1;
    if (!fs_write("font", (uint8_t*)name, len)) {
        fs_cd_abs("/");
        return 0;
    }
    fs_cd_abs("/");
    return 1;
}