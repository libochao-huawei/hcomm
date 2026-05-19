# rankDescVec_ 读写路径与架构分析

> 分析日期：2026-05-19 | 分支：develop

## 1. 概述

`rankDescVec_`（`std::vector<RankDesc>`）是 `Hccl::RankGraph` 的成员，在通信域初始化时一次性批量构建全部 8 字段，通过 6 层包装链路零拷贝暴露给 C API `HcclGetRankDescList`。本文档覆盖写入路径、读取路径、调用上下文、新旧实现对比及发现的潜在问题。

## 2. 整体架构

### 2.1 两个同名的 RankGraph 类

项目存在两个同名但不同命名空间的 `RankGraph` 类，极易混淆：

| 类 | 命名空间 | 文件 | 角色 |
|----|---------|------|------|
| `hccl::RankGraph` | hccl | `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_base.h` | V2 抽象基类，定义虚函数接口 |
| `Hccl::RankGraph` | Hccl | `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h` | 实际数据源，持有 `rankDescVec_` |

- `hccl::RankGraph`：接口层，定义"能做什么"。`GetRankDescList` 默认为非纯虚函数（返回 `HCCL_E_NOT_SUPPORT`），保证向后兼容。
- `Hccl::RankGraph`：数据层，实际持有 `std::vector<RankDesc> rankDescVec_` 并执行读写。

### 2.2 所有权链

```
hcclComm
  └─ std::unique_ptr<CollComm> collComm_
      └─ std::unique_ptr<hccl::RankGraph> rankgraph_      // 实际类型 RankGraphV2
          └─ std::unique_ptr<Hccl::IRankGraph> pImpl
              └─ void* rankGraphPtr_                       // 指向 Hccl::RankGraph
                  └─ std::vector<RankDesc> rankDescVec_    // 实际数据
```

`rankGraphPtr_` 为 `void*` 型裸指针：`hccl::RankGraph`（接口层）不能直接持有 `Hccl::RankGraph`（实现层）指针，中间通过 `RankGraphV2`（pImpl 模式）适配。

### 2.3 RankDesc 结构体

```cpp
// include/hccl/hccl_rank_graph.h
#define RANK_DESC_MAX_NET_LAYER 8

typedef struct {
    uint32_t localId;                              // 物理端口 ID
    char     superPodId[128];                      // 超节点标识
    uint32_t serverIdx;                            // Server 自然顺序索引
    uint32_t elecGroupId;                          // 电互联组 ID
    uint32_t ocsPlaneId;                           // OCS 并行平面 ID
    uint32_t ocsPlaneNum;                          // OCS 并行平面总数
    uint32_t netLayerNum;                          // 网络层级数量
    uint32_t netLayers[RANK_DESC_MAX_NET_LAYER];   // 网络层级列表
} RankDesc;
```

## 3. 写入路径

### 3.1 BuildRankGraph 10 阶段

`BuildRankGraph()` 位于 `rank_graph_builder.cc:587`，按序执行 10 个阶段：

```
[1] make_unique<RankGraph>(myRank_)
[2] CheckMyRankInRankTable()
[3] BuildFromRankTable()               ← 创建 Peer/NetInstance，填充 localId/netLayers
[4] BuildPeer2PeerLinks()
[5] UpdateRankGraph()
[6] UpdateTopoInstForMyRankOnly()
[7] DetourService::InsertDetourLinks()
[8] rankGraph_->BuildRankDescVec()     ← ★ 核心写入点
[9] SetEndpointDesc()
[10] InitFinish()
```

BuildRankDescVec 必须在 BuildFromRankTable（阶段 3）之后执行（依赖 peers_ 和 netLayers_），在 InitFinish（阶段 10）之前完成（rankDescVec_ 是 RankGraph 最终状态的一部分）。

### 3.2 BuildRankDescVec 实现

