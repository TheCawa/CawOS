#include "commands.h"
#include "drivers/io.h"
#include "kernel/config.h"


void cmd_reboot(char* args, int* row) {
    config_set_shutdown_clean(1);
    port_byte_out(0x64, 0xFE);
}

REGISTER_COMMAND("reboot", cmd_reboot, 0);