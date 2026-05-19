# elecGroupId V2 双路径汇合传导分析

> 分析日期：2026-05-19 | 分支：develop

## 1. 概述

HCOMM V2 通信域初始化有两条路径——自协商（`HcclCommInitRootInfo`）和 RankTable 启动（`HcclCommInitClusterInfo`）。两者以不同的方式构造 `RankTableInfo`，在 `CommunicatorImpl::InitRankGraph(RankTableInfo&)` 处汇合。`elecGroupId` 作为 `RankLevelInfo` 的成员贯穿全链路，最终写入 `RankDesc::elecGroupId`，并在 `ReparseGroupedPlaneForOcsMesh` 中被消费为 OCS 并行平面分组键。

本文档聚焦汇合**之前**：两条路径各自如何把 `elecGroupId` 从数据源传导到汇合点。

## 2. 数据结构基础

```
RankTableInfo
  └─ vector<NewRankInfo> ranks
       └─ vector<RankLevelInfo> rankLevelInfos      // 每 rank 多层网络拓扑
            └─ u32 elecGroupId{0}                   // rank_level_info.h:39
```

`elecGroupId` 是 per-rank per-layer 的属性，语义为"电互联组 ID"。JSON 键名 `"elec_group_id"`，默认值 0。

## 3. 路径 A：自协商（RootInfo）

### 3.1 流程总览

```
构造本地 RankTable (JSON → C++)
  → 序列化为 BinaryStream (elecGroupId 写入字节流)
    → TCP 发送至 Server
      → Server 反序列化 (elecGroupId 从字节流恢复)
        → Server 合并 → 序列化 → 广播
          → Client 反序列化 (elecGroupId 再次恢复)
            → 汇合点
```

两阶段协商中各执行一次（RootInfoDetect + RootInfoUpdate），以下以阶段 1 为例。

### 3.2 阶段 ①：JSON → C++（数据起源）

```
ConstructRankTable()                              rank_info_detect_client.cc:179
  ├─ 读 /etc/hccl_rootinfo.json 或设备查询 TopoAddrInfoGet
  ├─ 得到 JSON（含 "elec_group_id" 字段）
  └─ localRankTable.Deserialize(json)             line 221
       └─ NewRankInfo::Deserialize
            └─ RankLevelInfo::Deserialize          rank_level_info.cc:50
                 elecGroupId = json.value<u32>("elec_group_id", 0);
```

**关键点**：此时 `elecGroupId` 从 JSON 原生值转为 C++ `u32`，存在 `RankLevelInfo` 对象中。

### 3.3 阶段 ②：序列化 → TCP 发送

```
SendLocalRankTable()                              rank_info_detect_client.cc:105
  └─ GetBinStream(true, binaryStream)
       └─ NewRankInfo::GetBinStream
            └─ RankLevelInfo::GetBinStream         rank_level_info.cc:113-116
                 binStream << elecGroupId;         // u32 写入字节流
```

### 3.4 阶段 ③：Server 接收 → 反序列化

```
RankInfoDetectService::ParseRankTable             rank_info_detect_service.cc:222
  └─ RankTableInfo localRankInfo(binStream)
       └─ NewRankInfo(BinaryStream&)
            └─ RankLevelInfo(BinaryStream&)        rank_level_info.cc:89-95
                 binStream >> elecGroupId;         // 从字节流恢复 u32
```

Server 收到的每个 Client 的 `elecGroupId` 保持原值不变。

### 3.5 阶段 ④：Server 合并 → 广播

Server 将各 Client 的本地 RankTable 合并为全局 RankTable，**不修改** `elecGroupId`。然后通过 `BroadcastRankTable` 再次序列化（`GetBinStream << elecGroupId`）广播给所有 Client。

### 3.6 阶段 ⑤：Client 接收 → 汇合

```
RootInfoDetect → rankTable                        op_base_v2.cc:1680
  └─ rankInfoDetectAgent->GetRankTable(rankTable)  line 1640

CommInitRootInfo                                  op_base_v2.cc:1666
  └─ pComm->Init(rankTable)                       line 1706
       └─ HcclCommunicator::Init(RankTableInfo&)   hccl_communicator.cc:55
            └─ CommunicatorImpl::Init(CommParams, RankTableInfo, config)
                                                    communicator_impl.cc:207
                 └─ InitRankGraph(ranktable)        line 215
                      └─ InitRankGraph(RankTableInfo&)   line 1329  ← 汇合点
```

