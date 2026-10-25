#include <stdarg.h>
#include "libc/format.h"
#include "libc/number.h"

int vsnprintf(char* buf, int size, const char* fmt, va_list args) {
    if (size <= 0) return 0;

    int i = 0;

    while (*fmt && i < size - 1) {
        if (*fmt != '%') {
            buf[i++] = *fmt++;
            continue;
        }

        fmt++;

        if (*fmt == '\0') break;

        switch (*fmt) {
            case 's': {
                char* s = va_arg(args, char*);

                if (s) {
                    while (*s && i < size - 1) {
                        buf[i++] = *s++;
                    }
                }

                break;
            }

            case 'd': {
                int num = va_arg(args, int);
                char numbuf[12];

                itoa(num, numbuf);

                for (int j = 0; numbuf[j] && i < size - 1; j++) {
                    buf[i++] = numbuf[j];
                }

                break;
            }

            case 'u': {
                unsigned int num = va_arg(args, unsigned int);
                char numbuf[12];
                int len = 0;

                if (num == 0) {
                    numbuf[len++] = '0';
                } else {
                    while (num > 0) {
                        numbuf[len++] = (num % 10) + '0';
                        num /= 10;
                    }
                }

                numbuf[len] = '\0';

                for (int j = 0, k = len - 1; j < k; j++, k--) {
                    char temp = numbuf[j];
                    numbuf[j] = numbuf[k];
                    numbuf[k] = temp;
                }

                for (int j = 0; numbuf[j] && i < size - 1; j++) {
                    buf[i++] = numbuf[j];
                }

                break;
            }

            case 'x': {
                unsigned int num = va_arg(args, unsigned int);
                char hex[] = "0123456789abcdef";
                char tmp[9];

                for (int j = 7; j >= 0; j--) {
                    tmp[j] = hex[num & 0xF];
                    num >>= 4;
                }

                int start = 0;
                while (start < 7 && tmp[start] == '0') {
                    start++;
                }

                for (int j = start; j < 8 && i < size - 1; j++) {
                    buf[i++] = tmp[j];
                }

                break;
            }

            case 'p': {
                void* ptr = va_arg(args, void*);
                unsigned int addr = (unsigned int)ptr;

                if (i < size - 1) buf[i++] = '0';
                if (i < size - 1) buf[i++] = 'x';

                char hex[] = "0123456789abcdef";
                char tmp[8];

                for (int j = 7; j >= 0; j--) {
                    tmp[j] = hex[addr & 0xF];
                    addr >>= 4;
                }

                for (int j = 0; j < 8 && i < size - 1; j++) {
                    buf[i++] = tmp[j];
                }

                break;
            }

            case 'c': {
                int c = va_arg(args, int);

                if (i < size - 1) {
                    buf[i++] = (char)c;
                }

                break;
            }

            case '%': {
                if (i < size - 1) {
                    buf[i++] = '%';
                }
                break;
            }

            default: {
                if (i < size - 1) {
                    buf[i++] = '%';
                }

                if (*fmt && i < size - 1) {
                    buf[i++] = *fmt;
                }

                break;
            }
        }

        fmt++;
    }

    buf[i] = '\0';
    return i;
}

int snprintf(char* buf, int size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int result = vsnprintf(buf, size, fmt, args);

    va_end(args);
    return result;
}