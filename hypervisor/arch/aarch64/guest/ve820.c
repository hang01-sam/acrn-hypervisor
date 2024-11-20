/*
 * Copyright (C) 2019-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/e820.h>
#include <asm/mmu.h>
#include <asm/guest/vm.h>
#include <asm/guest/ept.h>
#include <reloc.h>
#include <logmsg.h>

#define ENTRY_GPA_L		2U
#define ENTRY_GPA_HI		8U

static struct e820_entry service_vm_e820[E820_MAX_ENTRIES];
static struct e820_entry pre_vm_e820[PRE_VM_NUM][E820_MAX_ENTRIES];

uint64_t find_space_from_ve820(struct acrn_vm *vm, uint32_t size, uint64_t min_addr, uint64_t max_addr)
{
	int32_t i;
	uint64_t gpa = INVALID_GPA;
	uint64_t round_min_addr = round_page_up(min_addr);
	uint64_t round_max_addr = round_page_down(max_addr);
	uint32_t round_size = round_page_up(size);

	for (i = (int32_t)(vm->e820_entry_num - 1U); i >= 0; i--) {
		struct e820_entry *entry = vm->e820_entries + i;
		uint64_t start, end, length;

		start = round_page_up(entry->baseaddr);
		end = round_page_down(entry->baseaddr + entry->length);
		length = (end > start) ? (end - start) : 0UL;

		if ((entry->type == E820_TYPE_RAM) && (length >= (uint64_t)round_size)
			&& (end > round_min_addr) && (start < round_max_addr)) {
			if (((start >= min_addr) && ((start + round_size) <= min(end, round_max_addr)))
				|| ((start < min_addr) && ((min_addr + round_size) <= min(end, round_max_addr)))) {
				gpa = (end > round_max_addr) ? (round_max_addr - round_size) : (end - round_size);
				break;
			}
		}
	}
	return gpa;
}

/* a sorted VM e820 table is critical for memory allocation or slot location,
 * for example, put ramdisk at the end of TOLUD(Top of LOW Usable DRAM) and
 * put kernel at its begining so that provide largest load capicity for them.
 */
static void sort_vm_e820(struct acrn_vm *vm)
{
	uint32_t i, j;
	struct e820_entry tmp_entry;

	/* Bubble sort */
	for (i = 0U; i < (vm->e820_entry_num - 1U); i++) {
		for (j = 0U; j < (vm->e820_entry_num - i - 1U); j++) {
			if (vm->e820_entries[j].baseaddr > vm->e820_entries[j + 1U].baseaddr) {
				tmp_entry = vm->e820_entries[j];
				vm->e820_entries[j] = vm->e820_entries[j + 1U];
				vm->e820_entries[j + 1U] = tmp_entry;
			}
		}
	}
}

static void filter_mem_from_service_vm_e820(struct acrn_vm *vm, uint64_t start_pa, uint64_t end_pa)
{
	uint32_t i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint32_t entries_count = vm->e820_entry_num;
	struct e820_entry *entry, new_entry = {0};

	for (i = 0U; i < entries_count; i++) {
		entry = &service_vm_e820[i];
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if ((entry->type != E820_TYPE_RAM) || (entry_end <= start_pa) || (entry_start >= end_pa)) {
			continue;
		}

		/* filter out the specific memory and adjust length of this entry*/
		if ((entry_start < start_pa) && (entry_end <= end_pa)) {
			entry->length = start_pa - entry_start;
			continue;
		}

		/* filter out the specific memory and need to create a new entry*/
		if ((entry_start < start_pa) && (entry_end > end_pa)) {
			entry->length = start_pa - entry_start;
			new_entry.baseaddr = end_pa;
			new_entry.length = entry_end - end_pa;
			new_entry.type = E820_TYPE_RAM;
			continue;
		}

		/* This entry is within the range of specific memory
		 * change to E820_TYPE_RESERVED
		 */
		if ((entry_start >= start_pa) && (entry_end <= end_pa)) {
			entry->type = E820_TYPE_RESERVED;
			continue;
		}

		if ((entry_start >= start_pa) && (entry_start < end_pa) && (entry_end > end_pa)) {
			entry->baseaddr = end_pa;
			entry->length = entry_end - end_pa;
			continue;
		}
	}

	if (new_entry.length > 0UL) {
		entries_count++;
		ASSERT(entries_count <= E820_MAX_ENTRIES, "e820 entry overflow");
		entry = &service_vm_e820[entries_count - 1U];
		entry->baseaddr = new_entry.baseaddr;
		entry->length = new_entry.length;
		entry->type = new_entry.type;
		vm->e820_entry_num = entries_count;
	}

}

/**
 * before boot Service VM, call it to hide HV and prelaunched VM memory in e820 table from Service VM
 *
 * @pre vm != NULL
 */
void create_service_vm_e820(struct acrn_vm *vm)
{
	uint16_t vm_id;
	uint32_t i;
	uint64_t hv_start_pa = hva2hpa((void *)(get_hv_image_base()));
	uint64_t hv_end_pa  = hv_start_pa + get_hv_ram_size();
	uint32_t entries_count = get_e820_entries_count();
	struct acrn_vm_config *service_vm_config = get_vm_config(vm->vm_id);

	(void)memcpy_s((void *)service_vm_e820, entries_count * sizeof(struct e820_entry),
				   (const void *)get_e820_entry(), entries_count * sizeof(struct e820_entry));

	vm->e820_entry_num = entries_count;
	vm->e820_entries = service_vm_e820;
	/* filter out hv memory from e820 table */
	filter_mem_from_service_vm_e820(vm, hv_start_pa, hv_end_pa);

	/* filter out prelaunched vm memory from e820 table */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		struct acrn_vm_config *vm_config = get_vm_config(vm_id);

		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			for (i = 0; i < vm_config->memory.region_num; i++) {
				filter_mem_from_service_vm_e820(vm, vm_config->memory.host_regions[i].start_hpa,
												vm_config->memory.host_regions[i].start_hpa
												+ vm_config->memory.host_regions[i].size_hpa);
			}
		}
	}

	for (i = 0U; i < vm->e820_entry_num; i++) {
		struct e820_entry *entry = &service_vm_e820[i];
		if (entry->type == E820_TYPE_RAM) {
			service_vm_config->memory.size += entry->length;
		}
	}
	sort_vm_e820(vm);
}

/**
 * @pre vm != NULL
 *
 * ve820 layout for pre-launched VM:
 */

void create_prelaunched_vm_e820(struct acrn_vm *vm)
{

	vm->e820_entries = pre_vm_e820[vm->vm_id];
	vm->e820_entry_num = 0;
}

void add_vm_e820_entry(struct acrn_vm *vm, uint64_t gpa, uint64_t length, uint64_t type)
{
	struct e820_entry *entry;

	entry = &vm->e820_entries[vm->e820_entry_num];
	entry->baseaddr = gpa;
	entry->length = length;
	entry->type = type;
	vm->e820_entry_num++;

	sort_vm_e820(vm);
	/* dump_e820_entries(&vm->e820_entries[0]); */
}
