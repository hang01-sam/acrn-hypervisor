/*
 * Copyright (c) 2015-2024, Arm Limited and Contributors. All rights reserved.
 * Copyright (c) 2023, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_IRQ_H__
#define __AARCH64_IRQ_H__

#include <types.h>
#include <asm/cpu.h>

#define HOST_IRQ (0)
#define GUEST_IRQ (1)
/*
 * x86 irq data
 */
struct aarch64_irq_data {
	uint32_t type;	/**< assigned vector */
	void     *vm;	/**< belong which vm */
};

#define NR_MAX_VECTOR          0xFFU
#define VECTOR_INVALID         (NR_MAX_VECTOR + 1U)
#define HYPERVISOR_CALLBACK_HSM_VECTOR 0xF3U

/**
 * @brief Get vector number of an interrupt from irq number
 *
 * @param[in]	irq	The irq_num to convert
 *
 * @return vector number
 */
uint32_t irq_to_vector(uint32_t irq);

/* Arch specific routines called from generic IRQ handling */

struct irq_desc;

/**
 * @brief Get irq descriptor from irq id
 *
 * @param[in]   irq The irq id
 */
struct irq_desc *irq_to_desc(uint32_t irq);

void init_irq_descs_arch(struct irq_desc *descs);

void setup_irqs_arch(void);

void init_interrupt_arch(uint16_t pcpu_id);

void free_irq_arch(uint32_t irq);

bool request_irq_arch(uint32_t irq);

void pre_irq_arch(const struct irq_desc *desc);

void post_irq_arch(const struct irq_desc *desc);

void kick_pcpu(uint16_t pcpu_id);

#endif
