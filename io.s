global outb





outb:
    mov al, [esp + 8]
    mov dx, [esp + 4]
    out dx, al
    ret


global inb

    ; inb - returns a byte from the given I/O port
    ; stack: [esp + 4] The address of the I/O port
    ;        [esp    ] The return address



    inb:
        mov dx, [esp + 4]
        in  al, dx
        ret
