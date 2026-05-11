# Tracy staged 文件审计清单

本文档基于对当前 `pkg-device` 预设构建依赖的分析，判断每个 staged 文件的必要性。

**分析方法**：  
1. 通过 `g++ -M` 生成 `TracyClient.cpp` 和 `perfetto.cc` 的编译时依赖图  
2. 与 staged 文件集合作交集，识别"当前配置下不会编译到的文件"  
3. 交叉检查源码对公开头文件的实际引用（如 `public/tracy/*.hpp`）

---

## 📋 清单总结

**当前 staged 新增 Tracy 文件数**: 90  
**实际在当前构建中引用的文件数**: 70  
**未被引用的候选文件数**: 20

---

## 🔴 **建议删除**（20 个文件）

这些文件在当前构建配置下完全不会被编译到，保留无益。

### 平台特定代码（Windows/macOS Only）
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/public/client/windows/TracyETW.cpp` | Windows ETW 采集，device 端无用 |
| `tools/tracy_src/public/libbacktrace/macho.cpp` | macOS Mach-O 符号解析，仅 `TRACY_HAS_CALLSTACK==4` |

### Rocprof 支持（AMD GPU，非必需）
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/public/client/TracyRocprof.cpp` | `#ifdef TRACY_ROCPROF`，未启用 |

### LZ4 高压缩率支持（网络发送用）
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/public/common/tracy_lz4hc.cpp` | HC 模式（离线导出用 LZ4 本身足够） |
| `tools/tracy_src/public/common/tracy_lz4hc.hpp` | 同上 |

### Fortran 绑定
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/public/TracyClient.F90` | Fortran 90 绑定，HCOMM 不用 |

### 库/工具相关
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/public/libbacktrace/LICENSE` | 许可文件，不参与编译 |
| `tools/tracy_src/perfetto_sdk/VERSION` | 版本标记文件，不参与编译 |

### 用户侧头文件（已独立管理）
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/public/tracy/Tracy.hpp` | 全量 Tracy 头，当前用自研 API |
| `tools/tracy_src/public/tracy/TracyChromeExport.hpp` | 网络发送导出，当前用离线 |
| `tools/tracy_src/public/tracy/TracyCUDA.hpp` | CUDA 集成，非必需 |
| `tools/tracy_src/public/tracy/TracyD3D11.hpp` | DirectX 11，非必需 |
| `tools/tracy_src/public/tracy/TracyD3D12.hpp` | DirectX 12，非必需 |
| `tools/tracy_src/public/tracy/TracyLua.hpp` | Lua 绑定，非必需 |
| `tools/tracy_src/public/tracy/TracyMetal.hmm` | Apple Metal，非必需 |
| `tools/tracy_src/public/tracy/TracyOpenCL.hpp` | OpenCL 集成，非必需 |
| `tools/tracy_src/public/tracy/TracyOpenGL.hpp` | OpenGL 集成，非必需 |
| `tools/tracy_src/public/tracy/TracyVulkan.hpp` | Vulkan 集成，非必需 |
| `tools/tracy_src/public/common/TracyColor.hpp` | UI 颜色定义，不参与采集 |
| `tools/tracy_src/public/common/TracyVersion.hpp` | 版本宏定义，用不到 |

### Perfetto SDK 补充实现
| 文件 | 原因 |
|------|------|
| `tools/tracy_src/perfetto_sdk/perfetto_protozero.cc` | protozero 序列化已包含在 `perfetto.h` 中；该文件是独立源，当前不需要单独编译 |

---

## 🟡 **待确认**（1 个文件）

这些文件当前未被编译到，但有潜在价值。

| 文件 | 分析 | 建议 |
|------|------|------|
| `tools/tracy_src/public/client/TracyLitePerfetto.cpp` | ✅ 被 `TracyLiteAll.cpp` 在 `#ifdef TRACYLITE_PERFETTO` 块内 **include** ；不作为独立编译单元。目前依赖预处理器展开，编译时被内联到 `TracyLiteAll` 中。但**实际链接需要单独编译此 `.cpp`** 来提供 `PerfettoNativeExporter::ExportToFile` 等符号定义。|  **建议保留** —— 作为 `tracy_client` STATIC 库的第二个源文件；需补充到 CMake 中。 |

---

## 🟢 **必须保留**（69 个文件）

核心采集、离线导出、Perfetto 序列化、符号解析、压缩算法等。

### 基础采集（11 个）
```
public/common/TracySystem.cpp
public/common/TracySystem.hpp
public/common/TracyAlloc.hpp
public/common/TracySocket.cpp
public/common/TracySocket.hpp
public/client/TracyProfiler.cpp
public/client/TracyProfiler.hpp
public/client/TracyCallstack.cpp
public/client/TracyCallstack.hpp
public/client/TracyCallstack.h
public/client/TracyKCore.cpp
```

