#include "commands.h"
#include "drivers/io.h"


void cmd_reboot(char* args, int* row) {
    port_byte_out(0x64, 0xFE);
}

REGISTER_COMMAND("reboot", cmd_reboot, 0);