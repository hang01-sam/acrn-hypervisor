/*-
 * Copyright (c) 2013  Chris Torek <torek @ torek net>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/uio.h>
#include <stdio.h>
#include <stddef.h>
#include <pthread.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

/*
 * Functions for dealing with generalized "virtual devices" as
 * defined by <https://www.google.com/#output=search&q=virtio+spec>
 */

/*
 * In case we decide to relax the "virtio struct comes at the
 * front of virtio-based device struct" constraint, let's use
 * this to convert.
 */
#define DEV_STRUCT(vs) ((void *)(vs))

/**
 * @brief Link a virtio_base to its constants, the virtio device,
 * and the PCI emulation.
 *
 * @param base Pointer to struct virtio_base.
 * @param vops Pointer to struct virtio_ops.
 * @param pci_virtio_dev Pointer to instance of certain virtio device.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param queues Pointer to struct virtio_vq_info, normally an array.
 *
 * @return None
 */
void
virtio_linkup(struct virtio_base *base, struct virtio_ops *vops,
	      void *pci_virtio_dev, struct pci_vdev *dev,
	      struct virtio_vq_info *queues)
{
	int i;

	/* base and pci_virtio_dev addresses must match */
	assert((void *)base == pci_virtio_dev);
	base->vops = vops;
	base->dev = dev;
	dev->arg = base;

	base->queues = queues;
	for (i = 0; i < vops->nvq; i++) {
		queues[i].base = base;
		queues[i].num = i;
	}
}

/**
 * @brief Reset device (device-wide).
 *
 * This erases all queues, i.e., all the queues become invalid.
 * But we don't wipe out the internal pointers, by just clearing
 * the VQ_ALLOC flag.
 *
 * It resets negotiated features to "none".
 * If MSI-X is enabled, this also resets all the vectors to NO_VECTOR.
 *
 * @param base Pointer to struct virtio_base.
 *
 * @return None
 */
void
virtio_reset_dev(struct virtio_base *base)
{
	struct virtio_vq_info *vq;
	int i, nvq;

/* if (base->mtx) */
/* assert(pthread_mutex_isowned_np(base->mtx)); */

	nvq = base->vops->nvq;
	for (vq = base->queues, i = 0; i < nvq; vq++, i++) {
		vq->flags = 0;
		vq->last_avail = 0;
		vq->save_used = 0;
		vq->pfn = 0;
		vq->msix_idx = VIRTIO_MSI_NO_VECTOR;
		vq->gpa_desc[0] = 0;
		vq->gpa_desc[1] = 0;
		vq->gpa_avail[0] = 0;
		vq->gpa_avail[1] = 0;
		vq->gpa_used[0] = 0;
		vq->gpa_used[1] = 0;
		vq->enabled = 0;
	}
	base->negotiated_caps = 0;
	base->curq = 0;
	/* base->status = 0; -- redundant */
	if (base->isr)
		pci_lintr_deassert(base->dev);
	base->isr = 0;
	base->msix_cfg_idx = VIRTIO_MSI_NO_VECTOR;
	base->device_feature_select = 0;
	base->driver_feature_select = 0;
	base->config_generation = 0;
}

/**
 * @brief Set I/O BAR (usually 0) to map PCI config registers.
 *
 * @param base Pointer to struct virtio_base.
 * @param barnum Which BAR[0..5] to use.
 *
 * @return None
 */
void
virtio_set_io_bar(struct virtio_base *base, int barnum)
{
	size_t size;

	/*
	 * ??? should we use CFG0 if MSI-X is disabled?
	 * Existing code did not...
	 */
	size = VIRTIO_CR_CFG1 + base->vops->cfgsize;
	pci_emul_alloc_bar(base->dev, barnum, PCIBAR_IO, size);
	base->legacy_pio_bar_idx = barnum;
}

/**
 * @brief Initialize MSI-X vector capabilities if we're to use MSI-X,
 * or MSI capabilities if not.
 *
 * We assume we want one MSI-X vector per queue, here, plus one
 * for the config vec.
 *
 *
 * @param base Pointer to struct virtio_base.
 * @param barnum Which BAR[0..5] to use.
 * @param use_msix If using MSI-X.
 *
 * @return 0 on success and non-zero on fail.
 */
int
virtio_intr_init(struct virtio_base *base, int barnum, int use_msix)
{
	int nvec;

	if (use_msix) {
		base->flags |= VIRTIO_USE_MSIX;
		VIRTIO_BASE_LOCK(base);
		virtio_reset_dev(base); /* set all vectors to NO_VECTOR */
		VIRTIO_BASE_UNLOCK(base);
		nvec = base->vops->nvq + 1;
		if (pci_emul_add_msixcap(base->dev, nvec, barnum))
			return -1;
	} else
		base->flags &= ~VIRTIO_USE_MSIX;

	/* Only 1 MSI vector for acrn-dm */
	pci_emul_add_msicap(base->dev, 1);

	/* Legacy interrupts are mandatory for virtio devices */
	pci_lintr_request(base->dev);

	return 0;
}

/**
 * @brief Initialize MSI-X vector capabilities if we're to use MSI-X,
 * or MSI capabilities if not.
 *
 * Wrapper function for virtio_intr_init() for cases we directly use
 * BAR 1 for MSI-X capabilities.
 *
 * @param base Pointer to struct virtio_base.
 * @param use_msix If using MSI-X.
 *
 * @return 0 on success and non-zero on fail.
 */
int
virtio_interrupt_init(struct virtio_base *base, int use_msix)
{
	return virtio_intr_init(base, 1, use_msix);
}

/*
 * Initialize the currently-selected virtio queue (base->curq).
 * The guest just gave us a page frame number, from which we can
 * calculate the addresses of the queue.
 * This interface is only valid for virtio legacy.
 */
static void
virtio_vq_init(struct virtio_base *base, uint32_t pfn)
{
	struct virtio_vq_info *vq;
	uint64_t phys;
	size_t size;
	char *vb;

	vq = &base->queues[base->curq];
	vq->pfn = pfn;
	phys = (uint64_t)pfn << VRING_PAGE_BITS;
	size = virtio_vring_size(vq->qsize);
	vb = paddr_guest2host(base->dev->vmctx, phys, size);

	/* First page(s) are descriptors... */
	vq->desc = (struct virtio_desc *)vb;
	vb += vq->qsize * sizeof(struct virtio_desc);

	/* ... immediately followed by "avail" ring (entirely uint16_t's) */
	vq->avail = (struct virtio_vring_avail *)vb;
	vb += (2 + vq->qsize + 1) * sizeof(uint16_t);

	/* Then it's rounded up to the next page... */
	vb = (char *)roundup2((uintptr_t)vb, VRING_ALIGN);

	/* ... and the last page(s) are the used ring. */
	vq->used = (struct virtio_vring_used *)vb;

	/* Mark queue as allocated, and start at 0 when we use it. */
	vq->flags = VQ_ALLOC;
	vq->last_avail = 0;
	vq->save_used = 0;
}

/*
 * Initialize the currently-selected virtio queue (base->curq).
 * The guest just gave us the gpa of desc array, avail ring and
 * used ring, from which we can initialize the virtqueue.
 * This interface is only valid for virtio modern.
 */
