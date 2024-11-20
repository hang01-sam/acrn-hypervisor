/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <asm/per_cpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vm_reset.h>
#include <asm/guest/virq.h>
#include <asm/lib/bits.h>
#include <asm/e820.h>
#include <boot.h>
#include <reloc.h>
#include <asm/guest/ept.h>
#include <asm/guest/vgic.h>
#include <console.h>
#include <ptdev.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <logmsg.h>
#include <asm/board.h>
#include <sbuf.h>
#include <vacpi.h>
#include <asm/irq.h>
#include <uart16550.h>
#ifdef CONFIG_SECURITY_VM_FIXUP
#include <quirks/security_vm_fixup.h>
#endif
#include <delay.h>

/* Local variables */

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

static struct acrn_vm *service_vm_ptr = NULL;

uint16_t get_unused_vmid(void)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if ((vm_config->name[0] == '\0') && ((vm_config->guest_flags & GUEST_FLAG_STATIC_VM) == 0U)) {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}

uint16_t get_vmid_by_name(const char *name)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if ((*name != '\0') && vm_has_matched_name(vm_id, name)) {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}

/**
 * @pre vm != NULL
 */
bool is_poweroff_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_POWERED_OFF);
}

/**
 * @pre vm != NULL
 */
bool is_created_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_CREATED);
}

/**
 * @pre vm != NULL
 */
bool is_paused_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_PAUSED);
}

bool is_service_vm(const struct acrn_vm *vm)
{
	return (vm != NULL)  && (get_vm_config(vm->vm_id)->load_order == SERVICE_VM);
}

/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_postlaunched_vm(const struct acrn_vm *vm)
{
	return (get_vm_config(vm->vm_id)->load_order == POST_LAUNCHED_VM);
}


/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_prelaunched_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	return (vm_config->load_order == PRE_LAUNCHED_VM);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_irq_pt_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_pmu_pt_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_PMU_PASSTHROUGH) != 0U);
}


/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_rt_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_RT) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 *
 * Stateful VM refers to VM that has its own state (such as internal file cache),
 * and will experience state loss (file system corruption) if force powered down.
 */
bool is_stateful_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/* TEE VM doesn't has its own state. The TAs will do the content
	 * flush by themselves, HV and OS doesn't need to care about the state.
	 */
	return ((vm_config->guest_flags & GUEST_FLAG_TEE) == 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_nvmx_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_NVMX_ENABLED) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_vcat_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_VCAT_ENABLED) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_static_configured_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_STATIC_VM) != 0U);
}

/* Ignore this non-aarch64 feature */
bool is_pi_capable(__unused const struct acrn_vm *vm)
{
	return 0;
}

struct acrn_vm *get_highest_severity_vm(bool runtime)
{
	uint16_t vm_id, highest_vm_id = 0U;

	for (vm_id = 1U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (runtime && is_poweroff_vm(get_vm_from_vmid(vm_id))) {
			/* If vm is non-existed or shutdown, it's not highest severity VM */
			continue;
		}

		if (get_vm_severity(vm_id) > get_vm_severity(highest_vm_id)) {
			highest_vm_id = vm_id;
		}
	}

	return get_vm_from_vmid(highest_vm_id);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool vm_hide_mtrr(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_HIDE_MTRR) != 0U);
}

/**
 * @brief Initialize the I/O bitmap for \p vm
 *
 * @param vm The VM whose I/O bitmap is to be initialized
 */
static void setup_io_bitmap(struct acrn_vm *vm)
{
	if (is_service_vm(vm)) {
		(void)memset(vm->arch_vm.io_bitmap, 0x00U, PAGE_SIZE * 2U);
	} else {
		/* block all IO port access from Guest */
		(void)memset(vm->arch_vm.io_bitmap, 0xFFU, PAGE_SIZE * 2U);
	}
}

/**
 * return a pointer to the virtual machine structure associated with
 * this VM ID
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	return &vm_array[vm_id];
}

/* return a pointer to the virtual machine structure of Service VM */
struct acrn_vm *get_service_vm(void)
{
	ASSERT(service_vm_ptr != NULL, "service_vm_ptr is NULL");

