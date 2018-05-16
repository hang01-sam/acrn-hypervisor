/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
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

#ifndef ATOMIC_H
#define ATOMIC_H

#define	BUS_LOCK	"lock ; "

/*
 *  #define atomic_set_int(P, V)		(*(unsigned int *)(P) |= (V))
 */
static inline void atomic_set_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "orl %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_int(P, V)		(*(unsigned int *)(P) &= ~(V))
 */
static inline void atomic_clear_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "andl %1,%0"
			:  "+m" (*p)
			:  "r" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_add_int(P, V)		(*(unsigned int *)(P) += (V))
 */
static inline void atomic_add_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "addl %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_subtract_int(P, V)	(*(unsigned int *)(P) -= (V))
 */
static inline void atomic_subtract_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "subl %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_swap_int(P, V) \
 *  (return (*(unsigned int *)(P)); *(unsigned int *)(P) = (V);)
 */
static inline int atomic_swap_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "xchgl %1,%0"
			:  "+m" (*p), "+r" (v)
			:
			:  "cc", "memory");
	return v;
}

/*
 *  #define atomic_readandclear_int(P) \
 *  (return (*(unsigned int *)(P)); *(unsigned int *)(P) = 0;)
 */
#define	atomic_readandclear_int(p)	atomic_swap_int(p, 0)

/*
 *  #define atomic_set_long(P, V)		(*(unsigned long *)(P) |= (V))
 */
static inline void atomic_set_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "orq %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_long(P, V)	(*(u_long *)(P) &= ~(V))
 */
static inline void atomic_clear_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "andq %1,%0"
			:  "+m" (*p)
			:  "r" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_add_long(P, V)		(*(unsigned long *)(P) += (V))
 */
static inline void atomic_add_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "addq %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_subtract_long(P, V)	(*(unsigned long *)(P) -= (V))
 */
static inline void atomic_subtract_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "subq %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_swap_long(P, V) \
 *  (return (*(unsigned long *)(P)); *(unsigned long *)(P) = (V);)
 */
static inline long atomic_swap_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "xchgq %1,%0"
			:  "+m" (*p), "+r" (v)
			:
			:  "cc", "memory");
	return v;
}

/*
 *  #define atomic_readandclear_long(P) \
 *  (return (*(unsigned long *)(P)); *(unsigned long *)(P) = 0;)
 */
#define	atomic_readandclear_long(p)	atomic_swap_long(p, 0)

/*
 *   #define atomic_load_acq_int(P)		(*(unsigned int*)(P))
 */
static inline int atomic_load_acq_int(unsigned int *p)
{
	int ret;

	__asm __volatile("movl %1,%0"
			:  "=r"(ret)
			:  "m" (*p)
			:  "cc", "memory");
	return ret;
}

/*
 *   #define atomic_store_rel_int(P, V)	(*(unsigned int *)(P) = (V))
 */
static inline void atomic_store_rel_int(unsigned int *p, unsigned int v)
{
	__asm __volatile("movl %1,%0"
			:  "=m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *   #define atomic_load_acq_long(P)		(*(unsigned long*)(P))
 */
static inline long atomic_load_acq_long(unsigned long *p)
{
	long ret;

	__asm __volatile("movq %1,%0"
			:  "=r"(ret)
			:  "m" (*p)
			:  "cc", "memory");
	return ret;
}

/*
 *   #define atomic_store_rel_long(P, V)	(*(unsigned long *)(P) = (V))
 */
static inline void atomic_store_rel_long(unsigned long *p, unsigned long v)
{
	__asm __volatile("movq %1,%0"
			:  "=m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

static inline int atomic_cmpxchg_int(unsigned int *p,
			int old, int new)
{
	int ret;

	__asm __volatile(BUS_LOCK "cmpxchgl %2,%1"
			: "=a" (ret), "+m" (*p)
			: "r" (new), "0" (old)
			: "memory");

	return ret;
}

#define atomic_load_acq_32		atomic_load_acq_int
#define atomic_store_rel_32		atomic_store_rel_int
#define atomic_load_acq_64		atomic_load_acq_long
#define atomic_store_rel_64		atomic_store_rel_long

#define build_atomic_xadd(name, size, type, ptr, v)		\
static inline type name(type *ptr, type v)			\
{								\
	asm volatile(BUS_LOCK "xadd" size " %0,%1"		\
			: "+r" (v), "+m" (*p)			\
			:					\
			: "cc", "memory");			\
	return v;						\
 }
build_atomic_xadd(atomic_xadd, "l", int, p, v)
build_atomic_xadd(atomic_xadd64, "q", long, p, v)

#define atomic_add_return(p, v)		( atomic_xadd(p, v) + v )
#define atomic_sub_return(p, v)		( atomic_xadd(p, -v) - v )

#define atomic_inc_return(v)		atomic_add_return((v), 1)
#define atomic_dec_return(v)		atomic_sub_return((v), 1)

#define atomic_add64_return(p, v)	( atomic_xadd64(p, v) + v )
#define atomic_sub64_return(p, v)	( atomic_xadd64(p, -v) - v )

#define atomic_inc64_return(v)		atomic_add64_return((v), 1)
#define atomic_dec64_return(v)		atomic_sub64_return((v), 1)

static inline int
atomic_cmpset_long(unsigned long *dst, unsigned long expect, unsigned long src)
{
	unsigned char res;

	__asm __volatile(BUS_LOCK "cmpxchg %3,%1\n\tsete %0"
			: "=q" (res), "+m" (*dst), "+a" (expect)
			: "r" (src)
			: "memory", "cc");

	return res;
}

#endif /* ATOMIC_H*/
