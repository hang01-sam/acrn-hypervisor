/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>

void *memset(void *base, uint8_t v, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		((uint8_t *)base)[i] = v;
	}

	return base;
}

#define ALIGN_U64(ADDR) (((ADDR) & 0x7) ? 0 : 1)
void memcpy_erms(void *d, const void *s, size_t slen)
{
	size_t i;
	uint64_t *d_tmp_64 = d;
	const uint64_t *s_tmp_64 = s;
	uint8_t *d_tmp_8 = d;
	const uint8_t *s_tmp_8 = s;

	if (ALIGN_U64((uint64_t)d) && ALIGN_U64((uint64_t)s)) {
		for (i = 0; i < slen / sizeof(uint64_t); i++) {
			*d_tmp_64++ = *s_tmp_64++;
		}
		slen = slen % sizeof(uint64_t);
		d_tmp_8 = (uint8_t *)d_tmp_64;
		s_tmp_8 = (const uint8_t *)s_tmp_64;
	}

	for (i = 0; i < slen; i++) {
		*d_tmp_8++ = *s_tmp_8++;
	}
}

/* copy data from tail to head (backwards) */
void memcpy_erms_backwards(void *d, const void *s, size_t slen)
{
	size_t i;
	uint8_t *d_tmp_8 = d;
	const uint8_t *s_tmp_8 = s;

	for (i = 0; i < slen; i++) {
		*d_tmp_8-- = *s_tmp_8--;
	}
}

/*
 * @brief  Copies at most slen bytes from src address to dest address, up to dmax.
 *
 *   INPUTS
 *
 * @param[in] d        pointer to Destination address
 * @param[in] dmax     maximum  length of dest
 * @param[in] s        pointer to Source address
 * @param[in] slen     maximum number of bytes of src to copy
 *
 * @return 0 for success and -1 for runtime-constraint violation.
 */
int32_t memcpy_s(void *d, size_t dmax, const void *s, size_t slen)
{
	int32_t ret = -1;

	if ((d != NULL) && (s != NULL) && (dmax >= slen) && ((d > (s + slen)) || (s > (d + dmax)))) {
		if (slen != 0U) {
			memcpy_erms(d, s, slen);
		}
		ret = 0;
	} else {
		(void)memset(d, 0U, dmax);
	}

	return ret;
}
