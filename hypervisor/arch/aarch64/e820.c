/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>
#include <asm/page.h>
#include <asm/e820.h>
#include <asm/mmu.h>
#include <boot.h>
#include <reloc.h>
#include <logmsg.h>
#include <asm/guest/ept.h>

#define DBG_LEVEL_E820	6U

#if ((CONFIG_HV_RAM_START & (MEM_2M - 1UL)) != 0UL)
#error "CONFIG_HV_RAM_START must be aligned to 2MB"
#endif

/*
 * e820.c contains the related e820 operations; like HV to get memory info for its MMU setup;
 * and hide HV memory from Service VM...
 */

extern uint8_t      ld_ram_start;
extern uint8_t      ld_ram_end;

static uint32_t hv_e820_entries_nr;
static uint64_t hv_e820_ram_size;
extern struct acrn_mem_info meminfo_list[];

/* Describe the dynamical memory layout the hypervisor uses */
static struct e820_entry hv_e820[E820_MAX_ENTRIES];
/* Describe the initial memory layout the hypervisor uses */
static struct e820_entry initial_hv_e820[E820_MAX_ENTRIES];

/*
 * @brief reserve some RAM, hide it from Service VM, return its start address
 *
 * e820_alloc_memory requires 4k alignment, so size_arg will be converted
 * in the function.
 *
 * @param size_arg Amount of memory to be found and marked reserved
 * @param max_addr Maximum address below which memory is to be identified
 *
 * @pre hv_e820_entries_nr > 0U
 * @return base address of the memory region
 */
uint64_t e820_alloc_memory(uint64_t size_arg, uint64_t max_addr)
{
	int32_t i;
	uint64_t size = round_page_up(size_arg);
	uint64_t ret = INVALID_HPA;
	struct e820_entry *entry, *new_entry;

	for (i = (int32_t)hv_e820_entries_nr - 1; i >= 0; i--) {
		entry = &hv_e820[i];
		uint64_t start, end, length;

		start = round_page_up(entry->baseaddr);
		end = round_page_down(entry->baseaddr + entry->length);
		length = (end > start) ? (end - start) : 0UL;

		if ((entry->type == E820_TYPE_RAM) && (length >= size) && ((start + size) <= max_addr)) {


			/* found exact size of e820 entry */
			if (length == size) {
				entry->type = E820_TYPE_RESERVED;
				ret = start;
			} else {

				/*
				 * found entry with available memory larger than requested (length > size)
				 * Reserve memory if
				 * 1) hv_e820_entries_nr < E820_MAX_ENTRIES
				 * 2) if end of this "entry" is <= max_addr
				 *    use memory from end of this e820 "entry".
				 */

				if ((hv_e820_entries_nr < E820_MAX_ENTRIES) && (end <= max_addr)) {

					new_entry = &hv_e820[hv_e820_entries_nr];
					new_entry->type = E820_TYPE_RESERVED;
					new_entry->baseaddr = end - size;
					new_entry->length = (entry->baseaddr + entry->length) - new_entry->baseaddr;
					/* Shrink the existing entry and total available memory */
					entry->length -= new_entry->length;
					hv_e820_entries_nr++;

					ret = new_entry->baseaddr;
				}
			}

			if (ret != INVALID_HPA) {
				break;
			}
		}
	}

	if ((ret == INVALID_HPA) || (ret == 0UL)) {
		/* current memory allocation algorithm is to find the available address from the highest
		 * possible address below max_addr. if ret == 0, means all memory is used up and we have to
		 * put the resource at address 0, this is dangerous.
		 * Also ret == 0 would make code logic very complicated, since memcpy_s() doesn't support
		 * address 0 copy.
		 */
		panic("Requested memory (size:0x%lx) from E820 cannot be reserved!!", size);
	}

	dev_dbg(DBG_LEVEL_E820, "(%s) addr:0x%lx, size:0x%lx", __func__, ret, size);
	return ret;
}

/*
 * @brief allocate memory from hv ram
 *
 * Highest ram may be allocated to Pre-luanch VM
 *
 * HV internal features (such as pagetable) should use HV memory
 */
uint64_t e820_alloc_hv_memory(uint64_t size_arg)
{
	uint64_t hv_ram_end = CONFIG_HV_RAM_START + CONFIG_HV_RAM_SIZE;

	return e820_alloc_memory(size_arg, hv_ram_end);
}

static void insert_e820_entry(uint32_t index, uint64_t addr, uint64_t length, uint64_t type)
{
	uint32_t i;

	hv_e820_entries_nr++;
	ASSERT(hv_e820_entries_nr <= E820_MAX_ENTRIES, "e820 entry overflow");

	for (i = hv_e820_entries_nr - 1; i > index; i--) {
		hv_e820[i] = hv_e820[i - 1];
	}

	hv_e820[index].baseaddr = addr;
	hv_e820[index].length = length;
	hv_e820[index].type = type;
}

