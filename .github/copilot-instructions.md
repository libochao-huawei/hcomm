# HCOMM 项目 Copilot 指南

本文件为 GitHub Copilot 在本仓库工作的**通用上下文与规则**。
针对 C/C++ 源文件的编码细则见 [`.github/instructions/cpp.instructions.md`](./instructions/cpp.instructions.md)（自动按文件类型生效）。
构建、安装与测试步骤以 [`docs/build.md`](../docs/build.md) 和 [`CMakePresets.json`](../CMakePresets.json) 为准；提交前流程见 [`docs/dev_guide/pre-commit-guide.md`](../docs/dev_guide/pre-commit-guide.md)。

---

## 1. 项目背景

- **领域**: 昇腾（Ascend）AI 加速器生态、CANN 计算框架、集合通信（AllReduce / Broadcast / AllGather / ReduceScatter 等）。
- **本仓库**: HCOMM —— 运行在昇腾加速器上的集合通信库。
- **核心模块**: CCU（Computing Cluster Unit）内核系统
  - `pkg_inc/hcomm/ccu/ccu_rep_context_v1.h` — rep 序列基类
  - `src/framework/next/comms/ccu/ccu_kernel/ccu_kernel.{h,cc}` — 内核类
  - `src/framework/next/comms/ccu/ccu_kernel/ccu_kernel_mgr.{h,cc}` — 内核管理器（翻译/加载）
  - `src/framework/next/coll_comms/api_c_adpt/coll_comm_res_c_adpt.cc` — 运行时适配层

## 2. 技术栈

- **构建系统**: CMake 3.16+，多预设（`pkg-device` / `host-pkg` / `full-device` / `full-hccd` / `host-full`）
- **目标平台**: Device (`aarch64-target-linux-gnu`) + Host (`x86_64`)
- **语言**: C++（主体）、C（API 适配）、Python（工具）
- **关键依赖**: OpenSSL、自定义内核编译器、DFX 框架

## 2.1 代码结构与落点

- **优先修改 `src/framework/next/`**：新框架/新接口/CCU 与 op-dev 能力优先落在 next 路径。
- **仅在必要时修改 `src/legacy/`**：仅当问题明确属于旧流程、兼容逻辑或已有 legacy 调用链时进入。
- **通信域与资源管理**：常见入口在 `src/framework/communicator/impl/`。
- **集群维护与故障检测**：定位 `src/framework/cluster_maintenance/`。
- **公开接口/ABI**：优先查看 `pkg_inc/`、`include/` 和 `docs/comm_mgr_api_c/`、`docs/comm_opdev_api/`，不要从实现细节反推接口语义。
- **DFX / 故障排查**：优先链接 `docs/dfx_task_exception_flow.md` 与 `docs/dfx_profiling_flow.md`，不要在说明文档里重复整段流程。
- **DFX triage 专项规则**：涉及 task exception、profiling、connection anomaly 时，优先遵循 `.github/instructions/dfx-triage.instructions.md`。

## 3. 总体行为准则

- **代码风格**: 严格遵循《华为 C/C++ 语言编程规范》与《安全编程规范》。
- **格式化要求**: C/C++ 代码以仓库根目录 `.clang-format` 为唯一格式真源；新增或修改 `*.{h,hpp,c,cc,cpp,cxx,inc}` 后，提交前对变更文件执行 `clang-format -i`，避免 pre-commit 因格式失败。
- **生成代码前**: 先读相邻文件了解既有命名/缩进/注释风格，与之保持一致。
- **修改范围**: 只改用户要求的内容；不顺手重构、不批量加注释、不"美化"无关代码。
- **错误处理**: 系统边界处必须检查返回值并记录日志；内部 helper 不要为不可能的情况做防御。
- **依赖**: 不引入新第三方库，除非用户明确同意。
- **平台差异**: 区分 device/host 代码路径；device 侧避免使用 host-only API。

## 4. 构建与校验

```bash
# 默认包构建（device + host）
cmake --preset pkg-device  && cmake --build --preset build-pkg-device  --parallel 8
cmake --preset host-pkg    && cmake --build --preset build-host-pkg    --parallel 8

# clang-tidy（变更文件）
HCOMM_CLANG_TIDY_DISABLE=1 HCOMM_CLANG_TIDY_SCOPE=changed bash scripts/clang_tidy_precommit.sh
```

- 以 `CMakePresets.json` 为构建配置真源；能用 preset 时不要手写新的 CMake 目录结构。
- 本仓库常同时存在 `build/`、`build_pkg_device/`、`build_pkg_host/` 等多个 `compile_commands.json`；做静态分析或跳转定位时，先确认当前工具使用的是与目标代码一致的 build 目录。
- 若任务只涉及 host 或 device 一侧，优先使用对应 preset 验证，避免无关平台噪音。

提交前自检：
- [ ] 命名/缩进/行长符合规范
- [ ] 公开接口有 Doxygen 注释
- [ ] 边界检查、资源 RAII、返回值检查
- [ ] clang-tidy 无新增警告
- [ ] 至少一个相关 preset 编译通过

## 5. 文档与注释语言

- 中文注释/中文回复均可；公开 API 注释推荐**中文 + Doxygen 标签**。
- 提交信息使用祈使句，可中可英，但保持单个 PR 内一致。
- `docs/**` 下的功能/子功能/小功能设计文档，优先遵循 [`.github/instructions/doc-srs-sd-sc.instructions.md`](./instructions/doc-srs-sd-sc.instructions.md) 的 **SRS / SD / SC** 组织模板。

## 6. 不要做的事

- ❌ 不要为单个改动新建 markdown 文档汇报工作。
- ❌ 不要在未读文件的情况下臆测函数签名。
- ❌ 不要使用 `--no-verify` 绕过钩子；不要 `git push --force` 已发布分支。
- ❌ 不要修改 `openssl-*-src/`、`third_party/`、`externel_depends/` 等第三方源码。
- ❌ 不要把 host-only 头文件包含进 device 翻译单元。

## 7. 可复用专项 instruction

- 代码检视 / PR review：优先遵循 [`.github/instructions/reviewing-gitcode.instructions.md`](./instructions/reviewing-gitcode.instructions.md)
- 单元测试生成与调试：优先遵循 [`.github/instructions/ut-generation.instructions.md`](./instructions/ut-generation.instructions.md)
