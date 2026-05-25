# 系统概述

## 架构分层图

![](diagrams/arch.excalidraw.svg)

## 核心模块

| 模块 | 路径 | 职责 |
|------|------|------|
|  |  |  |

## 数据流



## 模块依赖关系


# HCOMM 系统概述

## 一、整体项目架构

### 1.1 技术栈

| 维度 | 内容 |
|------|------|
| **语言** | C++14（主体）+ Python（工具/打包） |
| **构建系统** | CMake >= 3.16 + bash 包装脚本（build.sh） |
| **编译选项** | `-Werror -Wall -fno-common -fstack-protector-all -O3` |
| **测试框架** | Google Test + MockCpp + CTest |
| **代码规范** | clang-format（v16）、pre-commit、OAT 合规检查 |
| **运行环境** | Huawei Ascend NPU（昇腾），Linux x86_64 / aarch64 |
| **通信协议** | PCIe、HCCS（Huawei Cache Coherence System）、RDMA（RoCE/IB） |
| **版本** | 9.1.0（CANN 生态） |

### 1.2 项目定位

HCOMM（Huawei Communication）是 HCCL（Huawei Collective Communication Library）的**通信基础库**，为昇腾 NPU 上的集合通信算子（AllReduce、AllGather 等）提供标准化编程接口。它处于 CANN 软件栈的中间层，对上承载通信算子，对下屏蔽芯片差异。

### 1.3 架构分层图

```
┌─────────────────────────────────────────────────────────────┐
│                     AI 框架 / 应用层                         │
│            PyTorch / TensorFlow / MindSpore                 │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│          HCCL Comm API（对外接口层，include/）               │
│  HcclAllReduce / HcclBroadcast / HcclCommInitClusterInfo   │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│  ┌──────────────────────────────────────────────────────┐  │
│  │      控制面（Control Plane）- 框架层 framework/       │  │
│  │  · op_base（API 入口适配）   · communicator（通信域） │  │
│  │  · hcom（hcomm 接口实现）    · next/（新架构）        │  │
│  │  · cluster_maintenance（集群维护）· group（分组）     │  │
│  │  · nslbdp（网络负载均衡）                             │  │
│  └───────────────────────┬──────────────────────────────┘  │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐  │
│  │        数据面（Data Plane）- 算法层 algorithm/         │  │
│  │  · coll_executor（集合通信执行器）                     │  │
│  │  · alg_template（算法模板：Ring / Mesh / Pipeline）   │  │
│  │  · operator（算子注册与调度）                          │  │
│  │  · resource_manager（算法级资源管理）                  │  │
│  └───────────────────────┬──────────────────────────────┘  │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐  │
│  │    通信平台（Platform Layer）- 平台层 platform/       │  │
│  │  · resource（传输/通知/内存/Socket/流管理）           │  │
│  │  · hccp（HCCP 协议栈）  · task（任务调度）            │  │
│  │  · comm_primitive（通信原语）· ping_mesh（网络探测）  │  │
│  │  · remote_access（远端访问）                          │  │
│  └───────────────────────┬──────────────────────────────┘  │
└───────────────────────────┼─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                 昇腾硬件 / 驱动 / 固件                       │
│             Ascend NPU（910xx / 310P 系列）                  │
└─────────────────────────────────────────────────────────────┘
```

### 1.4 动态库构成

项目编译产出三个核心共享库，层次依赖关系如下：

```
hcomm    ← 主库（对外入口），链接 hccl_alg + hccl_plf + hccl_v2
  ↑
hccl_alg ← 算法库（集合通信执行），链接 hccl_plf
  ↑
hccl_plf ← 平台库（资源/通信原语），链接 hccl_legacy
  ↑
hccl_legacy ← 向后兼容库
```

此外还有 **ccl_kernel**（device 侧 AICPU 内核库，交叉编译）和 Python 包 `python/hccl/`。

### 1.5 两种执行模式

