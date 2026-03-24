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


# 匹配 [HCOMM_PROF] 日志行的正则表达式
# 日志实际格式: [filename:line] [tid][HCOMM_PROF]B,839201822719,1001,LoadOffloadCollOp
PROF_PATTERN = re.compile(r'\[HCOMM_PROF\]([BEIF]),(\d+),(\d+),([\w:.<>~]+)')


# def parse_frequency(lines):
#     """从日志中提取 TSC 频率校准信息"""
#     for line in lines:
#         m = re.search(r'\[HCOMM_PROF\]F,(\d+),0,(\w+)', line)
#         if m:
#             freq = int(m.group(1))
#             arch = m.group(2)
#             return freq, arch
#     return 0, "UNKNOWN"


# def estimate_x86_freq(lines):
#     """
#     x86 平台无法从寄存器直接获取 TSC 频率
#     尝试从 /proc/cpuinfo 推算，或回退到默认 2.5GHz
#     """
#     cpuinfo_path = "/proc/cpuinfo"
#     if os.path.exists(cpuinfo_path):
#         with open(cpuinfo_path, 'r') as f:
#             for line in f:
#                 if "model name" in line:
#                     # 尝试提取 "@ 2.50GHz" 这样的字段
#                     m = re.search(r'@\s*([\d.]+)\s*GHz', line)
#                     if m:
#                         ghz = float(m.group(1))
#                         return int(ghz * 1e9)
#                     break
#     # 默认回退 2.5 GHz
#     return 2500000000


# def convert_log_to_chrome_json(input_path, output_path, x86_freq_override=None):
#     """
#     核心转换逻辑：解析 HCCL 日志 -> Chrome Tracing JSON
#     """
#     with open(input_path, 'r', errors='replace') as f:
#         lines = f.readlines()

#     # 1. 获取频率
#     freq, arch = parse_frequency(lines)
#     if freq == 0:
#         if arch == "X86_RDTSC" or arch == "UNKNOWN":
#             freq = x86_freq_override or estimate_x86_freq(lines)
#             print(f"[INFO] x86 TSC frequency estimated: {freq / 1e9:.2f} GHz")
#         else:
#             print("[WARN] Cannot determine TSC frequency, defaulting to 50MHz (ARM)")
#             freq = 50000000
#     else:
#         print(f"[INFO] ARM64 CNTVCT frequency from log: {freq / 1e6:.1f} MHz")

#     # 2. 解析所有打点事件
#     trace_events = []
#     thread_names = set()

#     for line in lines:
#         m = PROF_PATTERN.search(line)
#         if not m:
#             continue

#         phase = m.group(1)
#         tsc = int(m.group(2))
#         tid = int(m.group(3))
#         name = m.group(4)

#         if phase == 'F':
#             continue  # 跳过频率校准行

#         # TSC -> 微秒 (Chrome Tracing 要求时间戳单位为微秒)
#         ts_us = tsc / (freq / 1e6)

#         event = {
#             "name": name,
#             "ph": phase,
#             "ts": ts_us,
#             "tid": tid,
#             "pid": 0,  # 单进程，pid 固定为 0
#         }

#         # 对于 Instant 事件，设置 scope
#         if phase == 'I':
#             event["s"] = "t"  # thread scope

#         trace_events.append(event)
#         thread_names.add(tid)

#     # 3. 为每个线程添加 thread_name 元数据
#     for tid in sorted(thread_names):
#         trace_events.append({
#             "name": "thread_name",
#             "ph": "M",
#             "pid": 0,
#             "tid": tid,
#             "args": {"name": f"HCCL-Worker-{tid}"}
#         })

#     # 4. 添加进程名元数据
#     trace_events.append({
#         "name": "process_name",
#         "ph": "M",
#         "pid": 0,
#         "tid": 0,
#         "args": {"name": "HCCL Communicator"}
#     })

#     # 5. 输出 JSON
#     output = {"traceEvents": trace_events}
#     with open(output_path, 'w') as f:
#         json.dump(output, f, indent=None, separators=(',', ':'))

#     print(f"[OK] Converted {len(trace_events)} events -> {output_path}")
#     print(f"[OK] Next steps:")
#     print(f"     1. View in Chrome:  Open chrome://tracing and load {output_path}")
#     print(f"     2. Convert to Tracy: ./import-chrome {output_path} output.tracy")


def main():
    pass
#     parser = argparse.ArgumentParser(
#         description="Convert HCCL [HCOMM_PROF] log to Chrome Tracing JSON (Tracy compatible)",
#         formatter_class=argparse.RawDescriptionHelpFormatter,
#         epilog="""
# Examples:
#   %(prog)s -i hccl_run.log -o trace.json
#   %(prog)s -i hccl_run.log -o trace.json --x86-freq 2500000000
#         """
#     )
#     parser.add_argument("-i", "--input", required=True, help="Input HCCL log file path")
#     parser.add_argument("-o", "--output", required=True, help="Output Chrome Tracing JSON file path")
#     parser.add_argument("--x86-freq", type=int, default=None,
#                         help="Override x86 TSC frequency in Hz (e.g. 2500000000 for 2.5GHz)")

#     args = parser.parse_args()

#     if not os.path.exists(args.input):
#         print(f"[ERROR] Input file not found: {args.input}", file=sys.stderr)
#         sys.exit(1)

#     convert_log_to_chrome_json(args.input, args.output, args.x86_freq)


if __name__ == "__main__":
    main()
