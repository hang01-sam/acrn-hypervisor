/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VTD_H__
#define __AARCH64_VTD_H__

#include <types.h>
#include <pci.h>
#include <platform_acpi_info.h>

/* copied from x86, for board_c.py compile */

/* Values for entry_type in ACPI_DMAR_DEVICE_SCOPE - device types */
enum acpi_dmar_scope_type {
	ACPI_DMAR_SCOPE_TYPE_NOT_USED       = 0,
	ACPI_DMAR_SCOPE_TYPE_ENDPOINT       = 1,
	ACPI_DMAR_SCOPE_TYPE_BRIDGE         = 2,
	ACPI_DMAR_SCOPE_TYPE_IOAPIC         = 3,
	ACPI_DMAR_SCOPE_TYPE_HPET           = 4,
	ACPI_DMAR_SCOPE_TYPE_NAMESPACE      = 5,
	ACPI_DMAR_SCOPE_TYPE_RESERVED       = 6 /* 6 and greater are reserved */
};

struct dmar_dev_scope {
	enum acpi_dmar_scope_type type;
	uint8_t id;
	uint8_t bus;
	uint8_t devfun;
};

struct dmar_drhd {
	uint32_t dev_cnt;
	uint16_t segment;
	uint8_t flags;
	bool ignore;
	uint64_t reg_base_addr;
	/* assume no pci device hotplug support */
	struct dmar_dev_scope *devices;
};

struct dmar_info {
	uint32_t drhd_count;
	struct dmar_drhd *drhd_units;
};

#endif /* __AARCH64_VTD_H__ */