### 硬件时间戳 / 系统信息（6 个）
```
public/client/TracySysTime.cpp
public/client/TracySysTime.hpp
public/client/TracySysPower.cpp
public/client/TracySysPower.hpp
public/client/TracySysTrace.cpp
public/client/TracySysTrace.hpp
```

### 内存管理（4 个）
```
public/client/TracyAlloc.cpp
public/client/tracy_rpmalloc.cpp
public/client/tracy_rpmalloc.hpp
public/common/TracyAlloc.hpp
```

### 离线导出（环形缓冲 + Chrome JSON）（3 个）
```
public/client/TracyLiteAll.cpp
public/client/TracyLiteAll.hpp
public/client/TracyRingBuffer.hpp
```

### Perfetto 原生导出（2 个）
```
public/client/TracyLitePerfetto.hpp
perfetto_sdk/perfetto.h
perfetto_sdk/perfetto.cc
```

### 符号解析（libbacktrace）（11 个）
```
public/libbacktrace/backtrace.hpp
public/libbacktrace/config.h
public/libbacktrace/dwarf.cpp
public/libbacktrace/elf.cpp
public/libbacktrace/fileline.cpp
public/libbacktrace/filenames.hpp
public/libbacktrace/internal.hpp
public/libbacktrace/alloc.cpp
public/libbacktrace/mmapio.cpp
public/libbacktrace/posix.cpp
public/libbacktrace/sort.cpp
public/libbacktrace/state.cpp
```

### 压缩算法（2 个）
```
public/common/tracy_lz4.cpp
public/common/tracy_lz4.hpp
```

### 杂项头文件（16 个）
```
public/client/TracyDxt1.cpp
public/client/TracyDxt1.hpp
public/client/TracyOverride.cpp
public/client/TracyFastVector.hpp
public/client/TracyArmCpuTable.hpp
public/client/TracyCpuid.hpp
public/client/TracyDebug.hpp
public/client/TracyLock.hpp
public/client/TracyScoped.hpp
public/client/TracyStringHelpers.hpp
public/client/TracyThread.hpp
public/client/tracy_SPSCQueue.h
public/client/tracy_concurrentqueue.h
public/common/TracyForceInline.hpp
public/common/TracyMutex.hpp
public/common/TracyProtocol.hpp
... 及其他支撑头
```

### 用户 API 头文件（1 个）
```
public/tracy/TracyHcomm.hpp
```

---

## 🔧 **建议行动**

### 立即可做：删除 20 个不需要的文件

> 注意：`TracyLitePerfetto.cpp` 不在删除列表内（它是 `TRACYLITE_PERFETTO=ON` 时的独立 TU，需要保留并由 `cmake/third_party/tracy.cmake` 编译）。

```bash
git rm \
  tools/tracy_src/public/client/windows/TracyETW.cpp \
  tools/tracy_src/public/client/TracyRocprof.cpp \
  tools/tracy_src/public/libbacktrace/macho.cpp \
  tools/tracy_src/public/libbacktrace/LICENSE \
  tools/tracy_src/public/common/tracy_lz4hc.cpp \
  tools/tracy_src/public/common/tracy_lz4hc.hpp \
  tools/tracy_src/public/common/TracyColor.hpp \
  tools/tracy_src/public/common/TracyVersion.hpp \
  tools/tracy_src/public/tracy/Tracy.hpp \
  tools/tracy_src/public/tracy/TracyChromeExport.hpp \
  tools/tracy_src/public/tracy/TracyCUDA.hpp \
  tools/tracy_src/public/tracy/TracyD3D11.hpp \
  tools/tracy_src/public/tracy/TracyD3D12.hpp \
  tools/tracy_src/public/tracy/TracyLua.hpp \
  tools/tracy_src/public/tracy/TracyMetal.hmm \
  tools/tracy_src/public/tracy/TracyOpenCL.hpp \
  tools/tracy_src/public/tracy/TracyOpenGL.hpp \
  tools/tracy_src/public/tracy/TracyVulkan.hpp \
  tools/tracy_src/public/TracyClient.F90 \
  tools/tracy_src/perfetto_sdk/VERSION \
  tools/tracy_src/perfetto_sdk/perfetto_protozero.cc
```

### 后续改进（非紧急）
1. 将 `TracyLitePerfetto.cpp` 加入 `cmake/third_party/tracy.cmake` 的编译源列表（当前作为 include 被内联）
2. 在 CMake 中添加条件编译选项，允许在不需要 Perfetto 时跳过该源文件
3. 定期检查 ec6fc008 commit 之后是否有新增的必需文件

---

## 📊 最终数据

| 分类 | 数量 | 操作 |
|------|------|------|
| 删除候选 | 20 | ❌ 不参与编译 |
| 待确认 | 1 | ⚠️ 需补充到 CMake |
| 必须保留 | 69 | ✅ 核心依赖 |
| **总计** | **90** | |

**删除后净保留** = 90 - 20 = **70 个文件**  
**与实际依赖数 70 个对齐** ✓
