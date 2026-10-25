#ifndef CAWOS_LIBC_INTERRUPT_STATE_H
#define CAWOS_LIBC_INTERRUPT_STATE_H

extern volatile int g_interrupt_requested;

void request_interrupt(void);
void clear_interrupt(void);
int is_interrupt_requested(void);

#endif