/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2017-2022 Intel Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef __AARCH64_BITS_H__
#define __AARCH64_BITS_H__
#include <asm/lib/atomic.h>

#define LONG_BITS     64
#define GENMASK(high, low) \
    (((~0UL) << (low)) & (~0UL >> (LONG_BITS - 1 - (high))))

/**
 *
 * INVALID_BIT_INDEX means when input paramter is zero,
 * bit operations function can't find bit set and return
 * the invalid bit index directly.
 *
 **/
#define INVALID_BIT_INDEX  0xffffU

/*
 *
 * fls32 - Find the Last (most significant) bit Set in value and
 *       return the bit index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of INVALID_BIT_INDEX means return value is
 *  invalid bit index when the input argument was zero.
 *
 *  Examples:
 *	fls32 (0x0) = INVALID_BIT_INDEX
 *	fls32 (0x01) = 0
 *	fls32 (0x80) = 7
 *	...
 *	fls32 (0x80000001) = 31
 *
 * @param value: 'uint32_t' type value
 *
 * @return value: zero-based bit index, INVALID_BIT_INDEX means
 * when 'value' was zero, bit operations function can't find bit
 * set and return the invalid bit index directly.
 *
 */

#define CLZ32(ret, value) \
		asm("clz\t%w0, %w1" : "=r" (ret) : "r" (value) : "cc");

#define CLZ64(ret, value) \
		asm("clz\t%x0, %x1" : "=r" (ret) : "r" (value) : "cc");

static inline uint16_t fls32(uint32_t value)
{
	int ret;
	CLZ32(ret, value);
	return 31 - ret;
}

static inline uint16_t fls64(uint64_t value)
{
	int ret;
	CLZ64(ret, value);
	return 63 - ret;
}

static inline uint16_t ffs32(uint32_t value)
{
	int ret;
	value = value & -value;
	CLZ32(ret, value);
	return 31 - ret;
}

/*
 *
 * ffs64 - Find the First (least significant) bit Set in value(Long type)
 * and return the index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of INVALID_BIT_INDEX means that the return value is the inalid
 *  bit index when the input argument was zero.
 *
 *  Examples:
 *	ffs64 (0x0) = INVALID_BIT_INDEX
 *	ffs64 (0x01) = 0
 *	ffs64 (0xf0) = 4
 *	ffs64 (0xf00) = 8
 *	...
 *	ffs64 (0x8000000000000001) = 0
 *	ffs64 (0xf000000000000000) = 60
 *
 * @param value: 'uint64_t' type value
 *
 * @return value: zero-based bit index, INVALID_BIT_INDEX means
 * when 'value' was zero, bit operations function can't find bit
 * set and return the invalid bit index directly.
 *
 */
static inline uint16_t ffs64(uint64_t value)
{
	int ret;
	value = value & -value;
	CLZ64(ret, value);
	return 63 - ret;
}

/*bit scan forward for the least significant bit '0'*/
static inline uint16_t ffz64(uint64_t value)
{
	return ffs64(~value);
}


/*
 * find the first zero bit in a uint64_t array.
 * @pre: the size must be multiple of 64.
 */
static inline uint64_t ffz64_ex(const uint64_t *addr, uint64_t size)
{
	uint64_t ret = size;
	uint64_t idx;

	for (idx = 0UL; (idx << 6U) < size; idx++) {
		if (addr[idx] != ~0UL) {
			ret = (idx << 6U) + ffz64(addr[idx]);
			break;
		}
	}

	return ret;
}
/*
 * Counts leading zeros.
 *
 * The number of leading zeros is defined as the number of
 * most significant bits which are not '1'. E.g.:
 *    clz(0x80000000)==0
 *    clz(0x40000000)==1
 *       ...
 *    clz(0x00000001)==31
 *    clz(0x00000000)==32
 *
 * @param value:The 32 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'value'.
 */
static inline uint16_t clz(uint32_t value)
{
	return ((value != 0U) ? (31U - fls32(value)) : 32U);
}

/*
 * Counts leading zeros (64 bit version).
 *
 * @param value:The 64 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'value'.
 */
static inline uint16_t clz64(uint64_t value)
{
	return ((value != 0UL) ? (63U - fls64(value)) : 64U);
}

/*
 * (*addr) |= (1UL<<nr);
 * Note:Input parameter nr shall be less than 64.
 * If nr>=64, it will be truncated.
 */
#define BIT_32(nr)          (1U << nr)
#define BIT_64(nr)          (1UL << nr)
#define BITMAP_MASK32(nr)		(1UL << ((nr) % 32))
#define BITMAP_MASK64(nr)		(1UL << ((nr) % 64))
#define BIT_MASK(OFF, LEN)      ((((1ULL << ((LEN) - 1)) << 1) - 1) << (OFF))

static inline uint64_t bit_get(uint64_t data, uint64_t bit)
{
	return data & BIT_64(bit);
}

static inline uint64_t bit_set(uint64_t data, uint64_t bit)
{
	return data |= BIT_64(bit);
}

static inline uint64_t bit_clear(uint64_t data, uint64_t bit)
{
	return data &= ~BIT_64(bit);
}

