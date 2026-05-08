#include "pit.h"
#include "arch/cpu/io.h"

/* The 8254 PIT oscillator runs at this fixed frequency, defined by the IBM
   PC standard.  Every PC-compatible system uses exactly this value. */
#define PIT_OSCILLATOR_HZ    1193182u

/* Rounding to nearest tick: (hz + 50) / 100 gives 11932, which is
   11932 / 1193182 ≈ 10.001 ms — well within calibration tolerance. */
#define PIT_10MS_DIVISOR     ((PIT_OSCILLATOR_HZ + 50u) / 100u)

#define PIT_CMD_PORT         0x43  /* write-only command register */
#define PIT_CH2_PORT         0x42  /* channel 2 data port */
#define PIT_CTRL_PORT        0x61  /* NMI status/control register */

/* Channel 2, access lo+hi bytes, mode 0 (output goes high when count hits 0),
   binary counting.  Mode 0 lets us detect completion by reading bit 5 of 0x61. */
#define PIT_CMD_CH2_ONESHOT  0xB0u

#define PIT_CTRL_GATE2       (1u << 0)  /* set to enable channel 2 counting */
#define PIT_CTRL_SPKR_EN     (1u << 1)  /* speaker data — must stay clear during measurement */
#define PIT_CTRL_CH2_OUT     (1u << 5)  /* output high when countdown reaches 0 (read-only) */

void pit_start_calibration(void) {
    /* Drop the gate and silence the speaker before programming, so the counter
       does not start until we are ready and the PC speaker does not beep */
    uint8_t ctrl = inb(PIT_CTRL_PORT) & (uint8_t)~(PIT_CTRL_GATE2 | PIT_CTRL_SPKR_EN);
    outb(PIT_CTRL_PORT, ctrl);

    outb(PIT_CMD_PORT, PIT_CMD_CH2_ONESHOT);

    /* Low byte must be written first; io_wait between bytes satisfies old
       ISA chipset setup-time requirements */
    outb(PIT_CH2_PORT, (uint8_t)(PIT_10MS_DIVISOR & 0xFFu));
    io_wait();
    outb(PIT_CH2_PORT, (uint8_t)(PIT_10MS_DIVISOR >> 8));

    /* Asserting the gate starts the countdown */
    outb(PIT_CTRL_PORT, ctrl | PIT_CTRL_GATE2);
}

void pit_wait_calibration(void) {
    while (!(inb(PIT_CTRL_PORT) & PIT_CTRL_CH2_OUT))
        ;
}
