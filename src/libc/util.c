#include "util.h"
#include "io.h"

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
    unsigned short total;
    unsigned char low, high;

    port_byte_out(0x70, 0x30);       
    low = port_byte_in(0x71);
    port_byte_out(0x70, 0x31);       
    high = port_byte_in(0x71);

    total = low | (high << 8);       
    return total / 1024;            
}


int strlen(const char* s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
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