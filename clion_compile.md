# 一键模式（推荐，避免 CLion 同时加载多个 profile 触发三方库并发下载）
```bash
bash scripts/clion_build_mode.sh --mode pkg -j1
bash scripts/clion_build_mode.sh --mode full -j1
bash scripts/clion_build_mode.sh --mode ut -j1
bash scripts/clion_build_mode.sh --mode st -j1
```

# 仅预览执行命令（不实际执行）
```bash
bash scripts/clion_build_mode.sh --mode full --dry-run
```

# 分步模式（保留）
# --pkg 对应
cmake --preset pkg-device-kernel
cmake --build --preset build-pkg-device-kernel
cmake --preset pkg-host
cmake --build --preset build-package-host

# --full 对应
cmake --preset full-device
cmake --build --preset build-full-device
cmake --preset full-hccd
cmake --build --preset build-full-hccd
cmake --preset pkg-host
cmake --build --preset build-package-host

# --ut 对应
cmake --preset ut
cmake --build --preset build-ut

# --st 对应
cmake --preset st
cmake --build --preset build-st-all
