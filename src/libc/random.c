#include "libc/random.h"

static unsigned int state = 0xACE1u;

void feed_entropy(unsigned char scancode) {
    state ^= (unsigned int)scancode;
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
}

int rand(int min, int max) {
    state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;

    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }

    if (min == max) {
        return min;
    }

    return (state % (max - min + 1)) + min;
}