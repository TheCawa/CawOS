#ifndef KEYBOARD_MAP_H
#define KEYBOARD_MAP_H

#define BACKSPACE 0x0E
#define ENTER 0x1C

const char ascii_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 0x00 - 0x09 */
    '9', '0', '-', '=', BACKSPACE, '\t',                /* 0x0A - 0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',   /* 0x10 - 0x19 */
    '[', ']', ENTER, 0,                                 /* 0x1A - 0x1D (1D - Ctrl) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x1E - 0x27 */
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',   /* 0x28 - 0x31 (2A - LShift) */
    'm', ',', '.', '/', 0,                              /* 0x32 - 0x36 (36 - RShift) */
    '*', 0, ' ', 0,                                     /* 0x37 - 0x3A (38 - Alt, 3A - Caps) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       /* 0x3B - 0x44 (F1-F10) */
    0, 0,                                               /* 0x45 - 0x46 (NumLock, ScrollLock) */
    '7', '8', '9', '-',                                 /* 0x47 - 0x4A (Numpad 7,8,9, minus) */
    '4', '5', '6', '+',                                 /* 0x4B - 0x4E (Numpad 4,5,6, plus) */
    '1', '2', '3',                                      /* 0x4F - 0x51 (Numpad 1,2,3) */
    '0', '.',                                           /* 0x52 - 0x53 (Numpad 0, dot) */
    0, 0, 0, 0, 0, 0                                    
};

#endif