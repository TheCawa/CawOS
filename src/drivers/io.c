#include "io.h"

__attribute__((visibility("default")))
unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a" (result) : "dN" (port));
    return result;
}

__attribute__((visibility("default")))
void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ volatile("outb %0, %1" : : "a" (data), "dN" (port));
}

__attribute__((visibility("default")))
void port_word_out(unsigned short port, unsigned short data) {
    __asm__ volatile("outw %0, %1" : : "a" (data), "dN" (port));
}

__attribute__((visibility("default")))
unsigned short port_word_in(unsigned short port) {
    unsigned short result;
    __asm__ volatile("inw %1, %0" : "=a" (result) : "dN" (port));
    return result;
}