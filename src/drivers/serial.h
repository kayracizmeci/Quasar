#pragma once

#include <stdint.h>

void serial_init(void);

/* Locked — safe to call from any core at any time */
void serial_puts(const char *s);
void serial_puthex64(uint64_t v);
void serial_putu64(uint64_t v);

/* Compound atomic print: hold the lock across multiple _raw calls */
void serial_lock(void);
void serial_unlock(void);
void serial_puts_raw(const char *s);
void serial_puthex64_raw(uint64_t v);
void serial_putu64_raw(uint64_t v);
