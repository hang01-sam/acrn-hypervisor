/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <logmsg.h>

void asm_assert(int32_t line, const char *file, const char *txt)
{
	pr_acrnlog("Assertion failed in file %s,line %d : %s",
			   file, line, txt);

	do {
		asm_pause();
	} while (1);
}
