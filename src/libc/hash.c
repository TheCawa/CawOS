#include "libc/hash.h"

unsigned int hash(char* str) {
    unsigned int h = 5381;
    int c;

    while ((c = *str++)) {
        h = ((h << 5) + h) + c;
    }

    return h;
}

void itoa_hex(uint32_t n, char* str) {
    char* hex_chars = "0123456789ABCDEF";
    str[0] = '0'; str[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        str[i + 2] = hex_chars[n & 0xF];
        n >>= 4;
    }
    str[10] = '\0';
}
