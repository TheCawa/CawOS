#include "ata.h"
#include "io.h"
#include "screen.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_SECONDARY_IO    0x170

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE       0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DF           0x20
#define ATA_SR_DRQ          0x08
#define ATA_SR_CORR         0x04
#define ATA_SR_IDX          0x02
#define ATA_SR_ERR          0x01

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

static int ata_wait_busy(uint16_t io_base) {
    uint32_t timeout = 10000000;
    while ((port_byte_in(io_base + ATA_REG_STATUS) & ATA_SR_BSY) && timeout--)
        ;
    return timeout > 0 ? 1 : 0;
}

static void ata_select_drive(uint8_t dev_id, uint32_t lba) {
    uint16_t io_base = (dev_id < 2) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    uint8_t slave = (dev_id % 2);
    uint8_t drive_head = 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F);
    
    port_byte_out(io_base + ATA_REG_DRIVE, drive_head);
    for(int i=0; i<4; i++) port_byte_in(io_base + ATA_REG_STATUS);
}

void ata_init() {
    uint8_t status = port_byte_in(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0xFF) {
    } else {
    }
}

ata_status_t ata_read_sectors(uint8_t dev_id, uint32_t lba, uint8_t count, uint8_t* buffer) {
    if (count == 0) return ATA_OK;
    
    uint16_t io_base = (dev_id < 2) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;

    ata_wait_busy(io_base);
    ata_select_drive(dev_id, lba);

    port_byte_out(io_base + ATA_REG_SECCOUNT, count);
    port_byte_out(io_base + ATA_REG_LBA_LO, (uint8_t)(lba));
    port_byte_out(io_base + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    port_byte_out(io_base + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));

    port_byte_out(io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (int s = 0; s < count; s++) {
        uint32_t timeout = 100000;
        while (timeout--) {
            uint8_t status = port_byte_in(io_base + ATA_REG_STATUS);
            if (status & ATA_SR_ERR) return ATA_ERR_DEVICE;
            if (status & ATA_SR_DRQ) break;
        }
        if (timeout == 0) return ATA_ERR_TIMEOUT;
        for (int i = 0; i < 256; i++) {
            uint16_t data = port_word_in(io_base + ATA_REG_DATA);
            *buffer++ = (uint8_t)(data & 0xFF);
            *buffer++ = (uint8_t)((data >> 8) & 0xFF);
        }
    }
    return ATA_OK;
}

ata_status_t ata_write_sectors(uint8_t dev_id, uint32_t lba, uint8_t count, const uint8_t* buffer) {
    if (count == 0) return ATA_OK;

    uint16_t io_base = (dev_id < 2) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;

    ata_wait_busy(io_base);
    ata_select_drive(dev_id, lba);

    port_byte_out(io_base + ATA_REG_SECCOUNT, count);
    port_byte_out(io_base + ATA_REG_LBA_LO, (uint8_t)(lba));
    port_byte_out(io_base + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    port_byte_out(io_base + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));

    port_byte_out(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (int s = 0; s < count; s++) {
        uint32_t timeout = 100000;
        while (timeout--) {
            uint8_t status = port_byte_in(io_base + ATA_REG_STATUS);
            if (status & ATA_SR_ERR) return ATA_ERR_DEVICE;
            if (status & ATA_SR_DRQ) break;
        }
        if (timeout == 0) return ATA_ERR_TIMEOUT;
        for (int i = 0; i < 256; i++) {
            uint16_t data = *buffer++;
            data |= (*buffer++ << 8);
            port_word_out(io_base + ATA_REG_DATA, data);
        }
    }

    port_byte_out(io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(io_base);

    return ATA_OK;
}