static void
virtio_vq_enable(struct virtio_base *base)
{
	struct virtio_vq_info *vq;
	uint16_t qsz;
	uint64_t phys;
	size_t size;
	char *vb;

	vq = &base->queues[base->curq];
	qsz = vq->qsize;

	/* descriptors */
	phys = (((uint64_t)vq->gpa_desc[1]) << 32) | vq->gpa_desc[0];
	size = qsz * sizeof(struct virtio_desc);
	vb = paddr_guest2host(base->dev->vmctx, phys, size);
	vq->desc = (struct virtio_desc *)vb;

	/* available ring */
	phys = (((uint64_t)vq->gpa_avail[1]) << 32) | vq->gpa_avail[0];
	size = (2 + qsz + 1) * sizeof(uint16_t);
	vb = paddr_guest2host(base->dev->vmctx, phys, size);
	vq->avail = (struct virtio_vring_avail *)vb;

	/* used ring */
	phys = (((uint64_t)vq->gpa_used[1]) << 32) | vq->gpa_used[0];
	size = sizeof(uint16_t) * 3 + sizeof(struct virtio_used) * qsz;
	vb = paddr_guest2host(base->dev->vmctx, phys, size);
	vq->used = (struct virtio_vring_used *)vb;

	/* Mark queue as allocated, and start at 0 when we use it. */
	vq->flags = VQ_ALLOC;
	vq->last_avail = 0;
	vq->save_used = 0;

	/* Mark queue as enabled. */
	vq->enabled = true;
}

/*
 * Helper inline for vq_getchain(): record the i'th "real"
 * descriptor.
 */
static inline void
_vq_record(int i, volatile struct virtio_desc *vd, struct vmctx *ctx,
	   struct iovec *iov, int n_iov, uint16_t *flags) {

	if (i >= n_iov)
		return;
	iov[i].iov_base = paddr_guest2host(ctx, vd->addr, vd->len);
	iov[i].iov_len = vd->len;
	if (flags != NULL)
		flags[i] = vd->flags;
}
#define	VQ_MAX_DESCRIPTORS	512	/* see below */

/*
 * Examine the chain of descriptors starting at the "next one" to
 * make sure that they describe a sensible request.  If so, return
 * the number of "real" descriptors that would be needed/used in
 * acting on this request.  This may be smaller than the number of
 * available descriptors, e.g., if there are two available but
 * they are two separate requests, this just returns 1.  Or, it
 * may be larger: if there are indirect descriptors involved,
 * there may only be one descriptor available but it may be an
 * indirect pointing to eight more.  We return 8 in this case,
 * i.e., we do not count the indirect descriptors, only the "real"
 * ones.
 *
 * Basically, this vets the flags and vd_next field of each
 * descriptor and tells you how many are involved.  Since some may
 * be indirect, this also needs the vmctx (in the pci_vdev
 * at base->dev) so that it can find indirect descriptors.
 *
 * As we process each descriptor, we copy and adjust it (guest to
 * host address wise, also using the vmtctx) into the given iov[]
 * array (of the given size).  If the array overflows, we stop
 * placing values into the array but keep processing descriptors,
 * up to VQ_MAX_DESCRIPTORS, before giving up and returning -1.
 * So you, the caller, must not assume that iov[] is as big as the
 * return value (you can process the same thing twice to allocate
 * a larger iov array if needed, or supply a zero length to find
 * out how much space is needed).
 *
 * If you want to verify the WRITE flag on each descriptor, pass a
 * non-NULL "flags" pointer to an array of "uint16_t" of the same size
 * as n_iov and we'll copy each flags field after unwinding any
 * indirects.
 *
 * If some descriptor(s) are invalid, this prints a diagnostic message
 * and returns -1.  If no descriptors are ready now it simply returns 0.
 *
 * You are assumed to have done a vq_ring_ready() if needed (note
 * that vq_has_descs() does one).
 */
int
vq_getchain(struct virtio_vq_info *vq, uint16_t *pidx,
	    struct iovec *iov, int n_iov, uint16_t *flags)
{
	int i;
	u_int ndesc, n_indir;
	u_int idx, next;

	volatile struct virtio_desc *vdir, *vindir, *vp;
	struct vmctx *ctx;
	struct virtio_base *base;
	const char *name;

	base = vq->base;
	name = base->vops->name;

	/*
	 * Note: it's the responsibility of the guest not to
	 * update vq->avail->idx until all of the descriptors
	 * the guest has written are valid (including all their
	 * next fields and vd_flags).
	 *
	 * Compute (last_avail - idx) in integers mod 2**16.  This is
	 * the number of descriptors the device has made available
	 * since the last time we updated vq->last_avail.
	 *
	 * We just need to do the subtraction as an unsigned int,
	 * then trim off excess bits.
	 */
	idx = vq->last_avail;
	ndesc = (uint16_t)((u_int)vq->avail->idx - idx);
	if (ndesc == 0)
		return 0;
	if (ndesc > vq->qsize) {
		/* XXX need better way to diagnose issues */
		fprintf(stderr,
		    "%s: ndesc (%u) out of range, driver confused?\r\n",
		    name, (u_int)ndesc);
		return -1;
	}

	/*
	 * Now count/parse "involved" descriptors starting from
	 * the head of the chain.
	 *
	 * To prevent loops, we could be more complicated and
	 * check whether we're re-visiting a previously visited
	 * index, but we just abort if the count gets excessive.
	 */
	ctx = base->dev->vmctx;
	*pidx = next = vq->avail->ring[idx & (vq->qsize - 1)];
	vq->last_avail++;
	for (i = 0; i < VQ_MAX_DESCRIPTORS; next = vdir->next) {
		if (next >= vq->qsize) {
			fprintf(stderr,
			    "%s: descriptor index %u out of range, "
			    "driver confused?\r\n",
			    name, next);
			return -1;
		}
		vdir = &vq->desc[next];
		if ((vdir->flags & ACRN_VRING_DESC_F_INDIRECT) == 0) {
			_vq_record(i, vdir, ctx, iov, n_iov, flags);
			i++;
		} else if ((base->device_caps &
		    ACRN_VIRTIO_RING_F_INDIRECT_DESC) == 0) {
			fprintf(stderr,
			    "%s: descriptor has forbidden INDIRECT flag, "
			    "driver confused?\r\n",
			    name);
			return -1;
		} else {
			n_indir = vdir->len / 16;
			if ((vdir->len & 0xf) || n_indir == 0) {
				fprintf(stderr,
				    "%s: invalid indir len 0x%x, "
				    "driver confused?\r\n",
				    name, (u_int)vdir->len);
				return -1;
			}
			vindir = paddr_guest2host(ctx,
			    vdir->addr, vdir->len);
			/*
			 * Indirects start at the 0th, then follow
			 * their own embedded "next"s until those run
			 * out.  Each one's indirect flag must be off
			 * (we don't really have to check, could just
			 * ignore errors...).
			 */
			next = 0;
			for (;;) {
				vp = &vindir[next];
				if (vp->flags & ACRN_VRING_DESC_F_INDIRECT) {
					fprintf(stderr,
					    "%s: indirect desc has INDIR flag,"
					    " driver confused?\r\n",
					    name);
					return -1;
				}
				_vq_record(i, vp, ctx, iov, n_iov, flags);
				if (++i > VQ_MAX_DESCRIPTORS)
					goto loopy;
				if ((vp->flags & ACRN_VRING_DESC_F_NEXT) == 0)
					break;
				next = vp->next;
				if (next >= n_indir) {
					fprintf(stderr,
					    "%s: invalid next %u > %u, "
					    "driver confused?\r\n",
					    name, (u_int)next, n_indir);
					return -1;
				}
			}
		}
		if ((vdir->flags & ACRN_VRING_DESC_F_NEXT) == 0)
			return i;
	}
loopy:
	fprintf(stderr,
	    "%s: descriptor loop? count > %d - driver confused?\r\n",
	    name, i);
	return -1;
}

