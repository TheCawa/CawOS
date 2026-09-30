#ifndef BMP_H
#define BMP_H
#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint8_t* rgb;
} bmp_image_t;

int  bmp_load(const char* path, bmp_image_t* out);
void bmp_free(bmp_image_t* img);
void bmp_blit_scaled(const bmp_image_t* img, int dx, int dy, int dw, int dh,
                     void (*put)(int, int, uint32_t));
#endif