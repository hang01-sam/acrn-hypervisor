/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/page.h>
#include <asm/config.h>

ENTRY(__start)

SECTIONS
{
    . = ACRN_VIRT_START;
    . = ACRN_VIRT_START + ACRN_BOOT_OFFSET;
    .boot : {
        ld_ram_start = .;
    }

    .head : {
        *(.header)
    }

    .text :
    {
        *(.text);
        *(.note.gnu.build-id)
    }
    . = ALIGN(PAGE_SIZE);

    .rodata : {
        *(.rodata*);
    }
    . = ALIGN(PAGE_SIZE);

    .data : {
        *(.data)
    }
    . = ALIGN(PAGE_SIZE);

    .bss : {
        __bss_start = .;
        *(.bss.cpu_stack);

        __boot_pgtable = .;
        . = __boot_pgtable + PAGE_SIZE;
        __boot_first = .;
        . = __boot_first + PAGE_SIZE;

        *(.bss)
        __bss_end = .;
    }
    . = ALIGN(PAGE_SIZE);

    ld_ram_end = .;
}