### 3.7 阶段 ⑥：两阶段协商中的第二次传输（RootInfoUpdate）

阶段 1 返回后，各 rank 分配 devicePort，再次进入 `RootInfoUpdate`（op_base_v2.cc:1722），重复 ②→⑤ 的序列化/反序列化往返。第二次传输的 `RankLevelInfo` 中 `elecGroupId` 不变，仅 `devicePort` 等字段更新。

**总结：自协商路径共经历 4 次 BinaryStream 读写（2 次 Client→Server，2 次 Server→Client）。**

## 4. 路径 B：RankTable 启动（ClusterInfo）

### 4.1 流程总览

```
读 JSON 文件 → 字符串（elecGroupId 仍在 JSON 文本中）
  → Init(string) → InitRankGraph(string)
    → JsonParser 解析 JSON → C++（唯一一次从 JSON 读出 elecGroupId）
      → 汇合点
```

全程无网络传输，无 BinaryStream 序列化。

### 4.2 阶段 ①：读 JSON 文件

```
HcclCommInitClusterInfoV2                         op_base_v2.cc:285
  └─ HcomLoadRankTableFileV2(clusterInfo, ranktableM)
                                                    param_check_v2.cc:445
       // 读整个 JSON 文件到 std::string ranktableM
       // 此时 elecGroupId 仍在 JSON 文本中："elec_group_id": N
```

### 4.3 阶段 ②：传入 Init(string)

```
opbasedCommInfoV2.pComm->Init(ranktableM)         op_base_v2.cc:329
  └─ HcclCommunicator::Init(string)               hccl_communicator.cc:50
       └─ CommunicatorImpl::Init(CommParams, string, config)
                                                    communicator_impl.cc:109
            └─ InitRankGraph(ranktableM)            line 117
```

此时 `ranktableM` 仍是原始 JSON 字符串，`elecGroupId` 未被解析。

### 4.4 阶段 ③：JSON → C++（唯一解析点）

```
InitRankGraph(const string &ranktableM)           communicator_impl.cc:1290
  ├─ JsonParser::ParseString(ranktableM, rankTableInfo)   line 1294
  │    └─ RankTableInfo::Deserialize
  │         └─ NewRankInfo::Deserialize
  │              └─ RankLevelInfo::Deserialize     rank_level_info.cc:50
  │                   elecGroupId = json.value<u32>("elec_group_id", 0);
  │
  └─ InitRankGraph(rankTableInfo)                  line 1295
       └─ InitRankGraph(RankTableInfo&)             line 1329  ← 汇合点
```

**关键点**：`RankLevelInfo::Deserialize` 在 line 50 的调用，是 ClusterInfo 路径唯一一次把 `"elec_group_id"` JSON 字段转为 C++ `u32`。

## 5. 双路径对比

| 维度 | RootInfo 自协商 | ClusterInfo RankTable |
|------|----------------|----------------------|
| **数据源** | 设备查询 或 `/etc/hccl_rootinfo.json` | ranktable JSON 文件 |
| **JSON→C++ 解析点** | `ConstructRankTable` (client, 本地) | `InitRankGraph(string)` (内部) |
| **BinaryStream 序列化次数** | 4 次（两趟来回） | 0 次 |
| **网络传输** | TCP Client↔Server 两阶段 | 无 |
| **elecGroupId 到达汇合点途径** | `RankTableInfo` 对象直接传入 `Init(RankTableInfo)` | JSON 字符串传入 `Init(string)`，内部解析后转入 `InitRankGraph(RankTableInfo)` |
| **汇合后路径** | 完全一致 | 完全一致 |

### 5.1 叠合时序图

```
RootInfo 自协商                          ClusterInfo RankTable
═══════════════                          ════════════════════
设备/文件查询                              读 JSON 文件
  │                                         │
  ▼                                         ▼
JSON ──Deserialize──► RankLevelInfo        std::string ranktableM
  │  (elecGroupId: u32)                      │  (含 "elec_group_id": N)
  │                                         │
  ▼                                         ▼
GetBinStream << elecGroupId               Init(string)
  │                                         │
  ▼                                         ▼
TCP → Server                              InitRankGraph(string)
  │                                         │
  ▼                                         ▼
BinaryStream >> elecGroupId              JsonParser::ParseString
  │                                         │
  ▼                                         ▼
合并 → Broadcast                          RankLevelInfo::Deserialize
  │                                       (elecGroupId: u32)
  ▼                                         │
BinaryStream >> elecGroupId                 ▼
  │                                       InitRankGraph(RankTableInfo&)
  ▼                                         │
Init(RankTableInfo)                         │
  │                                         │
  └────────────┬────────────────────────────┘
               ▼
    InitRankGraph(RankTableInfo&)     ← 汇合点
         communicator_impl.cc:1329
               │
               ▼
    BuildRankDescVec (rank_graph.cc:992)
      d.elecGroupId = lv.elecGroupId
               │
               ▼
    ReparseGroupedPlaneForOcsMesh (rank_graph.cc:929)
      按 elecGroupId 分组 → ocsPlaneId / ocsPlaneNum
```