	return service_vm_ptr;
}

/**
 * @pre vm_config != NULL
 */
static inline uint16_t get_configured_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	/*
	 * The set least significant bit represents the pCPU ID for BSP
	 * vm_config->cpu_affinity has been sanitized to contain valid pCPU IDs
	 */
	return ffs64(vm_config->cpu_affinity);
}

/**
 * @pre vm != NULL && vm_config != NULL
 */
static void prepare_prelaunched_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint32_t hpa_index;
	struct vm_hpa_regions tmp_vm_hpa;

	hpa_index = 0U;
	tmp_vm_hpa = vm_config->memory.host_regions[0];

	while (hpa_index < vm_config->memory.region_num) {
		/* add memory region to vm's e820 table */
		add_vm_e820_entry(vm, tmp_vm_hpa.start_hpa, tmp_vm_hpa.size_hpa, E820_TYPE_RAM);

		ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, tmp_vm_hpa.start_hpa,
				   tmp_vm_hpa.start_hpa, tmp_vm_hpa.size_hpa, EPT_VM_FLAGS);
		hpa_index++;
		tmp_vm_hpa = vm_config->memory.host_regions[hpa_index];

	}

}


/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @retval 0 on success
 *
 * @pre vm != NULL
 * @pre is_service_vm(vm) == true
 */
static void prepare_service_vm_memmap(struct acrn_vm *vm)
{
	uint32_t i;
	const struct e820_entry *entry;
	uint32_t entries_count = vm->e820_entry_num;

	pr_info("Service VM e820 layout:");
	for (i = 0U; i < entries_count; i++) {
		entry = &vm->e820_entries[i];
		pr_info("    BaseAddress: 0x%016lx length: 0x%016lx, type: 0x%x",
				entry->baseaddr, entry->length, entry->type);
		if (entry->type == E820_TYPE_RAM) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, entry->baseaddr,
					   entry->baseaddr, entry->length, EPT_VM_FLAGS);
		} else {
			/* TODO: such as adding PT device entry */
		}
	}
}

/**
 * @brief get bitmap of pCPUs whose vCPUs have LAPIC PT enabled
 *
 * @param[in] vm pointer to vm data structure
 * @pre vm != NULL
 *
 * @return pCPU bitmap
 */
static uint64_t lapic_pt_enabled_pcpu_bitmap(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint64_t bitmap = 0UL;

	if (is_irq_pt_configured(vm)) {
		foreach_vcpu(i, vm, vcpu) {
			bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &bitmap);
		}
	}

	return bitmap;
}

void prepare_vm_identical_memmap(struct acrn_vm *vm, uint16_t e820_entry_type, uint64_t prot_orig)
{
	const struct e820_entry *entry;
	const struct e820_entry *p_e820 = vm->e820_entries;
	uint32_t entries_count = vm->e820_entry_num;
	uint64_t *pml4_page = (uint64_t *)vm->arch_vm.nworld_eptp;
	uint32_t i;

	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if (entry->type == e820_entry_type) {
			ept_add_mr(vm, pml4_page, entry->baseaddr,
					   entry->baseaddr, entry->length,
					   prot_orig);
		}
	}
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM and should be trusty post-launched VM
 */
int32_t get_sworld_vm_index(uint16_t vm_id)
{
	int16_t i;
	int32_t vm_idx = MAX_TRUSTY_VM_NUM;
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);

	if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
		vm_idx = 0;

		for (i = 0; i < vm_id; i++) {
			vm_config = get_vm_config(i);
			if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
				vm_idx += 1;
			}
		}
	}

	if (vm_idx >= (int32_t)MAX_TRUSTY_VM_NUM) {
		pr_err("Can't find sworld memory for vm id: %d", vm_id);
		vm_idx = -EINVAL;
	}

	return vm_idx;
}

