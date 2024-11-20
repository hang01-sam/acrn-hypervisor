#!/usr/bin/env python3
#
# Copyright (C) 2023-2024 Samsung Electronics Co., Ltd.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities
from acrn_config_utilities import get_node


def alloc_infos(board_etree, scenario_etree, allocation_etree):
    cpus = board_etree.xpath("//processors//core")
    for cpu in cpus:
        cpu_id = cpu.attrib['id']
        hw_id = get_node("./hw_id/text()", cpu)
        cpu_node = acrn_config_utilities.append_node(f"//hv/cpuinfo/CPU", None, allocation_etree, id=cpu_id)
        acrn_config_utilities.append_node(f"./hw_id", hw_id, cpu_node)

def fn(board_etree, scenario_etree, allocation_etree):
    acrn_config_utilities.append_node("/acrn-config/hv/cpuinfo", None, allocation_etree)
    alloc_infos(board_etree, scenario_etree, allocation_etree)