## 6. 关键文件索引

| 文件 | 关键行 | 内容 |
|------|--------|------|
| `src/legacy/framework/topo/rank_info_detect/rank_info_detect_client.cc` | 179-229 | `ConstructRankTable`: 自协商数据起源 |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/rank_level_info.cc` | 35-81 | `Deserialize`: JSON → C++ |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/rank_level_info.cc` | 113-126 | `GetBinStream`: 序列化为字节流 |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/rank_level_info.cc` | 89-111 | `BinaryStream` 构造函数: 反序列化 |
| `src/legacy/framework/entrance/op_base/op_base_v2.cc` | 1666-1752 | `CommInitRootInfo`: RootInfo 编排 |
| `src/legacy/framework/entrance/op_base/op_base_v2.cc` | 285-366 | `HcclCommInitClusterInfoV2`: ClusterInfo 编排 |
| `src/legacy/framework/communicator/communicator_impl.cc` | 109-134 | `Init(CommParams, string, config)`: ClusterInfo 入口 |
| `src/legacy/framework/communicator/communicator_impl.cc` | 207-232 | `Init(CommParams, RankTableInfo, config)`: RootInfo 入口 |
| `src/legacy/framework/communicator/communicator_impl.cc` | 1290-1296 | `InitRankGraph(string)`: JSON→C++ 解析，**桥接到汇合点** |
| `src/legacy/framework/communicator/communicator_impl.cc` | 1329 | **汇合点** `InitRankGraph(RankTableInfo&)` |

## 7. 结论

1. **`elecGroupId` 是静态属性**。它在两条路径中均从拓扑配置 JSON 的 `"elec_group_id"` 字段读取，自始至终不被修改，仅作为纯数据负载从源头传导到消费点。

2. **两条路径的核心差异不在于值的计算，而在于获取方式**：
   - 自协商路径通过 TCP 两阶段协商，将各 rank 本地拓扑汇聚为全局视图。`elecGroupId` 随之经历 4 次 BinaryStream 序列化/反序列化。
   - RankTable 路径直接解析预配置的全局 JSON，无需协商，`elecGroupId` 仅经历 1 次 JSON 解析（`RankLevelInfo::Deserialize`）。

3. **汇合点**在 `CommunicatorImpl::InitRankGraph(const RankTableInfo&)` (communicator_impl.cc:1329)。ClusterInfo 路径的 `InitRankGraph(string)` 重载（line 1290）是到达此汇合点的最后一座桥梁——它在内部执行 `JsonParser::ParseString` 后立即调用汇合点重载。

4. **汇合后**，`elecGroupId` 仅在 `BuildRankDescVec`（rank_graph.cc:992）中当 `netType == OCS_MESH` 时写入 `RankDesc`，并最终作为 `ReparseGroupedPlaneForOcsMesh` 的分组键，推导 `ocsPlaneId`/`ocsPlaneNum`。

## 8. 子通信域（SubComm）的 elecGroupId 获取路径

子通信域不从 JSON 重新解析，不进行自协商，也不从设备查询。它**直接从父通信域的 `RankTableInfo` 继承**，通过全局索引映射完成子集过滤。整个过程发生在 `CreateSubComm` 中，子域 Init 之前即已完成。

### 8.1 父域保存 RankTableInfo

父通信域在 `InitRankGraph(RankTableInfo&)` 中将 `RankTableInfo` 保存为成员（`communicator_impl.cc:1334`）：

```cpp
void CommunicatorImpl::InitRankGraph(const RankTableInfo &ranktable) {
    // ...
    ranktableInfo = rankGraphBuilder.GetRankTableInfo();  // unique_ptr<RankTableInfo>
    // ...
}
```

该成员在父域生命周期内持有完整的全局 `RankTableInfo`（含所有 rank 的全部 `RankLevelInfo.elecGroupId`），是实现子域继承的关键。

### 8.2 子域创建入口

```
HcclCommunicator::CreateSubComm(CommParams, vector<u32> rankIds, shared_ptr<HcclCommunicator>)
    hccl_communicator.cc:60
  └─ pimpl->CreateSubComm(subCommParams, rankIds, subHcclComm->GetCommImpl())
       communicator_impl.cc:324
