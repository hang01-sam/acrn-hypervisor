/*
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_TIME_H__
#define __AARCH64_TIME_H__

#include <types.h>
#include <asm/arch.h>
#include <asm/sysreg.h>


/* Counter-timer Hypervisor Control register */
#define CNTHCTL_EL2_EVNTI      (15U<<4)
#define CNTHCTL_EL2_EVNTDIR    (1U<<3)
#define CNTHCTL_EL2_EVNTEN     (1U<<2)
#define CNTHCTL_EL2_EL1PCEN    (1U<<1)
#define CNTHCTL_EL2_EL1PCTEN   (1U<<0)

/* Counter-timer Kernel Control register */
#define CNTKCTL_EL1_EL0PTEN    (1U<<9)
#define CNTKCTL_EL1_EL0VTEN    (1U<<8)
#define CNTKCTL_EL1_EVNTI_MASK (15U<<4)
#define CNTKCTL_EL1_EVNTDIR    (1U<<3)
#define CNTKCTL_EL1_EVNTEN     (1U<<2)
#define CNTKCTL_EL1_EL0VCTEN   (1U<<1)
#define CNTKCTL_EL1_EL0PCTEN   (1U<<0)

#define CNT_ENABLE   (1U<<0)
#define CNT_IMASK    (1U<<1)
#define CNT_ISTATUS  (1U<<2)

/*timer irq*/
#define HYP_TIMER_PPI_IRQ  26
#define VIRT_TIMER_PPI_IRQ 27
#define PHY_TIMER_PPI_IRQ  30

static inline uint64_t get_pcount(void)
{
	isb();
	return read_cntpct_el0();
}

void init_hw_timer(void);
int reopen_timer(int64_t timeout);

#endif /* __AARCH64_TIME_H__ */
