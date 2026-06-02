#include "gui/mouse.h"
#include "drivers/io.h"

#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT   0x02
#define MOUSE_BBIT   0x01
#define MOUSE_WRITE  0xD4
#define MOUSE_V_BIT  0x08

static mouse_state_t g_mouse_state = {0};
static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[3];
extern volatile uint32_t system_ticks;
static uint32_t last_mouse_tick = 0;

mouse_state_t* mouse_get_state() {
    return &g_mouse_state;
}

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 500000;
    if (type == 0) { 
        while (timeout--) {
            if ((port_byte_in(MOUSE_STATUS) & MOUSE_BBIT) == 1) return;
        }
    } else { 
        while (timeout--) {
            if ((port_byte_in(MOUSE_STATUS) & MOUSE_ABIT) == 0) return;
        }
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    port_byte_out(MOUSE_STATUS, MOUSE_WRITE);
    mouse_wait(1);
    port_byte_out(MOUSE_PORT, data);
}

static uint8_t mouse_read() {
    mouse_wait(0);
    return port_byte_in(MOUSE_PORT);
}

void mouse_process_byte(uint8_t byte) {
    if (mouse_cycle != 0 && (system_ticks - last_mouse_tick > 20)) {
        mouse_cycle = 0;
    }

    if (mouse_cycle == 0) {
        if (!(byte & MOUSE_V_BIT)) return; 
        
        mouse_byte[0] = byte;
        mouse_cycle = 1;
        last_mouse_tick = system_ticks;
    } 
    else if (mouse_cycle == 1) {
        mouse_byte[1] = byte;
        mouse_cycle = 2;
        last_mouse_tick = system_ticks;
    } 
    else if (mouse_cycle == 2) {
        mouse_byte[2] = byte;
        mouse_cycle = 0;
        last_mouse_tick = system_ticks;

        g_mouse_state.buttons = mouse_byte[0] & 0x07;
        g_mouse_state.left_button = mouse_byte[0] & 0x01;
        g_mouse_state.right_button = mouse_byte[0] & 0x02;
        g_mouse_state.middle_button = mouse_byte[0] & 0x04;
        int dx = mouse_byte[1];
        int dy = mouse_byte[2];
        if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
        if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;
        dy = -dy;
        extern uint32_t g_width;
        extern uint32_t g_height;
        g_mouse_state.x += dx;
        g_mouse_state.y += dy;
        if (g_mouse_state.x < 0) g_mouse_state.x = 0;
        if (g_mouse_state.y < 0) g_mouse_state.y = 0;
        if (g_mouse_state.x >= (int)g_width) g_mouse_state.x = g_width - 1;
        if (g_mouse_state.y >= (int)g_height) g_mouse_state.y = g_height - 1;
    }
}

void mouse_init() {
    __asm__ volatile("cli");
    uint8_t status;
    mouse_wait(1);
    port_byte_out(MOUSE_STATUS, 0xA8);
    mouse_wait(1);
    port_byte_out(MOUSE_STATUS, 0x20); // Get status
    mouse_wait(0);
    status = port_byte_in(MOUSE_PORT);
    status |= 0x02;
    status |= 0x40;
    status &= ~0x20;
    mouse_wait(1);
    port_byte_out(MOUSE_STATUS, 0x60); // Set status
    mouse_wait(1);
    port_byte_out(MOUSE_PORT, status);
    mouse_write(0xF6);
    mouse_read(); // ACK
    mouse_write(0xF4);
    mouse_read(); // ACK
    mouse_cycle = 0;
    last_mouse_tick = system_ticks;
    __asm__ volatile("sti");
}