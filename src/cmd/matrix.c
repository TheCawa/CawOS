#include "commands.h"
#include "drivers/io.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/interrupt.h"

#define VIDEO_MEMORY ((char*)0xB8000)
#define MAX_SCREEN_WIDTH 80
#define MAX_SCREEN_HEIGHT 25

typedef struct {
    unsigned char tail;
    unsigned char body;
    unsigned char head;
} color_scheme_t;

static const color_scheme_t schemes[] = {
    {0x02, 0x0A, 0x0E}, // 0: Green (default)
    {0x04, 0x0C, 0x0F}, // 1: Red
    {0x01, 0x09, 0x0F}, // 2: Blue
    {0x03, 0x0B, 0x0F}, // 3: Cyan
    {0x08, 0x07, 0x0F}, // 4: White/Yellow
    {0x05, 0x0D, 0x0F}, // 5: Magenta
};

#define SCHEME_COUNT (sizeof(schemes) / sizeof(schemes[0]))
#define DEFAULT_SCHEME 0

static const char matrix_chars[] = "!@#$%^&*()_+-=[]{}|;':\",./<>?\\0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

typedef struct {
    int y;
    int length;
    int speed;
    int counter;
} drop_t;

static drop_t drops[MAX_SCREEN_WIDTH];
static color_scheme_t current_scheme;

void init_drops(int scheme_idx) {
    int rows = screen_get_rows();
    int cols = screen_get_cols();
    if (cols > MAX_SCREEN_WIDTH) cols = MAX_SCREEN_WIDTH;
    
    if (scheme_idx < 0 || scheme_idx >= SCHEME_COUNT) {
        scheme_idx = DEFAULT_SCHEME;
    }
    current_scheme = schemes[scheme_idx];
    
    for (int i = 0; i < cols; i++) {
        drops[i].y = -(rand(0, rows));
        drops[i].length = rand(5, 15);
        drops[i].speed = rand(1, 3);
        drops[i].counter = 0;
    }
}

char get_random_char() {
    int idx = rand(0, sizeof(matrix_chars) - 2);
    return matrix_chars[idx];
}

void clear_cell(int x, int y) {
    print_char_at(' ', y, x, 0x00);
}

void render_frame() {
    int rows = screen_get_rows();
    int cols = screen_get_cols();
    if (cols > MAX_SCREEN_WIDTH) cols = MAX_SCREEN_WIDTH;
    for (int x = 0; x < cols; x++) {
        drop_t* d = &drops[x];
        d->counter++;
        if (d->counter < d->speed) {
            continue;
        }
        d->counter = 0;
        int tail_end_y = d->y - d->length;
        if (tail_end_y >= 0 && tail_end_y < rows) {
            clear_cell(x, tail_end_y);
        }
        d->y++;
        if (d->y - d->length > rows + 5) {
            for (int y = 0; y < rows; y++) {
                clear_cell(x, y);
            }
            d->y = -rand(0, 10);
            d->length = rand(5, 15);
            d->speed = rand(1, 3);
            continue;
        }
        for (int i = 0; i < d->length; i++) {
            int y = d->y - i;
            if (y < 0 || y >= rows) continue;
            
            unsigned char color = current_scheme.tail;
            if (i == 0) {
                color = current_scheme.head;
            } else if (i < 3) {
                color = current_scheme.body;
            }
            char c = get_random_char();
            print_char_at(c, y, x, color);
        }
    }
}

int parse_scheme(char* args) {
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

void cmd_matrix(char* args, int* row) {
    (void)row;
    int scheme = parse_scheme(args);
    clear_screen();
    disable_cursor();
    init_drops(scheme);
    while (!is_interrupt_requested()) {
        render_frame();
        sleep_ms(30);
    }
    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
}

REGISTER_COMMAND("matrix", cmd_matrix, 1);