/*
 * Return the currently-first request chain back to the available queue.
 *
 * (This chain is the one you handled when you called vq_getchain()
 * and used its positive return value.)
 */
void
vq_retchain(struct virtio_vq_info *vq)
{
	vq->last_avail--;
}

/*
 * Return specified request chain to the guest, setting its I/O length
 * to the provided value.
 *
 * (This chain is the one you handled when you called vq_getchain()
 * and used its positive return value.)
 */
void
vq_relchain(struct virtio_vq_info *vq, uint16_t idx, uint32_t iolen)
{
	uint16_t uidx, mask;
	volatile struct virtio_vring_used *vuh;
	volatile struct virtio_used *vue;

	/*
	 * Notes:
	 *  - mask is N-1 where N is a power of 2 so computes x % N
	 *  - vuh points to the "used" data shared with guest
	 *  - vue points to the "used" ring entry we want to update
	 *  - head is the same value we compute in vq_iovecs().
	 *
	 * (I apologize for the two fields named idx; the
	 * virtio spec calls the one that vue points to, "id"...)
	 */
	mask = vq->qsize - 1;
	vuh = vq->used;

	uidx = vuh->idx;
	vue = &vuh->ring[uidx++ & mask];
	vue->idx = idx;
	vue->tlen = iolen;
	vuh->idx = uidx;
}

/*
 * Driver has finished processing "available" chains and calling
 * vq_relchain on each one.  If driver used all the available
 * chains, used_all should be set.
 *
 * If the "used" index moved we may need to inform the guest, i.e.,
 * deliver an interrupt.  Even if the used index did NOT move we
 * may need to deliver an interrupt, if the avail ring is empty and
 * we are supposed to interrupt on empty.
 *
 * Note that used_all_avail is provided by the caller because it's
 * a snapshot of the ring state when he decided to finish interrupt
 * processing -- it's possible that descriptors became available after
 * that point.  (It's also typically a constant 1/True as well.)
 */
void
vq_endchains(struct virtio_vq_info *vq, int used_all_avail)
{
	struct virtio_base *base;
	uint16_t event_idx, new_idx, old_idx;
	int intr;

	/*
	 * Interrupt generation: if we're using EVENT_IDX,
	 * interrupt if we've crossed the event threshold.
	 * Otherwise interrupt is generated if we added "used" entries,
	 * but suppressed by ACRN_VRING_AVAIL_F_NO_INTERRUPT.
	 *
	 * In any case, though, if NOTIFY_ON_EMPTY is set and the
	 * entire avail was processed, we need to interrupt always.
	 */
	base = vq->base;
	old_idx = vq->save_used;
	vq->save_used = new_idx = vq->used->idx;
	if (used_all_avail &&
	    (base->negotiated_caps & ACRN_VIRTIO_F_NOTIFY_ON_EMPTY))
		intr = 1;
	else if (base->negotiated_caps & ACRN_VIRTIO_RING_F_EVENT_IDX) {
		event_idx = VQ_USED_EVENT_IDX(vq);
		/*
		 * This calculation is per docs and the kernel
		 * (see src/sys/dev/virtio/virtio_ring.h).
		 */
		intr = (uint16_t)(new_idx - event_idx - 1) <
			(uint16_t)(new_idx - old_idx);
	} else {
		intr = new_idx != old_idx &&
		    !(vq->avail->flags & ACRN_VRING_AVAIL_F_NO_INTERRUPT);
	}
	if (intr)
		vq_interrupt(base, vq);
}

struct config_reg {
	uint16_t	offset;	/* register offset */
	uint8_t		size;	/* size (bytes) */
	uint8_t		ro;	/* true => reg is read only */
	const char	*name;	/* name of reg */
};

/* Note: these are in sorted order to make for a fast search */
static struct config_reg legacy_config_regs[] = {
	{ VIRTIO_CR_HOSTCAP,	4, 1, "HOSTCAP" },
	{ VIRTIO_CR_GUESTCAP,	4, 0, "GUESTCAP" },
	{ VIRTIO_CR_PFN,	4, 0, "PFN" },
	{ VIRTIO_CR_QNUM,	2, 1, "QNUM" },
	{ VIRTIO_CR_QSEL,	2, 0, "QSEL" },
	{ VIRTIO_CR_QNOTIFY,	2, 0, "QNOTIFY" },
	{ VIRTIO_CR_STATUS,	1, 0, "STATUS" },
	{ VIRTIO_CR_ISR,	1, 0, "ISR" },
	{ VIRTIO_CR_CFGVEC,	2, 0, "CFGVEC" },
	{ VIRTIO_CR_QVEC,	2, 0, "QVEC" },
};

/* Note: these are in sorted order to make for a fast search */
static struct config_reg modern_config_regs[] = {
	{ VIRTIO_COMMON_DFSELECT,	4, 0, "DFSELECT" },
	{ VIRTIO_COMMON_DF,		4, 1, "DF" },
	{ VIRTIO_COMMON_GFSELECT,	4, 0, "GFSELECT" },
	{ VIRTIO_COMMON_GF,		4, 0, "GF" },
	{ VIRTIO_COMMON_MSIX,		2, 0, "MSIX" },
	{ VIRTIO_COMMON_NUMQ,		2, 1, "NUMQ" },
	{ VIRTIO_COMMON_STATUS,		1, 0, "STATUS" },
	{ VIRTIO_COMMON_CFGGENERATION,	1, 1, "CFGGENERATION" },
	{ VIRTIO_COMMON_Q_SELECT,	2, 0, "Q_SELECT" },
	{ VIRTIO_COMMON_Q_SIZE,		2, 0, "Q_SIZE" },
	{ VIRTIO_COMMON_Q_MSIX,		2, 0, "Q_MSIX" },
	{ VIRTIO_COMMON_Q_ENABLE,	2, 0, "Q_ENABLE" },
	{ VIRTIO_COMMON_Q_NOFF,		2, 1, "Q_NOFF" },
	{ VIRTIO_COMMON_Q_DESCLO,	4, 0, "Q_DESCLO" },
	{ VIRTIO_COMMON_Q_DESCHI,	4, 0, "Q_DESCHI" },
	{ VIRTIO_COMMON_Q_AVAILLO,	4, 0, "Q_AVAILLO" },
	{ VIRTIO_COMMON_Q_AVAILHI,	4, 0, "Q_AVAILHI" },
	{ VIRTIO_COMMON_Q_USEDLO,	4, 0, "Q_USEDLO" },
	{ VIRTIO_COMMON_Q_USEDHI,	4, 0, "Q_USEDHI" },
};

