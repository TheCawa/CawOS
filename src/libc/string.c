#include "libc/string.h"

int strcmp(const char* s1, const char* s2) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void strcat(char* dest, const char* src) {
    int dest_len = strlen(dest);
    int i = 0;
    while (src[i] != '\0') {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
}

int strlen(const char* s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}

char* strchr(const char* s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    return 0;
}

int strcasecmp(const char* s1, const char* s2) {
    int i;
    for (i = 0; ; i++) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
    }
}

int strncasecmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

char* strcasestr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;

    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n &&
               (*h == *n || *h - 32 == *n || *h + 32 == *n)) {
            h++;
            n++;
        }

        if (!*n) return (char*)haystack;
    }

    return 0;
}

char* strstr(const char* haystack, const char* needle) {
    if (!needle[0]) return (char*)haystack;

    for (int i = 0; haystack[i] != '\0'; i++) {
        int j = 0;

        while (needle[j] != '\0' && haystack[i + j] == needle[j]) {
            j++;
        }

        if (needle[j] == '\0') return (char*)(haystack + i);
    }

    return 0;
}

char* strncpy(char* dest, const char* src, int n) {
    int i;

    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

int strnlen(const char* s, int max_len) {
    int i = 0;
    while (i < max_len && s[i] != '\0') i++;
    return i;
}

char* strncat(char* dest, const char* src, int n) {
    int dest_len = strlen(dest);
    int i = 0;

    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }

    dest[dest_len + i] = '\0';
    return dest;
}

int safe_strcpy(char* dest, const char* src, int dest_size) {
    if (dest_size <= 0) return 0;

    int i = 0;

    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    dest[i] = '\0';
    return i;
}

int safe_strcat(char* dest, const char* src, int dest_size) {
    if (dest_size <= 0) return 0;

    int dest_len = strnlen(dest, dest_size);

    if (dest_len >= dest_size - 1) return dest_len;

    int i = 0;

    for (i = 0; dest_len + i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }

    dest[dest_len + i] = '\0';
    return dest_len + i;
}