| 模式 | 扩展模式值 | 说明 |
|------|-----------|------|
| **AI_CPU** | HCCL_OP_EXPANSION_MODE_AI_CPU = 0 | 集合算子在 AICPU（AI CPU 核）上执行 |
| **AIV** | HCCL_OP_EXPANSION_MODE_AIV = 1 | 集合算子在 AIV（AI Vector 核）上执行 |
| **HOST** | HCCL_OP_EXPANSION_MODE_HOST = 2 | 集合算子在 Host CPU 上执行 |
| **HOST_TS** | HCCL_OP_EXPANSION_MODE_HOST_TS = 3 | Host 上的线程安全模式 |
| **CCU_MS** | HCCL_OP_EXPANSION_CCU_MS = 4 | CCU 微调度模式 |
| **CCU_SCHED** | HCCL_OP_EXPANSION_CCU_SCHED = 5 | CCU 调度模式 |
| **AIV_ONLY** | HCCL_OP_EXPANSION_AIV_ONLY = 6 | 仅 AIV 核执行（无 AICPU 辅助） |

### 1.6 整体运行逻辑

一个典型的集合通信调用链路（以 AllReduce 为例）：

```
用户代码调用 HcclAllReduce(sendBuf, recvBuf, count, ...)
    │
    ▼
include/hccl/hccl_comm.h  ── 弱符号声明
    │
    ▼
src/framework/op_base/  ── 接口分发（V1 / V2 路由）
    │
    ├── V1（旧架构）：通过 legacy 层执行
    │     框架层 communicator → 算法层 executor → 平台层资源
    │
    └── V2（新架构 next/）:
           │
           ▼
     next/coll_comms/api_c_adpt/  ── C 接口适配器
           │
           ▼
     next/coll_comms/communicator/coll_comm.cc  ── CollComm 通信域
           │
           ▼
     algorithm/impl/coll_executor/  ── 算法执行器
           │  例如 coll_all_reduce/AllReduceMeshExecutor
           │
           ▼
     platform/resource/ ── 获取传输通道、通知、内存
           │
           ▼
     platform/hccp/ ── HCCP 协议栈封装，实际下发 RDMA/PCIe/HCCS
           │
           ▼
     昇腾驱动 / 网卡硬件执行实际数据搬运
```

---

## 二、代码调用关系

### 2.1 通信域初始化链路

通信域（Communicator）是集合通信的基础，一个通信域包含一组可以互相通信的 rank。

```
HcclCommInitClusterInfo(clusterInfo, rank, &comm)    ← 用户入口
    │
    ▼
op_base 层分发
    ├── 弱符号 HcclCommInitClusterInfoV2（next 架构）
    │       │
    │       ▼
    │   coll_comm_c_adpt.cc  →  CollCommMgr::GetInstance()
    │       │
    │       ▼
    │   CollComm::Init(rankGraph, ...)
    │       ├── 创建 RankGraphV2（拓扑图，描述所有 rank 的连接关系）
    │       ├── 创建 CommEngineResMgr（通信引擎资源管理）
    │       ├── 创建 ContextManager（上下文管理）
    │       ├── 创建 CommMemMgr（通信内存管理）
    │       ├── 创建 ChannelManager（通道管理）
    │       └── 创建 MyRank（本 rank 信息）
    │
    └── V1 旧架构（hcclComm 类）
            │
            ▼
        hcclComm 构造函数（framework/communicator/）
            ├── planner（算法规划器）
            └── communicator_（HcclCommunicator 指针，legacy 层）
```

### 2.2 集合通信算子执行链路（以 AllReduce 为例）

