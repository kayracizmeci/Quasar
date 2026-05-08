#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

/* Writing to port 0x80 (POST diagnostic) takes ~1µs and is the conventional
   way to satisfy setup/hold time requirements on old ISA peripherals. */
static inline void io_wait(void) {
    outb(0x80, 0x00);
}
