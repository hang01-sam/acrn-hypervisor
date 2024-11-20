/*
 * Copyright (C) 2023-2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <types.h>
#include <asm/lib/spinlock.h>
#include <pci.h>
#include <uart16550.h>
#include <asm/io.h>

/* Register map */
#define PL011_DR        0x00    /* Data Register */
#define PL011_RSEC      0x04    /* Receive Status Register/Error Clear Register */
#define PL011_FR        0x18    /* Flag Register */
#define PL011_ILPR      0x20    /* IrDA Low-Power Counter Register */
#define PL011_IBRD      0x24    /* Integer Baud Rate Register */
#define PL011_FBRD      0x28    /* Fractional Baud Rate Register */
#define PL011_LCR_H     0x2C    /* Line Control Register */
#define PL011_CR        0x30    /* Control Register */
#define PL011_IFLS      0x34    /* Interrupt FIFO Level Select Register */
#define PL011_IMSC      0x38    /* Interrupt Mask Set/Clear Register */
#define PL011_RIS       0x3c    /* Raw Interrupt Status Register */
#define PL011_MIS       0x40    /* Masked Interrupt Status Register */
#define PL011_ICR       0x44    /* Interrupt Clear Register */
#define PL011_DMACR     0x48    /* DMA Control Register */

/* CR bits */
#define CTSEn   (1<<15) /* CTS hardware flow control enable */
#define RTSEn   (1<<14) /* RTS hardware flow control enable */
#define OUT2    (1<<13) /* UART Out2 modem status output */
#define OUT1    (1<<12) /* UART Out1 modem status output */
#define RTS     (1<<11) /* Request to send */
#define DTR     (1<<10) /* Data transmit ready */
#define RXE     (1<<9)  /* Receive enable */
#define TXE     (1<<8)  /* Transmit enable */
#define LBE     (1<<7)  /* Loop back enable */
#define SIRLP   (1<<2)  /* SIR low-power IrDA mode */
#define SIREN   (1<<1)  /* SIR enable */
#define UARTEN  (1<<0)  /* UART enable */

/* FR bits */
#define TXFE    (1<<7)  /* Transmit FIFO empty */
#define RXFF    (1<<6)  /* Receive FIFO full */
#define TXFF    (1<<5)  /* Transmit FIFO full */
#define RXFE    (1<<4)  /* Receive FIFO empty */
#define BUSY    (1<<3)  /* UART busy */
#define DCD     (1<<2)  /* Data carrier detect */
#define DSR     (1<<1)  /* Data set ready */
#define CTS     (1<<0)  /* Clear to send */

typedef struct console_uart {
	void *mmio_base_vaddr;

	spinlock_t rx_lock;
	spinlock_t tx_lock;
} console_uart;

static console_uart uart = {
	.mmio_base_vaddr = (void *)CONFIG_SERIAL_MMIO_BASE,
};

static void reg_write(uint32_t reg, uint32_t value)
{
	mmio_write32(value, uart.mmio_base_vaddr + reg);
}

static uint32_t reg_read(uint32_t reg)
{
	void *addr = uart.mmio_base_vaddr + reg;
	return mmio_read32(addr);
}

static void pl011_putc(char ch)
{
	reg_write(PL011_DR, (uint32_t)(unsigned char)ch);
}

static char pl011_getc(void)
{
	if (reg_read(PL011_FR) & RXFE) {
		return -1;
	}

	return reg_read(PL011_DR) & 0xff;
}

void uart16550_init(bool early_boot)
{
	uint32_t cr;

	(void)early_boot;
	cr = reg_read(PL011_CR);
	cr &= RTS | DTR;
	cr |= RXE | TXE | UARTEN;

	reg_write(PL011_CR, cr);
}

void uart16550_set_property(__unused bool enabled, __unused enum serial_dev_type uart_type, __unused uint64_t base_addr)
{
	return;
}

size_t uart16550_puts(const char *buf, uint32_t len)
{
	uint32_t i;
	for (i = 0; i < len; i++) {
		pl011_putc(buf[i]);
	}
	return len;
}

char uart16550_getc(void)
{
	return pl011_getc();
}

bool is_pci_dbg_uart(__unused union pci_bdf bdf_value)
{
	return false;
}

bool get_pio_dbg_uart_cfg(__unused uint16_t *pio_address, __unused uint32_t *nbytes)
{
	return false;
}
