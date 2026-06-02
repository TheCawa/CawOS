#include "commands.h"
#include "drivers/io.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/interrupt.h"

#define MAX_COLS 80
#define MAX_ROWS 25

typedef struct {
    unsigned char flake;
    unsigned char trail;
    unsigned char bg;
} snow_scheme_t;

static const snow_scheme_t schemes[] = {
    {0x0F, 0x07, 0x00},
    {0x0B, 0x03, 0x00},
    {0x0E, 0x06, 0x00},
    {0x0D, 0x05, 0x00},
    {0x0F, 0x0F, 0x01},
};
#define SCHEME_COUNT (sizeof(schemes) / sizeof(schemes[0]))

typedef struct {
    int x;
    int y;
    int speed;
    int drift;
    int counter;
    char symbol;
    int active;
} snowflake_t;

#define MAX_FLAKES 80
static snowflake_t flakes[MAX_FLAKES];
static snow_scheme_t current_scheme;

char get_snow_symbol() {
    const char symbols[] = "*.o+";
    return symbols[rand(0, 3)];
}

void spawn_flake(int idx, int cols) {
    flakes[idx].x = rand(0, cols - 1);
    flakes[idx].y = -rand(0, 5);
    flakes[idx].speed = rand(1, 3);
    flakes[idx].drift = rand(0, 2) - 1;
    flakes[idx].counter = 0;
    flakes[idx].symbol = get_snow_symbol();
    flakes[idx].active = 1;
}

void init_snow(int scheme_idx) {
    int cols = screen_get_cols();
    int rows = screen_get_rows();
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (scheme_idx < 0 || scheme_idx >= SCHEME_COUNT) {
        scheme_idx = 0;
    }
    current_scheme = schemes[scheme_idx];
    for (int i = 0; i < MAX_FLAKES; i++) {
        spawn_flake(i, cols);
    }
}

void clear_cell_snow(int x, int y) {
    print_char_at(' ', y, x, current_scheme.bg);
}

void render_frame_snow() {
    int rows = screen_get_rows();
    int cols = screen_get_cols();
    if (cols > MAX_COLS) cols = MAX_COLS;
    for (int i = 0; i < MAX_FLAKES; i++) {
        snowflake_t* f = &flakes[i];
        if (!f->active) continue;
        f->counter++;
        if (f->counter < f->speed) {
            continue;
        }
        f->counter = 0;
        if (f->y >= 0 && f->y < rows && f->x >= 0 && f->x < cols) {
            clear_cell_snow(f->x, f->y);
        }
        f->y++;
        f->x += f->drift;
        if (rand(0, 10) == 0) {
            f->drift = rand(0, 2) - 1;
        }
        if (f->x < 0) f->x = 0;
        if (f->x >= cols) f->x = cols - 1;
        if (f->y > rows) {
            spawn_flake(i, cols);
            continue;
        }

        if (f->y >= 0 && f->y < rows && f->x >= 0 && f->x < cols) {
            print_char_at(f->symbol, f->y, f->x, current_scheme.flake);
        }
    }
}

int parse_scheme_snow(char* args) {
    if (!args || args[0] == '\0') return 0;
    while (*args == ' ') args++;
    if (strcmp(args, "-white") == 0 || strcmp(args, "-w") == 0) return 0;
    if (strcmp(args, "-blue") == 0 || strcmp(args, "-b") == 0) return 1;
    if (strcmp(args, "-gold") == 0 || strcmp(args, "-g") == 0) return 2;
    if (strcmp(args, "-pink") == 0 || strcmp(args, "-p") == 0) return 3;
    if (strcmp(args, "-night") == 0 || strcmp(args, "-n") == 0) return 4;
    int scheme = 0;
    for (int i = 0; args[i] >= '0' && args[i] <= '9'; i++) {
        scheme = scheme * 10 + (args[i] - '0');
    }
    if (scheme >= 0 && scheme < SCHEME_COUNT) {
        return scheme;
    }
    return 0;
}

void cmd_snow(char* args, int* row) {
    (void)row;
    int scheme = parse_scheme_snow(args);
    clear_screen();
    disable_cursor();
    init_snow(scheme);
    while (!is_interrupt_requested()) {
        render_frame_snow();
        sleep_ms(40);
    }
    clear_interrupt();
    clear_screen();
    enable_cursor(13, 15);
}

REGISTER_COMMAND("snow", cmd_snow, 1);