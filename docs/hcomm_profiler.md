# HCOMM Profiler 集成指南

## 概述

HCOMM Profiler 是一套针对 HCCL/CANN 场景设计的**零网络依赖**、**零安全风险**的性能打点方案。

**核心思路**：在 AICPU/Host 侧通过硬件寄存器获取高精度时间戳，借助 HCCL 自研日志库落盘，
离线转换为 Tracy Profiler 可读取的 `.tracy` 格式，最终通过 Tracy GUI 进行可视化分析。

### 架构图

```
┌─────────────────────────────────────────────────────┐
│  NPU / AICPU 侧  (ARM64)                           │
│                                                     │
│  hcomm_profiler.h                                   │
│  ┌───────────────────────┐                          │
│  │ HCOMM_ZONE_SCOPED()   │──→ CNTVCT_EL0 (硬件TSC) │
│  │ HCOMM_ZONE_BEGIN/END  │──→ HCCL_RUN_INFO 落盘   │
│  └───────────────────────┘                          │
│           │                                         │
│           ▼                                         │
│   HCCL Run Log (华为日志系统)                        │
└───────────┬─────────────────────────────────────────┘
            │ (msnpureport / 华为日志导出工具)
            ▼
┌─────────────────────────────────────────────────────┐
│  Host 侧 (x86/ARM64)                               │
│                                                     │
│  hcomm_trace_to_tracy.py                            │
│  ┌───────────────────────┐                          │
│  │ 解析 [HCOMM_PROF] 行  │                          │
│  │ TSC -> 微秒换算        │                          │
│  │ 输出 Chrome JSON       │                          │
│  └───────────┬───────────┘                          │
│              ▼                                      │
│  import-chrome (Tracy 官方工具)                      │
│  ┌───────────────────────┐                          │
│  │ JSON -> .tracy 文件    │                          │
│  └───────────┬───────────┘                          │
│              ▼                                      │
│  Tracy GUI (可视化分析)                              │
└─────────────────────────────────────────────────────┘
```

## 文件清单

| 文件 | 位置 | 说明 |
|------|------|------|
| `hcomm_profiler.h` | `src/pub_inc/` | 打点头文件，包含宏定义和 TSC 获取函数 |
| `tracy.cmake` | `cmake/third_party/` | Tracy import-chrome 工具的 CMake 集成 |
| `hcomm_trace_to_tracy.py` | `scripts/` | 日志转换脚本 (HCCL Log → Chrome JSON) |

## 快速开始

### 1. 在代码中插入打点

```cpp
#define HCOMM_PROFILER_ENABLE  // 或在 CMake 中 add_definitions(-DHCOMM_PROFILER_ENABLE)
#include "hcomm_profiler.h"

HcclResult HcclCommunicator::Init(const std::string &ranktableM)
{
    HCOMM_PROFILER_INIT();           // 程序初始化时调用一次，输出频率校准
    HCOMM_ZONE_SCOPED();             // 自动记录本函数的 Begin/End
    return pimpl->Init(commParams, ranktableM, config);
}

HcclResult HcclCommunicator::LoadOffloadCollOp(std::string &opTag,
    const CollOpParams &opParams, void *stream)
{
    HCOMM_ZONE_SCOPED();             // 测量任务下发核心路径耗时
    std::lock_guard<std::mutex> lock(serialMutex);
    auto ret = pimpl->LoadOffloadCollOp(opTag, opParams, stream);
    return ret;
}
```

### 2. 导出日志

使用华为工具将 NPU 侧日志拉回 Host（具体命令依据 CANN 版本）：
```bash
# 示例（具体命令请参考 CANN 文档）
msnpureport -e /var/log/npu/slog/
```

### 3. 转换为 Chrome JSON

```bash
python3 scripts/hcomm_trace_to_tracy.py \
    -i /path/to/hccl_run.log \
    -o hcomm_trace.json
```

### 4. 可视化分析（三种方式任选）

#### 方式 A: Chrome 浏览器直接查看
打开 `chrome://tracing`，拖入 `hcomm_trace.json`。

#### 方式 B: Tracy GUI 查看（推荐）
```bash
# 编译 import-chrome（CMake 会自动处理）
cmake --build build --target third_party_tracy

# 转换
./build/third_party/tracy_install/bin/import-chrome hcomm_trace.json hcomm_result.tracy

# 用 Tracy GUI 打开 hcomm_result.tracy
```

#### 方式 C: 直接用 Tracy 官方 Release 的 GUI
从 https://github.com/wolfpld/tracy/releases 下载 Tracy GUI，打开 `.tracy` 文件。

## CMake 集成 Tracy

在 `CMakeLists.txt` 中按需添加（仅构建 import-chrome 转换工具）：

```cmake
# 在 ENABLE_TEST 或 BUILD_OPEN_PROJECT 分支中
include(cmake/third_party/tracy.cmake)
```

Tracy 通过 GitCode 镜像下载：`https://gitcode.com/gh_mirrors/tr/tracy`，国内网速无忧。

## 打点 API 参考

| 宏 | 参数 | 说明 |
|----|------|------|
| `HCOMM_PROFILER_INIT()` | 无 | 输出频率校准信息，**程序启动时调用一次** |
| `HCOMM_ZONE_SCOPED()` | 无 | RAII 自动打点，区域名 = `__FUNCTION__` |
| `HCOMM_ZONE_SCOPED_N(name)` | `const char*` | RAII 自动打点，自定义区域名 |
| `HCOMM_ZONE_BEGIN(name)` | `const char*` | 手动标记 Begin |
| `HCOMM_ZONE_END(name)` | `const char*` | 手动标记 End（需与 Begin 配对） |
| `HCOMM_ZONE_INSTANT(name)` | `const char*` | 标记瞬时事件 |

## 性能开销

| 平台 | 单次打点开销 | 方式 |
|------|-------------|------|
| ARM64 (AICPU) | ~10-20 ns | `mrs cntvct_el0` + HCCL_RUN_INFO 格式化 |
| x86_64 (Host) | ~8-15 ns | `rdtsc` + HCCL_RUN_INFO 格式化 |
| 未使能 (`HCOMM_PROFILER_ENABLE` 未定义) | **0 ns** | 宏展开为空 |

## 编译开关

- `HCOMM_PROFILER_ENABLE`: 定义此宏以使能打点。**未定义时所有打点代码为零开销空操作。**

```cmake
# 在开发调试时使能
target_compile_definitions(your_target PRIVATE HCOMM_PROFILER_ENABLE)
```

## 注意事项

1. **安全性**: 不涉及任何网络 Socket，所有数据走华为官方日志通道
2. **Begin/End 配对**: 手动使用 `HCOMM_ZONE_BEGIN/END` 时务必保证配对，否则 Tracy 时间轴会错乱
3. **频率校准**: `HCOMM_PROFILER_INIT()` 必须在打点开始前调用，否则离线转换无法正确计算时间
4. **x86 TSC 频率**: 在 x86 平台上，可通过 `--x86-freq` 参数手动指定频率（Hz）
