#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODE=""
JOBS=""
FRESH=0
DRY_RUN=0

usage() {
  cat <<'EOF'
Usage:
  bash scripts/clion_build_mode.sh --mode <pkg|full|ut|st> [-jN] [--fresh] [--dry-run]

Examples:
  bash scripts/clion_build_mode.sh --mode ut -j1
  bash scripts/clion_build_mode.sh --mode pkg --fresh
  bash scripts/clion_build_mode.sh --mode full -j2

Options:
  --mode      Build mode. One of: pkg, full, ut, st
  -jN         Parallel jobs passed to cmake --build (optional)
  --fresh     Reconfigure presets with --fresh before build
  --dry-run   Print commands only; do not execute
  -h|--help   Show this help
EOF
}

run_cmd() {
  if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "$*"
  else
    eval "$@"
  fi
}

build_with_preset() {
  local preset="$1"
  local cfg=""
  if [[ "$FRESH" -eq 1 ]]; then
    cfg="--fresh"
  fi
  run_cmd "cmake -S '$ROOT_DIR' --preset '$preset' $cfg"
}

run_build_preset() {
  local build_preset="$1"
  local build_extra=""
  if [[ -n "$JOBS" ]]; then
    build_extra="-- $JOBS"
  fi
  run_cmd "cmake --build '$ROOT_DIR' --preset '$build_preset' $build_extra"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)
      MODE="${2:-}"
      shift 2
      ;;
    -j*)
      JOBS="$1"
      shift
      ;;
    --fresh)
      FRESH=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$MODE" ]]; then
  echo "--mode is required" >&2
  usage
  exit 1
fi

case "$MODE" in
  ut)
    build_with_preset "ut"
    run_build_preset "build-ut"
    ;;
  st)
    build_with_preset "st"
    run_build_preset "build-st-all"
    ;;
  pkg)
    build_with_preset "pkg-device-kernel"
    run_build_preset "build-pkg-device-kernel"
    build_with_preset "pkg-host"
    run_build_preset "build-package-host"
    ;;
  full)
    build_with_preset "full-device"
    run_build_preset "build-full-device"
    build_with_preset "full-hccd"
    run_build_preset "build-full-hccd"
    build_with_preset "pkg-host"
    run_build_preset "build-package-host"
    ;;
  *)
    echo "Invalid mode: $MODE" >&2
    usage
    exit 1
    ;;
esac

