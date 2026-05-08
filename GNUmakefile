
.SUFFIXES:

override OUTPUT := quasaros


TOOLCHAIN :=
TOOLCHAIN_PREFIX :=
ifneq ($(TOOLCHAIN),)
    ifeq ($(TOOLCHAIN_PREFIX),)
        TOOLCHAIN_PREFIX := $(TOOLCHAIN)-
    endif
endif


ifneq ($(TOOLCHAIN_PREFIX),)
    CC := $(TOOLCHAIN_PREFIX)gcc
else
    CC := cc
endif

LD := $(TOOLCHAIN_PREFIX)ld

ifeq ($(TOOLCHAIN),llvm)
    CC := clang
    LD := ld.lld
endif

CFLAGS := -g -O2 -pipe

CPPFLAGS :=

NASMFLAGS := -g

LDFLAGS :=

override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep -q '^Target: '; echo $$?)

ifeq ($(CC_IS_CLANG),1)
    override CC += \
        -target x86_64-unknown-none-elf
    ifeq ($(TOOLCHAIN_PREFIX),)
        override LD := x86_64-elf-ld
    endif
endif

override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -m64 \
    -march=x86-64 \
    -mabi=sysv \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel


override CPPFLAGS := \
    -I src \
    $(CPPFLAGS) \
    -MMD \
    -MP


override NASMFLAGS := \
    -f elf64 \
    $(patsubst -g,-g -F dwarf,$(NASMFLAGS)) \
    -Wall

override LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    --gc-sections \
    -T linker.lds

override SRCFILES := $(shell find -L src -type f 2>/dev/null | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
override NASMFILES := $(filter %.asm,$(SRCFILES))
override OBJ := $(addprefix obj/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))

.PHONY: all
all: bin/$(OUTPUT)


-include $(HEADER_DEPS)


bin/$(OUTPUT): GNUmakefile linker.lds $(OBJ)
	mkdir -p "$(dir $@)"
	$(LD) $(LDFLAGS) $(OBJ) -o $@


obj/%.c.o: %.c GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@


obj/%.S.o: %.S GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@


obj/%.asm.o: %.asm GNUmakefile
	mkdir -p "$(dir $@)"
	nasm $(NASMFLAGS) $< -o $@


.PHONY: iso
iso: bin/$(OUTPUT)
	mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	cp -f bin/$(OUTPUT)                          iso_root/boot/
	cp -f limine.conf                            iso_root/boot/limine/
	cp -f limine-binary/limine-bios.sys          iso_root/boot/limine/
	cp -f limine-binary/limine-bios-cd.bin       iso_root/boot/limine/
	cp -f limine-binary/limine-uefi-cd.bin       iso_root/boot/limine/
	cp -f limine-binary/BOOTX64.EFI              iso_root/EFI/BOOT/
	cp -f limine-binary/BOOTIA32.EFI             iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    -hfsplus -apm-block-size 2048 \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    iso_root -o image.iso
	./limine-binary/limine bios-install image.iso

.PHONY: run
run: iso
	qemu-system-x86_64 -machine q35 -cdrom image.iso -m 256M \
	    -serial stdio -no-reboot -no-shutdown

.PHONY: run-kvm
run-kvm: iso
	qemu-system-x86_64 -machine q35 -cdrom image.iso -m 256M \
	    -serial stdio -no-reboot -no-shutdown \
	    -enable-kvm -cpu host

.PHONY: clean
clean:
	rm -rf bin obj

.PHONY: clean-iso
clean-iso: clean
	rm -rf iso_root image.iso