static inline const struct config_reg *
virtio_find_cr(const struct config_reg *p_cr_array, u_int array_size,
	       int offset) {
	u_int hi, lo, mid;
	const struct config_reg *cr;

	lo = 0;
	hi = array_size - 1;
	while (hi >= lo) {
		mid = (hi + lo) >> 1;
		cr = p_cr_array + mid;
		if (cr->offset == offset)
			return cr;
		if (cr->offset < offset)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

static inline const struct config_reg *
virtio_find_legacy_cr(int offset) {
	return virtio_find_cr(legacy_config_regs,
		sizeof(legacy_config_regs) / sizeof(*legacy_config_regs),
		offset);
}

static inline const struct config_reg *
virtio_find_modern_cr(int offset) {
	return virtio_find_cr(modern_config_regs,
		sizeof(modern_config_regs) / sizeof(*modern_config_regs),
		offset);
}

/*
 * Handle pci config space reads.
 * If it's to the MSI-X info, do that.
 * If it's part of the virtio standard stuff, do that.
 * Otherwise dispatch to the actual driver.
 */
static uint64_t
virtio_pci_legacy_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		       int baridx, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	const struct config_reg *cr;
	uint64_t virtio_config_size, max;
	const char *name;
	uint32_t newoff;
	uint32_t value;
	int error;

	/* XXX probably should do something better than just assert() */
	assert(baridx == base->legacy_pio_bar_idx);

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	vops = base->vops;
	name = vops->name;
	value = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;

	if (size != 1 && size != 2 && size != 4)
		goto bad;

	if (pci_msix_enabled(dev))
		virtio_config_size = VIRTIO_CR_CFG1;
	else
		virtio_config_size = VIRTIO_CR_CFG0;

	if (offset >= virtio_config_size) {
		/*
		 * Subtract off the standard size (including MSI-X
		 * registers if enabled) and dispatch to underlying driver.
		 * If that fails, fall into general code.
		 */
		newoff = offset - virtio_config_size;
		max = vops->cfgsize ? vops->cfgsize : 0x100000000;
		if (newoff + size > max)
			goto bad;
		error = (*vops->cfgread)(DEV_STRUCT(base), newoff,
					 size, &value);
		if (!error)
			goto done;
	}

bad:
	cr = virtio_find_legacy_cr(offset);
	if (cr == NULL || cr->size != size) {
		if (cr != NULL) {
			/* offset must be OK, so size must be bad */
			fprintf(stderr,
			    "%s: read from %s: bad size %d\r\n",
			    name, cr->name, size);
		} else {
			fprintf(stderr,
			    "%s: read from bad offset/size %jd/%d\r\n",
			    name, (uintmax_t)offset, size);
		}
		goto done;
	}

	switch (offset) {
	case VIRTIO_CR_HOSTCAP:
		value = base->device_caps;
		break;
	case VIRTIO_CR_GUESTCAP:
		value = base->negotiated_caps;
		break;
	case VIRTIO_CR_PFN:
		if (base->curq < vops->nvq)
			value = base->queues[base->curq].pfn;
		break;
	case VIRTIO_CR_QNUM:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].qsize : 0;
		break;
	case VIRTIO_CR_QSEL:
		value = base->curq;
		break;
	case VIRTIO_CR_QNOTIFY:
		value = 0;	/* XXX */
		break;
	case VIRTIO_CR_STATUS:
		value = base->status;
		break;
	case VIRTIO_CR_ISR:
		value = base->isr;
		base->isr = 0;		/* a read clears this flag */
		if (value)
			pci_lintr_deassert(dev);
		break;
	case VIRTIO_CR_CFGVEC:
		value = base->msix_cfg_idx;
		break;
	case VIRTIO_CR_QVEC:
		value = base->curq < vops->nvq ?
		    base->queues[base->curq].msix_idx :
		    VIRTIO_MSI_NO_VECTOR;
		break;
	}
done:
	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
	return value;
}

/*
 * Handle pci config space writes.
 * If it's to the MSI-X info, do that.
 * If it's part of the virtio standard stuff, do that.
 * Otherwise dispatch to the actual driver.
 */
static void
virtio_pci_legacy_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			int baridx, uint64_t offset, int size, uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_vq_info *vq;
	struct virtio_ops *vops;
	const struct config_reg *cr;
	uint64_t virtio_config_size, max;
	const char *name;
	uint32_t newoff;
	int error;

	/* XXX probably should do something better than just assert() */
	assert(baridx == base->legacy_pio_bar_idx);

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	vops = base->vops;
	name = vops->name;

	if (size != 1 && size != 2 && size != 4)
		goto bad;

	if (pci_msix_enabled(dev))
		virtio_config_size = VIRTIO_CR_CFG1;
	else
		virtio_config_size = VIRTIO_CR_CFG0;

	if (offset >= virtio_config_size) {
		/*
		 * Subtract off the standard size (including MSI-X
		 * registers if enabled) and dispatch to underlying driver.
		 */
		newoff = offset - virtio_config_size;
		max = vops->cfgsize ? vops->cfgsize : 0x100000000;
		if (newoff + size > max)
			goto bad;
		error = (*vops->cfgwrite)(DEV_STRUCT(base), newoff,
					  size, value);
		if (!error)
			goto done;
	}

bad:
	cr = virtio_find_legacy_cr(offset);
	if (cr == NULL || cr->size != size || cr->ro) {
		if (cr != NULL) {
			/* offset must be OK, wrong size and/or reg is R/O */
			if (cr->size != size)
				fprintf(stderr,
				    "%s: write to %s: bad size %d\r\n",
				    name, cr->name, size);
			if (cr->ro)
				fprintf(stderr,
				    "%s: write to read-only reg %s\r\n",
				    name, cr->name);
		} else {
			fprintf(stderr,
			    "%s: write to bad offset/size %jd/%d\r\n",
			    name, (uintmax_t)offset, size);
		}
		goto done;
	}

	switch (offset) {
	case VIRTIO_CR_GUESTCAP:
		base->negotiated_caps = value & base->device_caps;
		if (vops->apply_features)
			(*vops->apply_features)(DEV_STRUCT(base),
			    base->negotiated_caps);
		break;
	case VIRTIO_CR_PFN:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		virtio_vq_init(base, value);
		break;
	case VIRTIO_CR_QSEL:
		/*
		 * Note that the guest is allowed to select an
		 * invalid queue; we just need to return a QNUM
		 * of 0 while the bad queue is selected.
		 */
		base->curq = value;
		break;
	case VIRTIO_CR_QNOTIFY:
		if (value >= vops->nvq) {
			fprintf(stderr, "%s: queue %d notify out of range\r\n",
				name, (int)value);
			goto done;
		}
		vq = &base->queues[value];
		if (vq->notify)
			(*vq->notify)(DEV_STRUCT(base), vq);
		else if (vops->qnotify)
			(*vops->qnotify)(DEV_STRUCT(base), vq);
		else
			fprintf(stderr,
			    "%s: qnotify queue %d: missing vq/vops notify\r\n",
				name, (int)value);
		break;
	case VIRTIO_CR_STATUS:
		base->status = value;
		if (vops->set_status)
			(*vops->set_status)(DEV_STRUCT(base), value);
		if (value == 0)
			(*vops->reset)(DEV_STRUCT(base));
		break;
	case VIRTIO_CR_CFGVEC:
		base->msix_cfg_idx = value;
		break;
	case VIRTIO_CR_QVEC:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->msix_idx = value;
		break;
	}
	goto done;

