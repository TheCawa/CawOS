#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define SERIAL_COM1 0x3F8

extern int g_serial_mirror_enabled;

void serial_init(uint16_t port);
int serial_is_ready(uint16_t port);
void serial_putc(uint16_t port, char c);
void serial_puts(uint16_t port, const char* str);
void serial_printf(uint16_t port, const char* fmt, ...);
int serial_has_data(uint16_t port);
char serial_read(uint16_t port);
void serial_flush(uint16_t port);
void serial_mirror_line(const char* str);

#endif
