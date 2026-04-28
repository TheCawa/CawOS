#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

#define MAX_CPU_COUNT 16
#define PACKED __attribute__((packed))

extern uint32_t g_acpiCpuCount;
extern uint8_t g_acpiCpuIds[MAX_CPU_COUNT];
extern uint8_t* g_localApicAddr;
extern uint8_t* g_ioApicAddr;

void acpi_init();
uint32_t acpi_remap_irq(uint32_t irq);
void acpi_shutdown();

#endif