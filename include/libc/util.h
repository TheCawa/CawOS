#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

int strcmp(const char* s1, const char* s2);
char* strncpy(char* dest, const char* src, int n);
int strncmp(const char* s1, const char* s2, int n);
int strlen(const char* s);
int strnlen(const char* s, int max_len);
char* strchr(const char* s, int c);
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, int n);
char* strstr(const char* haystack, const char* needle);

void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, int n);
int safe_strcpy(char* dest, const char* src, int dest_size);
int safe_strcat(char* dest, const char* src, int dest_size);
int snprintf(char* buf, int size, const char* fmt, ...);
int vsnprintf(char* buf, int size, const char* fmt, va_list args);
void memset(void* dest, unsigned char val, int len);
void memmove(void* dest, const void* src, int len);
void memcpy(void* dest, const void* src, int len);
int memcmp(const void* s1, const void* s2, int n);

void get_cpu_info(char* buffer);
unsigned short get_total_memory();
void itoa(int n, char str[]);
void itoa_hex(uint32_t n, char* str);
int atoi(const char* s);
void ftoa(float n, char* res, int precision);
float atof(char* s);

extern void feed_entropy(unsigned char val);
int rand(int min, int max);
unsigned int hash(char* str);

void sleep_ms(uint32_t ms);

#endif