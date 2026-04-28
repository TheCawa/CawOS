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

void ata_init();

ata_status_t ata_read_sectors(uint8_t dev_id, uint32_t lba, uint8_t count, uint8_t* buffer);
ata_status_t ata_write_sectors(uint8_t dev_id, uint32_t lba, uint8_t count, const uint8_t* buffer);

#endif