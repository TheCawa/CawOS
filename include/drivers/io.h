#ifndef IO_H
#define IO_H

#include <stdint.h>

unsigned char port_byte_in(unsigned short port);
void port_byte_out(unsigned short port, unsigned char data);
void port_word_out(unsigned short port, unsigned short data);
unsigned short port_word_in(unsigned short port);
static inline uint32_t port_dword_in(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %w1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

static inline void port_dword_out(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %w1" : : "a" (value), "Nd" (port));
}

#endif