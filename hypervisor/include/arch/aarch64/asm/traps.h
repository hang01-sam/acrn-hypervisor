/*
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_TRAPS_H__
#define __AARCH64_TRAPS_H__

/* Hyp IPA bits*/
#define HPFAR_MASK	GENMASK(39, 4)

/* SPSR_EL2 bits */
#define SPSR_MODE_MASK        (0x10) // Execution state
#define SPSR_THUMB_MASK       (0x20) // T32 Instruction set state
#define SPSR_IT_MASK          (0x0600fc00) // If-Then

#define VA2PA_READ  (0U << 0)
#define VA2PA_WRITE (1U << 0)

/* Physical Address Register */
#define PADDR_F     (1U << 0)
#define PADDR_MASK  ((1ULL << 48) - 1)

/* Data Fault Status Code */
#define TRANSLATION_FAULT_LEVEL_0   (0x4)
#define TRANSLATION_FAULT_LEVEL_1   (0x5)
#define TRANSLATION_FAULT_LEVEL_2   (0x6)
#define TRANSLATION_FAULT_LEVEL_3   (0x7)

/* DAIF mask */
#define SPSR_DEBUG_EX_MASK             (1 << 9)
#define SPSR_SERROR_INTRRUPT_MASK      (1 << 8)
#define SPSR_IRQ_INTRRUPT_MASK         (1 << 7)
#define SPSR_FIQ_INTRRUPT_MASK         (1 << 6)

/* PSTATE.IT */
#define IT_MASK                 (0x7)
#define IT_BASE_COND_MASK       (0xe000)
#define IT_BASE_COND_SHIFT      (13)
#define IT_BITS_HIGH_MASK       (0x1c00)
#define IT_BITS_HIGH_SHIFT      (10 - 2)
#define IT_BITS_LOW_MASK        (0x3)
#define IT_BITS_LOW_SHIFT       (25)

/* Instruction Length */
#define INSTRUCTION_32_BIT      (0x4)
#define INSTRUCTION_16_BIT      (0x2)

extern uint32_t hyp_entry[];

void traps_init(void);
void ex_hyp_synchronous(struct intr_excp_ctx *regs);
void ex_hyp_irq(struct intr_excp_ctx *regs);
void ex_hyp_serror(struct intr_excp_ctx *regs);
void ex_hyp_invalid(struct intr_excp_ctx *regs, int index);

void ex_guest_synchronous(struct intr_excp_ctx *regs);
void ex_guest_irq(struct intr_excp_ctx *regs);
void ex_guest_serror(struct intr_excp_ctx *regs);
void ex_guest_invalid(struct intr_excp_ctx *regs, int index);

#endif /* __AARCH64_TRAPS_H__ */
