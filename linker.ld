ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    .limine_requests : ALIGN(0x1000) {
        KEEP(*(.limine_requests))
    }

    .text : ALIGN(0x1000) {
        *(.text*)
    }

    .rodata : ALIGN(0x1000) {
        *(.rodata*)
    }

    .data : ALIGN(0x1000) {
        *(.data*)
    }

    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
    }
}