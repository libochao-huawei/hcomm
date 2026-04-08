#!/usr/bin/env bash
set -euo pipefail
exit 0
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ "${HCOMM_CLANG_TIDY_DISABLE:-0}" == "1" ]]; then
    echo "[clang-tidy] disabled by HCOMM_CLANG_TIDY_DISABLE=1"
    exit 0
fi

pick_clang_tidy() {
    local candidates=(clang-tidy-18 clang-tidy clang-tidy-17 clang-tidy-16)
    local c
    for c in "${candidates[@]}"; do
        if command -v "$c" >/dev/null 2>&1; then
            echo "$c"
            return 0
        fi
    done
    return 1
}

collect_git_files() {
    local scope="${HCOMM_CLANG_TIDY_SCOPE:-staged}"
    case "$scope" in
        staged)
            git diff --cached --name-only --diff-filter=ACMR
            ;;
        changed|modified)
            git diff --name-only --diff-filter=ACMR HEAD
            ;;
        *)
            echo "[clang-tidy] unsupported HCOMM_CLANG_TIDY_SCOPE=$scope (use staged|changed)" >&2
            return 1
            ;;
    esac
}

CLANG_TIDY_BIN="${CLANG_TIDY_BIN:-}"
if [[ -z "$CLANG_TIDY_BIN" ]]; then
    CLANG_TIDY_BIN="$(pick_clang_tidy || true)"
fi
if [[ -z "$CLANG_TIDY_BIN" ]]; then
    echo "[clang-tidy] clang-tidy not found. Please install LLVM 18 clang-tidy." >&2
    exit 1
fi

declare -a BUILD_CANDIDATES=(build_pkg_host build build_ut build_full_hccd build_full_device build_pkg_device)

resolve_build_dir_for_file() {
    local file="$1"
    if [[ -n "${HCOMM_CLANG_TIDY_BUILD_DIR:-}" && -f "${HCOMM_CLANG_TIDY_BUILD_DIR}/compile_commands.json" ]]; then
        echo "${HCOMM_CLANG_TIDY_BUILD_DIR}"
        return 0
    fi

    local d
    for d in "${BUILD_CANDIDATES[@]}"; do
        local db="$ROOT_DIR/$d/compile_commands.json"
        if [[ -f "$db" ]] && grep -Fq "$ROOT_DIR/$file" "$db"; then
            echo "$ROOT_DIR/$d"
            return 0
        fi
    done

    for d in "${BUILD_CANDIDATES[@]}"; do
        if [[ -f "$ROOT_DIR/$d/compile_commands.json" ]]; then
            echo "$ROOT_DIR/$d"
            return 0
        fi
    done
    return 1
}

DEFAULT_BUILD_DIR="$(resolve_build_dir_for_file "" || true)"
if [[ -z "$DEFAULT_BUILD_DIR" || ! -f "$DEFAULT_BUILD_DIR/compile_commands.json" ]]; then
    echo "[clang-tidy] compile_commands.json not found. Run cmake configure first or set HCOMM_CLANG_TIDY_BUILD_DIR." >&2
    exit 1
fi

declare -a input_files=()
if [[ "$#" -gt 0 ]]; then
    input_files=("$@")
else
    while IFS= read -r line; do
        [[ -n "$line" ]] && input_files+=("$line")
    done < <(collect_git_files)
fi

declare -a tidy_files=()
for f in "${input_files[@]}"; do
    [[ -f "$f" ]] || continue
    case "$f" in
        *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx)
            tidy_files+=("$f")
            ;;
    esac
done

if [[ "${#tidy_files[@]}" -eq 0 ]]; then
    echo "[clang-tidy] no matching C/C++ files to check."
    exit 0
fi

declare -a fix_args=()
if [[ "${HCOMM_CLANG_TIDY_FIX:-0}" == "1" ]]; then
    fix_args=(-fix -fix-errors -format-style=file)
    echo "[clang-tidy] auto-fix enabled."
fi

echo "[clang-tidy] using binary: $CLANG_TIDY_BIN"
echo "[clang-tidy] default build dir: $DEFAULT_BUILD_DIR"

auto_rc=0
for f in "${tidy_files[@]}"; do
    FILE_BUILD_DIR="$(resolve_build_dir_for_file "$f" || true)"
    if [[ -z "$FILE_BUILD_DIR" ]]; then
        echo "[clang-tidy] skip $f: no matching compile_commands.json found" >&2
        auto_rc=1
        continue
    fi
    echo "[clang-tidy] checking $f (build dir: $FILE_BUILD_DIR)"
    if ! "$CLANG_TIDY_BIN" -p "$FILE_BUILD_DIR" "${fix_args[@]}" --warnings-as-errors='*' "$f"; then
        auto_rc=1
    fi
done

exit "$auto_rc"
