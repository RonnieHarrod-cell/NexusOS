#pragma once
#include <stdint.h>
#include "io.h"

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

static volatile int mouse_ready = 0;

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
    mouse_ready = 0;

    // flush any pending data
    while (inb(0x64) & 0x01)
        inb(0x60);

    // disable both devices during init
    outb(0x64, 0xAD); // disable keyboard
    for (volatile int i = 0; i < 10000; i++)
        ;
    outb(0x64, 0xA7); // disable mouse
    for (volatile int i = 0; i < 10000; i++)
        ;

    // flush again
    while (inb(0x64) & 0x01)
        inb(0x60);

    // read and modify controller config
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t config = inb(0x60);
    config |= 0x01;  // enable keyboard interrupt
    config |= 0x02;  // enable mouse interrupt
    config &= ~0x20; // clear mouse disable bit
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, config);

    // re-enable keyboard
    outb(0x64, 0xAE);
    for (volatile int i = 0; i < 10000; i++)
        ;

    // re-enable mouse
    outb(0x64, 0xA8);
    for (volatile int i = 0; i < 10000; i++)
        ;

    // reset mouse
    mouse_write(0xFF);
    mouse_wait_read();
    uint8_t ack = inb(0x60); // should be 0xFA
    if (ack != 0xFA)
        return; // no mouse present
    mouse_wait_read();
    inb(0x60); // 0xAA (self test passed)
    mouse_wait_read();
    inb(0x60); // 0x00 (mouse ID)

    // set defaults
    mouse_write(0xF6);
    mouse_wait_read();
    inb(0x60); // ACK

    // set sample rate to 40
    mouse_write(0xF3);
    mouse_wait_read();
    inb(0x60);
    mouse_write(40);
    mouse_wait_read();
    inb(0x60);

    // set resolution to 4 couts/mm
    mouse_write(0xE8);
    mouse_wait_read(); inb(0x60); // ACK
    mouse_write(0x02);
    mouse_wait_read(); inb(0x60); //ACK

    // set scaling to 1:1
    mouse_write(0xE6);
    mouse_wait_read(); inb(0x60); // ACK

    // Enable data reporting
    mouse_write(0xF4);
    mouse_wait_read();
    inb(0x60); // ACK

    mouse_ready = 1;
}

// Call from IRQ12 handler (vector 44)
__attribute__((no_caller_saved_registers)) static void mouse_handle_irq(void)
{
    if (!mouse_ready)
    {
        inb(0x60);
        return;
    }

    uint8_t status = inb(0x64);

    if (!(status & 0x20))
        return;

    uint8_t data = inb(MOUSE_PORT_DATA);

    // temporary: tack if getting any data
    static int packet_count = 0;
    packet_count++;

    switch (mouse_cycle)
    {
    case 0:
        // First byte: flags
        if (!(data & 0x08))
        {
            mouse_cycle = 0;
            return; // bit 3 must be set
        }
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