```
HcclAllReduce(sendBuf, recvBuf, count, dataType, op, comm, stream)
    │
    ▼
op_base 层：op_base.h → op_base.cpp
    │
    ▼
HcclAllReduce → 检查参数 → 获取通信域上下文
    │
    ├── 新架构 V2：HcclAllReduceV2
    │       │
    │       ▼
    │   CollComm 查找对应的算法执行策略
    │       │
    │       ▼
    │   coll_executor/coll_all_reduce/
    │   AllReduceMeshExecutor::RunAsync()  ← 算法执行器
    │       │
    │       ├── 计算通信算法（Ring / Mesh / Pipeline 策略）
    │       ├── 拆分数据为多个 slice
    │       ├── 为每个 slice 创建传输任务
    │       │   ├── 本地 Reduce（HcommLocalReduceOnThread）
    │       │   ├── 远端 Write（HcommWriteOnThread / RDMA Write）
    │       │   └── 同步 Notify（HcommChannelNotifyRecord/Wait）
    │       │
    │       ▼
    │   platform/resource/ 层获取传输通道
    │       │
    │       ▼
    │   platform/hccp/ 协议栈封装
    │       │
    │       ▼
    │   驱动层实际数据搬运
    │
    └── 旧架构 V1：通过 legacy/ 层执行
            │
            ▼
        HcclCommunicator（legacy/include/hccl_communicator.h）
            │
            ▼
        executor_impl.h → 选择算法模板（Ring / FullMesh 等）
            │
            ▼
        下发传输任务到 platform 层
```

### 2.3 关键类依赖关系

```
┌──────────────────────────────────────────────────┐
│  CollComm（next/coll_comms/communicator/）        │
│  集合通信域，管理通信生命周期                      │
├──────────────────────────────────────────────────┤
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │RankGraph │ │MyRank    │ │CommEngineResMgr│   │
│  │(拓扑图)   │ │(本rank)  │ │(引擎资源)     │   │
│  └──────────┘ └──────────┘ └──────────────┘    │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │CommMemMgr│ │ChnlMgr  │ │ContextMgr    │   │
│  │(内存管理) │ │(通道管理)│ │(上下文管理)   │   │
│  └──────────┘ └──────────┘ └──────────────┘    │
└──────────────────────┬───────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────┐
│  coll_executor（algorithm/impl/coll_executor/）   │
│  AllReduceMeshExecutor / AllGatherMeshExecutor   │
│  BroadcastMeshExecutor / ReduceScatterExecutor   │
│  ...                                              │
│  实现具体的算法逻辑，调用平台原语完成任务           │
└──────────────────────┬───────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────┐
│  CommPlf（platform/）                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │Transport │ │Notify    │ │Mem           │    │
│  │(传输通道) │ │(通知同步) │ │(内存注册)    │    │
│  └──────────┘ └──────────┘ └──────────────┘    │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │Socket    │ │Dispatcher│ │Task          │    │
│  │(TCP通信) │ │(分发器)   │ │(任务调度)    │    │
│  └──────────┘ └──────────┘ └──────────────┘    │
│  ┌──────────────────────────────────────────┐    │
│  │  HCCP 协议栈                              │    │
│  │  hccp_service（服务端）+ rdma_agent（客户端）│   │
│  └──────────────────────────────────────────┘    │
└──────────────────────────────────────────────────┘
```

### 2.4 模块间数据流

以一次 AllReduce 跨节点通信为例：

1. **算法层**（coll_executor）决定数据分片策略和各 step 的通信模式
2. **框架层**（CollComm）提供通信域上下文，包括远端 rank 的地址信息
3. **平台层**（Transport）获取与远端 rank 之间建立的传输通道
4. **平台层**（Notify）分配通知资源用于 step 间同步
5. **协议层**（HCCP）封装实际通信协议（RDMA Write / PCIe DMA）
6. **驱动层** 执行内存搬运，完成后通过 Notify 通知同步

### 2.5 "next/" 新架构 vs Legacy 旧架构

| 维度 | Legacy | next/（新架构） |
|------|--------|----------------|
| 通信域 | hcclComm（类，包含 HcclCommunicator 指针） | CollComm（聚合 RankGraph/CommMemMgr/ChannelManager 等） |
| 算法执行 | executor_impl → HcclImpl | coll_executor（独立执行器类） |
| 拓扑 | RankTable（json 解析） | RankGraphV2（图结构） |
| 资源管理 | 集成在 communicator 中 | 分离：CommEngineResMgr / CommMemMgr / ChannelManager |
| 通信引擎 | 单一 | 支持多引擎（CCU/AIV/Host） |
| 扩展模式 | 有限 | HCCL_OP_EXPANSION_CCU_MS / CCU_SCHED 等 |
| 代码位置 | src/legacy/ | src/framework/next/ |

