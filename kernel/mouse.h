#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// PS/2 Mouse Driver
// IRQ12 = vector 44
// ---------------------------------------------------------------------------

#define MOUSE_PORT_DATA 0x60
#define MOUSE_PORT_CMD 0x64

static volatile int mouse_x = 400;
static volatile int mouse_y = 300;
static volatile int mouse_left = 0;
static volatile int mouse_right = 0;

static int mouse_cycle = 0;
static uint8_t mouse_packet[3];

// Bounds for mouse (set to framebuffer size in gui_init)
static int mouse_max_x = 800;
static int mouse_max_y = 600;

static void mouse_wait_write(void)
{
    int timeout = 100000;
    while (timeout-- && (inb(0x64) & 0x02))
        ;
}

static void mouse_wait_read(void)
{
    int timeout = 100000;
    while (timeout-- && !(inb(0x64) & 0x01))
        ;
}

static void mouse_write(uint8_t val)
{
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read(void)
{
    mouse_wait_read();
    return inb(0x60);
}

static void mouse_init(int max_x, int max_y)
{
    mouse_max_x = max_x;
    mouse_max_y = max_y;
    mouse_x = max_x / 2;
    mouse_y = max_y / 2;

    // Enable auxiliary device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // Enable interrupts for mouse (IRQ12)
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t status = inb(0x60) | 2;
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    // Set defaults
    mouse_write(0xF6);
    mouse_read(); // ACK

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read(); // ACK
}

// Call from IRQ12 handler (vector 44)
static void mouse_handle_irq(void)
{
    uint8_t data = inb(MOUSE_PORT_DATA);

    switch (mouse_cycle)
    {
    case 0:
        // First byte: flags
        if (!(data & 0x08))
            return; // bit 3 must be set
        mouse_packet[0] = data;
        mouse_cycle++;
        break;
    case 1:
        mouse_packet[1] = data;
        mouse_cycle++;
        break;
    case 2:
        mouse_packet[2] = data;
        mouse_cycle = 0;

        // Decode packet
        uint8_t flags = mouse_packet[0];
        int dx = (int)mouse_packet[1] - ((flags & 0x10) ? 256 : 0);
        int dy = (int)mouse_packet[2] - ((flags & 0x20) ? 256 : 0);

        mouse_x += dx;
        mouse_y -= dy; // Y is inverted

        if (mouse_x < 0)
            mouse_x = 0;
        if (mouse_x >= mouse_max_x)
            mouse_x = mouse_max_x - 1;
        if (mouse_y < 0)
            mouse_y = 0;
        if (mouse_y >= mouse_max_y)
            mouse_y = mouse_max_y - 1;

        mouse_left = (flags & 0x01) ? 1 : 0;
        mouse_right = (flags & 0x02) ? 1 : 0;
        break;
    }
}