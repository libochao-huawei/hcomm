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


PROF_LINE_RE = re.compile(r"\[HCOMM_PROF\]([A-Za-z]),([^,]+),([^,]+),(.*)$")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert HCOMM profiler logs to Chrome tracing JSON"
    )
    parser.add_argument("-i", "--input", required=True, help="HCCL run log path")
    parser.add_argument("-o", "--output", required=True, help="Output Chrome JSON path")
    parser.add_argument(
        "--x86-freq",
        type=int,
        default=0,
        help="Fallback TSC frequency in Hz when no F calibration line exists",
    )
    parser.add_argument(
        "--pid",
        type=int,
        default=0,
        help="Process id used in Chrome trace events",
    )
    return parser.parse_args()


def parse_prof_lines(log_path):
    records = []
    freq_hz = 0
    arch = "unknown"

    with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
        for line_no, line in enumerate(f, start=1):
            match = PROF_LINE_RE.search(line)
            if not match:
                continue

            phase = match.group(1)
            tsc_str = match.group(2).strip()
            tid_str = match.group(3).strip()
            name = match.group(4).strip()

            try:
                tsc = int(tsc_str)
            except ValueError:
                continue

            if phase == "F":
                if tsc > 0:
                    freq_hz = tsc
                arch = name or arch
                continue

            try:
                tid = int(tid_str)
            except ValueError:
                tid = 0

            records.append(
                {
                    "phase": phase,
                    "tsc": tsc,
                    "tid": tid,
                    "name": name,
                    "line": line_no,
                }
            )

    return records, freq_hz, arch


def tsc_to_us(tsc, freq_hz):
    return (float(tsc) * 1000000.0) / float(freq_hz)


def build_trace_events(records, freq_hz, pid):
    events = []
    for item in records:
        phase = item["phase"]
        if phase not in ("B", "E", "I"):
            continue

        event = {
            "name": item["name"],
            "pid": pid,
            "tid": item["tid"],
            "ts": tsc_to_us(item["tsc"], freq_hz),
        }

        if phase == "I":
            event["ph"] = "i"
            event["s"] = "t"
        else:
            event["ph"] = phase

        events.append(event)

    events.sort(key=lambda e: (e["ts"], e["tid"]))
    return events


def ensure_parent_dir(path):
    parent = os.path.dirname(os.path.abspath(path))
    if parent:
        os.makedirs(parent, exist_ok=True)


def main():
    args = parse_args()
    records, freq_hz, arch = parse_prof_lines(args.input)

    if not records:
        print("No [HCOMM_PROF] events found in input log.", file=sys.stderr)
        return 1

    if freq_hz <= 0:
        if args.x86_freq > 0:
            freq_hz = args.x86_freq
        else:
            print(
                "Missing valid [HCOMM_PROF]F calibration line and --x86-freq is not provided.",
                file=sys.stderr,
            )
            return 2

    events = build_trace_events(records, freq_hz, args.pid)

    trace = {
        "traceEvents": events,
        "displayTimeUnit": "us",
        "hcomm_meta": {
            "freq_hz": freq_hz,
            "arch": arch,
            "event_count": len(events),
        },
    }

    ensure_parent_dir(args.output)
    with open(args.output, "w", encoding="utf-8") as out:
        json.dump(trace, out, ensure_ascii=False, indent=2)

    print(
        "Converted {} events (freq={} Hz, arch={}) -> {}".format(
            len(events), freq_hz, arch, args.output
        )
    )
    return 0

if __name__ == "__main__":
    sys.exit(main())
