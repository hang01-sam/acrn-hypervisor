/*
 * Copyright (C) 2019-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/virq.h>
#include <asm/lib/bits.h>
#include <logmsg.h>
#include <asm/cpu_caps.h>
#include <asm/per_cpu.h>
#include <asm/init.h>
#include <asm/guest/vm.h>
#include <asm/mmu.h>
#include <lib/sprintf.h>
#include <asm/irq.h>
#include <console.h>
#include <asm/arch.h>
#include <asm/cpu.h>

/* This is to make sure the 16 bits vpid won't overflow */
#if ((CONFIG_MAX_VM_NUM * MAX_VCPUS_PER_VM) > 0xffffU)
#error "VM number or VCPU number are too big"
#endif

uint32_t pa_size[8] = {32, 36, 40, 42, 44, 48, 52, 56};

/* stack_frame is linked with the sequence of stack operation in arch_switch_to() */
struct stack_frame {
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	union {
		uint64_t x30;
		void *lr;
	};
};
uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg)
{
	if (reg > 30) {
		return 0;
	}
	return vcpu->regs.x[reg];
}

void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val)
{
	if (reg > 30) {
		return;
	}
	vcpu->regs.x[reg] = val;
}

/* for hv_main.c compile */
uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu)
{
	return vcpu_get_pc(vcpu);
}

uint64_t vcpu_get_pc(struct acrn_vcpu *vcpu)
{
	return vcpu->regs.elr_el2;
}

void vcpu_set_pc(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->regs.elr_el2 = val;
}

/* TODO: need prapare all registers for vcpu */
static struct cpu_regs init_vregs = {
	.hcr_el2 = 0, /* IMO, FMO, FB, VM, SWIO, PTW, TSC, TWI, ... */
	.sctlr_el1 = 0,
	.vmpidr_el2 = 0,
	.hyp_vttbr = 0,
};

void reset_vcpu_regs(struct acrn_vcpu *vcpu, __unused enum reset_mode mode)
{
	set_vcpu_regs(vcpu, &init_vregs);
}


/* As a vcpu reset internal API, DO NOT touch any vcpu state transition in this function. */
static void vcpu_reset_internal(struct acrn_vcpu *vcpu, enum reset_mode mode)
{
	int32_t i;

	vcpu->launched = false;
	vcpu->arch.exception_info.exception = VECTOR_INVALID;
	vcpu->arch.cur_context = NORMAL_WORLD;
	vcpu->arch.emulating_lock = false;

	reset_vcpu_regs(vcpu, mode);

	for (i = 0; i < VCPU_EVENT_NUM; i++) {
		reset_event(&vcpu->events[i]);
	}
}

struct acrn_vcpu *get_running_vcpu(uint16_t pcpu_id)
{
	struct thread_object *curr = sched_get_current(pcpu_id);
	struct acrn_vcpu *vcpu = NULL;

	if ((curr != NULL) && (!is_idle_thread(curr))) {
		vcpu = container_of(curr, struct acrn_vcpu, thread_obj);
	}

	return vcpu;
}

struct acrn_vcpu *get_ever_run_vcpu(uint16_t pcpu_id)
{
	return per_cpu(ever_run_vcpu, pcpu_id);
}

void set_vcpu_regs(struct acrn_vcpu *vcpu, struct cpu_regs *vcpu_regs)
{
	struct acrn_vm *vm;

	vm = vcpu->vm;

	vcpu->proc_regs.hcr_el2 = vcpu_regs->hcr_el2;
	vcpu->proc_regs.sctlr_el1 = vcpu_regs->sctlr_el1;
	vcpu->proc_regs.hyp_vttbr = ((uint64_t)vm->arch_vm.nworld_eptp) | \
								(((uint64_t)(vm->vm_id) & 0xff) << 48);
	vcpu->proc_regs.vmpidr_el2 = cpu_id_to_mpidr(vcpu->vcpu_id);

	vcpu->regs.spsr_el2 = 0x3C5;

	/* Set VCPU entry point to kernel entry */
	vcpu->regs.elr_el2 = (uint64_t)vm->sw.kernel_info.kernel_entry_addr;
	vcpu->regs.x[0] = (uint64_t)vm->sw.kernel_info.kernel_dtb_addr;

}

