#include "panic.h"
#include "drivers/serial.h"

#include <stdint.h>

__attribute__((noreturn)) void kpanic_at(const char *file, int line, const char *msg) {
    serial_puts("\n[PANIC] ");
    serial_puts(file);
    serial_puts(":");
    serial_putu64((uint64_t)line);
    serial_puts(": ");
    serial_puts(msg);
    serial_puts("\n");
    for (;;) asm volatile("hlt");
}
