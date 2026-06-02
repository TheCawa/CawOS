#include "drivers/audio.h"
#include "drivers/io.h" 
#include "kernel/idt.h"
#include "libc/util.h"

static const float sin_table[] = {
    0.0000f, 0.0245f, 0.0491f, 0.0736f, 0.0980f, 0.1224f, 0.1467f, 0.1710f,
    0.1951f, 0.2191f, 0.2430f, 0.2667f, 0.2903f, 0.3137f, 0.3369f, 0.3599f,
    0.3827f, 0.4052f, 0.4276f, 0.4496f, 0.4714f, 0.4929f, 0.5141f, 0.5350f,
    0.5556f, 0.5758f, 0.5957f, 0.6152f, 0.6344f, 0.6532f, 0.6716f, 0.6895f,
    0.7071f, 0.7242f, 0.7409f, 0.7571f, 0.7729f, 0.7882f, 0.8030f, 0.8173f,
    0.8311f, 0.8443f, 0.8570f, 0.8691f, 0.8807f, 0.8917f, 0.9021f, 0.9119f,
    0.9211f, 0.9297f, 0.9377f, 0.9451f, 0.9519f, 0.9581f, 0.9637f, 0.9687f,
    0.9731f, 0.9769f, 0.9801f, 0.9827f, 0.9847f, 0.9861f, 0.9869f, 0.9871f,
    0.9867f, 0.9857f, 0.9841f, 0.9819f, 0.9791f, 0.9757f, 0.9717f, 0.9671f,
    0.9619f, 0.9561f, 0.9497f, 0.9427f, 0.9351f, 0.9269f, 0.9181f, 0.9087f,
    0.8987f, 0.8881f, 0.8769f, 0.8651f, 0.8527f, 0.8397f, 0.8261f, 0.8119f,
    0.7971f, 0.7817f, 0.7657f, 0.7491f, 0.7319f, 0.7141f, 0.6957f, 0.6767f,
    0.6571f, 0.6369f, 0.6161f, 0.5947f, 0.5727f, 0.5501f, 0.5269f, 0.5031f,
    0.4787f, 0.4537f, 0.4281f, 0.4019f, 0.3751f, 0.3477f, 0.3197f, 0.2911f,
    0.2619f, 0.2321f, 0.2017f, 0.1707f, 0.1391f, 0.1069f, 0.0741f, 0.0407f,
    0.0067f, -0.0279f, -0.0631f, -0.0989f, -0.1353f, -0.1723f, -0.2099f, -0.2481f,
    -0.2869f, -0.3263f, -0.3663f, -0.4069f, -0.4481f, -0.4899f, -0.5323f, -0.5753f,
    -0.6189f, -0.6631f, -0.7079f, -0.7533f, -0.7993f, -0.8459f, -0.8931f, -0.9409f,
    -0.9893f, -1.0000f, -0.9893f, -0.9409f, -0.8931f, -0.8459f, -0.7993f, -0.7533f,
    -0.7079f, -0.6631f, -0.6189f, -0.5753f, -0.5323f, -0.4899f, -0.4481f, -0.4069f,
    -0.3663f, -0.3263f, -0.2869f, -0.2481f, -0.2099f, -0.1723f, -0.1353f, -0.0989f,
    -0.0631f, -0.0279f, 0.0067f, 0.0407f, 0.0741f, 0.1069f, 0.1391f, 0.1707f,
    0.2017f, 0.2321f, 0.2619f, 0.2911f, 0.3197f, 0.3477f, 0.3751f, 0.4019f,
    0.4281f, 0.4537f, 0.4787f, 0.5031f, 0.5269f, 0.5501f, 0.5727f, 0.5947f,
    0.6161f, 0.6369f, 0.6571f, 0.6767f, 0.6957f, 0.7141f, 0.7319f, 0.7491f,
    0.7657f, 0.7817f, 0.7971f, 0.8119f, 0.8261f, 0.8397f, 0.8527f, 0.8651f,
    0.8769f, 0.8881f, 0.8987f, 0.9087f, 0.9181f, 0.9269f, 0.9351f, 0.9427f,
    0.9497f, 0.9561f, 0.9619f, 0.9671f, 0.9717f, 0.9757f, 0.9791f, 0.9819f,
    0.9841f, 0.9857f, 0.9867f, 0.9871f, 0.9869f, 0.9861f, 0.9847f, 0.9827f,
    0.9801f, 0.9769f, 0.9731f, 0.9687f, 0.9637f, 0.9581f, 0.9519f, 0.9451f,
    0.9377f, 0.9297f, 0.9211f, 0.9119f, 0.9021f, 0.8917f, 0.8807f, 0.8691f,
    0.8570f, 0.8443f, 0.8311f, 0.8173f, 0.8030f, 0.7882f, 0.7729f, 0.7571f,
    0.7409f, 0.7242f, 0.7071f, 0.6895f, 0.6716f, 0.6532f, 0.6344f, 0.6152f,
    0.5957f, 0.5758f, 0.5556f, 0.5350f, 0.5141f, 0.4929f, 0.4714f, 0.4496f,
    0.4276f, 0.4052f, 0.3827f, 0.3599f, 0.3369f, 0.3137f, 0.2903f, 0.2667f,
    0.2430f, 0.2191f, 0.1951f, 0.1710f, 0.1467f, 0.1224f, 0.0980f, 0.0736f,
    0.0491f, 0.0245f
};

float sin(float x) {
    while (x < 0) x += 6.28318f;
    while (x >= 6.28318f) x -= 6.28318f;
    int index = (int)(x * 40.743665f);
    if (index > 255) index = 255;
    return sin_table[index];
}

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

void beep(int duration, int frequence) {
    play_sound(frequence);
    sleep_ms(duration);
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

void generate_tone(uint8_t* buffer, uint32_t size) {
    double sample_rate = 44100.0;
    double freq = 440.0;
    double volume = 10000.0;
    int16_t* samples = (int16_t*)buffer;
    uint32_t count = size / 2;
    for (uint32_t i = 0; i < count; i++) {
        double time = (double)i / sample_rate;
        double val = sin(2.0 * 3.14159265 * freq * time);
        samples[i] = (int16_t)(val * volume);
    }
}