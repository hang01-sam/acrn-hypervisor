/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __AARCH64_VUART_H__
#define __AARCH64_VUART_H__
#include <types.h>
#include <asm/lib/spinlock.h>
#include <asm/vm_config.h>

#define RX_BUF_SIZE		256U

#define TX_BUF_SIZE		65536U
#define INVAILD_VUART_IDX	0xFFU

#define COM1_BASE		0x3F8U
#define COM2_BASE		0x2F8U
#define COM3_BASE		0x3E8U
#define COM4_BASE		0x2E8U
#define INVALID_COM_BASE	0U

#define COM1_IRQ		4U
#define COM2_IRQ		3U
#define COM3_IRQ		6U
#define COM4_IRQ		7U

struct vuart_fifo {
	char *buf;
	uint32_t rindex;	/* index to read from */
	uint32_t windex;	/* index to write to */
	uint32_t num;		/* number of characters in the fifo */
	uint32_t size;		/* size of the fifo */
	uint32_t granularity;
};

struct acrn_vuart {
	uint32_t dr;
	uint32_t rsr;
	uint32_t fr;
	uint32_t ilpr;
	uint32_t ibrd;
	uint32_t fbrd;
	uint32_t lcr_h;
	uint32_t cr;
	uint32_t ifls;
	uint32_t imsc;
	uint32_t ris;
	uint32_t mis;
	uint32_t icr;
	uint32_t dmacr;

	struct vuart_fifo rxfifo;
	struct vuart_fifo txfifo;
	union {
		uint16_t port_base;
		struct {
			uint64_t base;
			uint64_t size;
		};
	};
	uint32_t irq;
	char vuart_rx_buf[RX_BUF_SIZE];
	char vuart_tx_buf[TX_BUF_SIZE];
	bool thre_int_pending;	/* THRE interrupt pending */
	bool active;
	struct acrn_vuart *target_vu; /* Pointer to target vuart */
	struct acrn_vm *vm;
	struct pci_vdev *vdev;	/* pci vuart */
	spinlock_t lock;	/* protects all softc elements */
};

void init_legacy_vuarts(struct acrn_vm *vm, const struct vuart_config *vu_config);
void deinit_legacy_vuarts(struct acrn_vm *vm);
void init_pci_vuart(struct pci_vdev *vdev);
void deinit_pci_vuart(struct pci_vdev *vdev);

void vuart_putchar(struct acrn_vuart *vu, char ch);
char vuart_getchar(struct acrn_vuart *vu);
void vuart_toggle_intr(struct acrn_vuart *vu);

bool is_vuart_intx(const struct acrn_vm *vm, uint32_t intx_gsi);

uint32_t vuart_read_reg(struct acrn_vuart *vu, uint32_t offset);
void vuart_write_reg(struct acrn_vuart *vu, uint32_t offset, uint32_t value);
#endif /* __AARCH64_VUART_H__ */
