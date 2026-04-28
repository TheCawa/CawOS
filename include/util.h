#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
int strlen(const char* s);
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, int n);

void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
void memset(void* dest, unsigned char val, int len);
void memcpy(void* dest, const void* src, int len);

void get_cpu_info(char* buffer);
unsigned short get_total_memory();
void itoa(int n, char str[]);
int atoi(const char* s);
void ftoa(float n, char* res, int precision);
float atof(char* s);

extern void feed_entropy(unsigned char val);
int rand(int min, int max);
unsigned int hash(char* str);

void sleep_ms(uint32_t ms);

#endif