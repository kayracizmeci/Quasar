#pragma once
#include <stdint.h>

/* Arms PIT channel 2 for a ~10 ms one-shot countdown.
   Channel 2 can be polled via port 0x61 without requiring an IRQ,
   which keeps the calibration path free of interrupt-controller dependencies. */
void pit_start_calibration(void);

/* Spins until the PIT channel 2 output goes high (count reached zero). */
void pit_wait_calibration(void);
