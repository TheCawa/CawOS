#include "commands.h"
#include "drivers/io.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/interrupt.h"
#include "libc/keyboard_map.h"
#include "kernel/idt.h"

#define VIDEO_MEMORY ((char*)0xB8000)
#define MAX_SCREEN_HEIGHT 256

typedef struct {
    unsigned char tail;
    unsigned char body;
    unsigned char head;
} color_scheme_t;

static const color_scheme_t schemes[] = {
    {0x02, 0x0A, 0x0E}, // 0: Green
    {0x04, 0x0C, 0x0F}, // 1: Red
    {0x01, 0x09, 0x0F}, // 2: Blue
    {0x03, 0x0B, 0x0F}, // 3: Cyan (по умолчанию для catrix)
    {0x08, 0x07, 0x0F}, // 4: White/Yellow
    {0x05, 0x0D, 0x0F}, // 5: Magenta
};

#define SCHEME_COUNT (sizeof(schemes) / sizeof(schemes[0]))
#define DEFAULT_SCHEME 3

static const char matrix_chars[] = "!@#$%^&*()_+-=[]{}|;':\",./<>?\\0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char easter_word[] = "CAWOS";

typedef struct {
    int x;
    int length;
    int speed;
    int counter;
    int is_easter;
} stream_t;

static stream_t streams[MAX_SCREEN_HEIGHT];
static color_scheme_t current_scheme;

void init_stream(int scheme_idx) {
    int rows = screen_get_rows();
    int cols = screen_get_cols();
    if (rows > MAX_SCREEN_HEIGHT) rows = MAX_SCREEN_HEIGHT;
    
    if (scheme_idx < 0 || scheme_idx >= SCHEME_COUNT) {
        scheme_idx = DEFAULT_SCHEME;
    }
    current_scheme = schemes[scheme_idx];
    
    for (int i = 0; i < rows; i++) {
        streams[i].x = cols + rand(0, cols);
        streams[i].length = rand(5, 15);
        streams[i].speed = rand(1, 3);
        streams[i].counter = 0;
        streams[i].is_easter = 0;
    }
}

char get_random_chars() {
    int idx = rand(0, sizeof(matrix_chars) - 2);
    return matrix_chars[idx];
}

void clear_cells(int x, int y) {
    print_char_at(' ', y, x, 0x00);
}

void render_frames() {
    int rows = screen_get_rows();
    int cols = screen_get_cols();
    if (rows > MAX_SCREEN_HEIGHT) rows = MAX_SCREEN_HEIGHT;
    
    for (int y = 0; y < rows; y++) {
        stream_t* s = &streams[y];
        s->counter++;
        if (s->counter < s->speed) {
            continue;
        }
        s->counter = 0;
        
        int tail_end_x = s->x + s->length;
        if (tail_end_x < cols && tail_end_x >= 0) {
            clear_cells(tail_end_x, y);
        }
        
        s->x--;
        
        if (s->x + s->length < -5) {
            for (int x = 0; x < cols; x++) {
                clear_cells(x, y);
            }
            s->x = cols + rand(0, 10);
            s->speed = rand(1, 3);
            if (rand(0, 99) < 2) {
                s->is_easter = 1;
                s->length = 5;
            } else {
                s->is_easter = 0;
                s->length = rand(5, 15);
            }
            continue;
        }
        
        for (int i = 0; i < s->length; i++) {
            int x = s->x + i;
            if (x < 0 || x >= cols) continue;
            
            unsigned char color = current_scheme.tail;
            if (i == 0) {
                color = current_scheme.head;
            } else if (i < 3) {
                color = current_scheme.body;
            }
            
            char c;
            if (s->is_easter) {
                c = easter_word[i % 5];
            } else {
                c = get_random_chars();
            }
            
            print_char_at(c, y, x, color);
        }
    }
}

int parse_schemes(char* args) {
    if (!args || args[0] == '\0') return DEFAULT_SCHEME;
    while (*args == ' ') args++;
    if (strcmp(args, "-green") == 0 || strcmp(args, "-g") == 0) return 0;
    if (strcmp(args, "-red") == 0 || strcmp(args, "-r") == 0) return 1;
    if (strcmp(args, "-blue") == 0 || strcmp(args, "-b") == 0) return 2;
    if (strcmp(args, "-cyan") == 0 || strcmp(args, "-c") == 0) return 3;
    if (strcmp(args, "-white") == 0 || strcmp(args, "-w") == 0) return 4;
    if (strcmp(args, "-magenta") == 0 || strcmp(args, "-m") == 0) return 5;
    
    int scheme = 0;
    for (int i = 0; args[i] >= '0' && args[i] <= '9'; i++) {
        scheme = scheme * 10 + (args[i] - '0');
    }
    if (scheme >= 0 && scheme < SCHEME_COUNT) {
        return scheme;
    }

    return DEFAULT_SCHEME;
}

void cmd_catrix(char* args, int* row) {
    (void)row;
    int scheme = parse_schemes(args);
    clear_screen();
    disable_cursor();
    init_stream(scheme);
    while (!is_interrupt_requested()) {
        render_frames();
        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            if (!(scancode & 0x80) && scancode == ESC) {
                break;
            }
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        }
        sleep_ms(30);
    }
    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
}

REGISTER_COMMAND("catrix", cmd_catrix, 1);