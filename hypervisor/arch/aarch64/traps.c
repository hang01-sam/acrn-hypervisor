/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <common/irq.h>
#include <asm/sysreg.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <asm/cpu_asm.h>
#include <asm/gic.h>
#include <asm/traps.h>
#include <asm/arch.h>
#include <asm/guest/vsmc.h>
#include <lib/errno.h>
#include <asm/guest/vmexit.h>

void traps_init(void)
{
	write_vbar_el2((uint64_t)hyp_entry);
	write_hcr_el2(HCR_PTW | HCR_BSU_INNER_SH |
				  HCR_AMO | HCR_IMO | HCR_FMO | HCR_VM |
				  HCR_TACR | HCR_SWIO | HCR_TIDCP |
				  HCR_FB | HCR_RW | HCR_TSC);
}

static void show_backtrace(struct intr_excp_ctx *regs, uint64_t sp)
{
	uint64_t *frame, next_fp, next_addr, stack_low, stack_top, i;
	union sysreg_esr esr;

	pr_err("call trace:");
	pr_err("    [%#lx] (PC)", regs->elr_el2);
	pr_err("    [%#lx] (LR)", regs->x[30]);

	stack_low = sp;
	stack_top = (stack_low & (~(CONFIG_STACK_SIZE - 1))) + CONFIG_STACK_SIZE - 1;
	next_fp = regs->x[29];
	while (next_fp >= stack_low && next_fp < stack_top) {
		frame = (uint64_t *)next_fp;
		next_fp  = frame[0];
		next_addr  = frame[1];
		stack_low = (uint64_t)&frame[1];
		pr_err("    [%#lx]", next_addr);
	}

	esr.value = regs->esr_el2;

	pr_err("Registers:");
	pr_err("    [sp] 0x%016lx", sp);
	pr_err("    [spsr_el2] 0x%016lx", regs->spsr_el2);
	pr_err("    [esr_el2]  0x%016lx (EC:0x%x ISS:0x%x)", regs->esr_el2, esr.ec, esr.iss);
	for (i = 0; i < 30; i = i + 2) {
		pr_err("    [x%lu-x%lu] 0x%016lx 0x%016lx", i, i + 1,
			   regs->x[i], regs->x[i + 1]);
	}
	pr_err("    [x30] 0x%016lx", regs->x[30]);
}

static inline uint64_t get_hyp_fault_ipa(uint64_t gvaddr)
{
	return ((read_hpfar_el2() & HPFAR_MASK) << 8) | (gvaddr & ~PAGE_MASK);
}

static inline void jump_to_next_pc(struct intr_excp_ctx *regs, const union sysreg_esr esr)
{
	regs->elr_el2 += esr.il ? INSTRUCTION_32_BIT : INSTRUCTION_16_BIT;
}

static inline int translate_gva_to_ipa(uint64_t vaddr, uint64_t *paddr)
{
	uint64_t ipaddr;
	uint64_t saved_paddr = read_par_el1();

	asm volatile("at s1e1r, %0;" : : "r"(vaddr));
	isb();
	ipaddr = read_par_el1();

	write_par_el1(saved_paddr);

	if (ipaddr & PADDR_F) {
		return -EFAULT;
	}

	*paddr = (ipaddr & PADDR_MASK & PAGE_MASK) | (vaddr & ~PAGE_MASK);
	return 0;
}

static uint32_t mmio_size[4] = {1, 2, 4, 8};
static uint64_t mmio_size_mask[4] = {0xffU, 0xffffU, 0xffffffffU, 0xffffffffffffffffUL};

static int32_t ex_data_abort_handler(struct acrn_vcpu *vcpu,
									 struct intr_excp_ctx *regs, union sysreg_esr esr)
{
	uint64_t  dabt = esr.value;
	uint32_t  dfsc = (dabt & ESR_DABT_DFSC_MASK) >> ESR_DABT_DFSC_SHIFT;
	uint32_t  srt  = (dabt & ESR_DABT_SRT_MASK) >> ESR_DABT_SRT_SHIFT;
	uint32_t  sas  = (dabt & ESR_DABT_SAS_MASK) >> ESR_DABT_SAS_SHIFT;
	uint32_t  sse  = (dabt & ESR_DABT_SSE_MASK) >> ESR_DABT_SSE_SHIFT;
	struct    io_request io_req;
	uint64_t  gvaddr, gpaddr;
	uint16_t  size = 0;
	int32_t   ret = 0;

	if (dabt & ESR_DABT_EA_MASK) {
		/* External Abort */
		return -EFAULT;
	}

	gvaddr = read_far_el2();

	if (dabt & ESR_DABT_S1PTW_MASK) {
		gpaddr = get_hyp_fault_ipa(gvaddr);
	} else {
		ret = translate_gva_to_ipa(gvaddr, &gpaddr);
		if (ret == -EFAULT) {
			return ret;
		}
	}

