#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

extern volatile int g_interrupt_requested;

void request_interrupt();
void clear_interrupt();
int is_interrupt_requested();

#endif