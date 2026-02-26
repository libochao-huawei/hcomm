# HCOMM RoCE 通信机制设计文档

## 概述

HCOMM 中存在两套不同的 RoCE 通信机制，分别针对不同的硬件平台：

| 机制 | 适用平台 | 网卡类型 | 核心类 |
|------|----------|----------|--------|
| **NPU RoCE (TransportIbverbs)** | A2/A3 等老节点 | NPU 网卡 (NIC_DEPLOYMENT_DEVICE) | `TransportIbverbs` |
| **Host CPU RoCE (HostCpuRoceChannel)** | A5 新节点 | CPU 网卡 (NIC_DEPLOYMENT_HOST) | `HostCpuRoceChannel` |

---

## 第一部分：TransportIbverbs（NPU RoCE）

### 1.1 类位置与继承关系

```
src/platform/resource/transport/host/transport_ibverbs_pub.h
src/platform/resource/transport/host/transport_ibverbs.cc

class TransportIbverbs : public TransportNet
```

### 1.2 通信建联流程

#### 1.2.1 初始化流程（Init）

```
TransportIbverbs::Init()
    ├── 参数检查（设备ID、交换数据等）
    ├── 获取 Socket 连接（machinePara_.sockets[0]）
    ├── 获取设备类型（hrtGetDeviceType）
    ├── 获取 NIC RDMA Handle（GetNicHandle）
    │       └── 从 raResourceInfo.nicSocketMap 中获取设备侧网卡
    ├── 设置链路类型为 LINK_ROCE
    ├── 获取工作流模式（Offline / OpBase）
    ├── 获取 Notify 大小（GetNotifySize）
    └── 初始化 QP 连接（InitQpConnect）
```

#### 1.2.2 QP 连接初始化（InitQpConnect）

```
InitQpConnect()
    ├── 获取每连接 QP 数量（GetQpsPerConnection）
    │       └── 单算子模式下可配置多 QP，其他模式固定为 1
    ├── 创建 QP（CreateQp）
    │       ├── 创建单 QP（CreateSingleQp）
    │       │       ├── 根据 QP 模式选择创建方式
    │       │       │       ├── 普通 QP 模式：hrtRaAiQpCreate（AI CPU 模式）
    │       │       │       ├── 基础模式：HrtRaQpCreate
    │       │       │       └── 扩展模式：hrtRaQpCreateWithAttrs
    │       │       └── 配置 QP 属性（TC、SL、Timeout、Retry Count）
    │       └── 多 QP 模式下创建额外 QP（CreateMultiQp）
    ├── 计算交换数据总大小（FillExchangeDataTotalSize）
    ├── 构建发送交换数据（ConstructExchangeForSend）
    │       ├── 注册用户内存（输入/输出内存）
    │       ├── 创建 Notify Buffer（dataNotify/ackNotify/dataAckNotify）
    │       ├── 注册自定义用户内存（独立算子场景）
    │       └── 打包 QP 数量、MultiQpThreshold 等元数据
    ├── 创建 Notify Value Buffer
    ├── Socket 发送交换数据
    ├── Socket 接收对端交换数据
    ├── 解析接收到的交换数据（ParseReceivedExchangeData）
    │       ├── 校验 QP 数量一致性
    │       ├── 获取远端内存地址（输入/输出/Notify）
    │       └── 解析自定义用户内存信息
    └── 连接 QP（ConnectQp）
```

#### 1.2.3 QP 连接建立（ConnectQp）

```
ConnectQp()
    └── 连接单 QP（ConnectSingleQp）
            ├── 遍历所有 QP Handle
            ├── 异步连接 QP（HrtRaQpConnectAsync）
            │       └── 使用 socket fd 进行 QP 状态迁移
            └── 轮询等待 QP 状态就绪（hrtGetRaQpStatus）
                    └── qpStatus == 1 表示建链成功
```

### 1.3 HcclSend/HcclRecv 交互流程