	switch (dfsc) {
	case TRANSLATION_FAULT_LEVEL_0:
	case TRANSLATION_FAULT_LEVEL_1:
	case TRANSLATION_FAULT_LEVEL_2:
	case TRANSLATION_FAULT_LEVEL_3:
		if (!(dabt & ESR_DABT_ISV_MASK)) {
			pr_info("%s: invalid syndrome esr 0x%x", __func__, esr.value);
			return -EINVAL;
		}
		io_req.io_type = ACRN_IOREQ_TYPE_MMIO;
		io_req.reqs.mmio_request.address = gpaddr;
		io_req.reqs.mmio_request.size = mmio_size[sas];
		io_req.reqs.mmio_request.direction = (dabt & ESR_DABT_WnR_MASK) ? ACRN_IOREQ_DIR_WRITE : ACRN_IOREQ_DIR_READ;
		if (dabt & ESR_DABT_WnR_MASK) {
			io_req.reqs.mmio_request.value = (srt == 31) ? 0 : regs->x[srt] & mmio_size_mask[sas];
		}

		if (emulate_io(vcpu, &io_req)) {
			pr_info("%s[%d]: mmio failed", __func__, __LINE__);
		} else {
			if (!(dabt & ESR_DABT_WnR_MASK)) {
				regs->x[srt] = io_req.reqs.mmio_request.value & mmio_size_mask[sas];
				size = (1 << sas) * 8;
				if (sse && (regs->x[srt] & (1UL << (size - 1)))) {
					regs->x[srt] |= (~0UL) << size;
				}
			}
			jump_to_next_pc(regs, esr);
		}
		break;
	default:
		pr_info("Unsupported DFSC: HSR=%#x DFSC=%#x\n",
				esr.value, dfsc);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static void enter_from_hyp(struct intr_excp_ctx *regs)
{
	regs->elr_el2 = read_elr_el2();
	regs->spsr_el2 = read_spsr_el2();
	regs->esr_el2 = read_esr_el2();
}

static void exit_to_hyp(struct intr_excp_ctx *regs)
{
	disable_irq();
	write_elr_el2(regs->elr_el2);
	write_spsr_el2(regs->spsr_el2);
	write_esr_el2(regs->esr_el2);
}

static void enter_from_guest(struct intr_excp_ctx *regs)
{
	regs->elr_el2 = read_elr_el2();
	regs->spsr_el2 = read_spsr_el2();
	regs->esr_el2 = read_esr_el2();
	regs->sp_el1 = read_sp_el1();
	regs->sp_el0 = read_sp_el0();
}

static void exit_to_guest(struct intr_excp_ctx *regs)
{
	disable_irq();
	write_elr_el2(regs->elr_el2);
	write_spsr_el2(regs->spsr_el2);
	write_esr_el2(regs->esr_el2);
	write_sp_el1(regs->sp_el1);
	write_sp_el0(regs->sp_el0);
}

void ex_hyp_synchronous(struct intr_excp_ctx *regs)
{
	union sysreg_esr esr;
	uint64_t far_el2;
	int ret = 0;

	enter_from_hyp(regs);

	/* Inherit_bits DAIF status */
	if ((regs->spsr_el2 & SPSR_DEBUG_EX_MASK) == 0) {
		enable_debug_exceptions();
	}
	if ((regs->spsr_el2 & SPSR_SERROR_INTRRUPT_MASK) == 0) {
		enable_serror();
	}
	if ((regs->spsr_el2 & SPSR_IRQ_INTRRUPT_MASK) == 0) {
		enable_irq();
	}
	if ((regs->spsr_el2 & SPSR_FIQ_INTRRUPT_MASK) == 0) {
		enable_fiq();
	}

	far_el2 = read_far_el2();
	esr.value = regs->esr_el2;

	switch (esr.ec) {
	case ESR_EC_DABT_CUR_EL:
		pr_info("%s[%d] DATA_ABORT (0x%lx)", __func__, __LINE__, far_el2);
		ret = -EFAULT;
		break;
	case ESR_EC_INBT_CUR_EL:
		pr_info("%s[%d] INST_ABORT (0x%lx)", __func__, __LINE__, far_el2);
		ret = -EFAULT;
		break;
	default:
		pr_info("%s[%d] Unknown Hypervisor Abort!", __func__, __LINE__);
		ret = -ENOTTY;
		break;
	}

	if (ret) {
		show_backtrace(regs, (uint64_t)regs + ASMOFF_EXCP_SIZE);
		asm_hlt();
	}

	exit_to_hyp(regs);
}

static void ex_sys_inst_handler(struct acrn_vcpu *vcpu, struct intr_excp_ctx *regs, union sysreg_esr esr)
{
	switch (esr.value & ESR_SYS_INST_MASK) {
	/* TODO: add more */
	case ESR_SYS_INST_ICC_SGI1R_EL1:
	case ESR_SYS_INST_ICC_ASGI1R_EL1:
	case ESR_SYS_INST_ICC_SGI0R_EL1:
		if (!vgic_emulate(vcpu, regs, esr)) {
			/* return inject_undef64_exception(regs, hsr.len); */
		};
		break;
	}
}

void ex_guest_synchronous(struct intr_excp_ctx *regs)
{
	union sysreg_esr esr;
	struct acrn_vcpu *vcpu;
	int32_t ret = 0;

	enter_from_guest(regs);

	/* Clear AI mask */
	if ((regs->spsr_el2 & SPSR_SERROR_INTRRUPT_MASK) == 0) {
		enable_serror();
	}
	if ((regs->spsr_el2 & SPSR_IRQ_INTRRUPT_MASK) == 0) {
		enable_irq();
	}

	/* support vcpu_set_pc and vcpu_get_pc APIs to work */
	vcpu = get_running_vcpu(get_pcpu_id());
	vcpu->regs.elr_el2 = regs->elr_el2;

	esr.value = regs->esr_el2;

	switch (esr.ec) {
	case ESR_EC_HVC_AARCH64:
		ret = ex_hvc64_handler(vcpu, regs, esr);
		break;
	case ESR_EC_DABT_LOWER_EL:
		ret = ex_data_abort_handler(vcpu, regs, esr);
		break;
	case ESR_EC_SMC_AARCH64:
		ret = ex_smc64_handler(vcpu, regs, esr);
		jump_to_next_pc(regs, esr);
		break;
	case ESR_EC_SYS_INST:
		ex_sys_inst_handler(vcpu, regs, esr);
		jump_to_next_pc(regs, esr);
		break;
	default:
		pr_info("%s[%d] Unknown VM Abort!", __func__, __LINE__);
		ret = -ENOTTY;
		break;
	}

	if (ret) {
		show_backtrace(regs, (uint64_t)regs + ASMOFF_EXCP_SIZE);
		asm_hlt();
	}

	exit_to_guest(regs);
}

void ex_hyp_irq(struct intr_excp_ctx *regs)
{
	enter_from_hyp(regs);

	/* Inherit_bits DAF status and Keep I masked */
	if ((regs->spsr_el2 & SPSR_DEBUG_EX_MASK) == 0) {
		enable_debug_exceptions();
	}
	if ((regs->spsr_el2 & SPSR_SERROR_INTRRUPT_MASK) == 0) {
		enable_serror();
	}
	if ((regs->spsr_el2 & SPSR_FIQ_INTRRUPT_MASK) == 0) {
		enable_fiq();
	}

	gic_irq_hdl();

	exit_to_hyp(regs);
}

void ex_guest_irq(struct intr_excp_ctx *regs)
{
	struct acrn_vcpu *vcpu;

	enter_from_guest(regs);

	/* Clear A mask */
	if ((regs->spsr_el2 & SPSR_SERROR_INTRRUPT_MASK) == 0) {
		enable_serror();
	}

	/* support vcpu_set_pc and vcpu_get_pc APIs to work */
	vcpu = get_running_vcpu(get_pcpu_id());
	vcpu->regs.elr_el2 = regs->elr_el2;

	gic_irq_hdl();

	exit_to_guest(regs);
}

void ex_hyp_serror(struct intr_excp_ctx *regs)
{
	enter_from_hyp(regs);
	pr_info("%s[%d]", __func__, __LINE__);
	show_backtrace(regs, (uint64_t)regs + ASMOFF_EXCP_SIZE);
	asm_hlt();
	exit_to_hyp(regs);
}

void ex_guest_serror(struct intr_excp_ctx *regs)
{
	struct acrn_vcpu *vcpu;
	uint64_t hcr;

	enter_from_guest(regs);

	/* support vcpu_set_pc and vcpu_get_pc APIs to work */
	vcpu = get_running_vcpu(get_pcpu_id());
	vcpu->regs.elr_el2 = regs->elr_el2;

	pr_err("%s[%d] Inject Serror to guest!", __func__, __LINE__);
	hcr = read_hcr_el2() | HCR_VSE;
	write_hcr_el2(hcr);

	exit_to_guest(regs);
}

void ex_hyp_invalid(struct intr_excp_ctx *regs, __unused int index)
{
	enter_from_hyp(regs);
	pr_info("%s[%d]", __func__, __LINE__);
	show_backtrace(regs, (uint64_t)regs + ASMOFF_EXCP_SIZE);
	asm_hlt();
	exit_to_hyp(regs);
}

void ex_guest_invalid(struct intr_excp_ctx *regs, __unused int index)
{
	struct acrn_vcpu *vcpu;

	enter_from_guest(regs);

	/* support vcpu_set_pc and vcpu_get_pc APIs to work */
	vcpu = get_running_vcpu(get_pcpu_id());
	vcpu->regs.elr_el2 = regs->elr_el2;

	pr_info("%s[%d]", __func__, __LINE__);
	show_backtrace(regs, (uint64_t)regs + ASMOFF_EXCP_SIZE);
	asm_hlt();

	exit_to_guest(regs);
}
