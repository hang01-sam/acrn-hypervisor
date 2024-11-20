/*
 * Copyright (C) 2021 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_TSC_H__
#define __AARCH64_TSC_H__

#include <asm/arch.h>
#include <types.h>

#define TSC_PER_MS	((uint64_t)get_tsc_khz())

/**
 * @brief Read Time Stamp Counter (TSC).
 *
 * @return TSC value
 */
static inline uint64_t rdtsc(void)
{
	return read_cntvct_el0();
}

/**
 * @brief Get Time Stamp Counter (TSC) frequency in KHz.
 *
 * @return TSC frequency in KHz
 */
uint32_t get_tsc_khz(void);

/**
 * @brief Calibrate Time Stamp Counter (TSC) frequency.
 *
 * @remark Generic time related routines, e.g., cpu_tickrate(), us_to_ticks(),
 * udelay(), etc., relies on this function being called earlier during system initialization.
 */
void calibrate_tsc(void);

#endif	/* __AARCH64_TSC_H__ */
