/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_CPUINFO_H__
#define __AARCH64_CPUINFO_H__

void init_pcpu_capabilities(void);
uint32_t get_pa_range(void);

#endif /* __AARCH64_CPUINFO_H__ */
