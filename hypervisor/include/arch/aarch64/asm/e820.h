/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_E820_H__
#define __AARCH64_E820_H__
#include <types.h>

/* E820 memory types */
#define E820_TYPE_RAM		1U
#define E820_TYPE_RESERVED	2U
#define E820_TYPE_ACPI_RECLAIM	3U
#define E820_TYPE_ACPI_NVS	4U
#define E820_TYPE_UNUSABLE	5U
#define E820_TYPE_DEV		6U

#define E820_MAX_ENTRIES	128U

/** Defines a single entry in an E820 memory map. */
struct e820_entry {
	/** The base address of the memory range. */
	uint64_t baseaddr;
	/** The length of the memory range. */
	uint64_t length;
	/** The type of memory region. */
	uint32_t type;
} __packed;

struct mem_range {
	uint64_t mem_bottom;
	uint64_t mem_top;
	uint64_t total_mem_size;
};

/* HV read multiboot header to get e820 entries info and calc total RAM info */
void init_e820(void);

uint64_t e820_alloc_memory(uint64_t size_arg, uint64_t max_addr);
uint64_t e820_alloc_hv_memory(uint64_t size_arg);
uint64_t get_e820_ram_size(void);
/* get total number of the e820 entries */
uint32_t get_e820_entries_count(void);

/* dump the e802 entiries */
void dump_e820_entries(struct e820_entry *entry);

/* get the e802 entiries */
const struct e820_entry *get_e820_entry(void);
const struct e820_entry *get_initial_e820_entry(void);

#endif /* __AARCH64_E820_H__ */
