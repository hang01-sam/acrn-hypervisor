/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_SPINLOCK_H__
#define __AARCH64_SPINLOCK_H__

#include <types.h>
#include <rtl.h>

/** The architecture dependent spinlock type. */
typedef struct _spinlock {
	uint32_t head;
	uint32_t tail;
} spinlock_t;

/* Function prototypes */
static inline void spinlock_init(spinlock_t *lock)
{
	(void)memset(lock, 0U, sizeof(spinlock_t));
}

static inline void spinlock_obtain(spinlock_t *lock)
{

	uint32_t ONE = 1;
	uint32_t old_head_value;
	uint32_t new_head_value;
	uint32_t tmp;

	asm volatile(
		"   1:      \n\t"
		"   ldaxr   %w0, %w3 \n\t"
		"   add     %w1, %w0, %w5 \n\t"
		"   stxr    %w2, %w1, %w3 \n\t"
		"   cbnz    %w2, 1b \n\t"
		"   2:      \n\t"
		"   ldaxr   %w2, %w4 \n\t"
		"   cmp     %w0, %w2 \n\t"
		"   b.ne    2b\n\t"
		: "=&r"(old_head_value), "=&r"(new_head_value), "=&r"(tmp), "+Q"(lock->head), "+Q"(lock->tail)
		: "r"(ONE)
		: "memory");
}

static inline void spinlock_release(spinlock_t *lock)
{
	uint32_t ONE = 1;
	uint32_t old_tail_value;
	uint32_t new_tail_value;
	uint32_t tmp;

	asm volatile(
		"   1: \n\t"
		"   ldaxr %w0, %w3 \n\t"
		"   add %w1, %w0, %w4 \n\t"
		"   stxr %w2, %w1, %w3 \n\t"
		"   cbnz %w2, 1b \n\t"
		: "=&r"(old_tail_value), "=&r"(new_tail_value), "=&r"(tmp), "+Q"(lock->tail)
		: "r"(ONE)
		: "memory");
}

#define spin_lock(x) spinlock_obtain(x)
#define spin_unlock(x) spinlock_release(x)

#define spinlock_irqsave_obtain(lock, p_rflags)		\
	do {						\
		CPU_INT_ALL_DISABLE(p_rflags);		\
		spinlock_obtain(lock);			\
	} while (0)

#define spinlock_irqrestore_release(lock, rflags)	\
	do {						\
		spinlock_release(lock);			\
		CPU_INT_ALL_RESTORE(rflags);		\
	} while (0)
#endif /* __AARCH64_SPINLOCK_H__ */
