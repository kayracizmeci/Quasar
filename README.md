# Quasar

![Stars](https://img.shields.io/github/stars/kayracizmeci/Quasar?style=flat-square&color=gold)
![Forks](https://img.shields.io/github/forks/kayracizmeci/Quasar?style=flat-square&color=blue)
![License](https://img.shields.io/github/license/kayracizmeci/Quasar?style=flat-square&color=green)

## 🤔 What is Quasar?

Quasar is a lightweight and simple OS. 

## 📝 Requirements

### 🔑 Core Requirements 
* **nasm**
* **make**
* **gcc**
    * *Note for macOS:* Use `x86_64-elf-gcc`.
* **binutils**
* **curl**
* **tar & gzip**

### 🧙 ISO Creation Tools
* **xorriso**
* **libisoburn**
* **mtools**

### 🤫 Optionals
* **qemu-system-x86_64**


## 🧙 How to Create an ISO


### 1. Compiling 

Enter this command to your terminal.

```bash
make 
```

**NOTE:** For MacOS, you need to install x86_64-elf-gcc package. After installing, enter this command to your terminal.

```bash
make TOOLCHAIN_PREFIX=x86_64-elf-
```

### 2. Installing latest release of Limine

```bash
curl -L https://github.com/Limine-Bootloader/Limine/releases/latest/download/limine-binary.tar.gz
| gunzip | tar -xf -
```


### 3. Creating the ISO

```bash
make -C limine-binary

# Create a directory ISO root.
mkdir -p iso_root

# Copy the files over.
mkdir -p iso_root/boot
cp -v bin/quasaros iso_root/boot/
mkdir -p iso_root/boot/limine
cp -v limine.conf limine-binary/limine-bios.sys limine-binary/limine-bios-cd.bin \
      limine-binary/limine-uefi-cd.bin iso_root/boot/limine/

mkdir -p iso_root/EFI/BOOT
cp -v limine-binary/BOOTX64.EFI iso_root/EFI/BOOT/
cp -v limine-binary/BOOTIA32.EFI iso_root/EFI/BOOT/

# Create the ISO.
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o image.iso


./limine-binary/limine bios-install image.iso
```

## 🤫 **EXTRA**
For booting the ISO with QEMU, use this command.
```bash
qemu-system-x86_64 -cdrom image.iso -m 256M -serial stdio
```
