#ifndef START_MENU_H
#define START_MENU_H

#include <stdint.h>
#include <stddef.h>

#define START_MENU_WIDTH    200
#define START_MENU_HEIGHT   250
#define MENU_ITEM_HEIGHT    20
#define MENU_ICON_SIZE      16
#define MENU_TEXT_OFFSET    28

#define COLOR_MENU_BG       0x00C0C0C0
#define COLOR_MENU_HILITE   0x00000080
#define COLOR_MENU_TEXT     0x00000000
#define COLOR_MENU_GRAY     0x00808080

typedef enum {
    MENU_ITEM_NORMAL,
    MENU_ITEM_SEPARATOR,
    MENU_ITEM_SUBMENU,
    MENU_ITEM_DISABLED
} menu_item_type_t;

typedef struct {
    const char* text;
    menu_item_type_t type;
    uint32_t icon_color;
    void (*callback)(void);
} menu_item_t;

void start_menu_init(void);
void start_menu_draw(int x, int y);
void start_menu_handle_click(int x, int y);
void start_menu_handle_hover(int x, int y);
int start_menu_hit_test(int x, int y);
void start_menu_navigate_down(void);
void start_menu_navigate_up(void);
void start_menu_activate_selected(void);
int start_menu_get_hovered(void);
void start_menu_handle_key(unsigned char scancode);
void menu_run_programs(void);
void menu_run_documents(void);
void menu_run_settings(void);
void menu_run_find(void);
void menu_run_help(void);
void menu_run_run(void);
void menu_shutdown(void);

#endif