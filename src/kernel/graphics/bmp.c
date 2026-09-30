#include "gui/bmp.h"
#include "fs.h"
#include "libc/util.h"
#include "kernel/memory.h"

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t rd32s(const uint8_t* p) { return (int32_t)rd32(p); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int bmp_split_path(const char* path, char* dir, char* name) {
    const char* slash = NULL;
    for (const char* p = path; *p; p++) if (*p == '/') slash = p;
    if (!slash) return 0;
    int dlen = (int)(slash - path);
    if (dlen == 0) strcpy(dir, "/");
    else {
        if (dlen > 31) return 0;
        memcpy(dir, path, dlen);
        dir[dlen] = '\0';
    }
    const char* base = slash + 1;
    if (*base == '\0' || strlen(base) > 31) return 0;
    strcpy(name, base);
    return 1;
}

int bmp_load(const char* path, bmp_image_t* out) {
    out->width = 0; out->height = 0; out->rgb = NULL;
    char dir[32], name[32];
    if (!bmp_split_path(path, dir, name)) return 0;

    char saved[32];
    strcpy(saved, current_dir);
    fs_cd_abs(dir);
    int exists = fs_exists(name);
    uint32_t size = exists ? fs_get_size(name) : 0;
    if (!exists || size < 54 || size > 8 * 1024 * 1024) {
        fs_cd_abs(saved);
        return 0;
    }
    uint8_t* buf = (uint8_t*)malloc(size);
    if (!buf) { fs_cd_abs(saved); return 0; }
    int ok = fs_load_to_memory(name, buf);
    fs_cd_abs(saved);
    if (!ok) { free(buf); return 0; }

    int result = 0;
    do {
        if (buf[0] != 'B' || buf[1] != 'M') break;
        uint32_t data_off = rd32(buf + 10);
        int32_t w = rd32s(buf + 18);
        int32_t h = rd32s(buf + 22);
        uint16_t bpp = rd16(buf + 28);
        uint32_t compression = rd32(buf + 30);
        if (w <= 0 || h == 0 || compression != 0) break;
        if (bpp != 24 && bpp != 32) break;
        int top_down = (h < 0);
        int hh = top_down ? -h : h;
        if (w > 4096 || hh > 4096) break;
        uint32_t stride = ((uint32_t)w * (bpp / 8) + 3) & ~3u;
        if (data_off + (uint32_t)hh * stride > size) break;

        uint8_t* rgb = (uint8_t*)malloc((uint32_t)w * (uint32_t)hh * 3);
        if (!rgb) break;
        for (int y = 0; y < hh; y++) {
            int src_row = top_down ? y : (hh - 1 - y);
            const uint8_t* row = buf + data_off + (uint32_t)src_row * stride;
            uint8_t* dst = rgb + (uint32_t)y * (uint32_t)w * 3;
            for (int x = 0; x < w; x++) {
                const uint8_t* px = row + (uint32_t)x * (bpp / 8);
                dst[x * 3 + 0] = px[2];   /* R */
                dst[x * 3 + 1] = px[1];   /* G */
                dst[x * 3 + 2] = px[0];   /* B */
            }
        }
        out->width = w;
        out->height = hh;
        out->rgb = rgb;
        result = 1;
    } while (0);

    free(buf);
    return result;
}

void bmp_free(bmp_image_t* img) {
    if (img && img->rgb) {
        free(img->rgb);
        img->rgb = NULL;
    }
    if (img) { img->width = 0; img->height = 0; }
}

void bmp_blit_scaled(const bmp_image_t* img, int dx, int dy, int dw, int dh,
                     void (*put)(int, int, uint32_t)) {
    if (!img || !img->rgb || dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int sy = y * img->height / dh;
        if (sy < 0) sy = 0;
        if (sy >= img->height) sy = img->height - 1;
        const uint8_t* srow = img->rgb + (uint32_t)sy * (uint32_t)img->width * 3;
        for (int x = 0; x < dw; x++) {
            int sx = x * img->width / dw;
            if (sx < 0) sx = 0;
            if (sx >= img->width) sx = img->width - 1;
            const uint8_t* p = srow + (uint32_t)sx * 3;
            uint32_t color = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
            put(dx + x, dy + y, color);
        }
    }
}