# RFC: 外部网卡插件机制

- 起始日期：2026-05-22
- RFC PR编号：（待PR提交后填写）
- 相关Issue：（待创建Requirement Issue后填写）

---

## 概要

为 HCOMM 增加外部网卡（NIC）插件机制，允许开发者以独立 `.so` 文件形式扩展或替换通信协议实现，无需修改 HCOMM 原始代码。

---

## 背景与动机

### 背景

HCOMM 当前支持的通信协议类型（HCCS、RoCE、PCIe、SIO、UB 系列等）均以内置方式实现。每增加一种新网卡协议，需要增加协议枚举和修改内部实现

这种模式导致：
- 外部开发者无法在不修改 HCOMM 源码的情况下贡献新网卡支持
- 内置代码与第三方代码耦合，不利于社区协作
- 实验新协议的门槛高，需要深度理解 HCOMM 内部架构

### 目标场景

1. **新硬件适配**：开发者拥有非标准 RDMA 网卡（如第三方 RNIC），希望在 HCCL/HCOMM 生态中集成
2. **协议替换**：针对特定场景，用自定义实现替换现有内置协议（如自定义 RoCE 优化版本）
3. **性能实验**：在 `experimental/` 目录中快速验证新通信后端，无需经过完整 RFC 流程
4. **学术研究**：研究人员可独立开发并测试新型通信协议

### 动机

为 HCOMM 建立类似的低门槛、高解耦的插件扩展能力（当前仅支持host网卡扩展）。

---

## 详细设计

### 1. 总体架构

```
┌──────────────────────────────────────────────────────────────┐
│  libhcomm.so                                                  │
│                                                                │
│  控制面 (hcomm_res.h) — 函数入口即分发                          │
│  ┌─ HcommEndpointCreate(&desc, &handle)                      │
│  │   entry = NicPluginLoader::Find(desc.protocol)             │
│  │   → 命中: createEndpoint → new PluginEndpointCtx          │
│  │           handle = ctx | HCOMM_PLUGIN_HANDLE_FLAG         │
│  │   → 未命中: 走原有 Endpoint::CreateEndpoint               │
│  │                                                            │
│  ├─ HcommMemReg(handle, tag, mem, &mh)                       │
│  │   if (IS_PLUGIN_HANDLE(handle)) {                         │
│  │       ep = UNFLAG(handle);                                 │
│  │       return ep->ops->registerMemory(ep->ctx, mem, tag,..)│
│  │   }                                                        │
│  │   // 原有逻辑                                              │
│  │                                                            │
│  └─ HcommMemExport / HcommMemImport / ...  同理               │
│                                                                │
│  数据面 (hcomm_primitives.h) — 函数入口即分发                   │
│  ┌─ HcommWriteOnThread(thread, chHandle, dst, src, len)      │
│  │   if (IS_PLUGIN_HANDLE(chHandle)) {                       │
│  │       ch = UNFLAG(chHandle);                               │
│  │       return ch->ops->write(ch->ctx, thread, dst, src, len)│
│  │   }                                                        │
│  │   // 原有逻辑                                              │
│  │                                                            │
│  └─ （其余 25 个数据面 + 控制面函数同理）                       │
│                                                                │
│  NicPluginLoader (constructor 时执行)                          │
│  ├─ 默认路径 ${CANN_HOME_PATH}/hcomm_plugin/                  │
│  └─ 备选 HCOMM_NIC_PLUGIN_SO 环境变量                         │
└──────────────────────────────────────────────────────────────┘
         ▲ dlopen
         │
┌────────┴─────────────────────────────────────────────────────┐
│  libmy_nic.so  (experimental/nic_plugins/my_nic/)            │
│                                                                │
│  extern "C" {                                                  │
│    HcommNicPluginGetInfo()       → 元信息                      │
│    HcommNicPluginCreateEndpoint() → 分配 ctx + 填充 epOps     │
│    HcommNicPluginCreateChannel()  → 分配 ctx + 填充 chOps     │
│  }                                                             │
└──────────────────────────────────────────────────────────────┘
```