bad_qindex:
	fprintf(stderr,
	    "%s: write config reg %s: curq %d >= max %d\r\n",
	    name, cr->name, base->curq, vops->nvq);
done:
	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
}

static int
virtio_find_capability(struct virtio_base *base, uint8_t cfg_type)
{
	struct pci_vdev *dev = base->dev;
	uint8_t type;
	int rc, coff = 0;

	rc = pci_emul_find_capability(dev, PCIY_VENDOR, &coff);
	while (!rc) {
		type = pci_get_cfgdata8(dev,
			coff + offsetof(struct virtio_pci_cap, cfg_type));
		if (type == cfg_type)
			return coff;
		rc = pci_emul_find_capability(dev, PCIY_VENDOR, &coff);
	}

	return -1;
}

/*
 * Set virtio modern MMIO BAR (usually 4) to map the 4 capabilities.
 */
static int
virtio_set_modern_mmio_bar(struct virtio_base *base, int barnum)
{
	struct virtio_ops *vops;
	int rc;
	struct virtio_pci_cap cap = {
		.cap_vndr = PCIY_VENDOR,
		.cap_next = 0,
		.cap_len = sizeof(cap),
		.bar = barnum,
	};
	struct virtio_pci_notify_cap notify = {
		.cap.cap_vndr = PCIY_VENDOR,
		.cap.cap_next = 0,
		.cap.cap_len = sizeof(notify),
		.cap.cfg_type = VIRTIO_PCI_CAP_NOTIFY_CFG,
		.cap.bar = barnum,
		.cap.offset = VIRTIO_CAP_NOTIFY_OFFSET,
		.cap.length = VIRTIO_CAP_NOTIFY_SIZE,
		.notify_off_multiplier = VIRTIO_MODERN_NOTIFY_OFF_MULT,
	};
	struct virtio_pci_cfg_cap cfg = {
		.cap.cap_vndr = PCIY_VENDOR,
		.cap.cap_next = 0,
		.cap.cap_len = sizeof(cfg),
		.cap.cfg_type = VIRTIO_PCI_CAP_PCI_CFG,
	};

	vops = base->vops;

	if (vops->cfgsize > VIRTIO_CAP_DEVICE_SIZE) {
		fprintf(stderr,
			"%s: cfgsize %lu > max %d\r\n",
			vops->name, vops->cfgsize, VIRTIO_CAP_DEVICE_SIZE);
		return -1;
	}

	/* common configuration capability */
	cap.cfg_type = VIRTIO_PCI_CAP_COMMON_CFG;
	cap.offset = VIRTIO_CAP_COMMON_OFFSET;
	cap.length = VIRTIO_CAP_COMMON_SIZE;
	rc = pci_emul_add_capability(base->dev, (u_char *)&cap, sizeof(cap));
	assert(rc == 0);

	/* isr status capability */
	cap.cfg_type = VIRTIO_PCI_CAP_ISR_CFG;
	cap.offset = VIRTIO_CAP_ISR_OFFSET;
	cap.length = VIRTIO_CAP_ISR_SIZE;
	rc = pci_emul_add_capability(base->dev, (u_char *)&cap, sizeof(cap));
	assert(rc == 0);

	/* device specific configuration capability */
	cap.cfg_type = VIRTIO_PCI_CAP_DEVICE_CFG;
	cap.offset = VIRTIO_CAP_DEVICE_OFFSET;
	cap.length = VIRTIO_CAP_DEVICE_SIZE;
	rc = pci_emul_add_capability(base->dev, (u_char *)&cap, sizeof(cap));
	assert(rc == 0);

	/* notification capability */
	rc = pci_emul_add_capability(base->dev, (u_char *)&notify,
		sizeof(notify));
	assert(rc == 0);

	/* pci alternative configuration access capability */
	rc = pci_emul_add_capability(base->dev, (u_char *)&cfg, sizeof(cfg));
	assert(rc == 0);

	/* allocate and register modern memory bar */
	rc = pci_emul_alloc_bar(base->dev, barnum, PCIBAR_MEM64,
				VIRTIO_MODERN_MEM_BAR_SIZE);
	assert(rc == 0);

	base->cfg_coff = virtio_find_capability(base, VIRTIO_PCI_CAP_PCI_CFG);
	if (base->cfg_coff < 0) {
		fprintf(stderr,
			"%s: VIRTIO_PCI_CAP_PCI_CFG not found\r\n",
			vops->name);
		return -1;
	}

	base->modern_mmio_bar_idx = barnum;
	return 0;
}

/*
 * Set virtio modern PIO BAR (usually 2) to map notify capability.
 */
static int
virtio_set_modern_pio_bar(struct virtio_base *base, int barnum)
{
	int rc;
	struct virtio_pci_notify_cap notify_pio = {
		.cap.cap_vndr = PCIY_VENDOR,
		.cap.cap_next = 0,
		.cap.cap_len = sizeof(notify_pio),
		.cap.cfg_type = VIRTIO_PCI_CAP_NOTIFY_CFG,
		.cap.bar = barnum,
		.cap.offset = 0,
		.cap.length = 4,
		.notify_off_multiplier = 0,
	};

	/* notification capability */
	rc = pci_emul_add_capability(base->dev, (u_char *)&notify_pio,
		sizeof(notify_pio));
	assert(rc == 0);

	/* allocate and register modern pio bar */
	rc = pci_emul_alloc_bar(base->dev, barnum, PCIBAR_IO, 4);
	assert(rc == 0);

	base->modern_pio_bar_idx = barnum;
	return 0;
}

/**
 * @brief Set modern BAR (usually 4) to map PCI config registers.
 *
 * Set modern MMIO BAR (usually 4) to map virtio 1.0 capabilities and optional
 * set modern PIO BAR (usually 2) to map notify capability. This interface is
 * only valid for modern virtio.
 *
 * @param base Pointer to struct virtio_base.
 * @param use_notify_pio Whether use pio for notify capability.
 *
 * @return 0 on success and non-zero on fail.
 */
int
virtio_set_modern_bar(struct virtio_base *base, bool use_notify_pio)
{
	struct virtio_ops *vops;
	int rc = 0;

	vops = base->vops;

	if (!vops || (base->device_caps & ACRN_VIRTIO_F_VERSION_1) == 0)
		return -1;

	if (use_notify_pio)
		rc = virtio_set_modern_pio_bar(base,
			VIRTIO_MODERN_PIO_BAR_IDX);
	if (!rc)
		rc = virtio_set_modern_mmio_bar(base,
			VIRTIO_MODERN_MMIO_BAR_IDX);

	return rc;
}

/**
 * @brief Indicate the device has experienced an error.
 *
 * This is called when the device has experienced an error from which it
 * cannot re-cover. DEVICE_NEEDS_RESET is set to the device status register
 * and a config change intr is sent to the guest driver.
 *
 * @param base Pointer to struct virtio_base.
 *
 * @return None
 */
void
virtio_dev_error(struct virtio_base *base)
{
	if (base->negotiated_caps & ACRN_VIRTIO_F_VERSION_1) {
		/* see 2.1.2. if DRIVER_OK is set, need to send
		 * a device configuration change notification to the driver
		 */
		base->status |= VIRTIO_CR_STATUS_NEEDS_RESET;
		if (base->status & VIRTIO_CR_STATUS_DRIVER_OK)
			virtio_config_changed(base);
	}
}