static inline uint64_t bit_extract(uint64_t data, uint64_t bit, uint64_t len)
{
	return (data >> bit) & BIT_MASK(0, len);
}

static inline uint64_t bit_insert(uint64_t data, uint64_t value, uint64_t bit,
								  uint64_t len)
{
	return (~BIT_MASK(bit, len) & data) | ((BIT_MASK(0, len) & value) << bit);
}

static inline void bitmap_set_nolock(uint16_t nr_arg, volatile uint64_t *addr)
{
	*addr |= BITMAP_MASK64(nr_arg);
}

static inline void bitmap_set_lock(uint16_t nr_arg, volatile uint64_t *addr)
{
	atomic_set64(addr, BITMAP_MASK64(nr_arg));
}

static inline void bitmap32_set_nolock(uint16_t nr_arg, volatile uint32_t *addr)
{
	*addr |= BITMAP_MASK32(nr_arg);
}

static inline void bitmap32_set_lock(uint16_t nr_arg, volatile uint32_t *addr)
{
	atomic_set32(addr, BITMAP_MASK32(nr_arg));
}

/*
 * (*addr) &= ~(1UL<<nr);
 * Note:Input parameter nr shall be less than 64.
 * If nr>=64, it will be truncated.
 */
static inline void bitmap_clear_nolock(uint16_t nr_arg, volatile uint64_t *addr)
{
	*addr &= ~BITMAP_MASK64(nr_arg);
}

static inline void bitmap_clear_lock(uint16_t nr_arg, volatile uint64_t *addr)
{
	atomic_clear64(addr, BITMAP_MASK64(nr_arg));
}

static inline void bitmap32_clear_nolock(uint16_t nr_arg, volatile uint32_t *addr)
{
	*addr &= ~BITMAP_MASK32(nr_arg);
}

static inline void bitmap32_clear_lock(uint16_t nr_arg, volatile uint32_t *addr)
{
	atomic_clear32(addr, BITMAP_MASK32(nr_arg));
}

/*
 * return !!((*addr) & (1UL<<nr));
 * Note:Input parameter nr shall be less than 64. If nr>=64, it will
 * be truncated.
 */

#define BITMAP_TEST(data, bit) (!!((data) & bit));

static inline bool bitmap_test(uint16_t nr, const volatile uint64_t *addr)
{
	return BITMAP_TEST(*addr, BITMAP_MASK64(nr));
}

static inline bool bitmap32_test(uint16_t nr, const volatile uint32_t *addr)
{
	return BITMAP_TEST(*addr, BITMAP_MASK32(nr));
}

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) |= (1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64, it
 * will be truncated.
 */
static inline bool bitmap_test_and_set_nolock(uint16_t nr_arg, volatile uint64_t *addr)
{
	bool ret = BITMAP_TEST(*addr, BITMAP_MASK64(nr_arg));
	bitmap_set_nolock(nr_arg, addr);
	return ret;
}

static inline bool bitmap_test_and_set_lock(uint16_t nr_arg, volatile uint64_t *addr)
{
	int64_t old;
	int64_t mask = BITMAP_MASK64(nr_arg);
	old = atomic_xset64(addr, mask);
	return BITMAP_TEST(old, mask);
}

static inline bool bitmap32_test_and_set_nolock(uint16_t nr_arg, volatile uint32_t *addr)
{
	bool ret = BITMAP_TEST(*addr, BITMAP_MASK32(nr_arg));
	bitmap32_set_nolock(nr_arg, addr);
	return ret;
}

static inline bool bitmap32_test_and_set_lock(uint16_t nr_arg, volatile uint32_t *addr)
{
	int32_t old;
	uint32_t mask = BITMAP_MASK32(nr_arg);

	old = atomic_xset32(addr, mask);
	return BITMAP_TEST(old, mask);
}

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) &= ~(1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64,
 * it will be truncated.
 */
static inline bool bitmap_test_and_clear_nolock(uint16_t nr_arg, volatile uint64_t *addr)
{
	bool ret = BITMAP_TEST(*addr, BITMAP_MASK64(nr_arg));
	bitmap_clear_nolock(nr_arg, addr);
	return ret;
}

static inline bool bitmap_test_and_clear_lock(uint16_t nr_arg, volatile uint64_t *addr)
{
	int64_t old;
	uint64_t mask = BITMAP_MASK64(nr_arg);

	old = atomic_xclr64(addr, mask);
	return BITMAP_TEST(old, mask);
}

static inline bool bitmap32_test_and_clear_nolock(uint16_t nr_arg, volatile uint32_t *addr)
{
	bool ret = BITMAP_TEST(*addr, BITMAP_MASK32(nr_arg));
	bitmap32_clear_nolock(nr_arg, addr);
	return ret;
}

static inline bool bitmap32_test_and_clear_lock(uint16_t nr_arg, volatile uint32_t *addr)
{
	int64_t old;
	uint64_t mask = BITMAP_MASK32(nr_arg);

	old = atomic_xclr32(addr, mask);
	return BITMAP_TEST(old, mask);
}

static inline uint16_t bitmap_weight(uint64_t bits)
{
	return (uint16_t)__builtin_popcountl(bits);
}

#endif /* __AARCH64_BITS_H__*/
