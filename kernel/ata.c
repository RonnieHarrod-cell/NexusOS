#include "ata.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// ATA PIO driver — primary bus, master drive
// ---------------------------------------------------------------------------

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30

#define ATA_SR_BSY 0x80  // busy
#define ATA_SR_DRDY 0x40 // drive ready
#define ATA_SR_DRQ 0x08  // data request
#define ATA_SR_ERR 0x01  // error

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Wait until BSY clears, then return status.
// Returns -1 if ERR is set.
static int ata_wait(void)
{
    uint8_t status;
    int timeout = 10000000; // much larger for real hardware
    do
    {
        status = inb(0x1F7);
        timeout--;
    } while ((status & ATA_SR_BSY) && timeout > 0);

    if (timeout == 0)
        return -1;

    if (status & ATA_SR_ERR)
        return -1;

    return 0;
}

// Wait until DRQ (data ready) is set.
static int ata_wait_drq(void)
{
    uint8_t status;
    do
    {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR)
            return -1;
    } while (!(status & ATA_SR_DRQ));
    return 0;
}

// Send an LBA28 command to the primary master drive.
static void ata_send_command(uint32_t lba, uint8_t count, uint8_t cmd)
{
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F)); // LBA mode, master
    outb(ATA_ERROR, 0x00);
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LOW, (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, cmd);
}

int ata_read(uint32_t lba, uint8_t count, void *buf)
{
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 1000000; i++)
        ; // longer delay for real hardware

    if (ata_wait() < 0)
        return -1;

    ata_send_command(lba, count, ATA_CMD_READ);

    uint16_t *ptr = (uint16_t *)buf;

    for (int s = 0; s < count; s++)
    {
        if (ata_wait_drq() < 0)
            return -1;
        // Read 256 16-bit words per sector (= 512 bytes)
        for (int i = 0; i < 256; i++)
            ptr[i] = inw(ATA_DATA);
        ptr += 256;
    }

    return 0;
}

int ata_write(uint32_t lba, uint8_t count, const void *buf)
{
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F));
    for (volatile int i = 0; i < 1000000; i++)
        ; // longer delay for real hardware

    if (ata_wait() < 0)
        return -1;

    ata_send_command(lba, count, ATA_CMD_WRITE);

    const uint16_t *ptr = (const uint16_t *)buf;

    for (int s = 0; s < count; s++)
    {
        if (ata_wait_drq() < 0)
            return -1;
        // Write 256 16-bit words per sector
        for (int i = 0; i < 256; i++)
            outw(ATA_DATA, ptr[i]);
        ptr += 256;
    }

    // Flush write cache
    outb(ATA_COMMAND, 0xE7);
    ata_wait();

    return 0;
}