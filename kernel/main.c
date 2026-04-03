#include <stdint.h>
#include "limine.h"
#include "font.h"
#include "auth.h"
#include "splash.h"

volatile char auth_key = 0;
volatile int auth_key_ready = 0;

void shell_handle_key(char c);

void *memset(void *s, int c, __SIZE_TYPE__ n)
{
    uint8_t *p = s;
    while (n--)
        *p++ = (uint8_t)c;
    return s;
}

void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

// ---------------------------------------------------------------------------
// Framebuffer globals
// ---------------------------------------------------------------------------
static uint32_t *fb;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;

static int cursor_x = 0;
static int cursor_y = 0;

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

// ---------------------------------------------------------------------------
// Port I/O
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// IDT
// ---------------------------------------------------------------------------
struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];

static void set_idt_entry(int vec, void *handler)
{
    uint64_t addr = (uint64_t)handler;
    idt[vec].offset_low = addr & 0xFFFF;
    idt[vec].selector = 0x28; // Limine sets up GDT: 0x28 = 64-bit code segment
    idt[vec].ist = 0;
    idt[vec].type_attr = 0x8E; // present, DPL=0, interrupt gate
    idt[vec].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero = 0;
}

static void load_idt(void)
{
    struct idt_ptr p;
    p.limit = sizeof(idt) - 1;
    p.base = (uint64_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(p));
}

// ---------------------------------------------------------------------------
// PIC remap  (IRQ 0-15 → vectors 32-47)
// Mask everything except IRQ1 (keyboard)
// ---------------------------------------------------------------------------
static void pic_init(void)
{
    // Start init sequence (cascade mode)
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // Vector offsets
    outb(0x21, 0x20); // master: IRQ0 → vector 32
    outb(0xA1, 0x28); // slave:  IRQ8 → vector 40
    // Wiring
    outb(0x21, 0x04); // master: slave on IRQ2
    outb(0xA1, 0x02); // slave:  cascade identity
    // 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    // Mask all IRQs except IRQ1 (keyboard) on master; mask all on slave
    outb(0x21, 0xFD); // 1111 1101
    outb(0xA1, 0xFF);
}