**核心设计**：在 `HcommXxx` 公开 C API 的函数入口第一行做分发。Endpoint 和 Channel 的插件实例以带标记的裸指针编码在对应 Handle 中，绕过内部的 `EndpointMgr`/`ChannelMgr` 查找和 `Endpoint`/`Channel` 虚函数调用，实现零抽象开销分发。

### 2. 接口设计

#### 2.1 新增公开头文件 `include/hcomm_nic_plugin.h`

不依赖 HCOMM 内部头文件，确保插件可独立编译。

##### 协议号（独立定义，可覆盖内置协议）

```c
typedef enum {
    HCOMM_NIC_PROTO_HCCS       = 0,   // HCCS
    HCOMM_NIC_PROTO_ROCE       = 1,   // RDMA over Converged Ethernet
    HCOMM_NIC_PROTO_PCIE       = 2,   // PCI Express
    HCOMM_NIC_PROTO_SIO        = 3,   // SIO
    HCOMM_NIC_PROTO_UBC_CTP    = 4,   // Universal Bus CTP
    HCOMM_NIC_PROTO_UBC_TP     = 5,   // Universal Bus TP
    HCOMM_NIC_PROTO_UB_MEM     = 6,   // UB Memory
    HCOMM_NIC_PROTO_UBOE       = 7,   // UB over Ethernet
    HCOMM_NIC_PROTO_HCCS_ONLY  = 8,   // HCCS single-die
    HCOMM_NIC_PROTO_CUSTOM_BASE = 1000, // 自定义协议从 1000 开始
} HcommNicProtocol;
```

##### Endpoint 操作表（控制面 — 对应 `hcomm_res.h` 接口）

```c
typedef struct HcommNicEndpointOps {
    int32_t (*init)(void *ctx);
    int32_t (*registerMemory)(void *ctx, const CommMem *mem, const char *tag, void **handle);
    int32_t (*unregisterMemory)(void *ctx, void *handle);
    int32_t (*memoryExport)(void *ctx, void *handle, void **desc, uint32_t *descLen);
    int32_t (*memoryImport)(void *ctx, const void *desc, uint32_t descLen, CommMem *outMem);
    int32_t (*memoryUnimport)(void *ctx, const void *desc, uint32_t descLen);
    void    (*destroy)(void *ctx);
} HcommNicEndpointOps;
```

##### Channel 操作表（数据面 — 覆盖控制面 + 完整数据面接口）

Channel 操作表包含 **26 个函数指针**，按职责分为以下类别：

**控制面（4 个）**：

初始化通道、查询同步数量、获取远端内存、清理通道状态：
- `int32_t (*init)(void *ctx)` — 通道初始化
- `int32_t (*getNotifyNum)(void *ctx, uint32_t *num)` — 查询同步数量
- `int32_t (*getRemoteMem)(void *ctx, CommMem **mem, uint32_t *num, char ***tags)` — 获取对端注册内存
- `int32_t (*clean)(void *ctx)` — 重置通道状态

**数据面 — OnThread 版本（带 ThreadHandle 参数，13 个）**：

对应 `HcommXxxOnThread` 系列公开 API：
- `int32_t (*write)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len)`
- `int32_t (*writeReduce)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t count, int32_t dataType, int32_t reduceOp)`
- `int32_t (*writeWithNotify)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx)`
- `int32_t (*writeReduceWithNotify)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t count, int32_t dataType, int32_t reduceOp, uint32_t remoteNotifyIdx)`
- `int32_t (*read)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len)`
- `int32_t (*readReduce)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t count, int32_t dataType, int32_t reduceOp)`
- `int32_t (*writeNbi)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len)`
- `int32_t (*writeWithNotifyNbi)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx)`
- `int32_t (*readNbi)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len)`
- `int32_t (*batchTransfer)(void *ctx, ThreadHandle thread, const void *descs, uint32_t num)`
- `int32_t (*notifyRecord)(void *ctx, ThreadHandle thread, uint32_t remoteNotifyIdx)`
- `int32_t (*notifyWait)(void *ctx, ThreadHandle thread, uint32_t localNotifyIdx, uint32_t timeout)`
- `int32_t (*fence)(void *ctx, ThreadHandle thread)`

**数据面 — NoThread 版本（不带 ThreadHandle 参数，6 个）**：

