#include "io.h"
#include "util.h"
#include "screen.h"
#include <stdint.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_SECONDARY_IO    0x170

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_SECCOUNT0   0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07
#define ATA_REG_STATUS      0x07

// Status bits
#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_RDY  0x40
#define ATA_SR_BSY  0x80

// Commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_IDENTIFY        0xEC

#define ATA_TIMEOUT 1000000

typedef enum {
    ATA_OK = 0,
    ATA_ERR_TIMEOUT,
    ATA_ERR_DEVICE,
} ata_status_t;

typedef struct {
    uint16_t io_base;
    uint8_t  slave; // 0 = master, 1 = slave
} ata_device_t;

ata_device_t ata_devices[4] = {
    { ATA_PRIMARY_IO,   0 },
    { ATA_PRIMARY_IO,   1 },
    { ATA_SECONDARY_IO, 0 },
    { ATA_SECONDARY_IO, 1 }
};

static void ata_delay400(uint16_t io) {
    for (int i = 0; i < 4; i++)
        port_byte_in(io + ATA_REG_STATUS);
}

static ata_status_t ata_poll(uint16_t io) {
    uint32_t timeout = ATA_TIMEOUT;

    while ((port_byte_in(io + ATA_REG_STATUS) & ATA_SR_BSY) && timeout--)
        ;
    if (!timeout) return ATA_ERR_TIMEOUT;

    uint8_t status = port_byte_in(io + ATA_REG_STATUS);

    if (status & ATA_SR_ERR) return ATA_ERR_DEVICE;
    if (!(status & ATA_SR_DRQ)) return ATA_ERR_DEVICE;

    return ATA_OK;
}

static void ata_select_drive(ata_device_t *dev, uint32_t lba) {
    port_byte_out(dev->io_base + ATA_REG_HDDEVSEL,
                  0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 15; i++)
        port_byte_in(dev->io_base + ATA_REG_STATUS);
}

ata_status_t ata_identify(ata_device_t *dev, uint16_t *buffer) {

    port_byte_out(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | (dev->slave << 4));
    ata_delay400(dev->io_base);

    port_byte_out(dev->io_base + ATA_REG_SECCOUNT0, 0);
    port_byte_out(dev->io_base + ATA_REG_LBA0, 0);
    port_byte_out(dev->io_base + ATA_REG_LBA1, 0);
    port_byte_out(dev->io_base + ATA_REG_LBA2, 0);

    port_byte_out(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (port_byte_in(dev->io_base + ATA_REG_STATUS) == 0)
        return ATA_ERR_DEVICE;

    if (ata_poll(dev->io_base) != ATA_OK)
        return ATA_ERR_DEVICE;

    for (int i = 0; i < 256; i++)
        buffer[i] = port_word_in(dev->io_base + ATA_REG_DATA);

    return ATA_OK;
}

ata_status_t ata_read28(ata_device_t *dev, uint32_t lba,
                        uint8_t sector_count, uint8_t *buffer) {

    if (lba >= 0x0FFFFFFF)
        return ATA_ERR_DEVICE;

    uint32_t timeout = ATA_TIMEOUT;
    while ((port_byte_in(dev->io_base + ATA_REG_STATUS) & ATA_SR_BSY) && timeout--)
        ;

    ata_select_drive(dev, lba);

    port_byte_out(dev->io_base + ATA_REG_SECCOUNT0, sector_count);
    port_byte_out(dev->io_base + ATA_REG_LBA0, (uint8_t)lba);
    port_byte_out(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    port_byte_out(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    port_byte_out(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (int s = 0; s < sector_count; s++) {

        if (ata_poll(dev->io_base) != ATA_OK)
            return ATA_ERR_DEVICE;

        for (int i = 0; i < 256; i++) {
            uint16_t data = port_word_in(dev->io_base + ATA_REG_DATA);
            *buffer++ = data & 0xFF;
            *buffer++ = data >> 8;
        }
    }

    return ATA_OK;
}

ata_status_t ata_write28(ata_device_t *dev, uint32_t lba,
                         uint8_t sector_count, uint8_t *buffer) {

    if (lba >= 0x0FFFFFFF)
        return ATA_ERR_DEVICE;

    while (port_byte_in(dev->io_base + 0x07) & 0x80);
    port_byte_out(dev->io_base + 0x06, 0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F));
    port_byte_out(dev->io_base + 0x01, 0x00);
    port_byte_out(dev->io_base + 0x02, sector_count);
    port_byte_out(dev->io_base + 0x03, (uint8_t)lba);
    port_byte_out(dev->io_base + 0x04, (uint8_t)(lba >> 8));
    port_byte_out(dev->io_base + 0x05, (uint8_t)(lba >> 16));
    port_byte_out(dev->io_base + 0x07, 0x30); // WRITE PIO

    for (int s = 0; s < sector_count; s++) {
        while (1) {
            uint8_t st = port_byte_in(dev->io_base + 0x07);
            if (st & 0x01) return ATA_ERR_DEVICE; // ERR
            if (st & 0x08) break;                 // DRQ done
        }

        for (int i = 0; i < 256; i++) {
            uint16_t data = buffer[0] | ((uint16_t)buffer[1] << 8);
            port_word_out(dev->io_base + 0x00, data);
            buffer += 2;
        }
    }

    // CACHE FLUSH
    port_byte_out(dev->io_base + 0x07, 0xE7);
    while (port_byte_in(dev->io_base + 0x07) & 0x80);

    return ATA_OK;
}