static int32_t prepare_os_image_addr(struct acrn_vm *vm)
{
	int32_t ret = -EINVAL;
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	vm->sw.kernel_type = vm_config->os_config.kernel_type;
	vm->sw.kernel_info.kernel_src_addr = (void *)vm_config->os_config.kernel_entry_addr;
	vm->sw.kernel_info.kernel_entry_addr = (void *)vm_config->os_config.kernel_entry_addr;
	vm->sw.kernel_info.kernel_size = vm_config->os_config.kernel_size;
	vm->sw.kernel_info.kernel_dtb_addr = (void *)vm_config->os_config.kernel_dtb_addr;
	ret = 0;

	return ret;
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL && rtn_vm != NULL
 * @pre vm->state == VM_POWERED_OFF
 */
int32_t create_vm(uint16_t vm_id, uint64_t pcpu_bitmap, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	struct acrn_vm *vm = NULL;
	int32_t status = 0;
	uint16_t pcpu_id;
	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	vm->vm_id = vm_id;
	vm->hw.created_vcpus = 0U;

	init_ept_pgtable(&vm->arch_vm.ept_pgtable, vm->vm_id);
	vm->arch_vm.nworld_eptp = pgtable_create_root(&vm->arch_vm.ept_pgtable);

	(void)memcpy_s(&vm->name[0], MAX_VM_NAME_LEN, &vm_config->name[0], MAX_VM_NAME_LEN);

	if (prepare_os_image_addr(vm)) {
		pr_err("Failed to prepare os image for VM!");
	}


	if (is_service_vm(vm)) {
		/* Only for Service VM */
		create_service_vm_e820(vm);
		prepare_service_vm_memmap(vm);
	} else {
		if (status == 0) {
			if (vm_config->name[0] == '\0') {
				/* if VM name is not configured, specify with VM ID */
				snprintf(vm_config->name, 16, "ACRN VM_%d", vm_id);
			}

			if (vm_config->load_order == PRE_LAUNCHED_VM) {
				/*
				 * If a prelaunched VM has the flag GUEST_FLAG_TEE set then it
				 * is a special prelaunched VM called TEE VM which need special
				 * memmap, e.g. mapping the REE VM into its space. Otherwise,
				 * just use the standard preplaunched VM memmap.
				 */
				if ((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U) {
					/* NULL */
				} else {
					create_prelaunched_vm_e820(vm);
					prepare_prelaunched_vm_memmap(vm, vm_config);
				}
			}
		}
	}

	if (status == 0) {
		spinlock_init(&vm->vlapic_mode_lock);
		spinlock_init(&vm->ept_lock);
		spinlock_init(&vm->emul_mmio_lock);
		spinlock_init(&vm->arch_vm.iwkey_backup_lock);

		vm->intr_inject_delay_delta = 0UL;
		vm->nr_emul_mmio_regions = 0U;

		/* Set up IO bit-mask such that VM exit occurs on
		 * selected IO ranges
		 */
		setup_io_bitmap(vm);

		/* init_guest_pm(vm); */ /* TODO later we need to open pm */


#ifdef CONFIG_SECURITY_VM_FIXUP
		passthrough_smbios(vm, get_acrn_boot_info());
#endif

		if (status == 0) {
			/* Create virtual uart */
			init_legacy_vuarts(vm, vm_config->vuart);

			register_reset_port_handler(vm);

			/* vpic wire_mode default is INTR */
			vm->wire_mode = VPIC_WIRE_INTR;

			/* Populate return VM handle */
			*rtn_vm = vm;
			if ((vm_config->load_order == POST_LAUNCHED_VM)
				&& ((vm_config->guest_flags & GUEST_FLAG_IO_COMPLETION_POLLING) != 0U)) {
				/* enable IO completion polling mode per its guest flags in vm_config. */
				vm->sw.is_polling_ioreq = true;
			}
			if (status == 0) {
				vm->state = VM_CREATED;
			}
		}
	}

	if (status == 0) {
		/* We have assumptions:
		 *   1) vcpus used by Service VM has been offlined by DM before User VM re-use it.
		 *   2) pcpu_bitmap passed sanitization is OK for vcpu creating.
		 */
		vm->hw.cpu_affinity = pcpu_bitmap;

		uint64_t tmp64 = pcpu_bitmap;
		while (tmp64 != 0UL) {
			pcpu_id = ffs64(tmp64);
			bitmap_clear_nolock(pcpu_id, &tmp64);
			status = prepare_vcpu(vm, pcpu_id);
			if (status != 0) {
				break;
			}
		}
	}
	if ((status != 0) && (vm->arch_vm.nworld_eptp != NULL)) {
		(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
	}

	if (!is_irq_pt_configured(vm)) {
		vgic_init(vm, vm_config->vgic);
	}

	return status;
}

static bool is_ready_for_system_shutdown(void)
{
	bool ret = true;
	uint16_t vm_id;
	struct acrn_vm *vm;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm = get_vm_from_vmid(vm_id);
		/* TODO: Update code to cover hybrid mode */
		if (!is_poweroff_vm(vm) && is_stateful_vm(vm)) {
			ret = false;
			break;
		}
	}

	return ret;
}

static int32_t offline_lapic_pt_enabled_pcpus(__unused const struct acrn_vm *vm, __unused uint64_t pcpu_mask)
{

	return 0;
}

/*
 * @pre vm != NULL
 * @pre vm->state == VM_PAUSED
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	uint16_t i;
	uint64_t mask;
	struct acrn_vcpu *vcpu = NULL;
	struct acrn_vm_config *vm_config = NULL;
	int32_t ret = 0;

	/* Only allow shutdown paused vm */
	vm->state = VM_POWERED_OFF;

	if (is_service_vm(vm)) {
		sbuf_reset();
	}

	deinit_emul_io(vm);

	/* Free EPT allocated resources assigned to VM */
	destroy_ept(vm);

	mask = lapic_pt_enabled_pcpu_bitmap(vm);
	if (mask != 0UL) {
		ret = offline_lapic_pt_enabled_pcpus(vm, mask);
	}

	foreach_vcpu(i, vm, vcpu) {
		offline_vcpu(vcpu);
	}

	/* after guest_flags not used, then clear it */
	vm_config = get_vm_config(vm->vm_id);
	vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;
	if (!is_static_configured_vm(vm)) {
		memset(vm_config->name, 0U, MAX_VM_NAME_LEN);
	}

	if (is_ready_for_system_shutdown()) {
		/* If no any guest running, shutdown system */
		shutdown_system();
	}

	/* Return status to caller */
	return ret;
}

/**
 * @pre vm != NULL
 * @pre vm->state == VM_CREATED
 */
void start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *bsp = NULL;

	vm->state = VM_RUNNING;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	bsp = vcpu_from_vid(vm, BSP_CPU_ID);

	launch_vcpu(bsp);
}

