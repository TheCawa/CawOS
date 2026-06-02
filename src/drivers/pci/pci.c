#include "drivers/pci.h"
#include "drivers/io.h"

pci_device_t pci_devices[MAX_PCI_DEVICES];
int pci_device_count = 0;

static uint32_t pci_make_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    port_dword_out(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    return port_dword_in(PCI_CONFIG_DATA);
}

void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data) {
    uint32_t addr = pci_make_addr(bus, slot, func, offset);
    port_dword_out(PCI_CONFIG_ADDR, addr);
    port_word_out(PCI_CONFIG_DATA + (offset & 2), data);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t data = pci_config_read32(bus, slot, func, offset);
    return (uint16_t)(data >> ((offset & 2) * 8));
}

static uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t data = pci_config_read32(bus, slot, func, offset);
    return (uint8_t)(data >> ((offset & 3) * 8));
}

uint32_t pci_read_bar(pci_device_t* dev, int bar_index) {
    uint8_t offset = PCI_BAR0 + (bar_index * 4);
    return pci_config_read32(dev->bus, dev->slot, dev->func, offset);
}

static int pci_check_device(uint8_t bus, uint8_t slot, uint8_t func) {
    return (pci_config_read16(bus, slot, func, PCI_VENDOR_ID) != 0xFFFF);
}

static void pci_scan_bus(uint8_t bus_num) {
    for (int slot = 0; slot < 32; slot++) {
        if (!pci_check_device(bus_num, slot, 0)) continue;
        uint8_t header_type = pci_config_read8(bus_num, slot, 0, PCI_HEADER_TYPE);
        uint8_t num_funcs = (header_type & 0x80) ? 8 : 1;
        for (int func = 0; func < num_funcs; func++) {
            if (!pci_check_device(bus_num, slot, func)) continue;
            if (pci_device_count >= MAX_PCI_DEVICES) return;
            pci_device_t* dev = &pci_devices[pci_device_count]; 
            dev->bus = bus_num;
            dev->slot = slot;
            dev->func = func;
            dev->vendor_id = pci_config_read16(bus_num, slot, func, PCI_VENDOR_ID);
            dev->device_id = pci_config_read16(bus_num, slot, func, PCI_DEVICE_ID);
            dev->class_code = pci_config_read8(bus_num, slot, func, PCI_CLASS);
            dev->subclass = pci_config_read8(bus_num, slot, func, PCI_SUBCLASS);
            dev->prog_if = pci_config_read8(bus_num, slot, func, PCI_PROG_IF);
            dev->header_type = header_type;
            pci_device_count++;
        }
    }
}

pci_device_t* pci_find_device(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && 
            pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return 0;
}

void pci_init() {
    pci_scan_bus(0);
    for(int i=1; i<256; i++) pci_scan_bus(i);
}