```cpp
// rank_graph.cc:959-1017
void RankGraph::BuildRankDescVec(const RankTableInfo &rankTable,
                                 const std::vector<u32> *globalRankIds)
{
    // 1. 确定大小
    size_t rankCount = (globalRankIds == nullptr) ? rankTable.ranks.size()
                                                  : globalRankIds->size();
    // 2. 溢出检查
    if (rankCount > UINT32_MAX) { THROW(...); }
    u32 count = static_cast<u32>(rankCount);

    // 3. 分配 + 零初始化
    rankDescVec_.assign(count, RankDesc{});

    // 4. 逐 rank 填充
    for (u32 i = 0; i < count; i++) {
        RankDesc &d = rankDescVec_[i];
        u32 globalIdx = (globalRankIds == nullptr) ? i : (*globalRankIds)[i];

        const auto &rankInfo = rankTable.ranks[globalIdx];

        d.localId   = GetLocalId(i);          // Peer 成员
        d.serverIdx = rankInfo.serverIdx;     // parser 按 serverId 出现顺序自增 0,1,2...

        // elecGroupId & superPodId：遍历层级信息
        for (const auto &lv : rankInfo.rankLevelInfos) {
            if (lv.netType == NetType::OCS_MESH) d.elecGroupId = lv.elecGroupId;
            if (lv.netType == NetType::CLOS)     strncpy_s(d.superPodId, ...);
        }

        // netLayers：Peer 中预先累积的网络层集合
        std::set<u32> levels = GetLevels(i);
        d.netLayerNum = 0;
        for (u32 lv : levels) {
            if (d.netLayerNum < RANK_DESC_MAX_NET_LAYER)
                d.netLayers[d.netLayerNum++] = lv;
        }
    }

    // 5. 末尾调用 Reparse 填充 ocsPlaneId/ocsPlaneNum
    ReparseGroupedPlaneForOcsMesh(rankTable, globalRankIds);
}
```

### 3.3 各字段数据来源

| 字段 | 来源 | 数据路径 | 填充阶段 |
|------|------|---------|---------|
| `localId` | `Peer::localId_` | JSON `"local_id"` → NewRankInfo.localId → Peer 构造 → GetLocalId() | BuildFromRankTable |
| `serverIdx` | `NewRankInfo.serverIdx` | TopoInfoRanktableParser::GenerateServerIdx() 按 serverId 首次出现顺序自增分配 | JSON 解析 |
| `elecGroupId` | `RankLevelInfo.elecGroupId` | rankTable JSON `"elec_group_id"`（netType==OCS_MESH） | JSON 解析 |
| `superPodId` | `RankLevelInfo.netInstId` | rankTable JSON `"net_instance_id"`（netType==CLOS），strncpy_s 安全拷贝 | JSON 解析 |
| `netLayers` | `Peer::netLayers_` | AddNetInstance() 逐层 insert → GetLevels() 返回 set → 转数组 | BuildFromRankTable |
| `ocsPlaneId` | ReparseGroupedPlaneForOcsMesh | OCS_MESH 实例内按 elecGroupId 分组后的组内索引（0,1,2...） | BuildRankDescVec 末尾 |
| `ocsPlaneNum` | ReparseGroupedPlaneForOcsMesh | 同上分组的总组数 | BuildRankDescVec 末尾 |

### 3.4 ReparseGroupedPlaneForOcsMesh

`rank_graph.cc:885-957`。遍历 `netInsts_` 中所有 NetInstance，仅处理 `NetType::OCS_MESH`：

1. 对每个 OCS_MESH 实例获取其 rankIds
2. 映射链路：`rankId → globalRankId → rankTable.ranks[globalRankId].rankLevelInfos`，提取 OCS_MESH 层级的 `elecGroupId`
3. 按 `elecGroupId` 分组
4. `planeIdx` 从 0 递增，组内每个 rankId 设置 `ocsPlaneId = planeIdx`，`ocsPlaneNum = totalGroups`

边界检查层次：`rankId 合法性 → globalRankIds 边界 → rankTable 边界 → rankDescVec_ 边界`。

防御性设计：如果 `rankDescVec_` 为空（Reparse 被独立调用而非经过 BuildRankDescVec），先 `assign` 补齐——但正常路径不会触发此分支。

## 4. 读取路径

### 4.1 6 层调用链路

```
HcclGetRankDescList(comm, &descList, &descNum)         // C API 入口
  │
  ├─ [V2 路径] HCCLV2_FUNC_RUN 宏
  │   hcclComm->GetCollComm()->GetRankGraph()            // hccl::RankGraph* (= RankGraphV2)
  │     → [虚函数] rankGraph->GetRankDescList(descList, descNum)
  │       → RankGraphV2::GetRankDescList()               // pImpl 委托
  │         → IRankGraph::GetRankDescList()              // void* 还原 + const_cast
  │           → Hccl::RankGraph::GetRankDescVec()        // 返回 const vector<RankDesc>&
  │             → const_cast<RankDesc*>(vec.data())
  │             → *descNum = vec.size()
  │
  └─ [V1 路径] return HCCL_E_NOT_SUPPORT
```

### 4.2 逐层职责

| 层 | 类/函数 | 职责 |
|----|---------|------|
| C API | `HcclGetRankDescList` | 参数校验 + V2/V1 分发 |
| V2 分派 | `HCCLV2_FUNC_RUN` 宏 | 运行时通过 `hrtGetHcclV2Support()` 判断设备是否支持 V2 |
| 虚基类 | `hccl::RankGraph` | `GetRankDescList` 为非纯虚，默认返回 `HCCL_E_NOT_SUPPORT` |
| V2 包装 | `RankGraphV2` | pImpl 模式，委托给 `Hccl::IRankGraph` |
| 接口适配 | `Hccl::IRankGraph` | `void*` → `Hccl::RankGraph*` 还原 + `const_cast` 桥接 |
| 数据源 | `Hccl::RankGraph` | 持有 `rankDescVec_`，通过 `GetRankDescVec()` 返回 const 引用 |