#### 1.3.1 发送流程（TxAsync）

```
TxAsync(dstMemType, dstOffset, src, len, stream)
    └── TxSendDataAndNotify
            └── TxSendDataAndNotifyWithSingleQP
                    ├── 构造数据 WQE（TxPayLoad -> ConstructPayLoadWqe）
                    │       ├── 计算发送次数（单 WQE 最大 2GB）
                    │       └── 添加 WQE 到列表（AddWqeList）
                    │               └── 设置 RDMA_WRITE 操作码
                    ├── 添加 Notify WQE（数据同步信号）
                    └── 下发 RDMA 发送（RdmaSendAsync）
                            ├── 非模板模式：使用 dbIndex/dbInfo 下发
                            └── 模板模式：使用 sqIndex/wqeIndex 下发
                                    └── dispatcher_->RdmaSend()
```

#### 1.3.2 接收流程（RxAsync）

```
RxAsync(srcMemType, srcOffset, dst, len, stream)
    └── 等待数据 Notify 信号（LocalIpcNotify::Wait）
            └── 通过 dataNotify_ 等待对端发送完成通知
```

#### 1.3.3 数据同步机制

TransportIbverbs 使用 **RDMA Write + Notify** 模式进行数据同步：

1. **发送端**：
   - 下发 RDMA Write WQE 将数据写入对端内存
   - 下发额外的 Notify WQE（向对端 dataNotify_ 地址写入信号）
   
2. **接收端**：
   - 通过 Wait 操作轮询本地 dataNotify_，等待信号到达

### 1.4 关键数据结构

```cpp
// WQE 信息结构
struct WqeInfo {
    struct SendWrlistDataExt wqeData;
    u64 wqeType;           // WQE 类型（数据/Notify/Ack）
    u64 wqeDataOffset;     // 数据偏移
    u32 notifyId;          // Notify ID
};

// 内存消息结构
struct MemMsg {
    void* addr;            // 内存地址
    u64 len;               // 内存长度
    u32 lkey;              // 本地密钥
    u32 offset;            // Notify 偏移
    u32 notifyId;          // Notify ID
    MemType memType;       // 内存类型
    s32 mrRegFlag;         // MR 注册标志
};

// QP 模式
enum {
    NORMAL_QP_MODE = 0,      // 普通 QP（不使用）
    OFFLINE_QP_MODE = 1,     // 下沉模式
    OPBASE_QP_MODE = 2,      // 单算子模式
    OFFLINE_QP_MODE_EXT = 3, // 下沉模式（910B/910_93）
    OPBASE_QP_MODE_EXT = 4,  // 单算子模式（910B/910_93）
};
```

### 1.5 特点总结

| 特性 | 说明 |
|------|------|
| **网卡位置** | NPU 设备侧网卡（nicSocketMap） |
| **QP 创建** | 通过 HCCP 接口（hrtRaAiQpCreate/HrtRaQpCreate） |
| **数据下发** | 通过 Dispatcher 下发 RDMA Task |
| **同步机制** | RDMA Write + Notify（LocalIpcNotify） |
| **支持模式** | 下沉模式（Offline）和单算子模式（OpBase） |
| **不支持** | Host 网卡通信（仅支持 NPU 网卡） |

---

## 第二部分：HostCpuRoceChannel（Host CPU RoCE）

### 2.1 类位置与继承关系

```
src/framework/next/comms/endpoint_pairs/channels/host/host_cpu_roce_channel.h
src/framework/next/comms/endpoint_pairs/channels/host/host_cpu_roce_channel.cc

class HostCpuRoceChannel : public Channel
```

### 2.2 通信建联流程

#### 2.2.1 初始化流程（Init）

