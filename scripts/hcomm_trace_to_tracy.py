#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""
hcomm_trace_to_tracy.py - 将 HCCL 运行日志中的 [HCOMM_PROF] 打点数据转换为 Chrome Tracing JSON

用法:
    # 步骤 1: 从 NPU 侧导出日志 (使用华为工具 msnpureport 等)
    # 步骤 2: 转换为 Chrome JSON
    python3 hcomm_trace_to_tracy.py -i /path/to/hccl_run.log -o hcomm_trace.json

    # 步骤 3: 转换为 Tracy 格式 (使用 Tracy 的 import-chrome 工具)
    ./import-chrome hcomm_trace.json hcomm_result.tracy

    # 步骤 4: 用 Tracy GUI 打开 hcomm_result.tracy 进行可视化分析

    # 或者直接用 Chrome 浏览器查看:
    # 打开 chrome://tracing 并加载 hcomm_trace.json

日志格式 (由 hcomm_profiler.h 产生):
    [HCOMM_PROF]<phase>,<tsc>,<tid>,<name>
    [HCOMM_PROF]F,<freq>,0,<arch>          -- 频率校准行
"""

import argparse
import json
import os
import re
import sys

if __name__ == "__main__":
    pass