对应 `HcommXxx` 系列（无 OnThread 后缀）公开 API：
- `int32_t (*writeNbiNt)(void *ctx, void *dst, const void *src, uint64_t len)`
- `int32_t (*writeWithNotifyNbiNt)(void *ctx, void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx)`
- `int32_t (*readNbiNt)(void *ctx, void *dst, const void *src, uint64_t len)`
- `int32_t (*notifyRecordNt)(void *ctx, uint32_t remoteNotifyIdx)`
- `int32_t (*notifyWaitNt)(void *ctx, uint32_t localNotifyIdx, uint32_t timeout)`
- `int32_t (*fenceNt)(void *ctx)`

**销毁（1 个）**：
- `void (*destroy)(void *ctx)` — 释放通道上下文及关联资源

##### 三个导出符号

```c
typedef const HcommNicPluginInfo* (*HcommNicPluginGetInfo_t)(void);

typedef int32_t (*HcommNicPluginCreateEndpoint_t)(
    const void *endpointDescRaw, uint32_t epDescLen,
    void **outCtx, HcommNicEndpointOps **outOps);

typedef int32_t (*HcommNicPluginCreateChannel_t)(
    void *epCtx,
    const void *channelDescRaw, uint32_t chDescLen,
    void **outCtx, HcommNicChannelOps **outOps);
```

#### 2.2 插件发现机制

**默认路径加载**（主要方式）：

HCOMM 库加载时，自动扫描 `${CANN_HOME_PATH}/hcomm_plugin/` 目录，对该目录下所有 `*.so` 文件执行 `dlopen` → `dlsym` 三个导出符号 → 注册到插件表。

其中 `CANN_HOME_PATH` 为 CANN 工具包的安装根路径（运行时由 HCOMM 通过 `HcclGetCannHomePath()` 或等效接口获取）。用户只需将编译好的插件 `.so` 放置到 `<cann_install>/hcomm_plugin/` 目录下，重启进程即可生效，无需额外配置。

**环境变量（备选方案）**：

当默认路径不适用时（如开发调试阶段），可通过环境变量显式指定：

| 变量 | 格式 | 说明 |
|------|------|------|
| `HCOMM_NIC_PLUGIN_SO` | `.so` 路径（`:` 分隔多个） | 显式指定单个或多个插件 `.so` |

两种方式的加载优先级：默认路径加载全部 `.so` 后，再处理 `HCOMM_NIC_PLUGIN_SO`（如有设置）。若同一协议号被重复注册，后加载的覆盖先加载的并记录 WARNING 日志。

### 3. 核心数据结构

#### 3.1 Handle 编码

为在函数入口实现零开销分发，利用 `EndpointHandle`（`void*`）和 `ChannelHandle`（`uint64_t`）的高位做标记：

```
bit 63 = 1  → 插件实例，低 63 位指向 PluginEndpointCtx / PluginChannelCtx
bit 63 = 0  → 内置实例，走原有逻辑（完全不变）
```

```c
#define HCOMM_PLUGIN_HANDLE_FLAG   ((uintptr_t)1 << 63)
#define IS_PLUGIN_HANDLE(h)        ((((uintptr_t)(h)) & HCOMM_PLUGIN_HANDLE_FLAG) != 0)
#define PLUGIN_EP_CTX(h)           ((PluginEndpointCtx *)((uintptr_t)(h) & ~HCOMM_PLUGIN_HANDLE_FLAG))
#define PLUGIN_CH_CTX(h)           ((PluginChannelCtx *)((uint64_t)(h) & ~HCOMM_PLUGIN_HANDLE_FLAG))
#define MAKE_PLUGIN_EP_HANDLE(p)   ((EndpointHandle)((uintptr_t)(p) | HCOMM_PLUGIN_HANDLE_FLAG))
#define MAKE_PLUGIN_CH_HANDLE(p)   ((ChannelHandle)((uint64_t)(uintptr_t)(p) | HCOMM_PLUGIN_HANDLE_FLAG))
```

#### 3.2 PluginEndpointCtx

```c
struct PluginEndpointCtx {
    HcommNicEndpointOps *ops;
    void                *ctx;  // 插件通过 CreateEndpoint 分配的私有上下文
};
```

#### 3.3 PluginChannelCtx

