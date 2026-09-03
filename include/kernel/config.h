#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

void config_init();
int config_was_shutdown_clean();
void config_set_shutdown_clean(int clean);

#endif
