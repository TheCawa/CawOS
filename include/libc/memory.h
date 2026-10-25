#ifndef CAWOS_LIBC_MEMORY_H
#define CAWOS_LIBC_MEMORY_H

void memset(void* dest, unsigned char val, int len);
void memmove(void* dest, const void* src, int len);
void memcpy(void* dest, const void* src, int len);
int memcmp(const void* s1, const void* s2, int n);

#endif