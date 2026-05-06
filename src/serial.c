#include "serial.h"

/* COM1 base port and the registers layered on top of it */
#define COM1         0x3F8
#define COM1_DATA    (COM1)
#define COM1_IER     (COM1 + 1)   /* Interrupt Enable                   */
#define COM1_LCR     (COM1 + 3)   /* Line Control  (bit 7 = DLAB)       */
#define COM1_FCR     (COM1 + 2)   /* FIFO Control                       */
#define COM1_MCR     (COM1 + 4)   /* Modem Control                      */
#define COM1_LSR     (COM1 + 5)   /* Line Status   (bit 5 = TX empty)   */
#define COM1_LSR_THRE  (1 << 5)   /* TX holding register empty — safe to write next byte */

/* outb/inb are implementation details of this driver, not part of the API */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

void serial_init(void) {
    outb(COM1_IER, 0x00);  /* disable UART interrupts — we poll instead        */
    outb(COM1_LCR, 0x80);  /* set DLAB to reach the baud rate divisor register */
    outb(COM1_DATA, 0x01); /* divisor = 1  →  115200 baud (lo byte)            */
    outb(COM1_IER,  0x00); /* divisor = 1  →  115200 baud (hi byte)            */
    outb(COM1_LCR, 0x03);  /* 8N1 and clear DLAB                               */
    outb(COM1_FCR, 0xC7);  /* enable FIFO, clear TX/RX, 14-byte trigger        */
    outb(COM1_MCR, 0x0B);  /* RTS + DTR + OUT2 (OUT2 enables IRQ on real hw)   */
}

void serial_putchar(char c) {
    while (!(inb(COM1_LSR) & COM1_LSR_THRE))
        ;
    outb(COM1_DATA, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s)
        serial_putchar(*s++);
}

void serial_puthex64(uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    serial_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        serial_putchar(digits[(v >> shift) & 0xF]);
}

void serial_putu64(uint64_t v) {
    char buf[20];
    int i = 0;
    if (v == 0) { serial_putchar('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i-- > 0) serial_putchar(buf[i]);
}