static uint64_t e820_alloc_region(uint64_t addr, uint64_t size)
{
	uint32_t i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint64_t start_pa = round_page_down(addr);
	uint64_t end_pa = round_page_up(addr + size);
	struct e820_entry *entry;

	for (i = 0U; i < hv_e820_entries_nr; i++) {
		entry = &hv_e820[i];
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if ((entry->type != E820_TYPE_RAM) || (entry_end <= start_pa) || (entry_start >= end_pa)) {
			continue;
		}

		if ((entry_start <= start_pa) && (entry_end >= end_pa)) {
			entry->length = start_pa - entry_start;
			/*
			 * .......|start_pa... ....................End_pa|.....
			 * |entry_start..............................entry_end|
			 */
			if (end_pa < entry_end) {
				insert_e820_entry(i + 1, end_pa, entry_end - end_pa, E820_TYPE_RAM);
				break;
			}
		} else {
			pr_err("This region not in one entry!");
		}
	}

	return addr;
}

static void calculate_e820_ram_size(void)
{
	uint32_t i;

	for (i = 0; i < hv_e820_entries_nr; i++) {
		pr_info("hv_e820[%d]: type: 0x%x [0x%016lx, 0x%016lx)",
				i, hv_e820[i].type, hv_e820[i].baseaddr,
				hv_e820[i].baseaddr + hv_e820[i].length);

		if (hv_e820[i].type == E820_TYPE_RAM) {
			hv_e820_ram_size += hv_e820[i].length;
		}
	}

	pr_info("hv_e820[%d]: total ram size: 0x%016lx", E820_MAX_ENTRIES, hv_e820_ram_size);
}

void dump_e820_entries(struct e820_entry *entry)
{
	uint32_t i;
	int64_t total_size = 0;

	for (i = 0; i < E820_MAX_ENTRIES; i++) {
		if (!entry[i].baseaddr && !entry[i].length) {
			break;
		}

		pr_info("e820_entry[%d]: type: 0x%x [0x%016lx,  0x%016lx)",
				i, entry[i].type, entry[i].baseaddr,
				entry[i].baseaddr + entry[i].length);
		if (entry[i].type == E820_TYPE_RAM) {
			total_size += entry[i].length;
		}
	}
	pr_info("e820_entry[%d]: total ram size: 0x%016lx", E820_MAX_ENTRIES, total_size);
}

void init_e820(void)
{
	uint64_t i, hv_image_start, hv_image_size;
	uint64_t hv_ram_end = CONFIG_HV_RAM_START + CONFIG_HV_RAM_SIZE;
	uint64_t reserved_page;

	hv_e820_entries_nr = 0;
	memset(initial_hv_e820, 0, E820_MAX_ENTRIES * sizeof(struct e820_entry));
	memset(hv_e820, 0, E820_MAX_ENTRIES * sizeof(struct e820_entry));

	for (i = 0; i < E820_MAX_ENTRIES; i++) {
		if (!meminfo_list[i].baseaddr && !meminfo_list[i].length) {
			break;
		}
		initial_hv_e820[i].baseaddr = meminfo_list[i].baseaddr;
		initial_hv_e820[i].length = meminfo_list[i].length;
		initial_hv_e820[i].type = meminfo_list[i].type;
		pr_info("e820_entry[%d]: type: 0x%x [0x%016lx, 0x%016lx)",
				i, initial_hv_e820[i].type, initial_hv_e820[i].baseaddr,
				initial_hv_e820[i].baseaddr + initial_hv_e820[i].length);
	}

	for (i = 0; i < E820_MAX_ENTRIES; i++) {
		if (!initial_hv_e820[i].baseaddr && !initial_hv_e820[i].length) {
			break;
		}
		insert_e820_entry(i, initial_hv_e820[i].baseaddr, \
						  initial_hv_e820[i].length, initial_hv_e820[i].type);
	}

	calculate_e820_ram_size();

	/* reserve hv image memory */
	hv_image_start = get_hv_image_base();
	hv_image_size = (uint64_t)(&ld_ram_end - &ld_ram_start);
	e820_alloc_region(hv_image_start, hv_image_size);
	pr_info("HV image: [0x%016lx, 0x%016lx)", hv_image_start,
			hv_image_start + hv_image_size);

	/* split hv memory from other regions for e820_alloc_hv_memory to work */
	reserved_page = e820_alloc_region(hv_ram_end - PAGE_SIZE, PAGE_SIZE);
	memset((void *)reserved_page, 0xF1, PAGE_SIZE);
	pr_info("HV ram: [0x%016lx, 0x%016lx)", (uint64_t)CONFIG_HV_RAM_START,
			(uint64_t)(CONFIG_HV_RAM_START + CONFIG_HV_RAM_SIZE));
}

uint64_t get_e820_ram_size(void)
{
	return hv_e820_ram_size;
}

uint32_t get_e820_entries_count(void)
{
	return hv_e820_entries_nr;
}

const struct e820_entry *get_e820_entry(void)
{
	return hv_e820;
}

const struct e820_entry *get_initial_e820_entry(void)
{
	return initial_hv_e820;
}

uint64_t get_hv_image_delta(void)
{
	uint64_t offset;

	offset = (uint64_t)(&ld_ram_start) - CONFIG_HV_RAM_START;

	return offset;
}

/* get the actual Hypervisor load address (HVA) */
uint64_t get_hv_image_base(void)
{
	return (get_hv_image_delta() + CONFIG_HV_RAM_START);
}