```
HostCpuRoceChannel::Init()
    ├── 解析输入参数（ParseInputParam）
    │       ├── 从 endpointHandle 获取本地端点描述和 RDMA Handle
    │       ├── 从 channelDesc 获取远端端点、Socket 和 Notify 数量
    │       └── 从 memHandles 获取本地 RDMA RMA Buffer
    ├── 构建连接（BuildConnection）
    │       └── 创建 HostRdmaConnection 对象
    ├── 构建 Notify（BuildNotify）
    │       └── 从 DpuNotifyManager 分配 Notify ID
    └── 构建 Buffer（BuildBuffer）
```

#### 2.2.2 状态机驱动的建联（GetStatus）

```
GetStatus() 状态机
    ├── RdmaStatus::INIT
    │       └── CheckSocketStatus()
    │               └── Socket 状态 OK → 进入 SOCKET_OK
    ├── RdmaStatus::SOCKET_OK
    │       └── CreateQp()
    │               └── HostRdmaConnection::CreateQp()
    │                       └── 调用 ibv_create_qp 创建 QP
    │               └── 进入 QP_CREATED
    ├── RdmaStatus::QP_CREATED
    │       └── ExchangeData()          // 交换 QP 信息、Buffer 信息、Notify ID
    │               ├── 打包 Notify ID 列表（NotifyVecPack）
    │               ├── 打包 Buffer 信息（BufferVecPack）
    │               ├── 打包连接信息（ConnVecPack）
    │               ├── Socket 发送数据
    │               ├── Socket 接收数据
    │               └── 解包对端数据（Notify/Buffer/Conn）
    │       └── 进入 DATA_EXCHANGE
    ├── RdmaStatus::DATA_EXCHANGE
    │       └── ModifyQp()
    │               ├── 解析远端交换数据（ParseRmtExchangeDto）
    │               └── 修改 QP 状态为 RTR/RTS
    │       └── 进入 QP_MODIFIED
    └── 最终状态 → CONN_OK / ChannelStatus::READY
```

#### 2.2.3 数据交换详情

**发送的数据包结构**：

```
+----------------------------------+
|  Notify 数量（notifyNum_）        |
+----------------------------------+
|  Notify ID 列表                   |
+----------------------------------+
|  Buffer 数量（bufferNum_）        |
+----------------------------------+
|  Buffer 信息列表（ExchangeRdmaBufferDto）|
+----------------------------------+
|  连接数量（connNum_）             |
+----------------------------------+
|  连接信息列表（ExchangeRdmaConnDto）|
+----------------------------------+
```

### 2.3 HcclSend/HcclRecv 交互流程

#### 2.3.1 发送流程（WriteWithNotify）

```
WriteWithNotify(dst, src, len, remoteNotifyIdx)
    ├── 检查本地和远端 Buffer 是否就绪
    ├── 补充 RQ 中的 RQE（IbvPostRecv）
    │       └── 调用 ibv_post_recv 预投递接收 WQE
    ├── 准备 Send WR（PrepareWriteWrResource）
    │       ├── 设置源地址（本地 Buffer）
    │       ├── 设置目的地址（远端 Buffer）
    │       ├── 设置操作码为 IBV_WR_RDMA_WRITE_WITH_IMM
    │       ├── 设置远端 RKey 和地址
    │       └── 设置立即数（imm_data = dpuNotifyId）
    └── 下发 Send WR（ibv_post_send）
```

#### 2.3.2 接收流程（NotifyWait）

```
NotifyWait(localNotifyIdx, timeout)
    ├── 获取本地 Notify ID（localDpuNotifyIds_）
    ├── 轮询 CQ（ibv_poll_cq）
    │       └── 从 recvCq 中轮询完成项
    └── 检查完成项的 imm_data 是否匹配预期的 Notify ID
            └── 匹配成功 → 接收完成
```

#### 2.3.3 数据同步机制

HostCpuRoceChannel 使用 **RDMA Write With Immediate** 模式：

1. **发送端**：
   - 调用 `ibv_post_send` 下发 `IBV_WR_RDMA_WRITE_WITH_IMM` 操作
   - 数据写入对端内存的同时，携带立即数（imm_data = Notify ID）
   - 立即数会触发对端产生 CQE

