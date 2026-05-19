# RankTableInfo 到 RankDesc 传导分析

> 分析日期：2026-05-19 | 分支：develop

## 1. 概述

`RankTableInfo`（汇合数据结构，含所有 rank 的 `RankLevelInfo` 及 `elecGroupId`）在通信域初始化流程的汇合点 `InitRankGraph(RankTableInfo&)` 之后，经由 `BuildRankDescVec` 一次性转化为 `std::vector<RankDesc>`（`rankDescVec_`）。此过程对父域、子域完全共用，唯一的实质性差异是参数 `globalRankIds`：父域为 `nullptr`，子域为 `&rankIds`。

## 2. 数据结构

### 2.1 源端

```
RankTableInfo                                  rank_table_info.h:24
  └─ vector<NewRankInfo> ranks                 new_rank_info.h:28
       ├─ u32 rankId, deviceId, localId, serverIdx
       └─ vector<RankLevelInfo> rankLevelInfos  rank_level_info.h:32
            ├─ u32 netLayer
            ├─ string netInstId
            ├─ NetType netType                 // CLOS, OCS_MESH, MESH_1D, ...
            ├─ u32 elecGroupId                 // 电互联组 ID，JSON 键 "elec_group_id"
            └─ vector<AddressInfo> rankAddrs
```

### 2.2 目标端

```
RankDesc                                        hccl_rank_graph.h:387-396
  ├─ u32 localId        // 物理端口 ID
  ├─ char superPodId[128] // 超节点标识（CLOS 层 netInstId）
  ├─ u32 serverIdx      // Server 自然顺序索引
  ├─ u32 elecGroupId    // 电互联组 ID（OCS_MESH 层）  ★
  ├─ u32 ocsPlaneId     // OCS 并行平面 ID（分组推导）  ★
  ├─ u32 ocsPlaneNum    // OCS 并行平面总数（分组推导） ★
  ├─ u32 netLayerNum    // 通信层级数量
  └─ u32 netLayers[8]   // 通信层级列表
```

标 ★ 的三个字段是本文关注点。

## 3. 调用点

### 3.1 BuildRankDescVec（3 个调用点）

| # | 文件:行号 | 调用上下文 | globalRankIds | 场景 |
|---|----------|-----------|:---:|------|
| 1 | `rank_graph_builder.cc:610` | `RankGraphBuilder::BuildRankGraph()` | `nullptr` | 父域初始化 |
| 2 | `communicator_impl.cc:332` | `CreateSubComm` 无 config | `&rankIds` | 子域初始化 |
| 3 | `communicator_impl.cc:355` | `CreateSubComm` 带 config | `&rankIds` | 子域初始化 |

### 3.2 ReparseGroupedPlaneForOcsMesh（1 个调用点）

| # | 文件:行号 | 上下文 |
|---|----------|--------|
| 1 | `rank_graph.cc:1016` | `BuildRankDescVec` 的最后一行 |

无其他调用入口。

### 3.3 调用链

```
父域:
  InitRankGraph(RankTableInfo&)                  ← 汇合点
    └─ RankGraphBuilder::Build(ranktable, ...)
         └─ BuildRankGraph()
              ├─ BuildFromRankTable()            // 不涉及 RankDesc
              ├─ BuildPeer2PeerLinks()           // 不涉及 RankDesc
              └─ BuildRankDescVec(*rankTable_)   // line 610, globalRankIds=nullptr
                   ├─ 填充 8 字段
                   └─ ReparseGroupedPlaneForOcsMesh()

子域:
  CreateSubComm(..., rankIds, ...)
    ├─ CreateSubRankGraph(rankIds)               // 裁剪拓扑图
    ├─ BuildRankDescVec(*ranktableInfo, &rankIds) // line 332, globalRankIds=&rankIds
    │    ├─ 填充 8 字段: globalIdx = rankIds[i]
    │    └─ ReparseGroupedPlaneForOcsMesh()
    └─ Init(subRankGraph, ...)                   // 直接接管
```

## 4. BuildRankDescVec 逐段分析

**文件**: `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc:959-1017`

**函数签名**:
```cpp
void BuildRankDescVec(const RankTableInfo &rankTable,
                      const std::vector<u32> *globalRankIds)
```

### 4.1 分配 rankDescVec_（line 969）

```cpp
size_t rankCount = (globalRankIds == nullptr) ? rankTable.ranks.size()
                                              : globalRankIds->size();
rankDescVec_.assign(count, RankDesc{});
```

无条件全量覆盖分配。`RankDesc{}` 零值初始化：`elecGroupId=0`，`ocsPlaneId=0`，`ocsPlaneNum=0`。

### 4.2 全局索引映射（line 973）

```cpp
u32 globalIdx = (globalRankIds == nullptr) ? i : (*globalRankIds)[i];
```

