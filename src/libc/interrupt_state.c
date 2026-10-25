#include "libc/interrupt_state.h"

volatile int g_interrupt_requested = 0;

void request_interrupt(void) {
    g_interrupt_requested = 1;
}

void clear_interrupt(void) {
    g_interrupt_requested = 0;
}

int is_interrupt_requested(void) {
    return g_interrupt_requested;
}