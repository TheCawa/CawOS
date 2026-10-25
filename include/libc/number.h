#ifndef CAWOS_LIBC_NUMBER_H
#define CAWOS_LIBC_NUMBER_H

void itoa(int n, char str[]);
int atoi(const char* s);
float atof(char* s);
void ftoa(float n, char* res, int precision);

#endif