/*-
 * Copyright (c) 2012 NetApp, Inc.
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

#include <types.h>
#include <pci.h>
#include <uart16550.h>
#include <console.h>
#include <asm/guest/vuart.h>
#include <vmcs9900.h>
#include <asm/guest/vm.h>
#include <logmsg.h>
#include <io_req.h>

#define UARTDR		0x00
#define	UARTRSR		0x04
#define UARTFR		0x18
#define UARTILPR	0x20
#define UARTIBRD	0x24
#define UARTFBRD	0x28
#define UARTLCR_H	0x2c
#define UARTCR		0x30
#define UARTIFLS	0x34
#define UARTIMSC	0x38
#define UARTRIS		0x3c
#define UARTMIS		0x40
#define UARTICR		0x44
#define UARTDMACR	0x48
#define UARTPeriphID0	0xfe0
#define UARTPeriphID1	0xfe4
#define UARTPeriphID2	0xfe8
#define UARTPeriphID3	0xfec
#define UARTPCellID0	0xff0
#define UARTPCellID1	0xff4
#define UARTPCellID2	0xff8
#define UARTPCellID3	0xffc

/* Flag register bits */
#define FR_RXFE		(1U << 4)
#define FR_TXFF		(1U << 5)
#define FR_RXFF		(1U << 6)
#define FR_TXFE		(1U << 7)

/* fifo enable */
#define LCR_H_FEN	(1U << 4)

/* CR register bits */
#define CR_UARTEN	(1U << 0)
#define CR_TXE		(1U << 8)
#define CR_RXE          (1U << 9)

/* IMSC/RIS/MIS/ICR bits */
#define UART_IRQ_RTI		(1U << 6)
#define UART_IRQ_TXI		(1U << 5)
#define UART_IRQ_RXI		(1U << 4)

#define init_vuart_lock(vu)	spinlock_init(&((vu)->lock))
#define obtain_vuart_lock(vu, flags)	spinlock_irqsave_obtain(&((vu)->lock), &(flags))
#define release_vuart_lock(vu, flags)	spinlock_irqrestore_release(&((vu)->lock), (flags))

static inline void reset_fifo(struct vuart_fifo *fifo)
{
	fifo->rindex = 0U;
	fifo->windex = 0U;
	fifo->num = 0U;
}

static inline void fifo_putchar(struct vuart_fifo *fifo, char ch)
{
	fifo->buf[fifo->windex] = ch;
	if (fifo->num < fifo->size) {
		fifo->windex = (fifo->windex + 1U) % fifo->size;
		fifo->num++;
	} else {
		fifo->rindex = (fifo->rindex + 1U) % fifo->size;
		fifo->windex = (fifo->windex + 1U) % fifo->size;
	}
}

static inline char fifo_getchar(struct vuart_fifo *fifo)
{
	char c = -1;

	if (fifo->num > 0U) {
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1U) % fifo->size;
		fifo->num--;
	}
	return c;
}

static inline uint32_t fifo_numchars(const struct vuart_fifo *fifo)
{
	return fifo->num;
}

static inline bool fifo_isfull(const struct vuart_fifo *fifo)
{
	bool ret = false;
	/* When the FIFO has less than 16 empty bytes, it should be
	 * mask as full. As when the 16550 driver in OS receive the
	 * THRE interrupt, it will directly send 16 bytes without
	 * checking the LSR(THRE) */

	/* Desired value should be 16 bytes, but to improve
	 * fault-tolerant, enlarge 16 to 64. So that even the THRE
	 * interrupt is raised by mistake, only if it less than 4
	 * times, data in FIFO will not be overwritten. */
	if ((fifo->size - fifo->num) < 64U) {
		ret = true;
	}
	return ret;
}

void vuart_putchar(struct acrn_vuart *vu, char ch)
{
	uint64_t rflags;

	obtain_vuart_lock(vu, rflags);
	fifo_putchar(&vu->rxfifo, ch);
	release_vuart_lock(vu, rflags);
}

char vuart_getchar(struct acrn_vuart *vu)
{
	uint64_t rflags;
	char c;

	obtain_vuart_lock(vu, rflags);
	c = fifo_getchar(&vu->txfifo);
	release_vuart_lock(vu, rflags);
	return c;
}

