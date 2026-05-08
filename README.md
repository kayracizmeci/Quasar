# Quasar

![Stars](https://img.shields.io/github/stars/kayracizmeci/Quasar?style=flat-square&color=gold)
![Forks](https://img.shields.io/github/forks/kayracizmeci/Quasar?style=flat-square&color=blue)
![License](https://img.shields.io/github/license/kayracizmeci/Quasar?style=flat-square&color=green)

## What is Quasar?

Quasar is a lightweight x86-64 hobby operating system kernel.

## Requirements

| Tool | Notes |
|---|---|
| `nasm` | Assembler |
| `make` | Build system |
| `gcc` | C compiler — on macOS use `x86_64-elf-gcc` from Homebrew |
| `binutils` | Linker (`ld`) — on macOS use `x86_64-elf-binutils` |
| `xorriso` | ISO creation |
| `qemu-system-x86_64` | Optional — for running the kernel |

### macOS setup

```bash
brew install x86_64-elf-gcc x86_64-elf-binutils nasm xorriso qemu
```

## Building

### First time only — download Limine bootloader

```bash
curl -L https://github.com/limine-bootloader/limine/releases/latest/download/limine-binary.tar.gz \
  | tar -xz --strip-components=1 -C limine-binary/
```

### Compile the kernel

```bash
# Linux
make

# macOS
make TOOLCHAIN_PREFIX=x86_64-elf-
```

### Build a bootable ISO

```bash
# Linux
make iso

# macOS
make iso TOOLCHAIN_PREFIX=x86_64-elf-
```

### Rebuild after source changes

```bash
make iso                            # Linux
make iso TOOLCHAIN_PREFIX=x86_64-elf-   # macOS
```

## Running

### QEMU (any platform)

```bash
make run                            # Linux
make run TOOLCHAIN_PREFIX=x86_64-elf-   # macOS
```

### QEMU with KVM acceleration (Linux only)

```bash
make run-kvm
```

### Boot on real hardware

Write `image.iso` to a USB drive or burn it to a disc.

```bash
# Linux — replace /dev/sdX with your drive
sudo dd if=image.iso of=/dev/sdX bs=4M status=progress && sync
```

## Serial output

All kernel log messages go to COM1 at 115200 baud.  When running in QEMU
(`-serial stdio`) they appear directly in the terminal:

```
[GDT] loaded
[SERIAL] COM1 initialized
[IDT] loaded
[ACPI] LAPIC phys=0x00000000fee00000  ioapics=1  isos=2
[LAPIC] id=0x0000000000000000  version=0x0000000000000014
[LAPIC] initialized
[IOAPIC] id=0x00  gsi_base=0  lines=24
[IOAPIC] ISO: IRQ 0 -> GSI 2
[IOAPIC] initialized
[TIMER] 10 ms tick  LAPIC ticks/period=1193182
[BOOT] interrupts enabled
[BOOT] kernel started
```

To save logs to a file:

```bash
qemu-system-x86_64 -machine q35 -cdrom image.iso -m 256M \
    -serial file:serial.log -display none -no-reboot -no-shutdown
cat serial.log
```
## References

### Intel SDM (Software Developer Manual)

- Vol. 3A §4.5 — Paging
- Vol. 3A §6.3.1, §6.10, §6.14.1 — IDT & Exceptions
- Vol. 3A §7.2.3, §7.7 — TSS
- Vol. 3A §10.4.1, §10.4.4, §10.5–10.8 — LAPIC
- Vol. 2A — CPUID

### ACPI Specification 6.5

- §5.2.5 — RSDP
- §5.2.6 — SDT header
- §5.2.7 / §5.2.8 — RSDT / XSDT
- §5.2.12 — MADT
- §5.2.12.3, §5.2.12.5, §5.2.12.8 — MADT entries

### Intel 82093AA I/O APIC Datasheet

- §3.1, §3.2.2, §3.2.4 — MMIO map, version, redirection table

### Intel 8259A Datasheet

- §2.1 — ICW initialization sequence

### Intel 8254 PIT Datasheet

- §1.3 — Channel ports

### 16550 UART Datasheet

- §3, §4 — Register map, FIFO, baud

### IBM PC/AT Technical Reference Manual

- PIC remap, Port 0x61

### OSDev Wiki

https://wiki.osdev.org