```

### 8.3 CreateSubComm 核心逻辑

```
CommunicatorImpl::CreateSubComm(CommParams, vector<u32> rankIds, CommunicatorImpl *subCommImpl)
    communicator_impl.cc:324
  │
  ├─ subRankGraph = rankGraph->CreateSubRankGraph(rankIds)              // line 330
  │     // 从父域 RankGraph 裁剪拓扑：只保留 rankIds 中的 Peer 和 NetInstance 连接
  │     // elecGroupId 尚未进入子域 RankDesc
  │
  ├─ if (ranktableInfo != nullptr)
  │       subRankGraph->BuildRankDescVec(*ranktableInfo, &rankIds);     // line 331-333
  │         │                              │                │
  │         │  父域的全局 RankTableInfo      │                rankIds 作为 globalRankIds 指针
  │         │  (所有 rank 的 elecGroupId)    │
  │         │                              │
  │         └── 内部映射逻辑 (rank_graph.cc:973-992):
  │               for i in [0, rankIds.size()):
  │                 globalIdx = rankIds[i]                               // 子域本地 i → 父域全局 rankId
  │                 rankInfo  = ranktableInfo.ranks[globalIdx]            // 从父域 RankTableInfo 取值
  │                 for (auto &lv : rankInfo.rankLevelInfos):
  │                   if (lv.netType == OCS_MESH)
  │                     d.elecGroupId = lv.elecGroupId                   // line 992: 同源复制
  │
  └─ subCommImpl->Init(subCommParams, subRankGraph, devLogicId)         // line 335
```

### 8.4 子域 Init 路径

子域走 `Init(CommParams, unique_ptr<RankGraph>&, DevId)` 重载（`communicator_impl.cc:234`），与父域的两条路径不同：

```
CommunicatorImpl::Init(CommParams, unique_ptr<RankGraph>&, DevId)
    communicator_impl.cc:234
  └─ InitRankGraph(inputRankGraph)             // line 241
       └─ rankGraph = std::move(inputRankGraph) // 直接接管已构建好的子域 RankGraph
       └─ CheckRankGraph()
       └─ SaveTopoDesc(id)
```

**关键差异**：
- 子域**不走** `InitRankGraph(RankTableInfo&)` 重载，不会重新构建 RankGraph。
- 子域的 `ranktableInfo` 成员保持 `nullptr`，不保存 RankTableInfo。
- `BuildRankDescVec` 已在 `CreateSubComm` 中提前完成，子域 RankDesc 在 Init 之前已就绪。

### 8.5 数据流总结

```
父域 Init
  │
  ├─ ranktableInfo = GetRankTableInfo()        // 全局 RankTableInfo，含所有 rank elecGroupId
  ├─ rankGraph->BuildRankDescVec(*ranktable_)  // 父域 rankDescVec_ 写入
  │
  └─ CreateSubComm(rankIds)
       │
       ├─ CreateSubRankGraph(rankIds)          // 裁剪拓扑图
       │
       ├─ BuildRankDescVec(*ranktableInfo, &rankIds)
       │    使用父域 RankTableInfo + rankIds 做全局索引映射
       │    子域 rankDescVec_ 写入，elecGroupId 与父域同源
       │
       └─ Init(subRankGraph)                   // 子域直接接管，无需 RankTableInfo
```

### 8.6 结论

1. **子域 elecGroupId 是父域的真子集**。值和父域对应 rank 完全一致，因为读取的是同一个 `RankTableInfo.ranks[globalIdx].rankLevelInfos[OCS_MESH].elecGroupId`。

2. **不需要 RankTableInfo 的持久化**。`BuildRankDescVec` 是唯一消费 `RankTableInfo` 中 `elecGroupId` 的环节，执行完毕后子域不再需要 `RankTableInfo`，因此子域不保存它。

3. **与父域两条初始化路径无关**。无论父域是自协商还是 RankTable 启动，只要父域的 `ranktableInfo` 已构建（含 `elecGroupId`），子域就能通过此机制透明继承，不受父域初始化方式影响。
