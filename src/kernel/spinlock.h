#pragma once

typedef struct { int locked; } spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spinlock_acquire(spinlock_t *s) {
    while (__atomic_test_and_set(&s->locked, __ATOMIC_ACQUIRE))
        asm volatile("pause");
}

static inline void spinlock_release(spinlock_t *s) {
    __atomic_clear(&s->locked, __ATOMIC_RELEASE);
}