static struct cap_region {
	uint64_t	cap_offset;	/* offset of capability region */
	int		cap_size;	/* size of capability region */
	int		cap_id;		/* capability id */
} cap_regions[] = {
	{VIRTIO_CAP_COMMON_OFFSET, VIRTIO_CAP_COMMON_SIZE,
		VIRTIO_PCI_CAP_COMMON_CFG},
	{VIRTIO_CAP_ISR_OFFSET, VIRTIO_CAP_ISR_SIZE,
		VIRTIO_PCI_CAP_ISR_CFG},
	{VIRTIO_CAP_DEVICE_OFFSET, VIRTIO_CAP_DEVICE_SIZE,
		VIRTIO_PCI_CAP_DEVICE_CFG},
	{VIRTIO_CAP_NOTIFY_OFFSET, VIRTIO_CAP_NOTIFY_SIZE,
		VIRTIO_PCI_CAP_NOTIFY_CFG},
};

static inline int
virtio_get_cap_id(uint64_t offset, int size)
{
	int i, rc = -1;

	for (i = 0; i < ARRAY_SIZE(cap_regions); i++) {
		if (offset >= cap_regions[i].cap_offset &&
			offset + size <= cap_regions[i].cap_offset +
			cap_regions[i].cap_size)
			return cap_regions[i].cap_id;
	}

	return rc;
}

static uint32_t
virtio_common_cfg_read(struct pci_vdev *dev, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	const struct config_reg *cr;
	const char *name;
	uint32_t value;

	vops = base->vops;
	name = vops->name;
	value = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;

	cr = virtio_find_modern_cr(offset);
	if (cr == NULL || cr->size != size) {
		if (cr != NULL) {
			/* offset must be OK, so size must be bad */
			fprintf(stderr,
				"%s: read from %s: bad size %d\r\n",
				name, cr->name, size);
		} else {
			fprintf(stderr,
				"%s: read from bad offset/size %jd/%d\r\n",
				name, (uintmax_t)offset, size);
		}

		return value;
	}

	switch (offset) {
	case VIRTIO_COMMON_DFSELECT:
		value = base->device_feature_select;
		break;
	case VIRTIO_COMMON_DF:
		if (base->device_feature_select == 0)
			value = base->device_caps & 0xffffffff;
		else if (base->device_feature_select == 1)
			value = (base->device_caps >> 32) & 0xffffffff;
		else /* present 0, see 4.1.4.3.1 */
			value = 0;
		break;
	case VIRTIO_COMMON_GFSELECT:
		value = base->driver_feature_select;
		break;
	case VIRTIO_COMMON_GF:
		/* see 4.1.4.3.1. Present any valid feature bits the driver
		 * has written in driver_feature. Valid feature bits are those
		 * which are subset of the corresponding device_feature bits
		 */
		if (base->driver_feature_select == 0)
			value = base->negotiated_caps & 0xffffffff;
		else if (base->driver_feature_select == 1)
			value = (base->negotiated_caps >> 32) & 0xffffffff;
		else
			value = 0;
		break;
	case VIRTIO_COMMON_MSIX:
		value = base->msix_cfg_idx;
		break;
	case VIRTIO_COMMON_NUMQ:
		value = vops->nvq;
		break;
	case VIRTIO_COMMON_STATUS:
		value = base->status;
		break;
	case VIRTIO_COMMON_CFGGENERATION:
		value = base->config_generation;
		break;
	case VIRTIO_COMMON_Q_SELECT:
		value = base->curq;
		break;
	case VIRTIO_COMMON_Q_SIZE:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].qsize : 0;
		break;
	case VIRTIO_COMMON_Q_MSIX:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].msix_idx :
			VIRTIO_MSI_NO_VECTOR;
		break;
	case VIRTIO_COMMON_Q_ENABLE:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].enabled : 0;
		break;
	case VIRTIO_COMMON_Q_NOFF:
		value = base->curq;
		break;
	case VIRTIO_COMMON_Q_DESCLO:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].gpa_desc[0] : 0;
		break;
	case VIRTIO_COMMON_Q_DESCHI:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].gpa_desc[1] : 0;
		break;
	case VIRTIO_COMMON_Q_AVAILLO:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].gpa_avail[0] : 0;
		break;
	case VIRTIO_COMMON_Q_AVAILHI:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].gpa_avail[1] : 0;
		break;
	case VIRTIO_COMMON_Q_USEDLO:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].gpa_used[0] : 0;
		break;
	case VIRTIO_COMMON_Q_USEDHI:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].gpa_used[1] : 0;
		break;
	}

	return value;
}

static void
virtio_common_cfg_write(struct pci_vdev *dev, uint64_t offset, int size,
			uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_vq_info *vq;
	struct virtio_ops *vops;
	const struct config_reg *cr;
	const char *name;

	vops = base->vops;
	name = vops->name;

	cr = virtio_find_modern_cr(offset);
	if (cr == NULL || cr->size != size || cr->ro) {
		if (cr != NULL) {
			/* offset must be OK, wrong size and/or reg is R/O */
			if (cr->size != size)
				fprintf(stderr,
					"%s: write to %s: bad size %d\r\n",
					name, cr->name, size);
			if (cr->ro)
				fprintf(stderr,
					"%s: write to read-only reg %s\r\n",
					name, cr->name);
		} else {
			fprintf(stderr,
				"%s: write to bad offset/size %jd/%d\r\n",
				name, (uintmax_t)offset, size);
		}

		return;
	}

	switch (offset) {
	case VIRTIO_COMMON_DFSELECT:
		base->device_feature_select = value;
		break;
	case VIRTIO_COMMON_GFSELECT:
		base->driver_feature_select = value;
		break;
	case VIRTIO_COMMON_GF:
		if (base->status & VIRTIO_CR_STATUS_DRIVER_OK)
			break;
		if (base->driver_feature_select < 2) {
			value &= 0xffffffff;
			base->negotiated_caps =
				(value << (base->driver_feature_select * 32))
				& base->device_caps;
			if (vops->apply_features)
				(*vops->apply_features)(DEV_STRUCT(base),
					base->negotiated_caps);
		}
		break;
	case VIRTIO_COMMON_MSIX:
		base->msix_cfg_idx = value;
		break;
	case VIRTIO_COMMON_STATUS:
		base->status = value & 0xff;
		if (vops->set_status)
			(*vops->set_status)(DEV_STRUCT(base), value);
		if (base->status == 0)
			(*vops->reset)(DEV_STRUCT(base));
		break;
	case VIRTIO_COMMON_Q_SELECT:
		/*
		 * Note that the guest is allowed to select an
		 * invalid queue; we just need to return a QNUM
		 * of 0 while the bad queue is selected.
		 */
		base->curq = value;
		break;
	case VIRTIO_COMMON_Q_SIZE:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->qsize = value;
		break;
	case VIRTIO_COMMON_Q_MSIX:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->msix_idx = value;
		break;
	case VIRTIO_COMMON_Q_ENABLE:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		virtio_vq_enable(base);
		break;
	case VIRTIO_COMMON_Q_DESCLO:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->gpa_desc[0] = value;
		break;
	case VIRTIO_COMMON_Q_DESCHI:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->gpa_desc[1] = value;
		break;
	case VIRTIO_COMMON_Q_AVAILLO:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->gpa_avail[0] = value;
		break;
	case VIRTIO_COMMON_Q_AVAILHI:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->gpa_avail[1] = value;
		break;
	case VIRTIO_COMMON_Q_USEDLO:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->gpa_used[0] = value;
		break;
	case VIRTIO_COMMON_Q_USEDHI:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->gpa_used[1] = value;
		break;
	}

	return;

bad_qindex:
	fprintf(stderr,
		"%s: write config reg %s: curq %d >= max %d\r\n",
		name, cr->name, base->curq, vops->nvq);
}