---

## 三、核心概念与关键名词

### 3.1 通信相关

| 术语 | 说明 |
|------|------|
| **Rank** | 通信域中的一个参与节点，每个 rank 有唯一 ID |
| **Communicator (通信域)** | 一组 rank 组成的通信集合，域内所有 rank 可互相通信 |
| **World** | 默认的全局通信域，包含所有 rank |
| **SubComm** | 从 World 中划分的子通信域 |
| **Collection Communication** | 集合通信操作（AllReduce、AllGather、Broadcast 等），所有 rank 同时参与 |
| **P2P** | 点对点通信（Send/Recv），两个 rank 之间直接通信 |
| **RankTable** | 记录所有 rank 的拓扑信息（设备 ID、IP、端口等）的 JSON 表 |
| **RankGraph** | 通信拓扑的图表示，包含多层网络拓扑信息 |
| **Channel (通道)** | 两个 endpoint 之间的单向/双向数据传输通道，底层映射为 RDMA QP 或 HCCS link |
| **Endpoint** | 通信端点，每个 rank 可以有多个 endpoint（对应不同网卡/链路） |
| **Link** | 两个 endpoint 之间的物理/逻辑连接 |

### 3.2 硬件平台相关

| 术语 | 说明 |
|------|------|
| **Ascend NPU** | 华为昇腾 AI 处理器 |
| **AICPU (AI CPU)** | NPU 中的 AI CPU 核心，用于任务调度和控制 |
| **AIV (AI Vector)** | NPU 中的向量计算核心，用于执行向量运算 |
| **CCU (Cluster Control Unit)** | 集群控制单元，NPU 内部的控制核心，实现微调度 |
| **HCCS** | 华为 Cache Coherence System，昇腾芯片间高速互联协议 |
| **RDMA** | Remote Direct Memory Access，远程直接内存访问（RoCE / InfiniBand） |
| **PCIe** | PCI Express，芯片间/设备间互联 |
| **Device** | NPU 设备（昇腾芯片） |
| **Host** | CPU 侧（x86 / ARM） |
| **910xx / 310P** | 昇腾不同的芯片代际型号 |

### 3.3 模块和代码相关

