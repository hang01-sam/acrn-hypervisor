/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/spinlock.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vsmc.h>
#include <acrn_hv_defs.h>
#include <trace.h>
#include <logmsg.h>
#include <asm/psci.h>

static int32_t handle_arm_smc(__unused struct acrn_vcpu *vcpu,
							  __unused struct intr_excp_ctx *regs, uint32_t ftype)
{
	int32_t ret = 0;

	switch (ftype) {
	default:
		pr_err("%s: Unknown ftype 0x%lx!", __func__, ftype);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static int32_t handle_standard_smc(struct acrn_vcpu *vcpu,
								   struct intr_excp_ctx *regs, uint32_t ftype)
{
	uint64_t mpidr, entry_point, target_vcpu_id;
	struct acrn_vcpu *target_vcpu;
	int32_t ret = 0;

	switch (ftype) {
	case PSCI_FN_VERSION:
		regs->x[0] = PSCI_VERSION_0_2;
		break;
	case PSCI_FN_MIGRATE_INFO_TYPE:
		regs->x[0] = PSCI_MIGRATE_INFO_2;
		break;
	case PSCI_FN_CPU_ON:
		mpidr = regs->x[1];
		entry_point = regs->x[2];
		target_vcpu_id = mpidr_to_cpu_id(mpidr);
		pr_dbg("%s: mpidr:0x%lx (vcpu%lu), entry_point:0x%lx)",
			   __func__, mpidr, target_vcpu_id, entry_point);

		/* Boot secondary CPU for VM */
		if (target_vcpu_id > 0 && target_vcpu_id < vcpu->vm->hw.created_vcpus) {
			target_vcpu = vcpu_from_vid(vcpu->vm, target_vcpu_id);
			set_vcpu_startup_entry(target_vcpu, entry_point);
			launch_vcpu(target_vcpu);
			/* Return Success */
			regs->x[0] = PSCI_SUCCESS;
		} else {
			/* Return Invalid Parameters (vcpu not exit) */
			regs->x[0] = PSCI_INVALID_PARAMS;
		}
		break;
	case PSCI_FN_SYSTEM_RESET:
		pr_err("%s: SYSTEM_RESET is not supported!", __func__);
		ret = -ENOTTY;
		break;
	default:
		pr_err("%s: Unknown ftype 0x%lx!", __func__, ftype);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static int32_t handle_sip_smc(__unused struct acrn_vcpu *vcpu,
							  __unused  struct intr_excp_ctx *regs, uint32_t ftype)
{
	int32_t ret = 0;

	switch (ftype) {
	default:
		smc_call((struct smc_regs *)&regs->x[0]);
		if (regs->x[0] != PSCI_SUCCESS) {
			pr_err("%s: ftype 0x%lx failed (ret=0x%lx)!", __func__, ftype, regs->x[0]);
		}
		break;
	}

	return ret;
}

static int32_t handle_trustos_smc(struct acrn_vcpu *vcpu,
								  struct intr_excp_ctx *regs, uint32_t ftype)
{
	int32_t ret = 0;

	/* Spec asks VMID in x[7] */
	/* Provencore Secure OS asks VMID starts from 2 */
	regs->x[7] = vcpu->vm->vm_id + 2;

	switch (ftype) {
	default:
		smc_call((struct smc_regs *)&regs->x[0]);
		if (regs->x[0] != PSCI_SUCCESS) {
			pr_err("%s: ftype 0x%lx failed (ret=0x%lx)!", __func__, ftype, regs->x[0]);
		}
		break;
	}

	return ret;
}

int32_t ex_smc64_handler(struct acrn_vcpu *vcpu,
						 struct intr_excp_ctx *regs, __unused union sysreg_esr esr)
{
	int32_t ret = 0;
	uint32_t func_id = (uint32_t)regs->x[0];
	uint32_t owner;
	uint32_t func_type;

	owner = (func_id & SMC_OWNER_MASK) >> SMC_OWNER_SHIFT;
	func_type = (func_id & SMC_FUNCTION_MASK);
	pr_dbg("%s: fid:0x%lx, owner:0x%lx!", __func__, func_id, owner);
	switch (owner) {
	case SMC_OWNER_ARM:
		ret = handle_arm_smc(vcpu, regs, func_type);
		break;
	case SMC_OWNER_STANDARD_SMC:
		ret = handle_standard_smc(vcpu, regs, func_type);
		break;
	case SMC_OWNER_SIP:
		ret = handle_sip_smc(vcpu, regs, func_type);
		break;
	case SMC_OWNER_TRUST_OS_START ... SMC_OWNER_TRUST_OS_END:
		ret = handle_trustos_smc(vcpu, regs, func_type);
		break;
	default:
		pr_err("%s: Unknown owner 0x%lx!", __func__, func_type);
		ret = -ENOTTY;
		break;
	}

	return ret;
}
