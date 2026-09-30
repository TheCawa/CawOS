#ifndef DESKTOP_H
#define DESKTOP_H
extern volatile int g_desktop_exit_requested;
extern volatile int g_desktop_exit_reason;

void desktop_damage_rect(int x, int y, int w, int h);
void desktop_damage_taskbar(void);
void desktop_init();
void desktop_run();

#endif