#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_VENDOR_ID   0x00
#define PCI_DEVICE_ID   0x02
#define PCI_COMMAND     0x04
#define PCI_STATUS      0x06
#define PCI_PROG_IF     0x09
#define PCI_CLASS       0x0B
#define PCI_SUBCLASS    0x0A
#define PCI_HEADER_TYPE 0x0E
#define PCI_BAR0        0x10
#define PCI_BAR1        0x14
#define PCI_BAR2        0x18
#define PCI_BAR3        0x1C
#define PCI_BAR4        0x20
#define PCI_BAR5        0x24
#define PCI_INTERRUPT_LINE 0x3C
#define MAX_PCI_DEVICES 32

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
} pci_device_t;

extern pci_device_t pci_devices[MAX_PCI_DEVICES];
extern int pci_device_count;

void pci_init();
uint32_t pci_read_bar(pci_device_t* dev, int bar_index);
pci_device_t* pci_find_device(uint8_t class_code, uint8_t subclass);
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data);


#endif