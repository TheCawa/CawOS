#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"

void cmd_font(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: font <small | normal | large>", 0, row, 0x0E);
        return;
    }
    if (strcasecmp(args, "small") == 0) {
        screen_set_font_scale(5, 4, 5, 4);
        *row = 0;
        print_line_scroll("Font set to SMALL. Grid re-calculated.", 0, row, 0x0A);
    } 
    else if (strcasecmp(args, "normal") == 0) {
        screen_set_font_scale(3, 2, 7, 4);
        *row = 0;
        print_line_scroll("Font set to NORMAL. Grid re-calculated.", 0, row, 0x0A);
    } 
    else if (strcasecmp(args, "large") == 0) {
        screen_set_font_scale(2, 1, 2, 1);
        *row = 0;
        print_line_scroll("Font set to LARGE. Grid re-calculated.", 0, row, 0x0A);
    } 
    else {
        print_line_scroll("Unknown size! Use: small, normal or large.", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("font", cmd_font, 1);