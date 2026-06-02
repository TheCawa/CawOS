#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t buttons;
} mouse_packet_t;

typedef struct {
    int x;
    int y;
    uint8_t buttons;
    uint8_t left_button;
    uint8_t right_button;
    uint8_t middle_button;
} mouse_state_t;

void mouse_init();
mouse_state_t* mouse_get_state();
void mouse_handler();
void mouse_poll();
int mouse_is_waiting_for_packet();
void mouse_process_byte(uint8_t byte);

#endif
