#ifndef ATA_H
#define ATA_H

#include <stdint.h>

typedef enum {
    ATA_OK = 0,
    ATA_ERR_TIMEOUT,
    ATA_ERR_DEVICE,
} ata_status_t;

typedef struct {
    uint16_t io_base;
    uint8_t  slave; // 0 = master, 1 = slave
} ata_device_t;

extern ata_device_t ata_devices[4];

ata_status_t ata_identify(ata_device_t *dev, uint16_t *buffer);
ata_status_t ata_read28(ata_device_t *dev, uint32_t lba, uint8_t sector_count, uint8_t *buffer);
ata_status_t ata_write28(ata_device_t *dev, uint32_t lba, uint8_t sector_count, uint8_t *buffer);

#endif