2. **接收端**：
   - 预投递 RQ（`ibv_post_recv`）
   - 轮询 CQ（`ibv_poll_cq`）等待带有特定 imm_data 的 CQE
   - 匹配到对应的 Notify ID 后确认接收完成

### 2.4 关键数据结构

```cpp
// RDMA 状态机
enum class RdmaStatus {
    INIT,           // 初始状态
    SOCKET_OK,      // Socket 就绪
    QP_CREATED,     // QP 创建完成
    DATA_EXCHANGE,  // 数据交换完成
    QP_MODIFIED,    // QP 状态修改完成
    CONN_OK         // 连接建立成功
};

// QP 属性 DTO
struct QpAttrDto {
    uint32_t qpn;           // QP 号
    uint32_t psn;           // Packet Sequence Number
    uint32_t gid_idx;       // GID 索引
    unsigned char gid[HCCP_GID_RAW_LEN];  // GID
};

// 交换数据结构
struct ExchangeRdmaConnDto {
    uint32_t qpn;
    uint32_t psn;
    uint32_t gidIdx;
    std::vector<uint8_t> gid;
};

struct ExchangeRdmaBufferDto {
    uint64_t addr;
    uint32_t rkey;
    uint32_t lkey;
    uint64_t size;
    std::string memTag;
};
```

### 2.5 HostRdmaConnection 辅助类

```
HostRdmaConnection
    ├── Init()              // 初始化
    ├── CreateQp()          // 创建 QP（ibv_create_qp）
    ├── GetExchangeDto()    // 获取交换数据
    ├── ParseRmtExchangeDto()   // 解析远端数据
    └── ModifyQp()          // 修改 QP 状态（INIT → RTR → RTS）
```

### 2.6 特点总结

| 特性 | 说明 |
|------|------|
| **网卡位置** | Host CPU 侧网卡（hostNetSocketMap） |
| **QP 创建** | 直接使用 ibverbs 接口（ibv_create_qp） |
| **数据下发** | 直接调用 ibv_post_send/ibv_post_recv |
| **同步机制** | RDMA Write With Immediate + CQ Polling |
| **适用场景** | A5 新节点，支持 CPU 网卡通信 |
| **特点** | 不依赖 HCCP/HRT 抽象层，直接操作 Verbs |

---

## 第三部分：两套机制对比

### 3.1 架构层次对比

```
┌─────────────────────────────────────────────────────────────┐
│                     HcclSend/HcclRecv                        │
├─────────────────────────────────────────────────────────────┤
│  TransportIbverbs (A2/A3)      │  HostCpuRoceChannel (A5)   │
│  =========================     │  ======================    │
│  - HCCP/HRT 抽象层             │  - Orion Verbs 层          │
│  - Dispatcher 下发 Task        │  - 直接 ibv_post_send      │
│  - LocalIpcNotify 同步         │  - CQ Polling 同步         │
├─────────────────────────────────────────────────────────────┤
│  NPU 网卡 (NIC_DEPLOYMENT_DEVICE) │ CPU 网卡 (NIC_DEPLOYMENT_HOST) │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 关键差异对比表

| 对比项 | TransportIbverbs (A2/A3) | HostCpuRoceChannel (A5) |
|--------|--------------------------|------------------------|
| **目标硬件** | Ascend 910A/B, 910_93 | Ascend A5 及后续新节点 |
| **网卡位置** | NPU 设备侧 | Host CPU 侧 |
| **网卡获取** | `raResourceInfo.nicSocketMap` | `raResourceInfo.hostNetSocketMap` |
| **编程接口** | HCCP/HRT 高层抽象 | ibverbs 底层接口 |
| **QP 创建** | `hrtRaAiQpCreate` / `HrtRaQpCreate` | `ibv_create_qp` |
| **QP 连接** | `HrtRaQpConnectAsync` + 轮询状态 | `ibv_modify_qp` 状态机 |
| **数据发送** | `dispatcher_->RdmaSend` | `ibv_post_send` |
| **同步方式** | Write + Notify（内存写入信号）| Write With Immediate（立即数）|
| **等待机制** | `LocalIpcNotify::Wait` | `ibv_poll_cq` |
| **交换数据** | 自定义二进制格式（MemMsg）| BinaryStream 序列化 |
| **建联驱动** | Init 一次性完成 | GetStatus 状态机驱动 |
| **多 QP 支持** | 支持（配置化）| 当前设计单连接 |

### 3.3 数据流对比

#### TransportIbverbs 数据流：
```
[本地内存] → HRT RDMA接口 → [NPU网卡] ──RoCE──→ [NPU网卡] → 对端内存
                                              ↓
                                         Notify信号写入
                                              ↓
