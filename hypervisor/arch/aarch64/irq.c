/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/lib/bits.h>
#include <asm/lib/spinlock.h>
#include <asm/per_cpu.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/gic.h>
#include <asm/guest/virq.h>
#include <dump.h>
#include <logmsg.h>
#include "gic/common/gic_common.h"

extern struct irq_desc irq_desc_array[NR_IRQS];

static struct aarch64_irq_data irq_data[NR_IRQS];
static struct aarch64_irq_data local_irq_data[MAX_PCPU_NUM][32];

struct irq_desc *irq_to_desc(uint32_t irq)
{
	struct irq_desc *desc;

	ASSERT((irq < NR_IRQS), "irq number (%u) exceeds max (%u)!", irq, NR_IRQS);

	desc = &irq_desc_array[irq];

	return desc;
}

bool request_irq_arch(uint32_t irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	gic_set_irq_cfg(irq, desc->flags);
	gic_set_irq_priority(irq, GIC_IRQ_DEF_PRIORITY);
	gic_enable_irq(irq);

	return true;
}

void free_irq_arch(__unused uint32_t irq)
{
}

uint32_t irq_to_vector(__unused uint32_t irq)
{
	return 0;
}

void pre_irq_arch(__unused const struct irq_desc *desc)
{

}

void post_irq_arch(const struct irq_desc *desc)
{
	struct aarch64_irq_data *irq_data = (struct aarch64_irq_data *)desc->arch_data;

	if (irq_data->type == GUEST_IRQ) {
		interrupts_vm_inject(desc->irq);
		// inject irq
		gic_end_irq(desc->irq);
	} else {
		gic_end_irq(desc->irq);
		gic_deactivate_irq(desc->irq);
	}
}

/*
 * descs[] must have NR_IRQS entries
 */
void init_irq_descs_arch(struct irq_desc descs[])
{
	unsigned int i, j;

	for (i = 0; i < NR_IRQS; i++) {
		descs[i].arch_data = (void *)(&irq_data[i]);
		irq_data[i].type = HOST_IRQ;
	}

	for (i = 0; i < MAX_PCPU_NUM; i++) {
		for (j = 0; j < 32; j++) {
			per_cpu_data[i].local_irq_desc[j].irq = j;
			per_cpu_data[i].local_irq_desc[j].arch_data = (void *)(&local_irq_data[i][j]);
			local_irq_data[i][j].type = HOST_IRQ;
		}
	}
}

/* must be called after IRQ setup */
void setup_irqs_arch(void)
{
}

void init_interrupt_arch(uint16_t pcpu_id)
{
	if (pcpu_id == 0) {
		gic_pre_init();
	}

	gic_init(pcpu_id);
}

void kick_pcpu(uint16_t pcpu_id)
{
	raise_sgi_one(pcpu_id, GIC_SGI_WAKEUP);
}