| 场景 | globalRankIds | globalIdx 含义 |
|------|:---:|------|
| 父域 | `nullptr` | `i`，即 rankDescVec_ 索引 = 全局 rankId |
| 子域 | `&rankIds` | `rankIds[i]`，子域局部 i 映射到父域全局 rankId |

### 4.3 填充 elecGroupId（line 990-992）

```cpp
for (const auto &lv : rankInfo.rankLevelInfos) {
    if (lv.netType == NetType::OCS_MESH) {
        d.elecGroupId = lv.elecGroupId;
    }
}
```

**逻辑**：遍历该 rank 的所有 `RankLevelInfo`，找到 `netType == OCS_MESH` 的层级，取其 `elecGroupId` 直拷。若该 rank 不存在 OCS_MESH 层级，保持零值。

### 4.4 填充其他字段（line 984-1012）

| 字段 | 行号 | 来源 | 逻辑 |
|------|------|------|------|
| `localId` | 984 | `peers_[i]` | `GetLocalId(i)` 取物理端口 |
| `serverIdx` | 987 | `rankTable.ranks[globalIdx]` | `NewRankInfo::serverIdx` 直拷 |
| `superPodId` | 994-996 | `rankLevelInfos[CLOS].netInstId` | `strncpy_s` 安全拷贝 |
| `netLayerNum` / `netLayers[]` | 1006-1012 | `peers_[i]` | `GetLevels(i)` 返回有序集合 |

### 4.5 调用 ReparseGroupedPlaneForOcsMesh（line 1016）

```cpp
ReparseGroupedPlaneForOcsMesh(rankTable, globalRankIds);
```

`BuildRankDescVec` 的最后一步，计算 `ocsPlaneId` 和 `ocsPlaneNum`。

## 5. ReparseGroupedPlaneForOcsMesh 逐段分析

**文件**: `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc:885-957`

**函数签名**:
```cpp
void ReparseGroupedPlaneForOcsMesh(const RankTableInfo &rankTable,
                                   const std::vector<u32> *globalRankIds)
```

### 5.1 防御性分配（line 888-891）

```cpp
size_t count = (globalRankIds == nullptr) ? rankTable.ranks.size()
                                          : globalRankIds->size();
if (rankDescVec_.empty()) {
    rankDescVec_.assign(count, RankDesc{});
}
```

`BuildRankDescVec` 已在 line 969 无条件分配，此处 `rankDescVec_` 非空，`assign` 不会执行。这是一个防御性保护（独立调用场景预留）。

### 5.2 遍历 OCS_MESH NetInstance（line 893-899）

```cpp
for (const auto &netInstMap : netInsts_) {         // 按 netLayer 索引
    for (const auto &pair : netInstMap) {           // 按 netInstId 索引
        if (netInst->GetNetType() != NetType::OCS_MESH) continue;
```

`netInsts_` 是二维结构 `vector<map<netInstId, shared_ptr<NetInstance>>>`，按 `netLayer` 分组，每层按 `netInstId` 索引。只处理 `NetType::OCS_MESH` 的实例。

### 5.3 按 elecGroupId 分组（line 905-933）

```cpp
std::map<u32, std::vector<RankId>> elecGroups;
for (RankId rankId : netInst->GetRankIds()) {
    // rankId 是子域局部 rankId

    // 映射到全局 rankId
    u32 globalRankId = (globalRankIds == nullptr) ? rankId
                                                  : globalRankIds->at(rankId);

    // 从 RankTableInfo 读取该 rank 的 elecGroupId
    u32 elecGroupId = 0;
    for (const auto &levelInfo : rankTable.ranks[globalRankId].rankLevelInfos) {
        if (levelInfo.netType == NetType::OCS_MESH) {
            elecGroupId = levelInfo.elecGroupId;
            break;
        }
    }
    elecGroups[elecGroupId].push_back(rankId);     // 按 elecGroupId 分组
}
```

关键点：
- 使用 `std::map`，组按 `elecGroupId` 值升序遍历，排序可确定性复现。
- 同一 OCS_MESH NetInstance 内的 rank，按 `elecGroupId` 被分入不同组。

### 5.4 分配 ocsPlaneId / ocsPlaneNum（line 936-954）

```cpp
u32 totalGroups = static_cast<u32>(elecGroups.size()); // 有多少个不同 elecGroupId
u32 planeIdx = 0;
for (const auto &group : elecGroups) {                // map 升序遍历
    for (RankId rankId : group.second) {              // 该组内每个 rank
        rankDescVec_[rankId].ocsPlaneId  = planeIdx;  // 同组共享同一 planeIdx
        rankDescVec_[rankId].ocsPlaneNum = totalGroups; // 同 NetInstance 内统一值
    }
    planeIdx++;
}
```

## 6. 分组语义

在一个 OCS_MESH NetInstance 内：