| 术语 | 说明 |
|------|------|
| **HCCP** | HUAWEI Collective Communication Protocol，集合通信协议栈，封装 RDMA/HCCS/PCIe |
| **hccl_plf** | HCCL Platform 库，platform/ 层的编译产出 |
| **hccl_alg** | HCCL Algorithm 库，algorithm/ 层的编译产出 |
| **hcomm** | 主库，包含 framework/ 层逻辑，对外公开 HCCL 接口 |
| **next/** | 新架构代码（src/framework/next/），逐步替代 legacy |
| **legacy/** | 旧架构代码（src/legacy/），向后兼容 |
| **op_base** | 算子入口，负责 API 路由（V1 走 legacy，V2 走 next） |
| **coll_executor** | 集合通信算子执行器，每种算子（AllReduce、AllGather 等）有独立子目录 |
| **alg_template** | 算法模板（Ring、Mesh、Pipeline），被具体 executor 实例化 |
| **Dispatcher** | 任务分发器，将计算任务分发给不同的执行引擎 |
| **nslbdp** | Network Service Load Balancing Data Plane，数据面网络负载均衡 |
| **HCCD** | HCCL Communication Daemon，进程间点对点通信守护组件 |

### 3.4 设计理念

1. **控制面与数据面分离**：控制面（framework）负责资源管理、拓扑查询、通信域生命周期；数据面（algorithm + platform）负责实际数据传输和计算
2. **平台抽象**：platform 层封装了不同硬件（RDMA/HCCS/PCIe）的差异，对上提供统一的 Transport/Notify/Mem 接口
3. **算法与平台解耦**：algorithm 层通过标准平台接口使用资源，不直接感知硬件细节，支持新算法灵活插入
4. **双架构并行**：next/（新架构）和 legacy/（旧架构）共存，通过 V2 弱符号机制实现平滑过渡
5. **多种执行模式**：同一套 API 可以在 AICPU、AIV、CCU、Host 等多种执行引擎上运行

---

## 四、项目主要入口

### 4.1 对外 API 入口

**C 接口头文件（用户可见）**：

| 文件 | 用途 |
|------|------|
| `include/hccl/hccl_comm.h` | 通信域管理和集合通信操作（Init/Destroy/AllReduce/Broadcast 等） |
| `include/hccl/hccl_types.h` | 数据类型定义（HcclResult、HcclDataType、HcclReduceOp 等） |
| `include/hccl/hccl_res.h` | 通信资源查询接口 |
| `include/hccl/hccl_rank_graph.h` | RankGraph 拓扑查询接口 |
| `include/hccl/hccl_sym_win.h` | 对称内存窗口接口 |
| `include/hcomm_primitives.h` | 数据面编程原语接口（LocalCopy/Write/Read/Notify） |
| `include/hcomm_res.h` | 资源管理接口 |

**内部模块间接口（pkg_inc/）**：

| 文件 | 用途 |
|------|------|
| `pkg_inc/hccl/hcom.h` | 集合通信内部接口（HcomAllReduce 等 Hcom 前缀） |
| `pkg_inc/hccl/hccl_ex.h` | 扩展接口 |
| `pkg_inc/hccl/base.h` | 基础类型和工具 |
| `pkg_inc/hccl/workflow.h` | 工作流定义 |
| `pkg_inc/hcomm/ccu/` | CCU 相关接口头文件（40+ 个） |

### 4.2 实现入口点

**线程执行入口**：

- AICPU 模式：`HcommAcquireComm → HcommReleaseComm` 在 AI CPU 上执行
- Host 模式：集合通信 API 直接在 Host 线程上执行
- CCU 模式：CCU 微码在 CCU 硬件上执行

### 4.3 示例代码入口

`examples/01_communicators/01_one_device_per_process/main.cc` 展示了完整的用户调用流程：

```
MPI_Init → aclInit → aclrtSetDevice
    → HcclCommInitRootInfoConfig → HcclAllReduce → HcclCommDestroy
    → aclrtResetDevice → aclFinalize → MPI_Finalize
```

### 4.4 编译入口

| 入口 | 路径 | 说明 |
|------|------|------|
| 编译脚本 | `build.sh` | 参数化构建（--pkg / --ut / -s / --aicpu / --build_aarch） |
| CMake 入口 | `CMakeLists.txt` | 顶层 cmake，配置三方件和子模块 |
| 框架库定义 | `src/framework/hcomm.cmake` | hcomm 库的源文件和链接配置 |
| 平台库定义 | `src/platform/hccl_plf.cmake` | hccl_plf 库的源文件和链接配置 |
| 算法库定义 | `src/algorithm/hccl_alg.cmake` | hccl_alg 库的源文件和链接配置 |

---

## 五、新手学习指南

### 5.1 推荐学习顺序

**第一阶段：理解项目定位和用户视角（1-2 天）**

1. 阅读 `README.md` — 了解项目定位和整体目录
2. 阅读 `examples/01_communicators/01_one_device_per_process/main.cc` — 最直观的用户使用流程（AllReduce 示例）
3. 阅读 `include/hccl/hccl_comm.h` — 理解对外 API 的全貌
4. 阅读 `include/hcomm_primitives.h` — 理解数据面编程原语

**第二阶段：掌握构建和测试（1 天）**

1. 阅读 `build.sh` — 理解构建参数和流程
2. 阅读 `CMakeLists.txt` — 理解模块组织方式
3. 阅读 `test/ut/CMakeLists.txt` — 理解 UT 的目录结构
4. 阅读 `test/CMakeLists.txt` — 理解 UT/ST 的切换

**第三阶段：核心内部实现（3-5 天）**

按照调用链路从上到下阅读：

1. **框架入口**：`src/framework/op_base/src/op_base.h` → `op_base_v2.h` — API 路由
2. **通信域管理**：
   - `src/framework/next/coll_comms/communicator/coll_comm.h` — 新架构核心类
   - `src/framework/next/coll_comms/communicator/coll_comm_mgr.h` — 通信域管理器
   - `src/framework/communicator/hccl_comm.cc` — 旧架构核心类
3. **算法执行**：
   - `src/algorithm/impl/coll_executor/coll_executor_base.h` — 执行器基类
   - `src/algorithm/impl/coll_executor/coll_all_reduce/` — 以 AllReduce 为例
   - `src/algorithm/base/alg_template/` — 算法模板
4. **平台资源**：
   - `src/platform/resource/transport/` — 传输通道
   - `src/platform/resource/notify/` — 通知同步
   - `src/platform/hccp/` — 协议栈封装

**第四阶段：专题深入（按需选择）**

- **CCU 微架构**：`src/framework/next/comms/ccu/` + `pkg_inc/hcomm/ccu/`
- **集群维护**：`src/framework/cluster_maintenance/`
- **网络探测**：`src/platform/ping_mesh/`
- **负载均衡**：`src/framework/nslbdp/`
- **HCCD 进程通信**：`src/hccd/`
- **新算子开发**：`docs/zh/comm_op_dev_guide/`

**第五阶段：测试与调试（1-2 天）**

1. 阅读一个典型 UT：`test/ut/framework/communicator/` 下的测试文件
2. 阅读 stub 实现：`test/ut/stub/` — 理解如何 mock Ascend 依赖
3. 阅读 `test/ut/CMakeLists.txt` — 理解 UT 的 include 和链接配置

### 5.2 代码阅读优先级

```
第一优先级（必须读）：
├── include/hccl/hccl_comm.h          ← 对外 API 全貌
├── examples/*/main.cc                ← 用户使用示例
├── src/framework/next/coll_comms/    ← 新架构核心
├── src/algorithm/impl/coll_executor/ ← 算法执行器
├── CMakeLists.txt + build.sh         ← 构建理解

