#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"

void cmd_font(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        const font_t* f = font_get_current();
        char msg[128];
        snprintf(msg, sizeof(msg), "Current font: %s (%dx%d, %d glyphs)",
                 (f->name && f->name[0]) ? f->name : "unknown",
                 (int)f->width, (int)f->height, (int)f->num_glyphs);
        print_line_scroll(msg, 0, row, 0x0A);
        print_line_scroll("Usage: font <8x8 | 8x16 | filename.psf>", 0, row, 0x0E);
        return;
    }
    
    if (strcasecmp(args, "8x8") == 0) {
        font_set_current(font_get_8x8());
        screen_set_font_scale(1, 1);
        font_save_config("8x8");
        *row = 0;
        print_line_scroll("Font set to 8x8 (builtin).", 0, row, 0x0A);
    }
    else if (strcasecmp(args, "8x16") == 0) {
        font_set_current(font_get_8x16());
        screen_set_font_scale(1, 1);
        font_save_config("8x16");
        *row = 0;
        print_line_scroll("Font set to 8x16 (builtin).", 0, row, 0x0A);
    }
    else {
        char path[64];
        snprintf(path, sizeof(path), "/core/res/fonts/%s", args);
        if (font_load_psf(path)) {
            screen_set_font_scale(1, 1);
            font_save_config(args);
            *row = 0;
            char msg[128];
            snprintf(msg, sizeof(msg), "Font loaded: %s", args);
            print_line_scroll(msg, 0, row, 0x0A);
        } else {
            print_line_scroll("Error: Could not load font file.", 0, row, 0x0C);
        }
    }
}
REGISTER_COMMAND("font", cmd_font, 1);
