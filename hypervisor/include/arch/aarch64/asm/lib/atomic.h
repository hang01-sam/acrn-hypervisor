/*
 * Copyright (C) 2024 Samsung Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_ATOMIC_H__
#define __AARCH64_ATOMIC_H__
#include <types.h>
#include <asm/arch.h>

#define ATOMIC_OP32(op, ins)										\
static inline void atomic_##op##32(volatile uint32_t *ptr, int32_t i)		\
{																	\
    unsigned long status;											\
    uint32_t result;												\
    asm volatile(													\
"1:     ldaxr    %w1, %2\n"											\
"       "#ins"     %w1, %w1, %w3\n"									\
"       stlxr    %w0, %w1, %2\n"									\
"		cmp %w0, #1\n"												\
"		beq 1b\n"													\
"		dmb	ish"													\
	: "=&r" (status), "=&r" (result), "+Q" (*ptr)					\
	: "Ir" (i)														\
	: "cc", "memory");												\
}

ATOMIC_OP32(add, add)
ATOMIC_OP32(sub, sub)
ATOMIC_OP32(set, orr)
ATOMIC_OP32(clear, bic)
ATOMIC_OP32(xor, xor)

#define ATOMIC_OP64(op, ins)										\
static inline void atomic_##op##64(volatile uint64_t *ptr, int64_t i)		\
{																	\
    unsigned long status;											\
    uint64_t result;												\
    asm volatile(													\
"1:     ldaxr    %1, %2\n"											\
"       "#ins"     %1, %1, %3\n"									\
"       stlxr    %w0, %1, %2\n"										\
"		cmp %w0, #1\n"												\
"		beq 1b\n"													\
"		dmb	ish"													\
	: "=&r" (status), "=&r" (result), "+Q" (*ptr)					\
	: "Ir" (i)														\
	: "cc", "memory");												\
}

ATOMIC_OP64(add, add)
ATOMIC_OP64(sub, sub)
ATOMIC_OP64(set, orr)
ATOMIC_OP64(clear, bic)
ATOMIC_OP64(xor, xor)

#define ATOMIC_OP32_RETURN(op, ins)											\
static inline int32_t atomic_##op##32(volatile uint32_t *ptr, int32_t i)				\
{																			\
    unsigned long status;													\
    uint32_t result, original;												\
    asm volatile(															\
"1:     ldaxr    %w0, %3\n"													\
"       "#ins"     %w2, %w0, %w4\n"											\
"       stlxr    %w1, %w2, %3\n"											\
"		cmp %w1, #1\n"														\
"		beq 1b\n"															\
"		dmb	ish"															\
	: "=&r" (original), "=&r" (status), "=&r" (result), "+Q" (*ptr)			\
	: "Ir" (i)																\
	: "cc", "memory");														\
	return original;														\
}

ATOMIC_OP32_RETURN(xadd, add)
ATOMIC_OP32_RETURN(xsub, sub)
ATOMIC_OP32_RETURN(xset, orr)
ATOMIC_OP32_RETURN(xclr, bic)
ATOMIC_OP32_RETURN(xxor, xor)

#define ATOMIC_OP64_RETURN(op, ins)											\
static inline int64_t atomic_##op##64(volatile uint64_t *ptr, int64_t i)				\
{																			\
    unsigned long status;													\
    uint64_t result, original;												\
    asm volatile(															\
"1:     ldaxr    %0, %3\n"													\
"       "#ins"     %2, %0, %4\n"											\
"       stlxr    %w1, %2, %3\n"												\
"		cmp %w1, #1\n"														\
"		beq 1b\n"															\
"		dmb	ish"															\
	: "=&r" (original), "=&r" (status), "=&r" (result), "+Q" (*ptr)			\
	: "Ir" (i)																\
	: "cc", "memory");														\
	return original;														\
}

ATOMIC_OP64_RETURN(xadd, add)
ATOMIC_OP64_RETURN(xsub, sub)
ATOMIC_OP64_RETURN(xset, orr)
ATOMIC_OP64_RETURN(xclr, bic)
ATOMIC_OP64_RETURN(xxor, xor)

/* SWAP op, *ptr = i, return old value of *ptr */
static inline uint32_t atomic_swap32(uint32_t *ptr, uint32_t i)
{
	uint64_t tmp;
	uint32_t old;
	asm volatile(
		"1: ldaxr    %w0, %2\n"
		"   stlxr    %w1, %w3, %2\n"
		"	cmp %w1, #1\n"
		"	beq 1b\n"
		"	dmb	ish"
		: "=&r"(old), "=&r"(tmp), "+Q"(*ptr)
		: "r"(i)
		: "cc", "memory");
	return old;
}

