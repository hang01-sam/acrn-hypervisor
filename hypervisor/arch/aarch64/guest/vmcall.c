/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/spinlock.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <acrn_hv_defs.h>
#include <hypercall.h>
#include <trace.h>
#include <logmsg.h>

static char guest_log_buf[1024];
static uint64_t guest_log_cnt = 0;

static int32_t dispatch_hypercall(__unused struct acrn_vcpu *vcpu)
{
	return 0; /* TODO */
}

/* Temporarily dump VM log only for debug purpose */
static void do_print_op(uint64_t vmid, char c)
{
	if (c == '\n' || c == '\r' || c == 0x0 || guest_log_cnt == 1023) {
		guest_log_buf[guest_log_cnt] = 0;
		printf("(Dom%ld) %s\n\r", vmid, guest_log_buf);
		guest_log_cnt = 0;
	} else {
		guest_log_buf[guest_log_cnt++] = c;
		ASSERT(guest_log_cnt < 1024, "guest_log_buf overflow!!!");
	}
}

int32_t ex_hvc64_handler(struct acrn_vcpu *vcpu,
						 struct intr_excp_ctx *regs, union sysreg_esr esr)
{
	int32_t ret = 0;;
	uint64_t hypcall_id = 0xFFFF;
	struct acrn_vm *vm = vcpu->vm;

	switch (esr.iss) {
	case 0xF1F2:
		/* speical case for dumping VM log */
		do_print_op(vm->vm_id, (char)regs->x[0]);
		break;
	case 0xFF00:
		/* speical case for getting VM ID */
		regs->x[0] = vm->vm_id;
		break;
	case 0xAC00:
		/* ACRN hypercall */
		hypcall_id = regs->x[16];
		ret = dispatch_hypercall(vcpu);
		break;
	default:
		pr_info("%s[%d] Unknown hypercall type!", __func__, __LINE__);
		ret = -ENOTTY;
		break;
	}

	TRACE_2L(TRACE_VMEXIT_VMCALL, vm->vm_id, hypcall_id);

	return ret;
}