### 4.3 与兄弟 API 的关键差异

兄弟 API（`GetNetLayers`、`GetInstRanksByNetLayer`、`GetInstSizeListByNetLayer`、`GetLinks`）均在 IRankGraph 中持有本地缓存 vector，每次调用时 `clear()` → 计算 → 拷贝 → 返回 `data()`。

`GetRankDescList` 不同——IRankGraph 中没有对应的缓存成员，直接返回底层 `rankDescVec_.data()`：

| 对比维度 | 兄弟 API（拷贝路径） | GetRankDescList（直通路径） |
|---------|---------------------|---------------------------|
| IRankGraph 本地缓存 | 有（netLayersVec_ 等） | **无** |
| 数据所有权 | IRankGraph 临时代理 | 直指 Hccl::RankGraph |
| 每次调用开销 | 重新计算 + 拷贝 | 零分配，零拷贝 |
| const_cast | 不需要 | **需要**（数据源返回 const） |
| 指针稳定性 | 下次调用可能失效（clear） | 通信域存活期间始终有效 |

**为什么兄弟 API 需要拷贝？** 它们的查询结果需运行时计算组合（GetLinks 构建 CommLink 结构体，GetNetLayers 从 set 转 vector），数据不预存于现成 vector。

**为什么 GetRankDescList 不需要拷贝？** `rankDescVec_` 在 `BuildRankDescVec()` 中预先构建为完整连续 vector，不需要运行时组装。且 RankDesc 含 128 字节 `superPodId`，拷贝成本不可忽略。

### 4.4 const_cast 分析

`GetRankDescVec()` 返回 `const std::vector<RankDesc>&`——合理，从读取角度不应修改内部数据。但 C API 签名要求 `RankDesc**`（非 const）。`const_cast` 是必需的桥接，隔离在 IRankGraph 层内部。

风险由文档契约控制：`hccl_rank_graph.h` 明确声明"内存由库内管理，调用者严禁释放"。调用者通过非 const 指针写入属于未定义行为，但属于调用者错误使用。

### 4.5 HCCLV2_FUNC_RUN 控制流

```cpp
// param_check_basic_v2.h:18-25
#define HCCLV2_FUNC_RUN(func, ...) \
    do { \
        bool isSupportV2 = false; \
        CHK_RET(hrtGetHcclV2Support(&isSupportV2)); \
        if (isSupportV2) { return func; } \
    } while (0)
```

V2 支持时执行 lambda 并**直接 return**（不再执行 V1 fallback）。V1 代码仅在设备不支持 V2 时执行。AICPU（device 侧）编译时该宏定义为空，直接走 V1 fallback。

## 5. 调用上下文与生命周期

### 5.1 完整触发时序

```
通信域初始化（主通信域）
  HcclCommInitClusterInfo / 入口 API
    → CommunicatorImpl::Init()
      → InitRankGraph(ranktable)
        → RankGraphBuilder::Build()
          → BuildRankGraph()
            → BuildFromRankTable()          // Phase 3
            → BuildPeer2PeerLinks()         // Phase 4
            → ...                           // Phase 5-7
            → BuildRankDescVec()            // Phase 8 ★
            → SetEndpointDesc()             // Phase 9
            → InitFinish()                  // Phase 10

子通信域创建
  HcclCreateSubCommConfigV2()
    → CommunicatorImpl::CreateSubComm()
      → CreateSubRankGraph(rankIds)         // 新建独立 Hccl::RankGraph
      → BuildRankDescVec(*table, &rankIds)  // ★ 子通信域独立填充
      → subCommImpl->Init()

外部查询（任意时机）
  HcclGetRankDescList(comm, ...)
    → 返回 rankDescVec_.data()             // 零拷贝，直通
```

### 5.2 主通信域 vs 子通信域

| 属性 | 主通信域 | 子通信域 |
|------|----------|----------|
| `globalRankIds` | `nullptr` | `&rankIds` |
| `rankDescVec_` 大小 | `rankTable.ranks.size()` | `rankIds.size()` |
| 索引语义 | `rankDescVec_[i]` ⟷ `rankTable.ranks[i]` | `rankDescVec_[i]` ⟷ `rankTable.ranks[rankIds[i]]` |
| 数据内容 | 与全局 rankTable 一致 | 与全局对应 rank 的数据一致 |
| 独立性 | 独立 vector | **独立 vector**（完整副本，不共享） |