static inline uint64_t atomic_swap64(uint64_t *ptr, uint64_t i)
{
	uint64_t tmp;
	uint64_t old;
	asm volatile(
		"1: ldaxr    %0, %2\n"
		"   stlxr    %w1, %3, %2\n"
		"	cmp %w1, #1\n"
		"	beq 1b\n"
		"	dmb	ish"
		: "=&r"(old), "=&r"(tmp), "+Q"(*ptr)
		: "r"(i)
		: "cc", "memory");
	return old;
}

/*
From Will Deacon <will.deacon@arm.com>:
cmpxchg_relaxed offers an SMP-safe, barrier-less cmpxchg implementation
so we can define it in the same way that we implement cmpxchg_local on
arm64.
So here I choose Linux cmpxchg_relaxed to implement hypervisor side codes.
*/
/* cmpxchg op, *ptr = (*ptr == old) ? new : NULL , return old value of *ptr */
/* NOTICE: the return value is different with x86 cmpxchg */
static inline uint32_t atomic_cmpxchg32(volatile uint32_t *ptr, uint32_t old, uint32_t new)
{
	unsigned long tmp;
	int oldval;

	smp_mb();

	asm volatile(
		"1:     ldaxr    %w1, %2\n"
		"		xor		%w0, %w1, %w3\n"
		"       cbnz    %w0, 2f\n"
		"       stlxr    %w0, %w4, %2\n"
		"       cmp		%w0, #1\n"
		"		beq		1b\n"
		"		dmb	ish"
		"2:"
		: "=&r"(tmp), "=&r"(oldval), "+Q"(*ptr)
		: "Ir"(old), "r"(new)
		: "cc", "memory");

	smp_mb();
	return oldval;
}

static inline uint64_t atomic_cmpxchg64(volatile uint64_t *ptr, uint64_t old, uint64_t new)
{
	unsigned long tmp;
	uint64_t oldval;

	smp_mb();

	asm volatile(
		"1:     ldaxr    %1, %2\n"
		"		xor		%0, %1, %3\n"
		"       cbnz    %0, 2f\n"
		"       stlxr    %w0, %4, %2\n"
		"       cmp		%w0, #1\n"
		"		beq		1b\n"
		"		dmb	ish"
		"2:"
		: "=&r"(tmp), "=&r"(oldval), "+Q"(*ptr)
		: "Ir"(old), "r"(new)
		: "cc", "memory");

	smp_mb();
	return oldval;
}

/* INC op, *ptr++ */
static inline void atomic_inc32(uint32_t *ptr)
{
	atomic_add32(ptr, 1);
}

static inline void atomic_inc64(uint64_t *ptr)
{
	atomic_add64(ptr, 1);
}

/* DEC op, *ptr-- */
static inline void atomic_dec32(uint32_t *ptr)
{
	atomic_sub32(ptr, 1);
}

static inline void atomic_dec64(uint64_t *ptr)
{
	atomic_sub64(ptr, 1);
}

/*
* #define atomic_readandclear32(P) \
* (return (*(uint32_t *)(P)); *(uint32_t *)(P) = 0U;)
 */
static inline uint32_t atomic_readandclear32(uint32_t *p)
{
	return atomic_swap32(p, 0U);
}

/*
* #define atomic_readandclear64(P) \
* (return (*(uint64_t *)(P)); *(uint64_t *)(P) = 0UL;)
 */
static inline uint64_t atomic_readandclear64(uint64_t *p)
{
	return atomic_swap64(p, 0UL);
}

/* Linux ARM64 **_return use the same way with x86, so here use x86 codes */
static inline int32_t atomic_add_return(int32_t *p, int32_t v)
{
	return (atomic_xadd32((uint32_t *)p, v) + v);
}

static inline int32_t atomic_sub_return(int32_t *p, int32_t v)
{
	return (atomic_xadd32((uint32_t *)p, -v) - v);
}

static inline int32_t atomic_inc_return(int32_t *v)
{
	return atomic_add_return(v, 1);
}

static inline int32_t atomic_dec_return(int32_t *v)
{
	return atomic_sub_return(v, 1);
}

static inline int64_t atomic_add64_return(int64_t *p, int64_t v)
{
	return (atomic_xadd64((uint64_t *)p, v) + v);
}

static inline int64_t atomic_sub64_return(int64_t *p, int64_t v)
{
	return (atomic_xadd64((uint64_t *)p, -v) - v);
}

static inline int64_t atomic_inc64_return(int64_t *v)
{
	return atomic_add64_return(v, 1);
}

static inline int64_t atomic_dec64_return(int64_t *v)
{
	return atomic_sub64_return(v, 1);
}

#endif /* __AARCH64_ATOMIC_H__*/