[接收等待] ← LocalIpcNotify ← [对端Notify内存] ←┘
```

#### HostCpuRoceChannel 数据流：
```
[本地内存] → ibv_post_send → [CPU网卡] ──RoCE──→ [CPU网卡] → 对端内存
                                                    ↓
                                              带立即数CQE
                                                    ↓
[接收等待] ← ibv_poll_cq ← [接收CQ] ←─────────────┘
```

---

## 第四部分：以 HcclSend/HcclRecv 为例的完整流程

### 4.1 TransportIbverbs 场景

```
HcclSend
    ├── 获取 TransportIbverbs 实例
    ├── TxAsync(OUTPUT_MEM, offset, src, len, stream)
    │       └── TxSendDataAndNotifyWithSingleQP
    │               ├── AddWqeList(数据WQE, RDMA_WRITE)
    │               ├── AddWqeList(NotifyWQE, 写dataNotify)
    │               └── RdmaSendAsync
    │                       └── dispatcher_->RdmaSend(dbIndex, dbInfo, ...)
    └── 发送完成

HcclRecv
    ├── 获取 TransportIbverbs 实例
    ├── RxAsync(INPUT_MEM, offset, dst, len, stream)
    │       └── LocalIpcNotify::Wait(dataNotify_)
    │               └── 等待对端 Write 到 dataNotify_
    └── 接收完成（数据已写入 dst）
```

### 4.2 HostCpuRoceChannel 场景

```
HcclSend
    ├── 获取 HostCpuRoceChannel 实例
    ├── WriteWithNotify(dst, src, len, remoteNotifyIdx)
    │       ├── IbvPostRecv()  // 预补充RQ
    │       ├── PrepareWriteWrResource
    │       │       └── 构造 IBV_WR_RDMA_WRITE_WITH_IMM WR
    │       └── ibv_post_send(qp, &writeWithNotifyWr)
    └── 发送完成

HcclRecv
    ├── 获取 HostCpuRoceChannel 实例
    ├── NotifyWait(localNotifyIdx, timeout)
    │       └── ibv_poll_cq(recvCq, 1, &wc)
    │               └── 检查 wc.imm_data == dpuNotifyId
    └── 接收完成（数据已从对端写入本地Buffer）
```

---

## 附录：相关文件列表

### TransportIbverbs
- `src/platform/resource/transport/host/transport_ibverbs_pub.h`
- `src/platform/resource/transport/host/transport_ibverbs.cc`

### HostCpuRoceChannel
- `src/framework/next/comms/endpoint_pairs/channels/host/host_cpu_roce_channel.h`
- `src/framework/next/comms/endpoint_pairs/channels/host/host_cpu_roce_channel.cc`
- `src/framework/next/comms/endpoint_pairs/channels/host/host_rdma_connection.h`
- `src/framework/next/comms/endpoint_pairs/channels/host/host_rdma_connection.cc`

### 公共依赖
- `src/platform/resource/transport/host/transport_net_pub.h` (TransportNet 基类)
- `src/framework/next/comms/endpoint_pairs/channels/channel.h` (Channel 基类)
