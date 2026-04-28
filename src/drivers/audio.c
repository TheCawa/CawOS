#include "audio.h"
#include "io.h" 
#include "idt.h"
#include "util.h"

void play_sound(unsigned int nFrequence) {
    if (nFrequence == 0) return;
    unsigned int Div = 1193180 / nFrequence;
    port_byte_out(0x43, 0xb6);
    port_byte_out(0x42, (unsigned char) (Div) );
    port_byte_out(0x42, (unsigned char) (Div >> 8));
    unsigned char tmp = port_byte_in(0x61);
    if (tmp != (tmp | 3)) {
        port_byte_out(0x61, tmp | 3);
    }
}

void nosound() {
    unsigned char tmp = port_byte_in(0x61) & 0xFC;
    port_byte_out(0x61, tmp);
}

void beep() {
    play_sound(1000);
    sleep_ms(200);
    nosound();
}

void play_bytebeat(int duration) {
    for (int t = 0; t < duration; t++) {
        int b = t*(42&t>>10)%256*(1-t%2048/3000);
        unsigned int freq = (b % 1000) + 200;    
        play_sound(freq);
        sleep_ms(50);
        
        watchdog_reset();
    }
    nosound();
}