/* ignore driver writes to ISR region, and only support ISR region read */
static uint32_t
virtio_isr_cfg_read(struct pci_vdev *dev, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;
	uint32_t value = 0;

	value = base->isr;
	base->isr = 0;		/* a read clears this flag */
	if (value)
		pci_lintr_deassert(dev);

	return value;
}

static uint32_t
virtio_device_cfg_read(struct pci_vdev *dev, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	const char *name;
	uint32_t value;
	uint64_t max;
	int error;

	vops = base->vops;
	name = vops->name;
	value = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;
	max = vops->cfgsize ? vops->cfgsize : 0x100000000;

	if (offset + size > max) {
		fprintf(stderr,
			"%s: reading from 0x%lx size %d exceeds limit\r\n",
			name, offset, size);
		return value;
	}

	error = (*vops->cfgread)(DEV_STRUCT(base), offset, size, &value);
	if (error) {
		fprintf(stderr,
			"%s: reading from 0x%lx size %d failed %d\r\n",
			name, offset, size, error);
		value = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;
	}

	return value;
}

static void
virtio_device_cfg_write(struct pci_vdev *dev, uint64_t offset, int size,
			uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	const char *name;
	uint64_t max;
	int error;

	vops = base->vops;
	name = vops->name;
	max = vops->cfgsize ? vops->cfgsize : 0x100000000;

	if (offset + size > max) {
		fprintf(stderr,
			"%s: writing to 0x%lx size %d exceeds limit\r\n",
			name, offset, size);
		return;
	}

	error = (*vops->cfgwrite)(DEV_STRUCT(base), offset, size, value);
	if (error)
		fprintf(stderr,
			"%s: writing ot 0x%lx size %d failed %d\r\n",
			name, offset, size, error);
}

/*
 * ignore driver reads from notify region, and only support notify region
 * write
 */
static void
virtio_notify_cfg_write(struct pci_vdev *dev, uint64_t offset, int size,
			uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_vq_info *vq;
	struct virtio_ops *vops;
	const char *name;
	uint64_t idx;

	idx = offset / VIRTIO_MODERN_NOTIFY_OFF_MULT;
	vops = base->vops;
	name = vops->name;

	if (idx >= vops->nvq) {
		fprintf(stderr,
			"%s: queue %lu notify out of range\r\n", name, idx);
		return;
	}

	vq = &base->queues[idx];
	if (vq->notify)
		(*vq->notify)(DEV_STRUCT(base), vq);
	else if (vops->qnotify)
		(*vops->qnotify)(DEV_STRUCT(base), vq);
	else
		fprintf(stderr,
			"%s: qnotify queue %lu: missing vq/vops notify\r\n",
			name, idx);
}

static uint32_t
virtio_pci_modern_mmio_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			    int baridx, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	const char *name;
	uint32_t value;
	int capid;

	assert(base->modern_mmio_bar_idx == baridx);

	vops = base->vops;
	name = vops->name;
	value = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;

	if (size != 1 && size != 2 && size != 4) {
		fprintf(stderr,
			"%s: read from [%d:0x%lx] bad size %d\r\n",
			name, baridx, offset, size);
		return value;
	}

	capid = virtio_get_cap_id(offset, size);
	if (capid < 0) {
		fprintf(stderr,
			"%s: read from [%d:0x%lx] bad range %d\r\n",
			name, baridx, offset, size);
		return value;
	}

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	switch (capid) {
	case VIRTIO_PCI_CAP_COMMON_CFG:
		offset -= VIRTIO_CAP_COMMON_OFFSET;
		value = virtio_common_cfg_read(dev, offset, size);
		break;
	case VIRTIO_PCI_CAP_ISR_CFG:
		offset -= VIRTIO_CAP_ISR_OFFSET;
		value = virtio_isr_cfg_read(dev, offset, size);
		break;
	case VIRTIO_PCI_CAP_DEVICE_CFG:
		offset -= VIRTIO_CAP_DEVICE_OFFSET;
		value = virtio_device_cfg_read(dev, offset, size);
		break;
	default: /* guest driver should not read from notify region */
		fprintf(stderr,
			"%s: read from [%d:0x%lx] size %d not supported\r\n",
			name, baridx, offset, size);
	}

	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
	return value;
}

static void
virtio_pci_modern_mmio_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			     int baridx, uint64_t offset, int size,
			     uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	const char *name;
	int capid;

	assert(base->modern_mmio_bar_idx == baridx);

	vops = base->vops;
	name = vops->name;

	if (size != 1 && size != 2 && size != 4) {
		fprintf(stderr,
			"%s: write to [%d:0x%lx] bad size %d\r\n",
			name, baridx, offset, size);
		return;
	}

	capid = virtio_get_cap_id(offset, size);
	if (capid < 0) {
		fprintf(stderr,
			"%s: write to [%d:0x%lx] bad range %d\r\n",
			name, baridx, offset, size);
		return;
	}

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	switch (capid) {
	case VIRTIO_PCI_CAP_COMMON_CFG:
		offset -= VIRTIO_CAP_COMMON_OFFSET;
		virtio_common_cfg_write(dev, offset, size, value);
		break;
	case VIRTIO_PCI_CAP_DEVICE_CFG:
		offset -= VIRTIO_CAP_DEVICE_OFFSET;
		virtio_device_cfg_write(dev, offset, size, value);
		break;
	case VIRTIO_PCI_CAP_NOTIFY_CFG:
		offset -= VIRTIO_CAP_NOTIFY_OFFSET;
		virtio_notify_cfg_write(dev, offset, size, value);
		break;
	default: /* guest driver should not write to ISR region */
		fprintf(stderr,
			"%s: write to [%d:0x%lx] size %d not supported\r\n",
			name, baridx, offset, size);
	}

	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
}

static uint32_t
virtio_pci_modern_pio_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			   int baridx, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;

	assert(base->modern_pio_bar_idx == baridx);
	/* guest driver should not read notify pio */
	return 0;
}

static void
virtio_pci_modern_pio_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			    int baridx, uint64_t offset, int size,
			    uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_vq_info *vq;
	struct virtio_ops *vops;
	const char *name;
	uint64_t idx;

	assert(base->modern_pio_bar_idx == baridx);

	vops = base->vops;
	name = vops->name;
	idx = value;

	if (size != 1 && size != 2 && size != 4) {
		fprintf(stderr,
			"%s: write to [%d:0x%lx] bad size %d\r\n",
			name, baridx, offset, size);
		return;
	}

	if (idx >= vops->nvq) {
		fprintf(stderr,
			"%s: queue %lu notify out of range\r\n", name, idx);
		return;
	}

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	vq = &base->queues[idx];
	if (vq->notify)
		(*vq->notify)(DEV_STRUCT(base), vq);
	else if (vops->qnotify)
		(*vops->qnotify)(DEV_STRUCT(base), vq);
	else
		fprintf(stderr,
			"%s: qnotify queue %lu: missing vq/vops notify\r\n",
			name, idx);

	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
}

