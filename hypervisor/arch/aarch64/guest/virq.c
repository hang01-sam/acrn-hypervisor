/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/bits.h>
#include <asm/guest/virq.h>
#include <asm/gic.h>
#include <asm/mmu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <trace.h>
#include <logmsg.h>
#include <asm/irq.h>

void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid)
{
	bitmap_set_lock(eventid, &vcpu->arch.pending_req);
	kick_vcpu(vcpu);
}

/* Inject general protection exception(#GP) to guest */
void vcpu_inject_gp(__unused struct acrn_vcpu *vcpu, __unused uint32_t err_code)
{
	return;
}

/* Inject external interrupt to guest */
void vcpu_inject_extint(struct acrn_vcpu *vcpu)
{
	vcpu_make_request(vcpu, ACRN_REQUEST_EXTINT);
	signal_event(&vcpu->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
}

int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu)
{
	int32_t ret = 0;
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	uint64_t *pending_req_bits = &arch->pending_req;

	if (*pending_req_bits != 0UL) {

		if (bitmap_test_and_clear_lock(ACRN_REQUEST_VGIC_MSG, pending_req_bits)) {
			vcpu_vgic_msg_handler(vcpu);
		}
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_EPT_FLUSH, pending_req_bits)) {
			//need to implement invept in mmu
		}
	}

	return ret;
}

void vm_assign_virq(struct acrn_vm *vm, uint64_t id)
{
	struct irq_desc *desc = irq_to_desc(id);
	struct aarch64_irq_data *irq_data = (struct aarch64_irq_data *)desc->arch_data;

	irq_data->vm = vm;
	irq_data->type = GUEST_IRQ;

	vgic_set_hw(vm, id);
}

void interrupts_vm_inject(uint64_t id)
{
	struct irq_desc *desc = irq_to_desc(id);
	struct aarch64_irq_data *irq_data = (struct aarch64_irq_data *)desc->arch_data;
	struct acrn_vm *vm = (struct acrn_vm *)irq_data->vm;
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, 0);

	vgic_inject(vcpu, id, 0);
}
