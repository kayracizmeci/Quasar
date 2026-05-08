#include "serial.h"
#include "arch/cpu/io.h"
#include "kernel/spinlock.h"

#define COM1          0x3F8
#define COM1_IER      (COM1 + 1)
#define COM1_LCR      (COM1 + 3)
#define COM1_FCR      (COM1 + 2)
#define COM1_MCR      (COM1 + 4)
#define COM1_LSR      (COM1 + 5)
#define COM1_LSR_THRE (1 << 5)

static spinlock_t lock = SPINLOCK_INIT;

void serial_init(void) {
    outb(COM1_IER, 0x00);  /* disable UART interrupts — we poll instead        */
    outb(COM1_LCR, 0x80);  /* set DLAB to reach the baud rate divisor register */
    outb(COM1 + 0, 0x01);  /* divisor = 1  →  115200 baud (lo byte)            */
    outb(COM1_IER, 0x00);  /* divisor = 1  →  115200 baud (hi byte)            */
    outb(COM1_LCR, 0x03);  /* 8N1 and clear DLAB                               */
    outb(COM1_FCR, 0xC7);  /* enable FIFO, clear TX/RX, 14-byte trigger        */
    outb(COM1_MCR, 0x0B);  /* RTS + DTR + OUT2 (OUT2 enables IRQ on real hw)   */
}

static void putchar_raw(char c) {
    while (!(inb(COM1_LSR) & COM1_LSR_THRE))
        ;
    outb(COM1, (uint8_t)c);
}

void serial_puts_raw(const char *s) {
    while (*s) putchar_raw(*s++);
}

void serial_puthex64_raw(uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    putchar_raw('0'); putchar_raw('x');
    for (int shift = 60; shift >= 0; shift -= 4)
        putchar_raw(digits[(v >> shift) & 0xF]);
}

void serial_putu64_raw(uint64_t v) {
    char buf[20];
    int i = 0;
    if (v == 0) { putchar_raw('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i-- > 0) putchar_raw(buf[i]);
}

void serial_lock(void)   { spinlock_acquire(&lock); }
void serial_unlock(void) { spinlock_release(&lock); }

void serial_puts(const char *s) {
    serial_lock(); serial_puts_raw(s); serial_unlock();
}

void serial_puthex64(uint64_t v) {
    serial_lock(); serial_puthex64_raw(v); serial_unlock();
}

void serial_putu64(uint64_t v) {
    serial_lock(); serial_putu64_raw(v); serial_unlock();
}
