/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VM_CONFIG_H__
#define __AARCH64_VM_CONFIG_H__

#include <types.h>
#include <pci.h>
#include <board_info.h>
#include <boot.h>
#include <acrn_common.h>
#include <vm_configurations.h>
#include <acrn_hv_defs.h>
#include <schedule.h>

#define AFFINITY_CPU(n)		(1UL << (n))
#define MAX_VCPUS_PER_VM	MAX_PCPU_NUM
#define MAX_VM_OS_NAME_LEN	32U
#define MAX_MOD_TAG_LEN		32U

#if defined(CONFIG_SCHED_NOOP)
#define SERVICE_VM_IDLE		""
#elif defined(CONFIG_SCHED_PRIO)
#define SERVICE_VM_IDLE		""
#else
#define SERVICE_VM_IDLE		"idle=halt "
#endif

#define PCI_DEV_TYPE_PTDEV		(1U << 0U)
#define PCI_DEV_TYPE_HVEMUL		(1U << 1U)
#define PCI_DEV_TYPE_SERVICE_VM_EMUL	(1U << 2U)

#define MAX_MMIO_DEV_NUM	2U

#define CONFIG_SERVICE_VM	.load_order = SERVICE_VM,	\
				.severity = SEVERITY_SERVICE_VM

#define CONFIG_SAFETY_VM	.load_order = PRE_LAUNCHED_VM,	\
				.severity = SEVERITY_SAFETY_VM

#define CONFIG_PRE_STD_VM	.load_order = PRE_LAUNCHED_VM,	\
				.severity = SEVERITY_STANDARD_VM

#define CONFIG_PRE_RT_VM	.load_order = PRE_LAUNCHED_VM,	\
				.severity = SEVERITY_RTVM

#define CONFIG_POST_STD_VM	.load_order = POST_LAUNCHED_VM,	\
				.severity = SEVERITY_STANDARD_VM

#define CONFIG_POST_RT_VM	.load_order = POST_LAUNCHED_VM,	\
				.severity = SEVERITY_RTVM

/* Bitmask of guest flags that can be programmed by device model. Other bits are set by hypervisor only. */
#if (SERVICE_VM_NUM == 0)
#define DM_OWNED_GUEST_FLAG_MASK	0UL
#elif defined(CONFIG_RELEASE)
#define DM_OWNED_GUEST_FLAG_MASK	(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH \
					| GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)
#else
#define DM_OWNED_GUEST_FLAG_MASK	(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH \
					| GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING | GUEST_FLAG_PMU_PASSTHROUGH)
#endif

/* ACRN guest severity */
enum acrn_vm_severity {
	SEVERITY_SAFETY_VM = 0x40U,
	SEVERITY_RTVM = 0x30U,
	SEVERITY_SERVICE_VM = 0x20U,
	SEVERITY_STANDARD_VM = 0x10U,
};

struct vm_hpa_regions {
	uint64_t start_hpa;
	uint64_t size_hpa;
};

struct acrn_vm_mem_config {
	uint64_t size;		/* VM memory size configuration */
	uint64_t region_num;
	struct vm_hpa_regions  *host_regions;
};

struct target_vuart {
	uint8_t vm_id;		/* target VM id */
	uint8_t vuart_id;	/* target vuart index in a VM */
};

enum vuart_type {
	VUART_LEGACY_PIO = 0,	/* legacy PIO vuart */
	VUART_PCI,		/* PCI vuart, may removed */
	VUART_PL011,
};

union vuart_addr {
	uint16_t port_base;		/* addr for legacy type */
	struct {			/* addr for pci type */
		uint8_t f : 3;		/* BITs 0-2 */
		uint8_t d : 5;		/* BITs 3-7 */
		uint8_t b;		/* BITs 8-15 */
	} bdf;
	struct {
		uint64_t base;
		uint64_t size;
	};
};

