#include "framebuffer.h"
#include "serial_port.h"

void kmain()
{
    char msg[] = "NOS";

    serial_write(0x3F8, msg, 4);
    fb_write(msg, 4);
}