// ---------------------------------------------------------------------------
// Scancode → ASCII  (US QWERTY, set 1, make codes only)
// ---------------------------------------------------------------------------
static const char scancode_map[128] = {
    0,
    27,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8', //  0- 9
    '9',
    '0',
    '-',
    '=',
    '\b',
    '\t',
    'q',
    'w',
    'e',
    'r', // 10-19
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0, // 20-29
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';', // 30-39
    '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n', // 40-49
    'm',
    ',',
    '.',
    '/',
    0,
    '*',
    0,
    ' ',
    0, // 50-58
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void terminal_putchar(char c);

// ---------------------------------------------------------------------------
// Interrupt handlers
// The __attribute__((interrupt)) keyword makes the compiler:
//   - save/restore all needed registers
//   - use IRETQ instead of RET
//   - correctly align the stack
// ---------------------------------------------------------------------------

// Struct the compiler passes to __attribute__((interrupt)) handlers
struct interrupt_frame
{
    uint64_t ip;
    uint64_t cs;
    uint64_t flags;
    uint64_t sp;
    uint64_t ss;
};

// Generic stub for all unhandled vectors — just return
__attribute__((interrupt)) static void isr_stub(struct interrupt_frame *frame)
{
    (void)frame;
}

// Keyboard handler (IRQ1 → vector 33)
__attribute__((interrupt)) static void keyboard_isr(struct interrupt_frame *frame)
{
    (void)frame;

    uint8_t scancode = inb(0x60);

    // Bit 7 set = key release, ignore it
    if (!(scancode & 0x80))
    {
        if (scancode < sizeof(scancode_map))
        {
            char c = scancode_map[scancode];
            if (c)
            {
                auth_key = c;
                auth_key_ready = 1;
            }
        }
    }

    // Send End-Of-Interrupt to master PIC
    outb(0x20, 0x20);
}

// ---------------------------------------------------------------------------
// Limine requests
// ---------------------------------------------------------------------------
__attribute__((used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void put_pixel(int x, int y, uint32_t color)
{
    fb[y * (fb_pitch / 4) + x] = color;
}

static void draw_char(int x, int y, char c, uint32_t color)
{
    if (c < 32 || c > 126)
        return;
    for (int row = 0; row < 8; row++)
    {
        uint8_t line = font[c - 32][row];
        for (int col = 0; col < 8; col++)
        {
            if (line & (1 << (7 - col)))
                put_pixel(x + col, y + row, color);
        }
    }
}

// ---------------------------------------------------------------------------
// Terminal
// ---------------------------------------------------------------------------
static void scroll(void)
{
    // Copy rows upward (top-to-bottom to avoid overwriting source)
    for (int y = 0; y < (int)(fb_height - CHAR_HEIGHT); y++)
        for (int x = 0; x < (int)fb_width; x++)
            fb[y * (fb_pitch / 4) + x] = fb[(y + CHAR_HEIGHT) * (fb_pitch / 4) + x];

    // Clear the bottom line
    for (int y = (int)(fb_height - CHAR_HEIGHT); y < (int)fb_height; y++)
        for (int x = 0; x < (int)fb_width; x++)
            fb[y * (fb_pitch / 4) + x] = 0x000000;
}

void terminal_putchar(char c)
{
    if (c == '\b')
    {
        // Move cursor back one character and erase
        if (cursor_x >= CHAR_WIDTH)
        {
            cursor_x -= CHAR_WIDTH;
        }
        else if (cursor_y >= CHAR_HEIGHT)
        {
            cursor_y -= CHAR_HEIGHT;
            cursor_x = (int)fb_width - CHAR_WIDTH;
        }
        // Draw a black square to erase the character
        for (int row = 0; row < CHAR_HEIGHT; row++)
            for (int col = 0; col < CHAR_WIDTH; col++)
                put_pixel(cursor_x + col, cursor_y + row, 0x000000);
        return;
    }
    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y += CHAR_HEIGHT;
    }
    else
    {
        if (c >= 'a' && c <= 'z')
        {
            c -= 32;
        }
        draw_char(cursor_x, cursor_y, c, 0xFFFFFF);
        cursor_x += CHAR_WIDTH;
        if (cursor_x + CHAR_WIDTH > (int)fb_width)
        {
            cursor_x = 0;
            cursor_y += CHAR_HEIGHT;
        }
    }

    // Scroll if we've gone past the bottom
    if (cursor_y + CHAR_HEIGHT > (int)fb_height)
    {
        scroll();
        cursor_y = (int)fb_height - CHAR_HEIGHT;
    }
}

void terminal_write(const char *str)
{
    for (; *str; str++)
        terminal_putchar(*str);
}

#include "shell.h"

uint64_t nexus_total_ram = 0;

// ---------------------------------------------------------------------------
// Kernel entry
// ---------------------------------------------------------------------------
void kmain(void)
{
    // Get framebuffer from Limine
    fb = fb_request.response->framebuffers[0]->address;
    fb_width = fb_request.response->framebuffers[0]->width;
    fb_height = fb_request.response->framebuffers[0]->height;
    fb_pitch = fb_request.response->framebuffers[0]->pitch;

    __attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0,
    };

    // Clear screen
    for (uint32_t y = 0; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_pitch / 4; x++)
            fb[y * (fb_pitch / 4) + x] = 0x000000;

    // Set up PIC before touching the IDT
    pic_init();

    // Fill IDT with stub handlers
    for (int i = 0; i < 256; i++)
        set_idt_entry(i, isr_stub);

    // Install keyboard handler on vector 33 (IRQ1)
    set_idt_entry(33, keyboard_isr);

    // Load IDT, then enable interrupts
    load_idt();
    __asm__ volatile("sti");

    if (memmap_request.response)
    {
        for (uint64_t i = 0; i < memmap_request.response->entry_count; i++)
        {
            struct limine_memmap_entry *e = memmap_request.response->entries[i];
            if (e->type == LIMINE_MEMMAP_USABLE)
                nexus_total_ram += e->length;
        }
    }

    splash_show();

    terminal_write("NexusOS v0.2.0\n\n");
    auth_login();
    shell_init();

    while (1) {
        __asm__ volatile("hlt");
        if (auth_key_ready) {
            char c = auth_key;
            auth_key_ready = 0;
            shell_handle_key(c);
        }
    }
}