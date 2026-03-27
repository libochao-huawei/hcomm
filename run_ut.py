#!/usr/bin/env python3
import os
import sys
import time
import glob
import subprocess
import threading
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

MAX_WORKERS = 2
LOG_DIR = "ut_error_logs"
ERROR_EVENT = threading.Event()
ALL_PROCS = []
SOURCE_CMD = ""


def find_ut_binaries(test_dir):
    """遍历 test 目录找到所有 ELF 可执行文件（排除 .so 等库文件）"""
    binaries = []
    test_path = Path(test_dir)
    for f in test_path.rglob("*"):
        if not f.is_file() or not os.access(f, os.X_OK):
            continue
        if f.suffix in (".so", ".a", ".sh", ".py", ".json"):
            continue
        # 跳过已知的非测试工具
        if f.name in ("hccl_ut_binary_package", "hccl_ut_prepare_env"):
            continue
        # 判断是否为 ELF 可执行文件
        try:
            result = subprocess.run(
                ["file", "-b", str(f)], capture_output=True, text=True, timeout=5
            )
            if "ELF" in result.stdout:
                binaries.append(f)
        except Exception:
            continue
    return sorted(binaries)


def run_single_ut(binary_path, timeout=300):
    """执行单个 UT 二进制，返回 (path, returncode, has_failed, log_file)"""
    if ERROR_EVENT.is_set():
        return None

    script_dir = Path(__file__).resolve().parent
    os.makedirs(script_dir / LOG_DIR, exist_ok=True)
    log_file = str(script_dir / LOG_DIR / f"{binary_path.name}.log")

    abs_binary = binary_path.resolve()
    work_dir = str(abs_binary.parent)

    cmd = f'(set -o pipefail; {SOURCE_CMD} "{abs_binary}" 2>&1 | tee "{log_file}") 2>&1'
    proc = None
    try:
        proc = subprocess.Popen(
            cmd,
            shell=True,
            executable="/bin/bash",
            cwd=work_dir,
            start_new_session=True,
        )
        ALL_PROCS.append(proc)

        start_time = time.time()
        while proc.poll() is None:
            remaining = timeout - (time.time() - start_time)
            if remaining <= 0:
                try:
                    os.killpg(os.getpgid(proc.pid), 9)
                except:
                    proc.kill()
                proc.wait()
                return (binary_path, -1, True, log_file)
            if ERROR_EVENT.is_set():
                try:
                    os.killpg(os.getpgid(proc.pid), 9)
                except:
                    proc.kill()
                proc.wait()
                return (None, None, None, log_file)
            time.sleep(0.5)

        returncode = proc.returncode
    except KeyboardInterrupt:
        if proc and proc.poll() is None:
            try:
                os.killpg(os.getpgid(proc.pid), 9)
            except:
                proc.kill()
            proc.wait()
        return None
    except Exception:
        if proc and proc.poll() is None:
            try:
                os.killpg(os.getpgid(proc.pid), 9)
            except:
                proc.kill()
            proc.wait()
        return (binary_path, -1, True, log_file)

    if ERROR_EVENT.is_set():
        return None

    has_failed = returncode != 0

    with open(log_file, "r", encoding="utf-8", errors="replace") as f:
        output = f.read()

    if "Segmentation fault" in output or "core dumped" in output or "Aborted" in output:
        has_failed = True

    if returncode >= 128:
        sig_num = returncode - 128
        sig_names = {1: "SIGHUP", 2: "SIGINT", 3: "SIGQUIT", 6: "SIGABRT", 9: "SIGKILL", 11: "SIGSEGV", 15: "SIGTERM"}
        sig_name = sig_names.get(sig_num, f"signal {sig_num}")
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"\n--- Process killed by {sig_name} (exit code {returncode}) ---\n")

    if "[  FAILED  ]" in output or "FAILED TESTS" in output:
        has_failed = True

    if has_failed:
        ERROR_EVENT.set()
        return (binary_path, returncode, True, log_file)

    os.remove(log_file)
    return (binary_path, returncode, False, None)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="运行 UT 测试用例")
    parser.add_argument(
        "--test-dir", default=None, help="指定测试目录（默认: 脚本所在目录的 build/test）"
    )
    parser.add_argument(
        "--list", action="store_true", help="仅列出找到的可执行文件，不执行"
    )
    parser.add_argument(
        "--workers", type=int, default=MAX_WORKERS, help="最大并行线程数（默认 1）"
    )
    parser.add_argument(
        "--timeout", type=int, default=300, help="单个用例超时秒数（默认 300）"
    )
    parser.add_argument(
        "--source", type=str, default=None, help="执行前 source 的脚本路径"
    )
    args = parser.parse_args()

    global SOURCE_CMD
    if args.source:
        SOURCE_CMD = f'source "{args.source}" && '
    else:
        SOURCE_CMD = ""

    if args.test_dir:
        test_dir = Path(args.test_dir)
    else:
        script_dir = Path(__file__).resolve().parent
        test_dir = script_dir / "build" / "test"

    script_dir = Path(__file__).resolve().parent
    os.makedirs(script_dir / LOG_DIR, exist_ok=True)

    if not test_dir.exists():
        msg = f"[ERROR] 目录不存在: {test_dir}"
        print(msg)
        log_file = str(script_dir / LOG_DIR / "run_ut_error.log")
        with open(log_file, "w", encoding="utf-8") as f:
            f.write(msg + "\n")
        print(f"[INFO] 错误日志已保存: {log_file}")
        sys.exit(1)

    print(f"[INFO] 扫描目录: {test_dir}")
    binaries = find_ut_binaries(test_dir)

    if not binaries:
        msg = "[INFO] 未找到 UT 可执行文件"
        print(msg)
        log_file = str(script_dir / LOG_DIR / "run_ut_error.log")
        with open(log_file, "w", encoding="utf-8") as f:
            f.write(msg + "\n")
        print(f"[INFO] 错误日志已保存: {log_file}")
        sys.exit(0)

    print(f"[INFO] 找到 {len(binaries)} 个 UT 可执行文件")
    for b in binaries:
        print(f"  - {b.relative_to(test_dir)}")

    if args.list:
        sys.exit(0)

    max_workers = min(args.workers, len(binaries))
    print(f"[INFO] 使用 {max_workers} 个线程并行执行（超时 {args.timeout}s）\n")

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = [executor.submit(run_single_ut, b, args.timeout) for b in binaries]
        
        failed = False
        failed_log = None
        for future in futures:
            try:
                result = future.result(timeout=args.timeout + 10)
            except Exception:
                print(f"[ERROR] {future}")
                continue

            if result is None:
                continue

            binary_path, returncode, has_failed, log_file = result
            
            if binary_path is None:
                if log_file and os.path.exists(log_file):
                    os.remove(log_file)
                continue

            name = binary_path.relative_to(test_dir)

            if returncode != 0 or has_failed:
                print(f"[FAIL] {name}  exit={returncode}")
                print(f"       日志已保存: {log_file}")
                failed = True
                failed_log = log_file
            else:
                if log_file and os.path.exists(log_file):
                    os.remove(log_file)
                print(f"[ OK ] {name}")

            if failed:
                break

        if failed:
            print(f"\n[ABORT] 用例失败，终止所有任务")
            for log in glob.glob(str(Path(__file__).resolve().parent / LOG_DIR / "*.log")):
                if failed_log and os.path.abspath(log) == os.path.abspath(failed_log):
                    continue
                os.remove(log)
            time.sleep(0.2)
            for p in ALL_PROCS:
                try:
                    os.killpg(os.getpgid(p.pid), 9)
                except:
                    try:
                        p.kill()
                    except:
                        pass
            try:
                os.killpg(0, 9)
            except:
                pass
            os._exit(1)

    print(f"\n[DONE] 全部 {len(binaries)} 个 UT 执行成功")


if __name__ == "__main__":
    main()