struct vuart_config {
	enum vuart_type type;		/* legacy PIO or PCI  */
	union vuart_addr addr;		/* port addr if in legacy type, or bdf addr if in pci type */
	uint16_t irq;
	struct target_vuart t_vuart;	/* target vuart */
} __aligned(8);

struct vgic_config {
	uint64_t gicd_base;
	uint64_t gicr_base;
	uint64_t main_irq;
};

enum os_kernel_type {
	KERNEL_BZIMAGE = 1,
	KERNEL_RAWIMAGE,
	KERNEL_ELF,
	KERNEL_UNKNOWN,
};

struct acrn_vm_os_config {
	char name[MAX_VM_OS_NAME_LEN];			/* OS name, useful for debug */
	enum os_kernel_type kernel_type;		/* used for kernel specifc loading method */
	char kernel_mod_tag[MAX_MOD_TAG_LEN];		/* multiboot module tag for kernel */
	char ramdisk_mod_tag[MAX_MOD_TAG_LEN];		/* multiboot module tag for ramdisk */
	char bootargs[MAX_BOOTARGS_SIZE];		/* boot args/cmdline */
	uint64_t kernel_load_addr;
	uint64_t kernel_entry_addr;
	uint64_t kernel_size;
	uint64_t kernel_dtb_addr;
	uint64_t kernel_ramdisk_addr;
} __aligned(8);

struct acrn_vm_acpi_config {
	char acpi_mod_tag[MAX_MOD_TAG_LEN];		/* multiboot module tag for ACPI */
} __aligned(8);

struct pt_intx_config {
	uint32_t phys_gsi;	/* physical IOAPIC gsi to be forwarded to the VM */
	uint32_t virt_gsi;	/* virtual IOAPIC gsi triggered on the vIOAPIC */
} __aligned(8);

struct acrn_vm_config {
	enum acrn_vm_load_order load_order;		/* specify the load order of VM */
	char name[MAX_VM_NAME_LEN];				/* VM name identifier */
	uint8_t reserved[2];
	uint8_t severity;				/* severity of the VM */
	uint64_t cpu_affinity;				/* The set bits represent the pCPUs the vCPUs of
							 * the VM may run on.
							 */
	uint64_t guest_flags;				/* VM flags that we want to configure for guest
							 * Now we have two flags:
							 *	GUEST_FLAG_SECURE_WORLD_ENABLED
							 *	GUEST_FLAG_LAPIC_PASSTHROUGH
							 * We could add more guest flags in future;
							 */
	struct sched_params sched_params;		/* Scheduler params for vCPUs of this VM */
	uint16_t companion_vm_id;			/* The companion VM id for this VM */
	struct acrn_vm_mem_config memory;		/* memory configuration of VM */
	/*struct epc_section epc;*/				/* EPC memory configuration of VM */
	uint16_t pci_dev_num;				/* indicate how many PCI devices in VM */
	struct acrn_vm_os_config os_config;		/* OS information the VM */
	struct acrn_vm_acpi_config acpi_config;		/* ACPI config for the VM */

	/*
	 * below are variable length members (per build).
	 * Service VM can get the vm_configs[] array through hypercall, but Service VM may not
	 * need to parse these members.
	 */

	struct vuart_config vuart[MAX_VUART_NUM_PER_VM];/* vuart configuration for VM */
	struct vgic_config vgic;/* vgic configuration for VM */

	struct acrn_mmiodev mmiodevs[MAX_MMIO_DEV_NUM];

	uint16_t pt_intx_num; /* number of pt_intx_config entries pointed by pt_intx */
	struct pt_intx_config *pt_intx; /* stores the base address of struct pt_intx_config array */
} __aligned(8);

struct acrn_vm_config *get_vm_config(uint16_t vm_id);
uint8_t get_vm_severity(uint16_t vm_id);
bool vm_has_matched_name(uint16_t vmid, const char *name);

extern struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM];

#endif /* __AARCH64_VM_CONFIG_H__ */