/**
 * @pre vm != NULL
 * @pre vm->state == VM_PAUSED
 */
int32_t reset_vm(struct acrn_vm *vm)
{
	uint16_t i;
	uint64_t mask;
	struct acrn_vcpu *vcpu = NULL;
	int32_t ret = 0;

	mask = lapic_pt_enabled_pcpu_bitmap(vm);
	if (mask != 0UL) {
		ret = offline_lapic_pt_enabled_pcpus(vm, mask);
	}

	if (is_service_vm(vm)) {
		(void)prepare_os_image_addr(vm);
	}

	foreach_vcpu(i, vm, vcpu) {
		reset_vcpu(vcpu, COLD_RESET);
	}

	reset_vm_ioreqs(vm);
	vm->arch_vm.iwkey_backup_status = 0UL;
	vm->state = VM_CREATED;

	return ret;
}

/**
 * @pre vm != NULL
 */
void poweroff_if_rt_vm(struct acrn_vm *vm)
{
	if (is_rt_vm(vm) && !is_paused_vm(vm) && !is_poweroff_vm(vm)) {
		vm->state = VM_READY_TO_POWEROFF;
	}
}

/**
 * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	/* For RTVM, we can only pause its vCPUs when it is powering off by itself */
	if (((!is_rt_vm(vm)) && (vm->state == VM_RUNNING)) ||
		((is_rt_vm(vm)) && (vm->state == VM_READY_TO_POWEROFF)) ||
		(vm->state == VM_CREATED)) {
		foreach_vcpu(i, vm, vcpu) {
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
		}
		vm->state = VM_PAUSED;
	}
}


static uint8_t loaded_pre_vm_nr = 0U;
/**
 * Prepare to create vm/vcpu for vm
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL
 */
