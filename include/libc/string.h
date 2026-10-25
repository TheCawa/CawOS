#ifndef CAWOS_LIBC_STRING_H
#define CAWOS_LIBC_STRING_H

int strcmp(const char* s1, const char* s2);
void strcpy(char* dest, const char* src);
int strncmp(const char* s1, const char* s2, int n);
void strcat(char* dest, const char* src);

int strlen(const char* s);
char* strchr(const char* s, int c);

int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, int n);

char* strcasestr(const char* haystack, const char* needle);
char* strstr(const char* haystack, const char* needle);

char* strncpy(char* dest, const char* src, int n);
int strnlen(const char* s, int max_len);
char* strncat(char* dest, const char* src, int n);

int safe_strcpy(char* dest, const char* src, int dest_size);
int safe_strcat(char* dest, const char* src, int dest_size);

#endif