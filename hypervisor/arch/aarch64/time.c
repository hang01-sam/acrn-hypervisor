/*
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/time.h>
#include <asm/sysreg.h>
#include <irq.h>
#include <asm/arch.h>
#include <asm/guest/vgic.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <softirq.h>
#include <logmsg.h>

static uint64_t cpu_khz;

uint32_t cpu_tickrate(void)
{
	return cpu_khz;
}

static void hyp_timer_irq_action(__unused uint32_t irq, __unused void *priv_data)
{
	if (read_cnthp_ctl_el2() & CNT_ISTATUS) {
		fire_softirq(SOFTIRQ_TIMER);
		write_cnthp_ctl_el2(0);
	}
}

static void phy_timer_irq_action(__unused uint32_t irq, __unused void *priv_data)
{
	if (read_cntp_ctl_el0() & CNT_ISTATUS) {
		fire_softirq(SOFTIRQ_TIMER);
		write_cntp_ctl_el0(0);
	}
}

static void virt_timer_irq_action(uint32_t irq, __unused void *priv_data)
{
	uint16_t pcpu = get_pcpu_id();
	vcpu_t *vcpu = get_running_vcpu(pcpu);

	if (vcpu) {
		uint32_t regVal;
		regVal = read_cntv_ctl_el0();
		write_cntv_ctl_el0(regVal | CNT_IMASK);
		vgic_inject(vcpu, irq, 0);
		vcpu->arch.virt_timer.cnt_ctl_val = regVal;
	}
}

uint64_t cpu_ticks(void)
{
	return get_pcount();
}

int reopen_timer(int64_t timeout)
{
	uint32_t regVal = 0;

	if (timeout > 0) {
		regVal = CNT_ENABLE;
		write_cnthp_cval_el2(timeout);
	}

	write_cnthp_ctl_el2(regVal);
	isb();

	return 1;
}

static void register_timer_handler(void)
{
	request_irq(HYP_TIMER_PPI_IRQ, hyp_timer_irq_action, "hyp_timer", 1);
	request_irq(PHY_TIMER_PPI_IRQ, phy_timer_irq_action, "phy_timer", 1);
	request_irq(VIRT_TIMER_PPI_IRQ, virt_timer_irq_action, "vir_timer", 1);
}

void init_hw_timer(void)
{
	cpu_khz = read_cntfrq_el0() / 1000;

	write_cntvoff_el2(0);
	write_cnthctl_el2(1);
	write_cnthp_ctl_el2(0);
	write_cntp_ctl_el0(0);

	register_timer_handler();
}
