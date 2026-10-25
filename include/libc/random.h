#ifndef CAWOS_LIBC_RANDOM_H
#define CAWOS_LIBC_RANDOM_H

void feed_entropy(unsigned char scancode);
int rand(int min, int max);

#endif