/**
 * @brief Handle PCI configuration space reads.
 *
 * Handle virtio standard register reads, and dispatch other reads to
 * actual virtio device driver.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param baridx Which BAR[0..5] to use.
 * @param offset Register offset in bytes within a BAR region.
 * @param size Access range in bytes.
 *
 * @return register value.
 */
uint64_t
virtio_pci_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		int baridx, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;

	if (base->flags & VIRTIO_USE_MSIX) {
		if (baridx == pci_msix_table_bar(dev) ||
		    baridx == pci_msix_pba_bar(dev)) {
			return pci_emul_msix_tread(dev, offset, size);
		}
	}

	if (baridx == base->legacy_pio_bar_idx)
		return virtio_pci_legacy_read(ctx, vcpu, dev, baridx,
			offset, size);

	if (baridx == base->modern_mmio_bar_idx)
		return virtio_pci_modern_mmio_read(ctx, vcpu, dev, baridx,
			offset, size);

	if (baridx == base->modern_pio_bar_idx)
		return virtio_pci_modern_pio_read(ctx, vcpu, dev, baridx,
			offset, size);

	fprintf(stderr, "%s: read unexpected baridx %d\r\n",
		base->vops->name, baridx);
	return size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;
}

/**
 * @brief Handle PCI configuration space writes.
 *
 * Handle virtio standard register writes, and dispatch other writes to
 * actual virtio device driver.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param baridx Which BAR[0..5] to use.
 * @param offset Register offset in bytes within a BAR region.
 * @param size Access range in bytes.
 * @param value Data value to be written into register.
 *
 * @return None
 */
void
virtio_pci_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		 int baridx, uint64_t offset, int size, uint64_t value)
{
	struct virtio_base *base = dev->arg;

	if (base->flags & VIRTIO_USE_MSIX) {
		if (baridx == pci_msix_table_bar(dev) ||
		    baridx == pci_msix_pba_bar(dev)) {
			pci_emul_msix_twrite(dev, offset, size, value);
			return;
		}
	}

	if (baridx == base->legacy_pio_bar_idx) {
		virtio_pci_legacy_write(ctx, vcpu, dev, baridx,
			offset, size, value);
		return;
	}

	if (baridx == base->modern_mmio_bar_idx) {
		virtio_pci_modern_mmio_write(ctx, vcpu, dev, baridx,
			offset, size, value);
		return;
	}

	if (baridx == base->modern_pio_bar_idx) {
		virtio_pci_modern_pio_write(ctx, vcpu, dev, baridx,
			offset, size, value);
		return;
	}

	fprintf(stderr, "%s: write unexpected baridx %d\r\n",
		base->vops->name, baridx);
}

/**
 * @brief Handle PCI configuration space reads.
 *
 * Handle virtio PCI configuration space reads. Only the specific registers
 * that need speical operation are handled in this callback. For others just
 * fallback to pci core. This interface is only valid for virtio modern.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param coff Register offset in bytes within PCI configuration space.
 * @param bytes Access range in bytes.
 * @param rv The value returned as read.
 *
 * @return 0 on handled and non-zero on non-handled.
 */
int
virtio_pci_modern_cfgread(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			  int coff, int bytes, uint32_t *rv)
{
	struct virtio_base *base = dev->arg;
	struct virtio_pci_cfg_cap *cfg;
	uint32_t value;
	int cfg_coff = base->cfg_coff;
	size_t cfg_data_offset;

	cfg_data_offset = offsetof(struct virtio_pci_cfg_cap, pci_cfg_data);

	/* we only need to handle the read to
	 * virtio_pci_cfg_cap.pci_cfg_data[]
	 * fallback for anything else by return -1
	 */
	if ((cfg_coff > 0) && (coff >= cfg_coff + cfg_data_offset) &&
		(coff + bytes <= cfg_coff + sizeof(*cfg))) {
		cfg = (struct virtio_pci_cfg_cap *)&dev->cfgdata[cfg_coff];
		if (cfg->cap.bar == base->modern_pio_bar_idx)
			value = virtio_pci_modern_pio_read(ctx, vcpu, dev,
				cfg->cap.bar, cfg->cap.offset, cfg->cap.length);
		else if (cfg->cap.bar == base->modern_mmio_bar_idx)
			value = virtio_pci_modern_mmio_read(ctx, vcpu, dev,
				cfg->cap.bar, cfg->cap.offset, cfg->cap.length);
		else {
			fprintf(stderr, "%s: cfgread unexpected baridx %d\r\n",
				base->vops->name, cfg->cap.bar);
			value = 0;
		}

		/* update pci_cfg_data */
		if (cfg->cap.length == 1)
			pci_set_cfgdata8(dev, cfg_coff + cfg_data_offset,
				value);
		else if (cfg->cap.length == 2)
			pci_set_cfgdata16(dev, cfg_coff + cfg_data_offset,
				value);
		else
			pci_set_cfgdata32(dev, cfg_coff + cfg_data_offset,
				value);

		*rv = value;
		return 0;
	}

	return -1;
}

/**
 * @brief Handle PCI configuration space writes.
 *
 * Handle virtio PCI configuration space writes. Only the specific registers
 * that need speical operation are handled in this callback. For others just
 * fallback to pci core. This interface is only valid for virtio modern.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param coff Register offset in bytes within PCI configuration space.
 * @param bytes Access range in bytes.
 * @param val The value to write.
 *
 * @return 0 on handled and non-zero on non-handled.
 */
int
virtio_pci_modern_cfgwrite(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			   int coff, int bytes, uint32_t val)
{
	struct virtio_base *base = dev->arg;
	struct virtio_pci_cfg_cap *cfg;
	int cfg_coff = base->cfg_coff;
	size_t cfg_data_offset;

	cfg_data_offset = offsetof(struct virtio_pci_cfg_cap, pci_cfg_data);

	/* we only need to handle the write to
	 * virtio_pci_cfg_cap.pci_cfg_data[]
	 * fallback for anything else by return -1
	 */
	if ((cfg_coff > 0) && (coff >= cfg_coff + cfg_data_offset) &&
		(coff + bytes <= cfg_coff + sizeof(*cfg))) {
		/* default cfg write */
		if (bytes == 1)
			pci_set_cfgdata8(dev, coff, val);
		else if (bytes == 2)
			pci_set_cfgdata16(dev, coff, val);
		else
			pci_set_cfgdata32(dev, coff, val);

		cfg = (struct virtio_pci_cfg_cap *)&dev->cfgdata[cfg_coff];
		if (cfg->cap.bar == base->modern_pio_bar_idx)
			virtio_pci_modern_pio_write(ctx, vcpu, dev,
				cfg->cap.bar, cfg->cap.offset,
				cfg->cap.length, val);
		else if (cfg->cap.bar == base->modern_mmio_bar_idx)
			virtio_pci_modern_mmio_write(ctx, vcpu, dev,
				cfg->cap.bar, cfg->cap.offset,
				cfg->cap.length, val);
		else
			fprintf(stderr, "%s: cfgwrite unexpected baridx %d\r\n",
				base->vops->name, cfg->cap.bar);

		return 0;
	}

	return -1;
}
