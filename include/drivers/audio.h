#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

void play_sound(unsigned int nFrequence);
void nosound();
void beep(int duration, int frequence);
void play_bytebeat(int duration);
void generate_tone(uint8_t* buffer, uint32_t size);

#endif