第二优先级（推荐读）：
├── include/hcomm_primitives.h        ← 数据面原语
├── src/platform/resource/            ← 资源管理
├── src/platform/hccp/                ← 协议栈封装
├── src/framework/communicator/       ← 旧架构通信域
├── src/pub_inc/                      ← 公共内部接口

第三优先级（按需读）：
├── src/legacy/                       ← 旧架构代码
├── src/hccd/                         ← 进程间通信
├── src/framework/cluster_maintenance/← 集群维护
├── src/common/                       ← 公共工具
├── src/framework/next/comms/ccu/     ← CCU 微架构
```

### 5.3 调试技巧

- **日志**：`HCCL_INFO` / `HCCL_ERROR` 宏散布在代码各处，可通过环境变量开启 HCCL 日志
- **UT 运行**：`bash build.sh --ut` 编译并运行所有 UT，单模块 UT 可在 `build/` 目录下直接运行对应二进制
- **ST 运行**：`bash build.sh -s` 运行所有 ST，或 `--executor_hccl_test` 等参数筛选
- **地址检查**：`--asan` 编译选项开启 AddressSanitizer
- **覆盖率**：`--cov` 编译选项 + `--ut` 生成覆盖率报告

### 5.4 关键建议

1. **从例子入手**：examples 目录下的示例代码是最干净的入口，先跑通再深入
2. **AllReduce 是 "Hello World"**：理解了 AllReduce 的执行链路，就理解了项目的一半
3. **区分 V1/V2**：代码中有大量 V1/V2 接口并存的情况，注意看 `op_base_v2.h` 中的弱符号声明
4. **关注 next/ 目录**：新架构的核心代码在 `src/framework/next/`，这是未来的演进方向
5. **平台层是关键瓶颈**：理解 Transport/Notify/Mem 三个核心资源类，就理解了数据面的本质
6. **不要被 legacy 吓到**：legacy/ 目录代码量大但大部分是向后兼容，理解核心接口即可



