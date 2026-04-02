CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -ffreestanding -fno-stack-protector -mno-red-zone \
         -nostdlib -mgeneral-regs-only

LDFLAGS = -T linker.ld

all: iso

kernel.elf: boot.o main.o
	$(LD) $(LDFLAGS) -o $@ $^

boot.o:
	$(AS) -f elf64 kernel/boot.asm -o boot.o

main.o:
	$(CC) $(CFLAGS) -c kernel/main.c -o main.o

iso: kernel.elf
	mkdir -p iso/boot
	cp kernel.elf iso/boot/
	cp limine.cfg iso/boot/

	cp limine/limine-bios.sys iso/boot/
	cp limine/limine-bios-cd.bin iso/boot/
	cp limine/limine-uefi-cd.bin iso/boot/

	xorriso -as mkisofs -b boot/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image \
		--protective-msdos-label \
		iso -o nexus.iso
	./limine/limine bios-install nexus.iso

clean:
	rm -rf *.o *.elf iso nexus.iso