#!/usr/bin/env python3
"""从 Ascend/HCCL 日志目录中提取 ranktable / OXC / superPod 等目标日志内容。"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence

PRESETS = {
    "ranktable": {
        "description": "通用 ranktable 解析相关日志（跨版本宽匹配）",
        "patterns": [
            r"ranktable",
            r"ParserClusterInfo",
            r"GetClusterInfo",
            r"OXC_HCOMM",
            r"super_pod",
            r"superPod",
            r"super_device",
            r"superDevice",
            r"device_ip",
            r"backup_device_ip",
            r"host_ip",
            r"netPlane",
            r"\[Parse\]\[Summary",
            r"\[Parse\]\[Detail",
        ],
    },
    "ranktable-v12": {
        "description": "version=1.2 / superPod 场景常见日志关键字",
        "patterns": [
            r"ParserClusterInfo serverId",
            r"super_pod_list",
            r"super_pod_id",
            r"superPodId",
            r"superDeviceId",
            r"server id\[",
            r"serverIdx",
            r"host_ip",
            r"device_ip",
            r"backup_device_ip",
        ],
    },
    "ranktable-oxc": {
        "description": "OXC 1.4 / A3 成功与失败解析日志关键字",
        "patterns": [
            r"OXC_HCOMM",
            r"\[OXC_HCOMM\]\[Parse\]\[Summary",
            r"\[OXC_HCOMM\]\[Parse\]\[Detail",
            r"group_id",
            r"net_layer",
            r"net_type",
            r"net_instance_id",
            r"rank_addr",
            r"device_port",
            r"backup_device_port",
            r"super_pod_id",
            r"super_device_id",
        ],
    },
    "netplane": {
        "description": "netPlane / plane transformer / rankgraph 相关日志",
        "patterns": [
            r"netPlane",
            r"ParsePlane",
            r"TopoinfoPlaneTransformer",
            r"RankGraph",
            r"groupId",
            r"groupSize",
        ],
    },
}

DEFAULT_AREAS = ("run", "debug", "security")
DEFAULT_INCLUDE_GLOBS = ("*.log", "*.txt", "*.out", "*.err", "*")
MAX_FILE_BYTES = 50 * 1024 * 1024


@dataclass
class MatchRecord:
    file: str
    area: str
    line_no: int
    line: str
    matched_by: list[str]
    context_before: list[str]
    context_after: list[str]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="从日志目录中提取 ranktable / OXC / superPod / netplane 等目标日志内容。",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=(
            "示例:\n"
            "  python3 scripts/log_extract.py /path/to/log --preset ranktable-oxc\n"
            "  python3 scripts/log_extract.py /path/to/log --preset ranktable-v12 --areas debug run\n"
            "  python3 scripts/log_extract.py /path/to/log --pattern 'OXC_HCOMM' --pattern 'ranktable' --context 1\n"
            "  python3 scripts/log_extract.py /path/to/log --preset ranktable --format json --save result.json\n"
        ),
    )
    parser.add_argument("log_root", nargs="?", help="日志根目录，通常包含 debug/run/security")
    parser.add_argument("--preset", default="ranktable", choices=sorted(PRESETS), help="内置提取模式")
    parser.add_argument("--pattern", action="append", default=[], help="额外追加的正则关键字，可多次传入")
    parser.add_argument("--areas", nargs="*", default=list(DEFAULT_AREAS), help="只扫描这些一级目录，如 run debug security")
    parser.add_argument("--context", type=int, default=0, help="为每条命中额外带上前后 N 行上下文")
    parser.add_argument("--format", choices=("text", "json"), default="text", help="输出格式")
    parser.add_argument("--save", help="把结果写入文件")
    parser.add_argument("--case-sensitive", action="store_true", help="大小写敏感匹配")
    parser.add_argument("--include-glob", action="append", default=[], help="文件名 glob 过滤，可多次传入")
    parser.add_argument("--exclude-glob", action="append", default=[], help="排除文件名 glob，可多次传入")
    parser.add_argument("--max-matches-per-file", type=int, default=200, help="单文件最多保留多少条命中")
    parser.add_argument("--max-files", type=int, default=0, help="最多扫描多少个文件，0 表示不限制")
    parser.add_argument("--list-presets", action="store_true", help="仅列出可用 preset")
    return parser.parse_args()


def list_presets() -> str:
    lines = ["可用 presets:"]
    for name, meta in PRESETS.items():
        lines.append(f"- {name}: {meta['description']}")
    return "\n".join(lines)


def compile_patterns(preset: str, extra_patterns: Sequence[str], case_sensitive: bool) -> list[tuple[str, re.Pattern[str]]]:
    flags = 0 if case_sensitive else re.IGNORECASE
    patterns = list(PRESETS[preset]["patterns"])
    patterns.extend(extra_patterns)
    return [(pattern, re.compile(pattern, flags)) for pattern in patterns]


def should_include_file(path: Path, include_globs: Sequence[str], exclude_globs: Sequence[str]) -> bool:
    name = path.name
    if any(fnmatch.fnmatch(name, glob) for glob in exclude_globs):
        return False
    return any(fnmatch.fnmatch(name, glob) for glob in include_globs)


def iter_candidate_files(log_root: Path, areas: Sequence[str], include_globs: Sequence[str], exclude_globs: Sequence[str]) -> Iterable[tuple[str, Path]]:
    seen: set[Path] = set()
    for area in areas:
        area_path = log_root / area
        if not area_path.exists() or not area_path.is_dir():
            continue
        for path in area_path.rglob("*"):
            if not path.is_file():
                continue
            if path in seen:
                continue
            if not should_include_file(path, include_globs, exclude_globs):
                continue
            seen.add(path)
            yield area, path


def looks_binary(path: Path) -> bool:
    try:
        with path.open("rb") as handle:
            chunk = handle.read(4096)
    except OSError:
        return True
    return b"\x00" in chunk


def safe_read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def extract_matches(log_root: Path, patterns: Sequence[tuple[str, re.Pattern[str]]], areas: Sequence[str], context: int,
                    include_globs: Sequence[str], exclude_globs: Sequence[str], max_matches_per_file: int,
                    max_files: int) -> tuple[list[MatchRecord], dict[str, int]]:
    records: list[MatchRecord] = []
    stats = {"files_scanned": 0, "files_matched": 0, "matches": 0, "files_skipped_binary": 0, "files_skipped_size": 0}
    for index, (area, path) in enumerate(iter_candidate_files(log_root, areas, include_globs, exclude_globs), start=1):
        if max_files and index > max_files:
            break
        try:
            if path.stat().st_size > MAX_FILE_BYTES:
                stats["files_skipped_size"] += 1
                continue
        except OSError:
            continue
        if looks_binary(path):
            stats["files_skipped_binary"] += 1
            continue
        stats["files_scanned"] += 1
        lines = safe_read_lines(path)
        matched_in_file = 0
        for line_index, line in enumerate(lines):
            hit_patterns = [name for name, regex in patterns if regex.search(line)]
            if not hit_patterns:
                continue
            if matched_in_file == 0:
                stats["files_matched"] += 1
            if matched_in_file >= max_matches_per_file:
                break
            before = lines[max(0, line_index - context):line_index]
            after = lines[line_index + 1:line_index + 1 + context]
            records.append(
                MatchRecord(
                    file=str(path.relative_to(log_root)),
                    area=area,
                    line_no=line_index + 1,
                    line=line,
                    matched_by=hit_patterns,
                    context_before=before,
                    context_after=after,
                )
            )
            matched_in_file += 1
            stats["matches"] += 1
    return records, stats


def render_text(log_root: Path, preset: str, records: Sequence[MatchRecord], stats: dict[str, int]) -> str:
    lines = [
        f"log_root: {log_root}",
        f"preset: {preset}",
        "summary:",
        f"  files_scanned: {stats['files_scanned']}",
        f"  files_matched: {stats['files_matched']}",
        f"  matches: {stats['matches']}",
        f"  files_skipped_binary: {stats['files_skipped_binary']}",
        f"  files_skipped_size: {stats['files_skipped_size']}",
        "",
    ]
    current_file = None
    for record in records:
        if record.file != current_file:
            current_file = record.file
            lines.append(f"==> {record.file} ({record.area})")
        lines.append(f"{record.line_no}: {record.line}")
        if record.matched_by:
            lines.append(f"    matched_by: {', '.join(record.matched_by)}")
        for ctx_line in record.context_before:
            lines.append(f"    - {ctx_line}")
        for ctx_line in record.context_after:
            lines.append(f"    + {ctx_line}")
    if not records:
        lines.append("(no matches)")
    return "\n".join(lines)


def render_json(log_root: Path, preset: str, records: Sequence[MatchRecord], stats: dict[str, int]) -> str:
    payload = {
        "log_root": str(log_root),
        "preset": preset,
        "summary": stats,
        "matches": [asdict(record) for record in records],
    }
    return json.dumps(payload, ensure_ascii=False, indent=2)


def main() -> int:
    args = parse_args()
    if args.list_presets:
        print(list_presets())
        return 0
    if not args.log_root:
        print("error: 缺少日志根目录参数\n", file=sys.stderr)
        print(list_presets(), file=sys.stderr)
        return 2

    log_root = Path(args.log_root).expanduser().resolve()
    if not log_root.exists() or not log_root.is_dir():
        print(f"error: 日志根目录不存在或不是目录: {log_root}", file=sys.stderr)
        return 2

    include_globs = tuple(args.include_glob or DEFAULT_INCLUDE_GLOBS)
    exclude_globs = tuple(args.exclude_glob or ())
    patterns = compile_patterns(args.preset, args.pattern, args.case_sensitive)
    records, stats = extract_matches(
        log_root=log_root,
        patterns=patterns,
        areas=args.areas,
        context=max(args.context, 0),
        include_globs=include_globs,
        exclude_globs=exclude_globs,
        max_matches_per_file=max(args.max_matches_per_file, 1),
        max_files=max(args.max_files, 0),
    )
    output = render_json(log_root, args.preset, records, stats) if args.format == "json" else render_text(log_root, args.preset, records, stats)
    print(output)
    if args.save:
        save_path = Path(args.save).expanduser()
        save_path.parent.mkdir(parents=True, exist_ok=True)
        save_path.write_text(output, encoding="utf-8")
    return 0 if records else 1


if __name__ == "__main__":
    raise SystemExit(main())
