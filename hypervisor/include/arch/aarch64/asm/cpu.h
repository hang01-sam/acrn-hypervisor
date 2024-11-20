/*-
 * Copyright (c) 1989, 1990 William F. Jolitz
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2017-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)segments.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#ifndef __AARCH64_CPU_H__
#define __AARCH64_CPU_H__
#include <types.h>
#include <util.h>
#include <asm/sysreg.h>
#include <acrn_common.h>
#include <board_info.h>
#include <asm/board.h>
#include <asm/arch.h>

/* Define CPU stack alignment */
#define CPU_STACK_ALIGN         16UL

/* Number of GPRs saved / restored for guest in VCPU structure */
#define NUM_GPRS                            16U

#ifndef ASSEMBLER

#define ALL_CPUS_MASK		((1UL << get_pcpu_nums()) - 1UL)
#define AP_MASK			(ALL_CPUS_MASK & ~(1UL << BSP_CPU_ID))

/*
 * To support per_cpu access, we use a special struct "per_cpu_region" to hold
 * the pattern of per CPU data. And we allocate memory for per CPU data
 * according to multiple this struct size and pcpu number.
 *
 *   +-------------------+------------------+---+------------------+
 *   | percpu for pcpu0  | percpu for pcpu1 |...| percpu for pcpuX |
 *   +-------------------+------------------+---+------------------+
 *   ^                   ^
 *   |                   |
 *   <per_cpu_region size>
 *
 * To access per cpu data, we use:
 *   per_cpu_base_ptr + sizeof(struct per_cpu_region) * curr_pcpu_id
 *   + offset_of_member_per_cpu_region
 * to locate the per cpu data.
 */

/* Boot CPU ID */
#define BSP_CPU_ID             0U

/**
 *The invalid cpu_id (INVALID_CPU_ID) is error
 *code for error handling, this means that
 *caller can't find a valid physical cpu
 *or virtual cpu.
 */
#define INVALID_CPU_ID 0xffffU
/**
 *The broadcast id (BROADCAST_CPU_ID)
 *used to notify all valid phyiscal cpu
 *or virtual cpu.
 */
#define BROADCAST_CPU_ID 0xfffeU

struct descriptor_table {
	uint16_t limit;
	uint64_t base;
} __packed;

/* CPU states defined */
enum pcpu_boot_state {
	PCPU_STATE_RESET = 0U,
	PCPU_STATE_INITIALIZING,
	PCPU_STATE_RUNNING,
	PCPU_STATE_HALTED,
	PCPU_STATE_DEAD,
};

#define	NEED_OFFLINE		(1U)
#define	NEED_SHUTDOWN_VM	(2U)
void make_pcpu_offline(uint16_t pcpu_id);
bool need_offline(uint16_t pcpu_id);

struct segment_sel {
	uint16_t selector;
	uint64_t base;
	uint32_t limit;
	uint32_t attr;
};

/*
 * Definition of the stack frame layout
 */
struct intr_excp_ctx {
	/* hypervisor/include/arch/aarch64/asm/cpu_asm.h */
	/* Update ASMOFF_EXCP_XXX when modify this struct */
	uint64_t x[31];		/* x29:fp, x30:lr */
	uint64_t elr_el2;
	uint64_t spsr_el2;
	uint64_t esr_el2;
	/* Guest Exception */
	uint64_t spsr_el1;
	uint64_t sp_el0;
	uint64_t sp_el1;
	uint64_t elr_el1;
} __attribute__((aligned(16)));  /* makes size always aligned to 16 to respect */
/* stack alignment */

void dispatch_exception(struct intr_excp_ctx *ctx);

void cpu_do_idle(void);
void cpu_dead(void);
void load_pcpu_state_data(void);
void init_pcpu_pre(bool is_bsp);
/* The function should be called on the same CPU core as specified by pcpu_id,
 * hereby, pcpu_id is actually the current physcial cpu id.
 */
void init_pcpu_post(uint16_t pcpu_id);
bool start_pcpus(uint64_t mask);
void wait_pcpus_offline(uint64_t mask);
void stop_pcpus(void);
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync);

/* Disables interrupts on the current CPU */
#ifdef CONFIG_KEEP_IRQ_DISABLED
#define CPU_IRQ_DISABLE_ON_CONFIG()		do { } while (0)
#else
#define CPU_IRQ_DISABLE_ON_CONFIG()     disable_irq()
#endif

/* Enables interrupts on the current CPU
 * If CONFIG_KEEP_IRQ_DISABLED is 'y', all interrupts
 * received in root mode will be handled in external interrupt
 * vmexit after next VM entry. The postpone latency is negligible.
 * Permanently turning off interrupts in root mode can be useful in
 * many scenarios (e.g., x86_tee).
 */
#ifdef CONFIG_KEEP_IRQ_DISABLED
#define CPU_IRQ_ENABLE_ON_CONFIG()		do { } while (0)
#else
#define CPU_IRQ_ENABLE_ON_CONFIG()      enable_irq()
#endif

/* Macro to save rflags register */
#define local_save_flags(x)                                      \
{                                                               \
    asm volatile ("mrs    %0, daif    \n"			\
				: "=r" (*(x))                                       \
				:                                                \
				: "memory");                                     \
}

/* Macro to restore rflags register */
#define local_irq_restore(x)                                     \
{                                                               \
    asm volatile ("msr    daif, %0"				 \
		:                                                        \
		: "r" (x)                                                \
		: "memory");                                             \
}

/* This macro locks out interrupts and saves the current architecture status
 * register / state register to the specified address.  This function does not
 * attempt to mask any bits in the return register value and can be used as a
 * quick method to guard a critical section.
 * NOTE:  This macro is used in conjunction with CPU_INT_ALL_RESTORE
 *        defined below and CPU_INT_CONTROL_VARS defined above.
 */

#define CPU_INT_ALL_DISABLE(p_rflags)               \
{                                                   \
	local_save_flags(p_rflags);	             \
	CPU_IRQ_DISABLE_ON_CONFIG();                    \
}

/* This macro restores the architecture status / state register used to lockout
 * interrupts to the value provided.  The intent of this function is to be a
 * fast mechanism to restore the interrupt level at the end of a critical
 * section to its original level.
 * NOTE:  This macro is used in conjunction with CPU_INT_ALL_DISABLE
 *        and CPU_INT_CONTROL_VARS defined above.
 */
#define CPU_INT_ALL_RESTORE(rflags)                 \
{                                                   \
	local_irq_restore(rflags);                 \
}

/*
 * Macro to get CPU ID
 * @pre: the return CPU ID would never equal or large than MAX_PCPU_NUM.
 */
static inline uint16_t get_pcpu_id(void)
{
	uint64_t tpidr = read_tpidr_el2();
	return (uint16_t) tpidr;
}

uint64_t cpu_id_to_mpidr(uint16_t cpuid);
uint16_t mpidr_to_cpu_id(uint64_t mpidr);

/*
 * @post return <= MAX_PCPU_NUM
 */
uint16_t get_pcpu_nums(void);
bool is_pcpu_active(uint16_t pcpu_id);
uint64_t get_active_pcpu_bitmap(void);
#else /* ASSEMBLER defined */

#endif /* ASSEMBLER defined */

#endif /* __AARCH64_CPU_H__ */
