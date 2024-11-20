/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/gic.h>
#include <asm/cpu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <irq.h>
#include <debug/logmsg.h>

gic_ops_t *gic_ops = NULL;

void raise_sgi_mask(uint32_t cpumask, enum gic_sgi sgi)
{
	gic_ops->raise_sgi(sgi, cpumask);
}

void raise_sgi_one(uint32_t cpu, enum gic_sgi sgi)
{
	uint32_t cpumask = 1U << cpu;
	raise_sgi_mask(cpumask, sgi);
}

void raise_sgi_self(enum gic_sgi sgi)
{
	uint32_t cpumask = 1U << get_pcpu_id();
	raise_sgi_mask(cpumask, sgi);
}

void do_sgi(uint32_t irq)
{
	switch (irq) {
	case GIC_SGI_WAKEUP:
		break;
	default:
		break;
	}
}

void gic_irq_hdl(void)
{
	uint32_t irq;

	do {
		irq = gic_ops->ack_irq();

		if (irq >= 16 && irq < 1020) {
			do_irq(irq);
		} else if (irq < 16) {
			do_sgi(irq);
			do_irq(irq);
		} else {
			break;
		}
	} while (1);
}

void gic_enable_irq(uint32_t id)
{
	gic_ops->enable_irq(id);
}

void gic_disable_irq(uint32_t id)
{
	gic_ops->disable_irq(id);
}

void gic_set_irq_pending(uint32_t id)
{
	gic_ops->set_irq_pending(id);
}

void gic_clear_irq_pending(uint32_t id)
{
	gic_ops->clear_irq_pending(id);
}

void gic_set_irq_cfg(uint32_t id, uint32_t cfg)
{
	gic_ops->set_irq_cfg(id, cfg);
}

void gic_set_irq_priority(uint32_t id, uint32_t priority)
{
	gic_ops->set_irq_priority(id, priority);
}

void gic_end_irq(uint32_t id)
{
	gic_ops->end_irq(id);
}

void gic_deactivate_irq(uint32_t id)
{
	gic_ops->deactivate_irq(id);
}

void gic_init(uint16_t pcpu_id)
{
	gic_ops->init(pcpu_id);
	if (pcpu_id == 0) {
		reserve_irq_num(GIC_SGI_WAKEUP);
	}
}

void gic_register_ops(gic_ops_t *ops)
{
	gic_ops = ops;
}
