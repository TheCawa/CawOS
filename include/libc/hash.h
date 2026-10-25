#ifndef CAWOS_LIBC_HASH_H
#define CAWOS_LIBC_HASH_H

#include <stdint.h>

unsigned int hash(char* str);
void itoa_hex(uint32_t n, char* str);

#endif