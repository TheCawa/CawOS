#include "libc/number.h"
#include "libc/string.h"

void itoa(int n, char str[]) {
    int i, sign;

    if ((sign = n) < 0) {
        n = -n;
    }

    i = 0;

    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) {
        str[i++] = '-';
    }

    str[i] = '\0';

    for (int j = 0, k = i - 1; j < k; j++, k--) {
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