```c
struct PluginChannelCtx {
    HcommNicChannelOps *ops;
    void               *ctx;  // 插件通过 CreateChannel 分配的私有上下文
};
```

#### 3.4 插件加载器注册表

```cpp
struct NicPluginEntry {
    HcommNicPluginInfo               info;
    void                            *dlHandle;
    HcommNicPluginCreateEndpoint_t   createEndpoint;
    HcommNicPluginCreateChannel_t    createChannel;
};

class NicPluginLoader {
    std::unordered_map<int32_t, NicPluginEntry> plugins_;  // key = 协议号
public:
    static NicPluginLoader &Instance();
    void LoadAll();
    const NicPluginEntry *Find(int32_t protocol) const;
};
```

### 4. 关键调用流程

#### 4.1 控制面：HcommEndpointCreate → HcommMemReg

```
HcommEndpointCreate(&desc, &handle)
    │
    ├─ entry = NicPluginLoader::Find(desc.protocol)
    │       │
    │       ├─ 命中:
    │       │   entry->createEndpoint(&desc, sizeof(desc), &ctx, &ops)
    │       │   → 插件分配 ctx，填充 ops
    │       │   → HCOMM: auto *epCtx = new PluginEndpointCtx{ops, ctx}
    │       │   → *handle = MAKE_PLUGIN_EP_HANDLE(epCtx)
    │       │   → return
    │       │
    │       └─ 未命中: 走原有 Endpoint::CreateEndpoint → EndpointMgr

HcommMemReg(handle, "my_mem", &mem, &memHandle)
    │
    ├─ if (IS_PLUGIN_HANDLE(handle)) {
    │       PluginEndpointCtx *ep = PLUGIN_EP_CTX(handle);
    │       return ep->ops->registerMemory(ep->ctx, &nicMem, "my_mem", &memHandle);
    │   }
    │
    └─ // 原有逻辑: EndpointMgr::Lookup → ep->RegisterMemory(...)
```

`HcommMemExport`、`HcommMemImport`、`HcommMemUnreg`、`HcommEndpointDestroy` 均采用相同模式：函数入口第一行判断 `IS_PLUGIN_HANDLE`，命中则直接从 `PluginEndpointCtx` 取 ops 调用，否则走原有逻辑。

#### 4.2 控制面：HcommChannelCreate

```
HcommChannelCreate(epHandle, engine, &desc, ..., &chHandle)
    │
    ├─ entry = NicPluginLoader::Find(desc.remoteEndpoint.protocol)
    │       │
    │       ├─ 命中:
    │       │   1. epCtx = PLUGIN_EP_CTX(epHandle)->ctx  // 插件 endpoint 上下文
    │       │   2. entry->createChannel(epCtx, &desc, sizeof(desc), &chCtx, &chOps)
    │       │      → 插件分配 chCtx，填充 chOps
    │       │   3. HCOMM: auto *chCtx = new PluginChannelCtx{chOps, chCtx}
    │       │   4. *chHandle = MAKE_PLUGIN_CH_HANDLE(chCtx)
    │       │   5. chOps.init(chCtx)  // 通道初始化（建立连接、交换内存）
    │       │   6. return
    │       │
    │       └─ 未命中: 走原有 Channel::CreateChannel → ChannelMgr
```

#### 4.3 数据面：HcommWriteOnThread 分发

```
HcommWriteOnThread(thread, chHandle, dst, src, len)
    │
    ├─ if (IS_PLUGIN_HANDLE(chHandle)) {
    │       PluginChannelCtx *ch = PLUGIN_CH_CTX(chHandle);
    │       return ch->ops->write(ch->ctx, (ThreadHandle)thread, dst, src, len);
    │   }
    │
    └─ // 原有逻辑: Thread* → UbTransportLiteImpl* → Write(...)
```

其余 25 个数据面函数（`HcommReadOnThread`、`HcommWriteWithNotifyOnThread`、`HcommChannelNotifyRecord`、`HcommWriteNbi` 等）均采用完全相同的模式：函数体第一行做 `IS_PLUGIN_HANDLE` 判断，命中则直接从 `PluginChannelCtx` 取出 ops 表中对应函数调用，参数逐一直传。OnThread 版本调用 ops 中带 `ThreadHandle` 参数的函数，NoThread 版本调用 `Nt` 后缀函数。

