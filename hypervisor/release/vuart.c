/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void vuart_init(__unused struct acrn_vm *vm) {}

struct acrn_vuart *vuart_console_active(void)
{
	return NULL;
}

void vuart_console_tx_chars(__unused struct acrn_vuart *vu) {}
void vuart_console_rx_chars(__unused struct acrn_vuart *vu) {}
