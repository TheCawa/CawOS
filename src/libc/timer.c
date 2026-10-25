#include "libc/timer.h"

extern volatile unsigned int system_ticks;

void sleep_ms(unsigned int ms) {
    unsigned int ticks_to_wait = ms / 10;

    if (ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }

    unsigned int start_tick = system_ticks;

    while ((system_ticks - start_tick) < ticks_to_wait) {
        __asm__ volatile("hlt");
    }
}