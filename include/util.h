#ifndef UTIL_H
#define UTIL_H

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
int strlen(const char* s);

void strcpy(char* dest, const char* src);
void memset(void* dest, unsigned char val, int len);
void memcpy(void* dest, const void* src, int len);

void get_cpu_info(char* buffer);
unsigned short get_total_memory();
void itoa(int n, char str[]);

#endif