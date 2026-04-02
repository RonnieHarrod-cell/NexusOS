#pragma once
#include <stdint.h>

// Read `count` 512-byte sectors starting at LBA `lba` into `buf`.
// Returns 0 on success, -1 on error.
int ata_read(uint32_t lba, uint8_t count, void *buf);

// Write `count` 512-byte sectors starting at LBA `lba` from `buf`.
// Returns 0 on success, -1 on error.
int ata_write(uint32_t lba, uint8_t count, const void *buf);