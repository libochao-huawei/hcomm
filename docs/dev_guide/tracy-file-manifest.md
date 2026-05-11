# Tracy 文件清单与同步指引

本文档记录 HCOMM 集成的 Tracy 源文件集合，以及各文件对应的上游 commit、
功能开关与同步规范。

**上游仓库**: `../tracy`  
**关键 commit**: `ec6fc008415dd0fedc339835a5e4a911bdd330f6`  
（TracyLite: Offline export, CMake overhaul, Perfetto support）

---

## 1. 文件清单

### 1.1 基础 Tracy 客户端（始终需要，`TRACY_ENABLE=ON`）

| 目录（相对 `tools/tracy_src/public/`）| 文件 | 说明 |
|---|---|---|
| `.` | `TracyClient.cpp` | Unity build 入口，`#include` 下列所有 `.cpp` |
| `common/` | `TracySystem.cpp` | 线程/系统原语 |
| `common/` | `tracy_lz4.cpp` | LZ4 压缩 |
| `common/` | `TracySocket.cpp` | 网络发送 |
| `common/` | `TracyStackFrames.cpp` | 符号解析辅助 |
| `client/` | `TracyProfiler.cpp` | 采集主循环 |
| `client/` | `TracyCallstack.cpp` | 调用栈展开 |
| `client/` | `TracySysPower.cpp` | CPU 功耗采样 |
| `client/` | `TracySysTime.cpp` | 高精度时钟 |
| `client/` | `TracySysTrace.cpp` | Linux perf/kernel trace |
| `client/` | `tracy_rpmalloc.cpp` | 无锁内存分配 |
| `client/` | `TracyDxt1.cpp` | 截图压缩（可选） |
| `client/` | `TracyAlloc.cpp` | 内存钩子 |
| `client/` | `TracyOverride.cpp` | new/delete override |
| `client/` | `TracyKCore.cpp` | KCore 调度适配 |

> 对应头文件（`.hpp`/`.h`/`.hpp`）均在同目录，随 `.cpp` 一并维护，不单独列举。

---

### 1.2 离线导出扩展（`TRACY_SAVE_NO_SEND=ON`）

> 来自 commit `ec6fc008`，零 overhead when disabled。

| 目录 | 文件 | 说明 |
|---|---|---|
| `client/` | `TracyLiteAll.cpp` | 离线环形缓冲区 + Chrome JSON 导出 |
| `client/` | `TracyLiteAll.hpp` | 对应公开 API |
| `client/` | `TracyLitePerfetto.hpp` | Perfetto 导出接口头；未定义 `TRACYLITE_PERFETTO` 时整个头文件为空（不是空桩，不可无条件 include 调用） |
| `tracy/` | `TracyHcomm.hpp` | HCOMM 用户侧封装宏（`ChromeZoneScoped` / `ZoneNamedLite*` / `ChromeSetOutputCallback` 等） |

`TracyLiteAll.cpp` 由 `TracyClient.cpp` 在 `#ifdef TRACY_SAVE_NO_SEND` 块内
`#include`，不作为独立编译单元。

---

### 1.3 Perfetto 原生导出（`TRACYLITE_PERFETTO=ON`，需同时开启 `TRACY_SAVE_NO_SEND`）

> 使用 protozero-only 快路径，不启动 Perfetto runtime/IPC。

| 目录（相对 `tools/tracy_src/`）| 文件 | 说明 |
|---|---|---|
| `public/client/` | `TracyLitePerfetto.cpp` | 直接序列化为 `.perfetto-trace`；作为独立 TU 编译，不被 `TracyLiteAll.cpp` include |
| `public/client/` | `TracyLitePerfetto.hpp` | 对应接口；未启用时整个头文件为空 |
| `perfetto_sdk/` | `perfetto.h` | Perfetto 离线合并头（175K 行，勿手动修改） |
| `perfetto_sdk/` | `perfetto.cc` | Perfetto 离线合并实现（67K 行，作为独立 TU 编译） |
| `perfetto_sdk/` | `perfetto_protozero.cc` | protozero 序列化实现（11K 行） |
| `perfetto_sdk/` | `VERSION` | SDK 版本标记 |

`TracyLitePerfetto.cpp` 与 `perfetto.cc` 由 `cmake/third_party/tracy.cmake` 在
`TRACYLITE_PERFETTO=ON` 时追加为独立编译单元（`TRACY_COMPILE_SOURCES`）。

---

## 2. CMake 开关

| CMake 变量 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `ENABLE_TRACY` | BOOL | `ON`（base preset） | 总开关 |
| `TRACY_SAVE_NO_SEND` | BOOL | `ON`（base preset） | 离线导出，禁用网络发送 |
| `TRACYLITE_PERFETTO` | BOOL | `ON`（base preset） | 额外 Perfetto 格式输出 |

host 侧与 device 侧目标库均会注入 Tracy client，按侧别选择不同载体（host 侧通常为 so，device 侧通常为 .a 或 device 专用 so）；具体注入边界由对应 preset 与 `cmake/third_party/tracy.cmake` 控制。

---

## 3. 构建方式

Tracy 通过 `cmake/third_party/tracy.cmake` 以 **STATIC library** 方式集成：

```
tracy_client (STATIC)
  ├── TracyClient.cpp            # 基础采集
  │   └── [TRACY_SAVE_NO_SEND] TracyLiteAll.cpp     # 离线导出 (include)
  ├── [TRACYLITE_PERFETTO] TracyLitePerfetto.cpp    # 独立 TU
  └── [TRACYLITE_PERFETTO] perfetto.cc              # 独立 TU
```

`ccl_kernel_plf` / `ccl_kernel_plf_a` → `target_link_libraries(... tracy_client)`，
通过 PUBLIC 属性自动继承头文件路径与宏定义。

---

## 4. 上游同步规程

每次更新 tracy 上游时，执行以下步骤：

1. 记录新 commit hash。
2. 对比本文档的 **§1 文件清单**，`diff` 上游同名文件：
   ```bash
   TRACY=../tracy
   for f in \
       public/TracyClient.cpp \
       public/client/TracyLiteAll.cpp \
       public/client/TracyLiteAll.hpp \
       public/client/TracyLitePerfetto.cpp \
       public/client/TracyLitePerfetto.hpp \
       public/tracy/TracyHcomm.hpp; do
     diff "$TRACY/$f" "tools/tracy_src/$f"
   done
   diff $TRACY/scripts/perfetto-sdk-offline/perfetto.h    tools/tracy_src/perfetto_sdk/perfetto.h
   diff $TRACY/scripts/perfetto-sdk-offline/perfetto.cc   tools/tracy_src/perfetto_sdk/perfetto.cc
   diff $TRACY/scripts/perfetto-sdk-offline/perfetto_protozero.cc tools/tracy_src/perfetto_sdk/perfetto_protozero.cc
   ```
3. 若有变更，覆盖对应文件，更新本文档的 **commit hash**。
4. 跑 `cmake --preset pkg-device && cmake --build --preset build-pkg-device --parallel 8` 验证编译。
5. 若引入新的 `.cpp` 文件，在 **§1** 追加对应行，并检查 `cmake/third_party/tracy.cmake` 是否需要相应配置。

> **不要修改** `perfetto_sdk/` 内文件，仅允许整体替换（来自上游离线包）。
