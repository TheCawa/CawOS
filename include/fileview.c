#include "kernel/syscall.h"

void _start() {
    sys_print("=================================\n");
    sys_print("      FILE VIEWER TEST\n");
    sys_print("=================================\n");
    sys_print("\n");
    sys_print("Opening file 'test'...\n");
    int fd = sys_open("test");
    if (fd < 0) {
        sys_print("Error: Could not open file!\n");
        sys_exit();
    }
    sys_print("File opened successfully!\n");
    sys_print("Reading content...\n");
    sys_print("\n--- FILE CONTENT ---\n");
    char buffer[256];
    int bytes_read = sys_read(fd, buffer, 255);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        sys_print(buffer);
    } else {
        sys_print("(empty file or read error)");
    }
    sys_print("\n--- END OF FILE ---\n");
    sys_print("\n");
    sys_close(fd);
    sys_print("File closed.\n");
    sys_exit();
    while(1);
}