### 5.3 生命周期状态机

```
[不存在] ──BuildRankGraph()──→ [填充中] ──InitFinish()──→ [有效]
                                                              │
                              CreateSubComm ───→ 子通信域 [有效]（独立副本）
                              DestroyComm ─────→ [已销毁]
```

返回给调用者的指针在通信域存活期间始终有效。文档警告"重复调用可能使前次结果失效"为防御性措辞——当前实现中重复调用返回同一有效指针。

### 5.4 V1 为什么不支持

V1 通信域使用 `RankGraphV1`，直接继承 `hccl::RankGraph`。基类 `GetRankDescList` 默认返回 `HCCL_E_NOT_SUPPORT`，V1 无 override。V1 的 `RankGraphV1` 不持有 `Hccl::RankGraph`（legacy），无等价数据结构可返回全量 RankDesc 数组。

## 6. 新旧实现对比

### 6.1 核心差异

| 维度 | 旧 `cachedRankDesc_` | 新 `rankDescVec_` |
|------|---------------------|-------------------|
| 存储模型 | `hcclComm` 内缓存 **1 条** | `RankGraph` 内存储 **全部 rank** |
| 返回数量 | `*descNum = 1` | `*descNum = vec.size()` |
| 构建时机 | 每次 `HcclGetRankDescList` 调用时动态查询 | `BuildRankGraph` / `CreateSubComm` 时一次性批量构建 |
| 数据查询 | 运行时调 `GetNetLayers`/`GetOcsPlaneId`/`GetOcsPlaneNum` | 直接从 rankTable 和 Peer 预填字段 |
| 线程安全 | 更早的 `g_cachedRankDesc` 全局变量有数据竞争；per-comm 版本已安全 | 安全（每通信域独立 vector） |

### 6.2 演变历史

```
408fa94e  feat(api): implement HcclGetRankDescList V2 path with netLayers
80adb943  feat(api): wire ocsPlaneId/ocsPlaneNum via RankGraph path
61ed0849  feat(api): replace global g_cachedRankDesc with per-comm cachedRankDesc_
ddc60364  feat(api): simplify HcclGetRankDescList to return full rankDescVec_
b8b858e3  fix(api): expose rank desc list through RankGraph wrapper (proper V2 chain)
```

## 7. 潜在问题

### 7.1 RecoverSubComm 缺少 BuildRankDescVec 调用

`CreateSubComm`（两处重载，`communicator_impl.cc:330-332` 和 `352-356`）均调用了 `BuildRankDescVec`，但 `RecoverSubComm`（快照恢复路径，`communicator_impl.cc:2278-2316`）没有：

```
CreateSubComm:   CreateSubRankGraph → BuildRankDescVec → Init  ✅
RecoverSubComm:  CreateSubRankGraph → Init                    ⚠️
```

恢复后的子通信域 `rankDescVec_` 为空（size = 0），调用 `HcclGetRankDescList` 将返回 `descNum=0`，调用者行为未定义。

**建议**：在 `RecoverSubComm` 中添加与 `CreateSubComm` 一致的 `BuildRankDescVec` 调用。

### 7.2 NetType 多层级覆盖

`BuildRankDescVec` 中 `elecGroupId` 和 `superPodId` 采用"后值覆盖"策略——遍历所有 `rankLevelInfos`，最后匹配到的值胜出。若未来出现一个 rank 属于多个同类型（OCS_MESH/CLOS）层级，仅最后一个生效。

**当前影响**：低。实际部署中一个 rank 通常只有一个 OCS_MESH 层级和一个 CLOS 层级。

## 8. 相关文件索引

| 文件 | 角色 |
|------|------|
| `include/hccl/hccl_rank_graph.h` | RankDesc 结构体定义 + C API 声明 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h` | Hccl::RankGraph 声明，包含 rankDescVec_ / BuildRankDescVec / GetRankDescVec |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc` | BuildRankDescVec + ReparseGroupedPlaneForOcsMesh 实现 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc` | BuildRankGraph() 调用 BuildRankDescVec |
| `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_base.h` | hccl::RankGraph 基类，GetRankDescList 虚函数 |
| `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.h/.cc` | RankGraphV2，pImpl 委托 |
| `src/legacy/interface/rank_graph_interface.h/.cc` | IRankGraph，void* + const_cast 桥接 |
| `src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc` | C API HcclGetRankDescList 实现 |
| `src/framework/next/coll_comms/communicator/coll_comm.h` | CollComm，持有 rankgraph_ |
| `src/legacy/framework/communicator/communicator_impl.cc` | CreateSubComm / RecoverSubComm |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/new_rank_info.h` | NewRankInfo 结构体（serverIdx 等） |