void init_vcpu_protect_mode_regs(__unused struct acrn_vcpu *vcpu, __unused uint64_t vgdt_base_gpa)
{
}

void set_vcpu_startup_entry(struct acrn_vcpu *vcpu, uint64_t entry)
{
	vcpu_set_pc(vcpu, entry);
}

/*
 * @pre vm != NULL && rtn_vcpu_handle != NULL
 */
int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle)
{
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;
	int32_t ret;

	pr_info("Creating VCPU working on PCPU%hu", pcpu_id);

	/*
	 * vcpu->vcpu_id = vm->hw.created_vcpus;
	 * vm->hw.created_vcpus++;
	 */
	vcpu_id = vm->hw.created_vcpus;
	if (vcpu_id < MAX_VCPUS_PER_VM) {
		/* Allocate memory for VCPU */
		vcpu = &(vm->hw.vcpu_array[vcpu_id]);
		(void)memset((void *)vcpu, 0U, sizeof(struct acrn_vcpu));

		/* Initialize CPU ID for this VCPU */
		vcpu->vcpu_id = vcpu_id;
		vcpu->pcpu_id = pcpu_id;
		per_cpu(ever_run_vcpu, pcpu_id) = vcpu;

		/* Initialize the parent VM reference */
		vcpu->vm = vm;

		/* Initialize the virtual ID for this VCPU */
		/* FIXME:
		 * We have assumption that we always destroys vcpus in one
		 * shot (like when vm is destroyed). If we need to support
		 * specific vcpu destroy on fly, this vcpu_id assignment
		 * needs revise.
		 */

		pr_info("Create VM%d-VCPU%d, Role: %s",
				vcpu->vm->vm_id, vcpu->vcpu_id,
				is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

		/*
		 * ACRN uses the following approach to manage VT-d PI notification vectors:
		 * Allocate unique Activation Notification Vectors (ANV) for each vCPU that
		 * belongs to the same pCPU, the ANVs need only be unique within each pCPU,
		 * not across all vCPUs. The max numbers of vCPUs may be running on top of
		 * a pCPU is CONFIG_MAX_VM_NUM, since ACRN does not support 2 vCPUs of same
		 * VM running on top of same pCPU. This reduces # of pre-allocated ANVs for
		 * posted interrupts to CONFIG_MAX_VM_NUM, and enables ACRN to avoid switching
		 * between active and wake-up vector values in the posted interrupt descriptor
		 * on vCPU scheduling state changes.
		 *
		 * We maintain a per-pCPU array of vCPUs, and use vm_id as the index to the
		 * vCPU array
		 */
		per_cpu(vcpu_array, pcpu_id)[vm->vm_id] = vcpu;

		/* Create per vcpu vgic */
		vgic_cpu_init(vcpu);

		/* Populate the return handle */
		*rtn_vcpu_handle = vcpu;
		vcpu_set_state(vcpu, VCPU_INIT);

		vcpu_reset_internal(vcpu, POWER_ON_RESET);
		(void)memset((void *)&vcpu->req, 0U, sizeof(struct io_request));
		vm->hw.created_vcpus++;
		ret = 0;
	} else {
		pr_err("%s, vcpu id is invalid!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static void flush_guest_tlb(void)
{
	isb();
	dsbsy();
	asm volatile("tlbi vmalls12e1is" : : : "memory");
	isb();
	dsbsy();
}

static void init_stage2_mmu(struct acrn_vcpu *vcpu)
{
	uint64_t vttbr, hcr, sctlr, vtcr;
	uint32_t pa_range;

	hcr = read_hcr_el2();
	write_hcr_el2(hcr & ~HCR_VM);
	isb();

	vttbr = vcpu->proc_regs.hyp_vttbr;
	write_vttbr_el2(vttbr);

	flush_guest_tlb();
	sctlr = read_sctlr_el1();
	sctlr &= ~(SCTLR_EOS | SCTLR_ENRCTX | SCTLR_C | SCTLR_M);

	write_sctlr_el1(sctlr);
	isb();

	hcr |= HCR_RW;
	write_hcr_el2(hcr);
	isb();

	pa_range = get_pa_range();
	if (pa_range > PA_RANGE_MAX) {
		panic("parange get error, please check system.");
	}

	vtcr = VTCR_RES1 | ((pa_range << VTCR_PS_OFFSET) & VTCR_PS_MASK) |
		   VTCR_TG0_4KB | VTCR_SH0_IS | VTCR_ORGN0_WBRAWAC | VTCR_IRGN0_WBRAWAC |
		   ((pa_size[pa_range] < 44) ? VTCR_SL0_LEVEL_12 : VTCR_SL0_LEVEL_01) |
		   VTCR_T0SZ(64 - pa_size[pa_range]);

	write_vtcr_el2(vtcr);
}

static void restore_vcpu_context(struct acrn_vcpu *vcpu)
{
	write_vmpidr_el2(vcpu->proc_regs.vmpidr_el2);

	init_stage2_mmu(vcpu);
}

extern void jump_to_vm(struct intr_excp_ctx *regs);

/*
 * @pre vcpu != NULL
 */
int32_t run_vcpu(struct acrn_vcpu *vcpu)
{
	int32_t status = 0;

	{
		/* If this VCPU is not already launched, launch it */
		if (!vcpu->launched) {
			pr_info("VM %d Starting VCPU %hu",
					vcpu->vm->vm_id, vcpu->vcpu_id);

#ifdef CONFIG_HYPERV_ENABLED
			if (is_vcpu_bsp(vcpu)) {
				hyperv_init_time(vcpu->vm);
			}
#endif
			/* init system register */
			restore_vcpu_context(vcpu);

			/* Set vcpu launched */
			vcpu->launched = true;

			pr_info("Jump to VCPU %u (spsr:0x%lx, vttbr:0x%lx, pc:0x%lx, x0:0x%lx)",
					vcpu->vcpu_id, vcpu->regs.spsr_el2, vcpu->proc_regs.hyp_vttbr,
					vcpu->regs.elr_el2, vcpu->regs.x[0]);

			isb();
			esb();

			jump_to_vm(&vcpu->regs);
			panic("Not Reachable");
		} else {
			/*currently support run launched VCPU*/
		}
	}
	return status;
}

/*
 *  @pre vcpu != NULL
 *  @pre vcpu->state == VCPU_ZOMBIE
 */
void offline_vcpu(struct acrn_vcpu *vcpu)
{
	per_cpu(ever_run_vcpu, pcpuid_from_vcpu(vcpu)) = NULL;

	/* This operation must be atomic to avoid contention with posted interrupt handler */
	per_cpu(vcpu_array, pcpuid_from_vcpu(vcpu))[vcpu->vm->vm_id] = NULL;

	vcpu_set_state(vcpu, VCPU_OFFLINE);
}

void kick_vcpu(struct acrn_vcpu *vcpu)
{
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	if ((get_pcpu_id() != pcpu_id)) {
		kick_pcpu(pcpu_id);
	}
}

static uint64_t build_stack_frame(struct acrn_vcpu *vcpu)
{
	uint64_t stacktop = (uint64_t)&vcpu->stack[CONFIG_STACK_SIZE - 1];
	struct stack_frame *frame;

	/* Reset stack content to special characator for debug purpose */
	(void)memset((void *)vcpu->stack, 0xFEU, CONFIG_STACK_SIZE);

	stacktop &= ~(CPU_STACK_ALIGN - 1UL);
	pr_info("vcpu%u stack: [0x%lx ~ 0x%lx]", vcpu->vcpu_id, (uint64_t)&vcpu->stack[0], stacktop);
	frame = (struct stack_frame *)stacktop;
	frame -= 1;

	frame->x30 = (uint64_t)vcpu->thread_obj.thread_entry; /*return address*/
	frame->x19 = 0UL;
	frame->x20 = 0UL;
	frame->x21 = 0UL;
	frame->x22 = 0UL;
	frame->x23 = 0UL;
	frame->x24 = 0UL;
	frame->x25 = 0UL;
	frame->x26 = 0UL;
	frame->x27 = 0UL;
	frame->x28 = 0UL;
	frame->x29 = 0UL;

	return (uint64_t)frame;
}

/* NOTE:
 * vcpu should be paused before call this function.
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_ZOMBIE
 */
void reset_vcpu(struct acrn_vcpu *vcpu, enum reset_mode mode)
{
	pr_dbg("vcpu%hu reset", vcpu->vcpu_id);

	vcpu_reset_internal(vcpu, mode);
	vcpu_set_state(vcpu, VCPU_INIT);
}

void zombie_vcpu(struct acrn_vcpu *vcpu, enum vcpu_state new_state)
{
	enum vcpu_state prev_state;
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	pr_dbg("vcpu%hu paused, new state: %d",	vcpu->vcpu_id, new_state);

	if (((vcpu->state == VCPU_RUNNING) || (vcpu->state == VCPU_INIT)) && (new_state == VCPU_ZOMBIE)) {
		prev_state = vcpu->state;
		vcpu_set_state(vcpu, new_state);

		if (prev_state == VCPU_RUNNING) {
			if (pcpu_id == get_pcpu_id()) {
				sleep_thread(&vcpu->thread_obj);
			} else {
				sleep_thread_sync(&vcpu->thread_obj);
			}
		}
	}
}

/* TODO:
 * Now we have switch_out and switch_in callbacks for each thread_object, and schedule
 * will call them every thread switch. We can implement lazy context swtich , which
 * only do context swtich when really need.
 */
static void context_switch_out(__unused struct thread_object *prev)
{
	/* TODO */
}

static void context_switch_in(__unused struct thread_object *next)
{
	/* TODO */
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_INIT
 */
void launch_vcpu(struct acrn_vcpu *vcpu)
{
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	pr_info("vcpu%hu scheduled on pcpu%hu", vcpu->vcpu_id, pcpu_id);
	vcpu_set_state(vcpu, VCPU_RUNNING);
	wake_thread(&vcpu->thread_obj);

}

/* help function for vcpu create */
int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id)
{
	int32_t ret, i;
	struct acrn_vcpu *vcpu = NULL;
	char thread_name[16];

	ret = create_vcpu(pcpu_id, vm, &vcpu);
	if (ret == 0) {
		snprintf(thread_name, 16U, "vm%hu:vcpu%hu", vm->vm_id, vcpu->vcpu_id);
		(void)strncpy_s(vcpu->thread_obj.name, 16U, thread_name, 16U);
		vcpu->thread_obj.sched_ctl = &per_cpu(sched_ctl, pcpu_id);
		vcpu->thread_obj.thread_entry = vcpu_thread;
		vcpu->thread_obj.pcpu_id = pcpu_id;
		vcpu->thread_obj.host_sp = build_stack_frame(vcpu);
		vcpu->thread_obj.switch_out = context_switch_out;
		vcpu->thread_obj.switch_in = context_switch_in;
		init_thread_data(&vcpu->thread_obj, NULL);
		for (i = 0; i < VCPU_EVENT_NUM; i++) {
			init_event(&vcpu->events[i]);
		}
	}

	return ret;
}

/**
 * @pre vcpu != NULL
 */
uint16_t pcpuid_from_vcpu(const struct acrn_vcpu *vcpu)
{
	return sched_get_pcpuid(&vcpu->thread_obj);
}

uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask)
{
	uint16_t vcpu_id;
	uint64_t dmask = 0UL;
	struct acrn_vcpu *vcpu;

	for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
		if ((vdmask & (1UL << vcpu_id)) != 0UL) {
			vcpu = vcpu_from_vid(vm, vcpu_id);
			bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &dmask);
		}
	}

	return dmask;
}
/*
 * @brief Update the state of vCPU
 *
 * The vlapic state of VM shall be updated for some vCPU
 * state update cases, such as from VCPU_INIT to VCPU_RUNNING.

 * @pre (vcpu != NULL)
 */
void vcpu_set_state(struct acrn_vcpu *vcpu, enum vcpu_state new_state)
{
	vcpu->state = new_state;
}
