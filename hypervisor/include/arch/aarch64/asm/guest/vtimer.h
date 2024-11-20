/*
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VTIMER_H__
#define __AARCH64_VTIMER_H__
#include <types.h>
#include <asm/cpu.h>
#include <asm/sysreg.h>
#include <asm/guest/vcpu.h>
#include <asm/time.h>
#include <timer.h>

struct virtimer {
	struct acrn_vcpu *vcpu;
	struct hv_timer hv_timer;
	uint64_t cnt_cmp_val;
	uint32_t cnt_ctl_val;
	int irq_no;
};

#endif /* __AARCH64_VTIMER_H__ */
