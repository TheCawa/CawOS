#include "drivers/serial.h"
#include "drivers/io.h"
#include "libc/util.h"
#include <stdarg.h>

int g_serial_mirror_enabled = 0;

void serial_init(uint16_t port) {
    port_byte_out(port + 1, 0x00);
    port_byte_out(port + 3, 0x80);
    port_byte_out(port + 0, 0x01);
    port_byte_out(port + 1, 0x00);
    port_byte_out(port + 3, 0x03);
    port_byte_out(port + 2, 0xC7);
}

int serial_is_ready(uint16_t port) {
    return port_byte_in(port + 5) & 0x20;
}

void serial_putc(uint16_t port, char c) {
    while (!serial_is_ready(port)) {
        __asm__ volatile("nop");
    }
    port_byte_out(port, (unsigned char)c);
}

void serial_puts(uint16_t port, const char* str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putc(port, str[i]);
    }
}

void serial_printf(uint16_t port, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    serial_puts(port, buf);
}

int serial_has_data(uint16_t port) {
    return port_byte_in(port + 5) & 0x01;
}

char serial_read(uint16_t port) {
    return (char)port_byte_in(port);
}

void serial_flush(uint16_t port) {
    while (serial_has_data(port)) {
        (void)serial_read(port);
    }
}

void serial_mirror_line(const char* str) {
    if (!g_serial_mirror_enabled || !str) return;
    serial_puts(SERIAL_COM1, str);
    serial_putc(SERIAL_COM1, '\n');
}
