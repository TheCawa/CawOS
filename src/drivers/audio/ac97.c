#include "drivers/ac97.h"
#include "drivers/pci.h"
#include "drivers/io.h"
#include "libc/util.h"
#include "kernel/memory.h"
#include <stdint.h>

#define AC97_MASTER_VOL     0x02
#define AC97_PCM_OUT_VOL    0x18
#define AC97_POWERDOWN      0x26
#define AC97_PO_BDBAR       0x10  // PCM Out Buffer Descriptor Base Address
#define AC97_PO_CIV         0x14  // Current Index Value
#define AC97_PO_LVI         0x15  // Last Valid Index
#define AC97_PO_SR          0x16  // Status Register
#define AC97_PO_CR          0x1B  // Control Register
#define AC97_GLOB_CNT       0x2C  // Global Control
#define AC97_GLOB_STA       0x30  // Global Status
#define AC97_CR_RPBM        (1 << 0)  // Run/Pause Bus Master
#define AC97_CR_RR          (1 << 1)  // Reset Registers
#define AC97_CR_LVBIE       (1 << 2)  // Last Valid Buffer Interrupt Enable
#define AC97_CR_FEIE        (1 << 3)  // FIFO Error Interrupt Enable
#define AC97_CR_IOCE        (1 << 4)  // Interrupt On Completion Enable

typedef struct {
    uint32_t addr;
    uint16_t samples;
    uint16_t flags;
} __attribute__((packed)) ac97_bd_t;

#define AC97_BD_IOC  (1 << 15)
#define AC97_BD_BUP  (1 << 14)

#define AC97_BDL_SIZE 32

static uint32_t s_nam_base = 0;
static uint32_t s_nabm_base = 0;
static ac97_bd_t s_bdl[AC97_BDL_SIZE] __attribute__((aligned(8)));
static uint8_t* s_buf[AC97_BDL_SIZE];
static uint8_t s_buf_static[AC97_BDL_SIZE][4096] __attribute__((aligned(4)));
static int using_static_buffers = 0;

int ac97_ready = 0;

static void nam_write16(uint8_t reg, uint16_t val) {
    port_word_out(s_nam_base + reg, val);
}

static uint16_t nabm_read8(uint8_t reg) {
    return port_byte_in(s_nabm_base + reg);
}

static void nabm_write8(uint8_t reg, uint8_t val) {
    port_byte_out(s_nabm_base + reg, val);
}

static void nabm_write16(uint8_t reg, uint16_t val) {
    port_word_out(s_nabm_base + reg, val);
}

static void nabm_write32(uint8_t reg, uint32_t val) {
    port_dword_out(s_nabm_base + reg, val);
}

static uint32_t nabm_read32(uint8_t reg) {
    return port_dword_in(s_nabm_base + reg);
}

int ac97_init() {
    pci_device_t* dev = pci_find_device(0x04, 0x01);
    if (!dev) return -1;
    uint16_t cmd = pci_config_read16(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= 0x0005;
    pci_config_write16(dev->bus, dev->slot, dev->func, 0x04, cmd);
    s_nam_base  = pci_read_bar(dev, 0) & ~0x3;
    s_nabm_base = pci_read_bar(dev, 1) & ~0x3;
    if (!s_nam_base || !s_nabm_base) return -1;
    nabm_write32(AC97_GLOB_CNT, 0x00000002);
    for (volatile int i = 0; i < 100000; i++);
    int timeout = 1000000;
    while (!(nabm_read32(AC97_GLOB_STA) & 0x100) && timeout--)
        __asm__ volatile("pause");
    if (timeout <= 0) return -1;
    nabm_write8(AC97_PO_CR, AC97_CR_RR);
    for (volatile int i = 0; i < 10000; i++);
    nam_write16(AC97_MASTER_VOL, 0x0000);
    nam_write16(AC97_PCM_OUT_VOL, 0x0000);
    int malloc_failed = 0;
    for (int i = 0; i < AC97_BDL_SIZE; i++) {
        s_buf[i] = (uint8_t*)malloc(4096);
        if (!s_buf[i]) {
            malloc_failed = 1;
            break;
        }
        memset(s_buf[i], 0, 4096);
    }
    if (malloc_failed) {
        for (int i = 0; i < AC97_BDL_SIZE; i++) {
            if (s_buf[i]) {
                free(s_buf[i]);
                s_buf[i] = NULL;
            }
        }
        for (int i = 0; i < AC97_BDL_SIZE; i++) {
            s_buf[i] = s_buf_static[i];
            memset(s_buf[i], 0, 4096);
        }
        using_static_buffers = 1;
    }
    memset(s_bdl, 0, sizeof(s_bdl));
    for (int i = 0; i < AC97_BDL_SIZE; i++) {
        s_bdl[i].addr    = (uint32_t)(uintptr_t)s_buf[i];
        s_bdl[i].samples = 4096 / 2;
        s_bdl[i].flags   = AC97_BD_IOC;
    }
    nabm_write32(AC97_PO_BDBAR, (uint32_t)(uintptr_t)s_bdl);
    nabm_write8(AC97_PO_LVI, AC97_BDL_SIZE - 1);
    ac97_ready = 1;
    return 0;
}

void ac97_play_pcm(uint8_t* data, uint32_t size) {
    if (!ac97_ready) return;
    uint32_t offset = 0;
    uint8_t idx = 0;
    memset(s_bdl, 0, sizeof(s_bdl));
    while (offset < size && idx < AC97_BDL_SIZE) {
        uint32_t chunk = size - offset;
        if (chunk > 4096) chunk = 4096;
        memcpy(s_buf[idx], data + offset, chunk);
        s_bdl[idx].addr    = (uint32_t)(uintptr_t)s_buf[idx];
        s_bdl[idx].samples = chunk / 2;
        if (offset + chunk >= size) {
            s_bdl[idx].flags = AC97_BD_IOC;
        } else {
            s_bdl[idx].flags = 0;
        }

        offset += chunk;
        idx++;
    }

    nabm_write8(AC97_PO_CR, 0);
    nabm_write32(AC97_PO_BDBAR, (uint32_t)(uintptr_t)s_bdl);
    nabm_write8(AC97_PO_LVI, idx - 1);
    nabm_write16(AC97_PO_SR, 0x1E);
    nabm_write8(AC97_PO_CR, AC97_CR_RPBM | AC97_CR_IOCE);
}

void ac97_stop() {
    if (!ac97_ready) return;
    nabm_write8(AC97_PO_CR, 0);
}