#ifndef AC97_H
#define AC97_H

#include <stdint.h>

int ac97_init();
void ac97_play_pcm(uint8_t* data, uint32_t size);
extern int ac97_ready;
extern uint8_t sound_buffer[131072]; 

#endif