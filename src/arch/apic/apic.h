#pragma once
#include <stdint.h>

/* Local APIC MMIO register offsets (Intel SDM Vol. 3A §10.4.1) */
#define LAPIC_REG_ID          0x020
#define LAPIC_REG_VERSION     0x030
#define LAPIC_REG_TPR         0x080
#define LAPIC_REG_LDR         0x0D0  /* Logical Destination Register */
#define LAPIC_REG_DFR         0x0E0  /* Destination Format Register  */
#define LAPIC_REG_EOI         0x0B0
#define LAPIC_REG_SVR         0x0F0
#define LAPIC_REG_ESR         0x280
#define LAPIC_REG_LVT_TIMER   0x320
#define LAPIC_REG_LVT_THERMAL 0x330
#define LAPIC_REG_LVT_PERF    0x340
#define LAPIC_REG_LVT_LINT0   0x350
#define LAPIC_REG_LVT_LINT1   0x360
#define LAPIC_REG_LVT_ERROR   0x370
#define LAPIC_REG_TIMER_ICR   0x380
#define LAPIC_REG_TIMER_CCR   0x390
#define LAPIC_REG_TIMER_DCR   0x3E0

#define LAPIC_SVR_ENABLE      (1u << 8)
#define LAPIC_LVT_MASKED      (1u << 16)
#define LAPIC_SPURIOUS_VECTOR 0xFF

#define MSR_APIC_BASE         0x1Bu
#define MSR_APIC_BASE_ENABLE  (1ull << 11)
#define MSR_APIC_BASE_X2APIC  (1ull << 10)

#define CPUID1_EDX_APIC       (1u << 9)

#define LAPIC_TIMER_DIV1      0x0Bu
#define LAPIC_TIMER_ONESHOT   (0u << 17)
#define LAPIC_TIMER_PERIODIC  (1u << 17)

void    apic_init(uint64_t lapic_phys);
void    apic_init_ap(void);
void    apic_eoi(void);
uint8_t apic_id(void);

void     apic_timer_arm_calibration(void);
uint32_t apic_timer_read_elapsed(void);
void     apic_timer_start_periodic(uint32_t ticks_per_period, uint8_t vector);
