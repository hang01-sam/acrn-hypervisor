/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define PTDEV_HI_MMIO_SIZE			0UL

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x00U}, \
                                                .vbar_base[0] = 0x91414000UL, \
                                                .vbar_base[1] = 0x91424000UL, \
                                                .vbar_base[5] = 0x91423000UL

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}, \
                                                .vbar_base[0] = 0x91400000UL

#define NETWORK_CONTROLLER_0                    .pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0x91200000UL

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[2] = 0x91104000UL, \
                                                .vbar_base[4] = 0x91100000UL

#endif /* PCI_DEVICES_H_ */