```
                    OCS_MESH NetInstance
                    ┌──────────────────────┐
                    │  rank 0: elecGroupId=0  │──┐
                    │  rank 1: elecGroupId=0  │──┤ 同组 → ocsPlaneId=0
                    │  rank 2: elecGroupId=0  │──┘
                    │                      │
                    │  rank 3: elecGroupId=3  │──┐
                    │  rank 4: elecGroupId=3  │──┤ 同组 → ocsPlaneId=1
                    │                      │  │
                    │  rank 5: elecGroupId=7  │─── 独组 → ocsPlaneId=2
                    └──────────────────────┘

                    totalGroups = 3 → 所有 rank 的 ocsPlaneNum = 3
```

- 同一 `elecGroupId` → 同一 `ocsPlaneId`，表示这些 rank 能通过同一 OCS 平面直接通信。
- 不同 `elecGroupId` → 不同 `ocsPlaneId`，彼此隔离。
- `ocsPlaneNum` 全 NetInstance 统一，表示总的并行平面数。

## 7. 数据流总图

```
RankTableInfo                              rankDescVec_
═════════════                              ═════════════
                                           
ranks[0] ────────────────────────────────► rankDescVec_[0]
  ├─ serverIdx ──────────────────────────►   .serverIdx
  │                                           
  ├─ rankLevelInfos                           .
  │    ├─ [netType==CLOS].netInstId ─────►   .superPodId
  │    ├─ [netType==OCS_MESH].elecGroupId ─►  .elecGroupId
  │    └─ ...                                 
  │                                           
  └─ (peers_[0]) ────────────────────────►   .localId
                                       ───►   .netLayerNum / .netLayers[]
                                              
ranks[1] ────────────────────────────────► rankDescVec_[1]
  ...                                      ...
                                           
                       ReparseGroupedPlaneForOcsMesh:
                         netInsts_ → OCS_MESH instances
                           → group by elecGroupId
                             → ocsPlaneId / ocsPlaneNum
                                               
                                         ▼   rankDescVec_[0].ocsPlaneId
                                         ▼   rankDescVec_[0].ocsPlaneNum
                                         ▼   rankDescVec_[1].ocsPlaneId
                                         ▼   ...
```

## 8. 父域 vs 子域关键差异

| 维度 | 父域 | 子域 |
|------|------|------|
| 调用点 | `rank_graph_builder.cc:610` | `communicator_impl.cc:332` |
| `rankTable` 来自 | `RankGraphBuilder` 新构建 | 父域 `ranktableInfo` |
| `globalRankIds` | `nullptr` | `&rankIds` |
| `netInsts_` 范围 | 所有 rank 的完整拓扑 | 从父域裁剪的子集 |
| `rankDescVec_` 长度 | 全局 rank 总数 | 子域 rankIds 数量 |
| `ocsPlaneNum` | 全局各 OCS_MESH 实例的组数 | 子域涉及实例的组数（≤ 父域） |

## 9. 关键文件索引

| 文件 | 行号 | 内容 |
|------|------|------|
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc` | 959-1017 | `BuildRankDescVec` 完整实现 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc` | 885-957 | `ReparseGroupedPlaneForOcsMesh` 完整实现 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc` | 1016 | `ReparseGroupedPlaneForOcsMesh` 唯一调用点 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc` | 587-617 | `BuildRankGraph`：父域调用方 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc` | 610 | 父域 `BuildRankDescVec` 调用 |
| `src/legacy/framework/communicator/communicator_impl.cc` | 324-344, 346-368 | `CreateSubComm` 两个重载：子域调用方 |
| `src/legacy/framework/communicator/communicator_impl.cc` | 332, 355 | 子域 `BuildRankDescVec` 调用 |
| `include/hccl/hccl_rank_graph.h` | 387-396 | `RankDesc` 结构体定义 |

## 10. 结论

1. **一次分配，8 字段齐套**。`BuildRankDescVec` 的 `assign` 一次性分配整个 `rankDescVec_`，然后在一个循环中把所有能直接从 `RankTableInfo` 和 `peers_` 取到的字段全部填入（`localId`、`serverIdx`、`elecGroupId`、`superPodId`、`netLayerNum`、`netLayers`）。没有增量追加，没有后续补填。

2. **ocsPlaneId / ocsPlaneNum 延迟到末尾计算**。这两个字段依赖分组逻辑（同一 `elecGroupId` 的所有 rank 形成一组），需要先读完所有 rank 的 `elecGroupId` 才知道有多少组。因此 `ReparseGroupedPlaneForOcsMesh` 只能在 8 字段循环之后执行，作为 `BuildRankDescVec` 的最后一步。

3. **`RankTableInfo` 只在 `BuildRankDescVec` 中被消费**。RankDesc 写入完成后，后续的 `InitCommResource` 各子系统不再读取 `RankTableInfo`——它们直接使用 `rankDescVec_`。这也是为什么子域可以不保存 `ranktableInfo`（设为 `nullptr`）。

4. **父域子域共用同一套逻辑**。唯一的区分点是 `globalRankIds` 参数。主域传 `nullptr`，子域传 `&rankIds`，其余代码路径完全相同。
