#include "libc/memory.h"

__attribute__((force_align_arg_pointer))
void memset(void* dest, unsigned char val, int len) {
    if (len <= 0) return;

    unsigned int dval = (unsigned int)val;
    dval |= dval << 8;
    dval |= dval << 16;
    dval |= dval << 24;

    unsigned char* d = (unsigned char*)dest;

    while (len > 0 && ((unsigned int)d & 3)) {
        *d++ = val;
        len--;
    }

    int dwords = len / 4;

    __asm__ volatile(
        "cld\n\t"
        "rep stosl"
        : "+D"(d), "+c"(dwords)
        : "a"(dval)
        : "memory"
    );

    len %= 4;

    while (len--) {
        *d++ = val;
    }
}

__attribute__((force_align_arg_pointer))
void memmove(void* dest, const void* src, int len) {
    if (dest == src || len <= 0) return;

    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d < s || d >= s + len) {
        while (len > 0 && ((unsigned int)d & 3)) {
            *d++ = *s++;
            len--;
        }

        int dwords = len / 4;

        __asm__ volatile(
            "cld\n\t"
            "rep movsl"
            : "+D"(d), "+S"(s), "+c"(dwords)
            :
            : "memory"
        );

        len %= 4;

        while (len--) {
            *d++ = *s++;
        }
    } else {
        d += len - 1;
        s += len - 1;

        __asm__ volatile(
            "std\n\t"
            "rep movsb\n\t"
            "cld"
            : "+D"(d), "+S"(s), "+c"(len)
            :
            : "memory"
        );
    }
}

__attribute__((force_align_arg_pointer))
void memcpy(void* dest, const void* src, int len) {
    if (len <= 0) return;

    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    while (len > 0 && ((unsigned int)d & 3)) {
        *d++ = *s++;
        len--;
    }

    int dwords = len / 4;

    __asm__ volatile(
        "cld\n\t"
        "rep movsl"
        : "+D"(d), "+S"(s), "+c"(dwords)
        :
        : "memory"
    );

    len %= 4;

    while (len--) {
        *d++ = *s++;
    }
}

int memcmp(const void* s1, const void* s2, int n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    for (int i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }

    return 0;
}