#### 4.4 性能分析：入口分发 vs 虚函数分发

| 方案 | 内置通道开销 | 插件通道开销 |
|------|------------|------------|
| **入口分发（本方案）** | +1 次 bit test + 1 次预测跳转（≈0 cycles） | 1 次 bit test + 1 次间接调用（≈1-3ns） |
| 虚函数分发（上一版） | 无额外开销 | EndpointMgr/ChannelMgr 查找 + 虚函数查找 + ops 调用（≈5-10ns） |
| 查找表 | +1 次数组访问 + 1 次空指针检查 | 同内置通道 + 函数指针调用 |

**入口分发方案的优势**：

- **代码局部性**：控制面/数据面函数的修改仅在其开头增加 6 行 if 块，不触及函数体内的现有复杂逻辑，合并冲突概率低
- **分支预测友好**：内置通道的 `IS_PLUGIN_HANDLE` 位始终为 0，CPU 分支预测器会在首次调用后稳定预测 not-taken，实现零开销
- **无需破坏现有抽象**：不改动 `Endpoint`/`Channel` C++ 类体系，不引入 Wrapper 子类，不影响内部 `EndpointMgr`/`ChannelMgr` 管理逻辑
- **插件路径最短**：bit test → cast → ops 调用，共 1 次间接跳转，无 map 查找、无虚函数表查找

#### 4.5 插件加载时机

`NicPluginLoader::LoadAll()` 通过 `__attribute__((constructor))` 在 `libhcomm.so` 被加载时自动执行。加载顺序：

1. 获取 CANN 安装根路径（通过 `HcclGetCannHomePath()` 或等效接口），拼接默认目录 `<cann_home>/hcomm_plugin/`
2. `opendir` / `readdir` 扫描该目录下所有 `*.so` 文件，对每个调用 `LoadOneSo()`
3. 若设置了 `HCOMM_NIC_PLUGIN_SO` 环境变量（备选方案），按 `:` 分隔后逐路径调用 `LoadOneSo()`
4. `LoadOneSo()` 内部执行 `dlopen(path, RTLD_NOW)` → `dlsym("HcommNicPluginGetInfo")` → `dlsym("HcommNicPluginCreateEndpoint")` → `dlsym("HcommNicPluginCreateChannel")` → 调用 `GetInfo()` 获取协议号 → 写入 `plugins_` map
5. 单个 .so 中任一符号缺失或 `GetInfo()` 返回 NULL 时，`dlclose` 并记录 WARNING 日志
6. 同一协议号重复注册时后加载覆盖先加载，记录 WARNING 日志
7. 默认目录不存在或为空时静默跳过，不影响正常功能

#### 4.6 插件目录结构

```
experimental/nic_plugins/<plugin_name>/
├── README.md              # [必须] 插件说明（动机、设计、编译、使用、限制）
├── CMakeLists.txt         # [推荐] 独立编译，不链接 libhcomm.so
└── src/
    └── <plugin_name>.c    # 实现 3 个导出函数 + 填充 ops 表
```

编译和部署：
```bash
cd experimental/nic_plugins/<plugin_name>
mkdir build && cd build
cmake .. && make -j
cp lib<plugin_name>.so ${ASCEND_HOME_PATH}/hcomm_plugin/
```

#### 4.7 协议覆盖机制

插件可声明**任意**协议号（0-8 覆盖内置协议，1000+ 为新增自定义协议）。`NicPluginLoader` 注册表按协议号索引，`HcommEndpointCreate` 和 `HcommChannelCreate` 在处理时优先查表：

- 若注册表中有插件声明处理当前 `desc.protocol`，则调用插件的 `CreateEndpoint` / `CreateChannel`
- 若注册表中无对应条目，则走原有内置实现
- 因此插件声明 `HCOMM_NIC_PROTO_ROCE = 1` 即可完全替换内置 RoCE 实现，声明 `HCOMM_NIC_PROTO_CUSTOM_BASE + 0 = 1000` 即为新增自定义协议

此设计同时确保了：默认路径下无插件 .so 时系统行为完全不变；部署插件后对该协议的全部 Endpoint/Channel 创建即自动切换到插件实现，无需修改上层 HCCL 算子代码。

