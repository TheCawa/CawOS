#ifndef DESKTOP_H
#define DESKTOP_H

extern volatile int g_desktop_exit_requested;

void desktop_init();
void desktop_run();

#endif
