/*
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_IO_H__
#define __AARCH64_IO_H__

#include <types.h>
/** Writes a 64 bit value to a memory mapped IO device.
 *
 *  @param value The 64 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write64(uint64_t value, void *addr)
{
	volatile uint64_t *addr64 = (volatile uint64_t *)addr;
	*addr64 = value;
}

/** Writes a 32 bit value to a memory mapped IO device.
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write32(uint32_t value, void *addr)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = value;
}

/** Writes a 16 bit value to a memory mapped IO device.
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write16(uint16_t value, void *addr)
{
	volatile uint16_t *addr16 = (volatile uint16_t *)addr;
	*addr16 = value;
}

/** Writes an 8 bit value to a memory mapped IO device.
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write8(uint8_t value, void *addr)
{
	volatile uint8_t *addr8 = (volatile uint8_t *)addr;
	*addr8 = value;
}

/** Reads a 64 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 64 bit value read from the given address.
 */
static inline uint64_t mmio_read64(const void *addr)
{
	return *((volatile const uint64_t *)addr);
}

/** Reads a 32 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t mmio_read32(const void *addr)
{
	return *((volatile const uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t mmio_read16(const void *addr)
{
	return *((volatile const uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 8 bit  value read from the given address.
 */
static inline uint8_t mmio_read8(const void *addr)
{
	return *((volatile const uint8_t *)addr);
}

static inline void mmio_clrbits32(uint32_t clear, void *addr)
{
	mmio_write32(mmio_read32(addr) & ~clear, addr);
}

static inline void mmio_setbits32(uint32_t set, void *addr)
{
	mmio_write32(mmio_read32(addr) | set, addr);
}

static inline void mmio_clrsetbits32(uint32_t clear, uint32_t set, void *addr)
{
	mmio_write32((mmio_read32(addr) & ~clear) | set, addr);
}

static inline uint64_t mmio_read(const void *addr, uint64_t sz)
{
	uint64_t val;
	switch (sz) {
	case 1U:
		val = (uint64_t)mmio_read8(addr);
		break;
	case 2U:
		val = (uint64_t)mmio_read16(addr);
		break;
	case 4U:
		val = (uint64_t)mmio_read32(addr);
		break;
	default:
		val = mmio_read64(addr);
		break;
	}
	return val;
}

static inline void mmio_write(void *addr, uint64_t sz, uint64_t val)
{
	switch (sz) {
	case 1U:
		mmio_write8((uint8_t)val, addr);
		break;
	case 2U:
		mmio_write16((uint16_t)val, addr);
		break;
	case 4U:
		mmio_write32((uint32_t)val, addr);
		break;
	default:
		mmio_write64(val, addr);
		break;
	}
}

#endif