static inline void init_fifo(struct acrn_vuart *vu)
{
	vu->txfifo.buf = vu->vuart_tx_buf;
	vu->rxfifo.buf = vu->vuart_rx_buf;
	vu->txfifo.size = TX_BUF_SIZE;
	vu->rxfifo.size = RX_BUF_SIZE;
	vu->txfifo.granularity = TX_BUF_SIZE >> 3;
	vu->rxfifo.granularity = 16 >> 3;
	reset_fifo(&(vu->txfifo));
	reset_fifo(&(vu->rxfifo));
}

static uint8_t ifls_num[8] = {2, 4, 8, 12, 14,};

static void vuart_tx_update(struct acrn_vuart *vu)
{
	vu->ris |= UART_IRQ_TXI; /* transmit interrupt */
	vu->fr &= ~FR_TXFF; /* Clear Transmit FIFO full */
	vu->fr |= FR_TXFE; /* Transmit FIFO empty */
}

static void vuart_rx_update(struct acrn_vuart *vu)
{
	if (fifo_numchars(&vu->rxfifo) >= ifls_num[(vu->ifls >> 3) & 0x7]) {
		vu->ris |= UART_IRQ_RXI; /* receive interrupt */
	} else {
		vu->ris &= ~UART_IRQ_RXI; /* clear receive interrupt */
	}

	if (fifo_isfull(&vu->rxfifo)) {
		vu->fr |= FR_RXFF;    /* Receive FIFO full */
	} else {
		vu->fr &= ~FR_RXFF;    /* Clear Receive FIFO full */
	}

	if (fifo_numchars(&vu->rxfifo)) {
		vu->ris |= UART_IRQ_RTI; /* receive timeout interrupt */
		vu->fr &= ~FR_RXFE; /* clear receive fifo empty */
	} else {
		vu->ris &= ~UART_IRQ_RTI; /* Clear receive timeout interrupt */
		vu->fr |= FR_RXFE; /* receive fifo empty */
	}
}

static void vuart_post_irq(struct acrn_vuart *vu)
{
	vu->mis = vu->imsc & vu->ris;
	if (vu->mis) {
		if (!(vu->cr & CR_UARTEN)) {
			return;
		}

		if (((vu->mis & UART_IRQ_TXI) && (vu->cr & CR_TXE))
			|| ((vu->mis & (UART_IRQ_RXI | UART_IRQ_RTI)) && (vu->cr & CR_RXE))) {
			uint16_t pcpu = get_pcpu_id();
			struct acrn_vcpu *vcpu = &vu->vm->hw.vcpu_array[0];

			if (vcpu->pcpu_id != pcpu) {
				struct acrn_vm *vm = vu->vm;
				uint64_t data = VGIC_MSG_DATA(vm->vm_id, 0, vu->irq, 0, vcpu->vcpu_id);
				vgic_send_msg(vcpu, VGIC_INJECT, data);
			} else {
				vgic_inject(vcpu, vu->irq, 0);
			}
		}
	}
}

void vuart_toggle_intr(struct acrn_vuart *vu)
{
	if (!vu) {
		return;
	}

	vuart_tx_update(vu);

	vuart_rx_update(vu);

	vuart_post_irq(vu);
}

uint32_t vuart_read_reg(struct acrn_vuart *vu, uint32_t offset)
{
	uint32_t value = 0;
	uint64_t rflags;

	obtain_vuart_lock(vu, rflags);
	switch (offset) {
	case UARTDR:
		/* uart enable and receive enable */
		if ((vu->cr & CR_UARTEN) && (vu->cr & CR_RXE)) {
			if (fifo_numchars(&vu->rxfifo)) {
				value = fifo_getchar(&vu->rxfifo);
				vuart_rx_update(vu);
			} else {
				pr_info("%s: read data when fifo is empty", __func__);
			}
		}
		break;
	case UARTRSR:
		value = vu->rsr;
		break;
	case UARTFR:
		value = vu->fr;
		break;
	case UARTILPR:
		value = vu->ilpr;
		break;
	case UARTIBRD:
		value = vu->ibrd;
		break;
	case UARTFBRD:
		value = vu->fbrd;
		break;
	case UARTLCR_H:
		value = vu->lcr_h;
		break;
	case UARTCR:
		value = vu->cr;
		break;
	case UARTIFLS:
		value = vu->ifls;
		break;
	case UARTIMSC:
		value = vu->imsc;
		break;
	case UARTRIS:
		value = vu->ris;
		break;
	case UARTMIS:
		value = vu->mis;
		break;
	case UARTDMACR:
		value = vu->dmacr;
		break;
	case UARTPeriphID0:
		value = 0x11U;
		break;
	case UARTPeriphID1:
		value = 0x10U;
		break;
	case UARTPeriphID2:
		value = 0x14U; /* revision 1 instead of 3 */
		break;
	case UARTPeriphID3:
		value = 0x00U;
		break;
	case UARTPCellID0:
		value = 0x0dU;
		break;
	case UARTPCellID1:
		value = 0xf0U;
		break;
	case UARTPCellID2:
		value = 0x05U;
		break;
	case UARTPCellID3:
		value = 0xb1U;
		break;
	case UARTICR:
	default:
		pr_info("%s: vuart read invalid reg 0x%x", __func__, offset);
		break;
	}
	release_vuart_lock(vu, rflags);

	return value;
}