int32_t prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config)
{
	int32_t err = 0;
	struct acrn_vm *vm = NULL;

	/*
	 * (Workaround) Delay some seconds before booting VM1 to make sure:
	 * UFS host driver initialiation done in Linux
	 */
	if (vm_id == 1) {
		udelay(1000 * 1000 * 10);
	}

#ifdef CONFIG_SECURITY_VM_FIXUP
	security_vm_fixup(vm_id);
#endif
	if (get_vmid_by_name(vm_config->name) != vm_id) {
		pr_err("Invalid VM name: %s", vm_config->name);
		err = -1;
	} else {
		/* Service VM and pre-launched VMs launch on all pCPUs defined in vm_config->cpu_affinity */
		err = create_vm(vm_id, vm_config->cpu_affinity, vm_config, &vm);
	}

	if (err == 0) {
		if (is_prelaunched_vm(vm)) {
			/* NULL */
		}

		if (is_service_vm(vm)) {
			/* We need to ensure all modules of pre-launched VMs have been loaded already
			 * before loading Service VM modules, otherwise the module of pre-launched VMs could
			 * be corrupted because Service VM kernel might pick any usable RAM to extract kernel
			 * when KASLR enabled.
			 * In case the pre-launched VMs aren't loaded successfuly that cause deadlock here,
			 * use a 10000ms timer to break the waiting loop.
			 */
			uint64_t start_tick = cpu_ticks();

			while (loaded_pre_vm_nr != PRE_VM_NUM) {
				uint64_t timeout = ticks_to_ms(cpu_ticks() - start_tick);

				if (timeout > 10000U) {
					pr_err("Loading pre-launched VMs timeout!");
					break;
				}
			}
		}

		if (is_prelaunched_vm(vm)) {
			loaded_pre_vm_nr++;
		}
	}

	return err;
}

/**
 * @pre vm_config != NULL
 * @Application constraint: The validity of vm_config->cpu_affinity should be guaranteed before run-time.
 */
void launch_vms(uint16_t pcpu_id)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		if (((vm_config->guest_flags & GUEST_FLAG_REE) != 0U) &&
			((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U)) {
			ASSERT(false, "%s: Wrong VM (VM id: %u) configuration, can't set both REE and TEE flags",
				   __func__, vm_id);
		}

		if ((vm_config->load_order == SERVICE_VM) || (vm_config->load_order == PRE_LAUNCHED_VM)) {
			if (pcpu_id == get_configured_bsp_pcpu_id(vm_config)) {
				if (vm_config->load_order == SERVICE_VM) {
					service_vm_ptr = &vm_array[vm_id];
				}

				/*
				 * We can only start a VM when there is no error in prepare_vm.
				 * Otherwise, print out the corresponding error.
				 *
				 * We can only start REE VM when get the notification from TEE VM.
				 * so skip "start_vm" here for REE, and start it in TEE hypercall
				 * HC_TEE_VCPU_BOOT_DONE.
				 */
				if (prepare_vm(vm_id, vm_config) == 0) {
					if ((vm_config->guest_flags & GUEST_FLAG_REE) != 0U) {
						/* Nothing need to do here, REE will start in TEE hypercall */
					} else {
						start_vm(get_vm_from_vmid(vm_id));
						pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
					}
				}
			}
		}
	}
}

/**
 * if there is RT VM return true otherwise return false.
 */
bool has_rt_vm(void)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (is_rt_vm(get_vm_from_vmid(vm_id))) {
			break;
		}
	}

	return (vm_id != CONFIG_MAX_VM_NUM);
}

void make_shutdown_vm_request(uint16_t pcpu_id)
{
	bitmap_set_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		kick_pcpu(pcpu_id);
	}
}

bool need_shutdown_vm(uint16_t pcpu_id)
{
	return bitmap_test_and_clear_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
}

/*
 * @pre vm != NULL
 */
void get_vm_lock(struct acrn_vm *vm)
{
	spinlock_obtain(&vm->vm_state_lock);
}
/*
 * @pre vm != NULL
 */
void put_vm_lock(struct acrn_vm *vm)
{
	spinlock_release(&vm->vm_state_lock);
}
