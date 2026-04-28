#include "util.h"
#include "io.h"
#include "idt.h"

static unsigned int state = 0xACE1;
extern unsigned int bios_get_mem();



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

void memset(void* dest, unsigned char val, int len) {
    unsigned char* temp = (unsigned char*)dest;
    for ( ; len != 0; len--) *temp++ = val;
}

void memcpy(void* dest, const void* src, int len) {
    const unsigned char* sp = (const unsigned char*)src;
    unsigned char* dp = (unsigned char*)dest;
    for (; len != 0; len--) *dp++ = *sp++;
}

void get_cpu_info(char* out_str) {
    unsigned int regs[4];
    __asm__ __volatile__("cpuid" : "=a"(regs[0]) : "a"(0x80000000));

    if (regs[0] >= 0x80000004) {
        for (unsigned int i = 0; i < 3; i++) {
            __asm__ __volatile__("cpuid" 
                : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3]) 
                : "a"(0x80000002 + i));
            
            ((unsigned int*)out_str)[i * 4 + 0] = regs[0];
            ((unsigned int*)out_str)[i * 4 + 1] = regs[1];
            ((unsigned int*)out_str)[i * 4 + 2] = regs[2];
            ((unsigned int*)out_str)[i * 4 + 3] = regs[3];
        }
        out_str[47] = '\0';
    } else {
        __asm__ __volatile__("cpuid" : "=b"(regs[1]), "=d"(regs[3]), "=c"(regs[2]) : "a"(0));
        ((unsigned int*)out_str)[0] = regs[1];
        ((unsigned int*)out_str)[1] = regs[3];
        ((unsigned int*)out_str)[2] = regs[2];
        out_str[12] = '\0';
    }
}

unsigned short get_total_memory() {
    bios_get_mem();
    return *(unsigned short*)0x9D04;
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

void itoa(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    for (int j = 0, k = i-1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

int atoi(const char* s) {
    int res = 0;
    int sign = 1;
    int i = 0;

    if (s[0] == '-') {
        sign = -1;
        i++;
    }

    for (; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '9') break;
        res = res * 10 + s[i] - '0';
    }

    return sign * res;
}

float atof(char* s) {
    float res = 0.0f;
    float div = 1.0f;
    int decimal = 0;
    int sign = 1;

    if (*s == '-') {
        sign = -1;
        s++;
    }

    while (*s) {
        if (*s == '.' || *s == ',') {
            decimal = 1;
            s++;
            continue;
        }
        int digit = *s - '0';
        if (digit >= 0 && digit <= 9) {
            if (decimal) {
                div *= 10.0f;
                res = res + (float)digit / div;
            } else {
                res = res * 10.0f + (float)digit;
            }
        }
        s++;
    }
    return res * (float)sign;
}

void ftoa(float n, char* res, int precision) {
    if (n < 0) {
        *res++ = '-';
        n = -n;
    }
    int ipart = (int)n;
    float fpart = n - (float)ipart;
    itoa(ipart, res);
    if (precision > 0) {
        int len = strlen(res);
        res[len] = '.';
        for (int i = 0; i < precision; i++) {
            fpart *= 10.0f;
        }
        int ifpart = (int)(fpart + 0.5f);
        itoa(ifpart, res + len + 1);
    }
}

void feed_entropy(unsigned char scancode) {
    state ^= (unsigned int)scancode;
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
}

int rand(int min, int max) {
    state = (state * 1103515245 + 12345) & 0x7FFFFFFF;
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    if (min == max) {
        return min;
    }
    return (state % (max - min + 1)) + min;
}

unsigned int hash(char* str) {
    unsigned int h = 5381;
    int c;
    while ((c = *str++)) {
        h = ((h << 5) + h) + c; 
    }
    return h;
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

void sleep_ms(uint32_t ms) {
    uint32_t ticks_to_wait = ms / 10; 
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    uint32_t start_tick = system_ticks;
    while ((system_ticks - start_tick) < ticks_to_wait) {
        __asm__ volatile("hlt"); 
    }
}