void vuart_write_reg(struct acrn_vuart *vu, uint32_t offset, uint32_t value)
{
	uint64_t rflags;

	obtain_vuart_lock(vu, rflags);

	switch (offset) {
	case UARTDR:
		/* uart enable and transmit enable */
		if ((vu->cr & CR_UARTEN) && (vu->cr & CR_TXE)) {
			fifo_putchar(&vu->txfifo, (char)value);
			vuart_tx_update(vu);
		}
		break;
	case UARTRSR:
		vu->rsr = value;
		break;
	case UARTFR:
		break;
	case UARTILPR:
		vu->ilpr = value;
		break;
	case UARTIBRD:
		vu->ibrd = value;
		break;
	case UARTFBRD:
		vu->fbrd = value;
		break;
	case UARTLCR_H:
		vu->lcr_h = value;
		vuart_toggle_intr(vu);
		break;
	case UARTCR:
		vu->cr = value;
		vuart_post_irq(vu);
		break;
	case UARTIFLS:
		vu->ifls = value;
		vuart_toggle_intr(vu);
		break;
	case UARTIMSC:
		vu->imsc = value;
		vuart_post_irq(vu);
		break;
	case UARTICR:
		vu->ris &= ~value;
		vu->mis &= ~value;
		break;
	case UARTDMACR:
		vu->dmacr = value;
		break;
	case UARTRIS:
	case UARTMIS:
	default:
		pr_info("%s: vuart write invalid reg 0x%x val 0x%x", __func__, offset, value);
		break;
	}

	release_vuart_lock(vu, rflags);
}

int32_t vuart_mmio_access_handler(struct io_request *io_req, void *handler_private_data)
{
	struct acrn_mmio_request *mmio_request = &(io_req->reqs.mmio_request);
	struct acrn_vuart *vu = (struct acrn_vuart *)handler_private_data;

	uint64_t offset = mmio_request->address - vu->base;

	if (mmio_request->direction == ACRN_IOREQ_DIR_WRITE) {
		vuart_write_reg(vu, offset, mmio_request->value);
	} else {
		mmio_request->value = vuart_read_reg(vu, offset);
	}

	return 0;
}

static void setup_vuart(struct acrn_vm *vm, uint16_t vuart_idx)
{
	struct acrn_vuart *vu = &vm->vuart[vuart_idx];

	vu->vm = vm;
	init_fifo(vu);
	init_vuart_lock(vu);
	vu->thre_int_pending = true;

	vu->rsr = 0U;
	vu->fr = FR_TXFE | FR_RXFE; /* transmit & receive fifo empty */
	vu->ilpr = 0U;
	vu->ibrd = 0U;
	vu->fbrd = 0U;
	vu->lcr_h = 0x70U;
	vu->cr = CR_UARTEN | CR_TXE | CR_RXE; /* enable uart and transmit for early print */
	vu->ifls = 0x12;
	vu->imsc = 0U;
	vu->ris = 0U; /* transmit interrupt */
	vu->mis = 0U; /* transmit interrupt */
	vu->dmacr = 0U;
	vuart_toggle_intr(vu);
	vu->target_vu = NULL;
}

void init_legacy_vuarts(struct acrn_vm *vm, const struct vuart_config *vu_config)
{
	uint8_t i;
	struct acrn_vuart *vu;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		if (vu_config[i].type == VUART_PL011) {
			vu = &vm->vuart[i];
			setup_vuart(vm, i);
			vu->irq = vu_config[i].irq;
			vu->base = vu_config[i].addr.base;
			vu->size = vu_config[i].addr.size;
			register_mmio_emulation_handler(vm, vuart_mmio_access_handler, vu->base, vu->base + vu->size, (void *)vu, 0);
			vu->active = true;
		}
	}
}