### 5. 兼容性考虑

- **向后兼容**：默认路径下不存在插件 .so 时，行为与现有版本完全一致；仅多一次 `opendir` 调用（目录不存在直接返回，无额外开销）
- **性能影响**：内置通道仅多 1 次 bit test 判断（`IS_PLUGIN_HANDLE`），CPU 分支预测 always-not-taken，零开销
- **ABI 兼容**：插件通过 `endpointDescRaw`/`chDescLen` 传递结构体原始字节，支持结构体版本扩展（通过 `CommAbiHeader.version`）
- **部署策略**：将插件 .so 放入 `<cann>/hcomm_plugin/` 目录（生产环境）或通过 `HCOMM_NIC_PLUGIN_SO` 环境变量指定（调试环境），单节点独立控制不受集群中其他节点影响

### 6. 测试方案

| 层级 | 测试内容 | 方法 |
|------|---------|------|
| 单元测试 | `NicPluginLoader` 加载/查找逻辑 | 构造 mock .so，验证 dlopen/dlsym/注册流程 |
| 单元测试 | `IS_PLUGIN_HANDLE` 编码/解码 | 验证 bit63 标记和指针还原 |
| 集成测试 | 插件 Endpoint 创建 → Channel 创建 → Write/Read 完整流程 | 使用 hello_nic 示例插件 + 现有 UT 框架 |
| 系统测试 | 多节点通信（内置+插件混合） | 在测试集群上部署插件，运行 AllReduce/Broadcast 等算子 |
| 性能测试 | 数据面 bit test 开销 | 对比启用/未启用插件时的通信延迟（预期无差异） |

---

## 风险评估

| 风险 | 影响 | 应对 |
|------|------|------|
| 插件 .so 加载失败 | 静默 fallback 到内置实现，无感知 | 日志中记录 WARNING |
| 插件内存泄漏 | 进程生命周期内不卸载 .so，泄漏累积 | 插件开发者自行保证；HCOMM 侧 destroy() 中释放 PluginChannelCtx |
| ChannelHandle bit63 冲突 | 若现有代码使用 bit63 做标记则冲突 | 经代码审查确认当前无此用法，且 bit63 在用户态地址空间保留 |
| 插件滥用协议覆盖 | 可能意外替换内置协议 | 默认路径仅运维可控；调试环境变量由开发者自行把握 |

---

## 替代方案

### 方案 A：编译时宏注册（类似 `REGISTER_EXEC` 模式）

- **优点**：零运行时开销，类型安全
- **缺点**：需要修改 HCOMM 源码和 CMake 构建，无法做到独立 `.so` 动态加载，不符合"不解耦"目标
- **结论**：不采用

### 方案 B：完整的 C++ 子类工厂（继承 Endpoint/Channel）

- **优点**：代码复用度高，插件直接使用 HCOMM 内部工具类
- **缺点**：插件需要链接 `libhcomm.so`，破坏独立编译目标；需要暴露大量内部头文件
- **结论**：不采用

### 方案 C：LD_PRELOAD 劫持

- **优点**：零代码修改
- **缺点**：无法做条件分发（不支持"仅当协议匹配时走插件"）；无法与内置实现共存
- **结论**：不采用

---

## 开放问题

1. **多插件冲突**：同一协议号被多个插件声明时，当前设计为"后加载覆盖先加载"。是否需要更复杂的优先级/回退策略？
2. **插件卸载**：当前设计不卸载已加载插件（`dlclose` 未调用）。是否需要支持运行时卸载和重新加载？
3. **数据面 Nbi 变体**：`HcommWriteNbi` 等实验性 API 的 NoThread 版本是否需要单独的 vtable 入口，还是由 HCOMM 框架层转换为 OnThread 调用？
4. **性能基准**：需要在真实硬件上测量插件数据面开销（bit test + vtable 间接调用），确认低于 1% 的额外延迟。
5. **HCCL 侧协同**：HCOMM 插件机制是否需要与 HCCL 的 `experimental/ops/` 算子插件协同工作（如算子插件直接调用 HCOMM 插件创建的 Channel）？

---

## 评审记录

评审过程在PR评论区进行，详细评审意见请参阅对应的PR评论。