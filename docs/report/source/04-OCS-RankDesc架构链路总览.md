# OCS RankDesc 架构链路总览

> 分析日期：2026-05-19 | 分支：develop  
> 范围：整合 `rankDescVec_` 读写路径、`elecGroupId` 双路径传导、`RankTableInfo` 到 `RankDesc` 映射、OCS 新增结构与枚举。

## 1. 概述

OCS RankDesc 链路的本质，是把拓扑配置中的静态网络属性固化为通信域级只读描述表。

`elecGroupId` 从 RankTable JSON 的 `"elec_group_id"` 字段进入 `RankLevelInfo`，经 RootInfo 自协商或 ClusterInfo RankTable 启动到达统一汇合点 `CommunicatorImpl::InitRankGraph(const RankTableInfo&)`。汇合后，`RankGraph::BuildRankDescVec` 将 `RankTableInfo` 与已构建拓扑图中的 Peer 信息一次性合成为 `rankDescVec_`；随后 `ReparseGroupedPlaneForOcsMesh` 以 `elecGroupId` 为分组键推导 `ocsPlaneId` 与 `ocsPlaneNum`。对外查询时，`HcclGetRankDescList` 沿 V2 RankGraph 包装链路零拷贝返回该连续数组。

这条链路有三个关键性质：

| 性质 | 结论 |
|------|------|
| 数据静态 | `elecGroupId` 来源于拓扑配置，链路中只传递、不计算、不改写。 |
| 构建前置 | `rankDescVec_` 在通信域初始化阶段一次性生成，不在查询时临时拼装。 |
| 查询直通 | C API 返回 `rankDescVec_.data()`，内存归通信库所有，调用方只读。 |

## 2. 架构边界

OCS RankDesc 横跨新框架接口层与 legacy 拓扑实现层。理解这条边界，是理解 `rankDescVec_` 生命周期的前提。

| 层次 | 类型/文件 | 职责 |
|------|-----------|------|
| C API | `HcclGetRankDescList` | 参数校验，按 V2 能力分发，向外暴露 `RankDesc* + descNum`。 |
| V2 接口层 | `hccl::RankGraph` | 定义统一 RankGraph 查询接口；默认 `GetRankDescList` 返回不支持。 |
| V2 包装层 | `RankGraphV2` | 通过 pImpl 委托给 legacy `IRankGraph`。 |
| 适配层 | `Hccl::IRankGraph` | 将 `void* rankGraphPtr_` 还原为 `Hccl::RankGraph*`，桥接 const 与 C API 非 const 签名。 |
| 数据层 | `Hccl::RankGraph` | 持有 `std::vector<RankDesc> rankDescVec_`，执行构建、OCS 分组和零拷贝读取。 |

所有权链如下：

```text
hcclComm
  -> CollComm::rankgraph_                 // std::unique_ptr<hccl::RankGraph>
       -> RankGraphV2
            -> Hccl::IRankGraph::rankGraphPtr_
                 -> Hccl::RankGraph::rankDescVec_
```

这里有两个同名 `RankGraph`：

| 类 | 命名空间 | 角色 |
|----|----------|------|
| `hccl::RankGraph` | `hccl` | V2 抽象接口，面向新框架。 |
| `Hccl::RankGraph` | `Hccl` | legacy 拓扑图实现，实际持有 `rankDescVec_`。 |

## 3. 数据主线

### 3.1 双入口：RootInfo 与 ClusterInfo

HCOMM V2 通信域初始化有两条入口路径。两者在汇合前的差异只体现在 `RankTableInfo` 的获得方式上，汇合后完全共用同一条 RankGraph 构建路径。

| 维度 | RootInfo 自协商 | ClusterInfo RankTable |
|------|----------------|------------------------|
| 入口 | `HcclCommInitRootInfo` | `HcclCommInitClusterInfo` |
| 数据源 | 设备查询或 `/etc/hccl_rootinfo.json` | 用户提供的 ranktable JSON 文件 |
| JSON 解析点 | Client 本地构造 `RankTableInfo` 时解析 | `InitRankGraph(string)` 内部解析 |
| 网络传输 | Client/Server 两阶段 TCP 协商 | 无 |
| 序列化 | `RankLevelInfo::GetBinStream` / BinaryStream 构造函数 | 无 BinaryStream |
| 汇合形态 | 直接传入 `Init(RankTableInfo)` | JSON string 解析后转入 `InitRankGraph(RankTableInfo&)` |

`elecGroupId` 在两条路径中的语义一致：它是 `RankLevelInfo` 的普通字段，随 `RankTableInfo` 传递。RootInfo 路径会经历多次 BinaryStream 读写；ClusterInfo 路径只经历一次 JSON 到 C++ 的解析。两者都不重新解释该值。

### 3.2 汇合点

统一汇合点是：

```text
CommunicatorImpl::InitRankGraph(const RankTableInfo&)
  -> RankGraphBuilder::Build(...)
      -> RankGraphBuilder::BuildRankGraph()
          -> BuildFromRankTable()
          -> BuildPeer2PeerLinks()
          -> UpdateRankGraph()
          -> rankGraph_->BuildRankDescVec(*rankTable_)
          -> SetEndpointDesc()
          -> InitFinish()
```

`BuildRankDescVec` 位于 `BuildRankGraph` 的后段：它依赖 `BuildFromRankTable` 已经创建 Peer 与 NetInstance，也必须在 `InitFinish` 前完成，使 `rankDescVec_` 成为通信域最终拓扑状态的一部分。

### 3.3 RankTableInfo 到 RankDesc

`BuildRankDescVec` 的职责是把两类数据压平到连续数组：

| 数据来源 | 进入 `RankDesc` 的内容 |
|----------|------------------------|
| `RankTableInfo::ranks[globalIdx]` | `serverIdx`、`elecGroupId`、`superPodId`。 |
| `RankGraph::peers_[i]` | `localId`、`netLayerNum`、`netLayers[]`。 |
| `RankGraph::netInsts_` | OCS 分组上下文，用于推导 `ocsPlaneId`、`ocsPlaneNum`。 |

核心索引规则只有一个：

```text
globalIdx = (globalRankIds == nullptr) ? i : (*globalRankIds)[i]
```

父域中，`rankDescVec_[i]` 对应全局 `rankTable.ranks[i]`。子域中，`rankDescVec_[i]` 对应父域全局 `rankTable.ranks[rankIds[i]]`。

### 3.4 OCS 平面推导

`BuildRankDescVec` 先填入能逐 rank 直接确定的字段，再尾调 `ReparseGroupedPlaneForOcsMesh` 计算 OCS 平面字段：

```text
遍历 OCS_MESH NetInstance
  -> 取该实例内 rankIds
  -> 映射到 RankTableInfo 中的 globalRankId
  -> 读取 OCS_MESH 层 RankLevelInfo.elecGroupId
  -> 按 elecGroupId 分组
  -> 组序号写入 ocsPlaneId
  -> 组总数写入 ocsPlaneNum
```

分组使用 `std::map<u32, vector<RankId>>`，因此 `ocsPlaneId` 按 `elecGroupId` 升序确定，具备可复现性。同一 OCS_MESH NetInstance 内：

| 条件 | 结果 |
|------|------|
| 相同 `elecGroupId` | 分到同一 OCS 平面，拥有相同 `ocsPlaneId`。 |
| 不同 `elecGroupId` | 分到不同 OCS 平面，`ocsPlaneId` 递增。 |
| 同一 NetInstance | 所有 rank 的 `ocsPlaneNum` 等于该实例内不同 `elecGroupId` 的数量。 |

## 4. RankDesc 字段契约

`RankDesc` 是对外 API 的稳定数据契约。其字段不是同源数据的简单搬运，而是 RankTable、Peer、NetInstance 三类信息的压缩视图。

```cpp
typedef struct {
    uint32_t localId;
    char     superPodId[128];
    uint32_t serverIdx;
    uint32_t elecGroupId;
    uint32_t ocsPlaneId;
    uint32_t ocsPlaneNum;
    uint32_t netLayerNum;
    uint32_t netLayers[RANK_DESC_MAX_NET_LAYER];
} RankDesc;
```

| 字段 | 语义 | 来源 | 填充阶段 |
|------|------|------|----------|
| `localId` | 本 rank 物理端口 ID | `Peer::localId_` | `BuildRankDescVec` |
| `superPodId` | 超节点标识 | `RankLevelInfo.netInstId`，仅 `NetType::CLOS` 层 | `BuildRankDescVec` |
| `serverIdx` | Server 自然顺序索引 | `NewRankInfo::serverIdx` | JSON 解析/拓扑构建阶段生成，`BuildRankDescVec` 直拷 |
| `elecGroupId` | 电互联组 ID | `RankLevelInfo.elecGroupId`，仅 `NetType::OCS_MESH` 层 | `BuildRankDescVec` |
| `ocsPlaneId` | OCS 并行平面 ID | OCS_MESH 实例内按 `elecGroupId` 分组后的组序号 | `ReparseGroupedPlaneForOcsMesh` |
| `ocsPlaneNum` | OCS 并行平面总数 | OCS_MESH 实例内不同 `elecGroupId` 的数量 | `ReparseGroupedPlaneForOcsMesh` |
| `netLayerNum` | 网络层级数量 | `Peer::netLayers_` | `BuildRankDescVec` |
| `netLayers[]` | 网络层级列表 | `Peer::netLayers_` 有序集合 | `BuildRankDescVec` |

字段写入策略是“一次分配、逐 rank 填充、末尾分组修正”。`rankDescVec_.assign(count, RankDesc{})` 先建立零初始化数组；逐 rank 循环填充可直接确定的字段；最后通过 OCS 全局分组补齐 `ocsPlaneId` 与 `ocsPlaneNum`。

## 5. 父域与子域

父域与子域共用 `BuildRankDescVec` 与 `ReparseGroupedPlaneForOcsMesh`，唯一分叉点是 `globalRankIds`。

| 维度 | 父域 | 子域 |
|------|------|------|
| 调用位置 | `RankGraphBuilder::BuildRankGraph` | `CommunicatorImpl::CreateSubComm` |
| `rankTable` 来源 | 当前初始化流程构建出的全局 `RankTableInfo` | 父域保存的 `ranktableInfo` |
| `globalRankIds` | `nullptr` | `&rankIds` |
| `rankDescVec_` 长度 | 全局 rank 数 | 子域 rank 数 |
| 索引语义 | 本地索引等于全局 rankId | 本地索引映射到 `rankIds[i]` |
| 数据独立性 | 父域自有 vector | 子域自有 vector，值来自父域全局 RankTable 投影 |

子域不重新解析 JSON、不自协商、不查询设备拓扑。它在 `CreateSubComm` 中先由父域 RankGraph 裁剪出子图，再用父域保存的 `RankTableInfo` 与 `rankIds` 做子集投影，提前构建自己的 `rankDescVec_`，随后 `Init(subRankGraph)` 直接接管。

因此，子域 `elecGroupId` 是父域对应 rank 的真子集；子域 `ocsPlaneNum` 则按子域裁剪后的 OCS_MESH 实例重新计算，可能小于或等于父域对应实例的平面数。

## 6. 接口与生命周期

### 6.1 查询链路

```text
HcclGetRankDescList(comm, &descList, &descNum)
  -> HCCLV2_FUNC_RUN
      -> CollComm::GetRankGraph()
          -> hccl::RankGraph::GetRankDescList()
              -> RankGraphV2::GetRankDescList()
                  -> IRankGraph::GetRankDescList()
                      -> Hccl::RankGraph::GetRankDescVec()
                          -> rankDescVec_.data()
```

V2 支持时，宏直接返回 V2 查询结果；V1 路径不持有 legacy `Hccl::RankGraph`，基类默认返回 `HCCL_E_NOT_SUPPORT`。

### 6.2 零拷贝契约

`GetRankDescList` 与兄弟查询 API 的差异在于：它不在 `IRankGraph` 中构造临时缓存，而是直接返回 `Hccl::RankGraph::rankDescVec_` 的底层数组。

| 对比项 | 兄弟 API | `GetRankDescList` |
|--------|----------|-------------------|
| 数据准备 | 查询时计算并缓存 | 初始化时已完整构建 |
| 返回内存 | `IRankGraph` 临时 vector | `Hccl::RankGraph::rankDescVec_` |
| 拷贝成本 | 有 | 无 |
| 指针有效期 | 受缓存刷新影响 | 通信域存活期间有效 |
| 写权限 | 不承诺 | C API 签名为非 const，但契约要求调用者只读 |

`GetRankDescVec()` 返回 `const std::vector<RankDesc>&`。由于 C API 形参是 `RankDesc**`，`IRankGraph` 内部使用 `const_cast` 桥接。这不是授权调用方修改内部状态，而是旧 C 接口签名与内部只读模型之间的适配。

## 7. OCS 新增能力索引

OCS 支持由枚举、拓扑实例、字段、映射表和构建门控共同组成。

| 类别 | 关键内容 | 作用 |
|------|----------|------|
| 网络类型 | `NetType::OCS_MESH` | 使 RankTable JSON `"net_type": "OCS_MESH"` 可反序列化，并驱动 NetInstance 创建。 |
| 对外拓扑 | `CommTopo::COMM_TOPO_OCS_MESH` | 对外 API 可识别 OCS 光互联拓扑。 |
| 拓扑实例 | `OcsMeshNetInstance` | 继承 `NetInstance`，实现 Peer -> Fabric -> Peer 双跳路径查找。 |
| RankTable 字段 | `RankLevelInfo::elecGroupId` | OCS 平面分组键。 |
| RankDesc 字段 | `serverIdx`、`elecGroupId`、`ocsPlaneId`、`ocsPlaneNum` | 对外暴露服务器顺序、电互联组和 OCS 平面信息。 |
| 构建函数 | `BuildRankDescVec`、`ReparseGroupedPlaneForOcsMesh` | 固化 RankDesc 并推导 OCS 平面。 |
| API | `HcclGetRankDescList` | 对外返回全量 RankDesc 数组。 |

OCS_MESH 与 CLOS 在 Fabric 建模上具有相似性：均允许交换/光交换中间节点参与路径构造。构建侧因此将部分 CLOS 门控扩展为 `CLOS || OCS_MESH`，同时在物理拓扑校验上允许 OCS_MESH 复用 CLOS 的端口关系。

## 8. 架构风险与建议

### 8.1 RecoverSubComm 未构建 rankDescVec_

`CreateSubComm` 两个重载均在子域 `Init` 前调用：

```text
CreateSubRankGraph(rankIds)
  -> BuildRankDescVec(*ranktableInfo, &rankIds)
  -> Init(subRankGraph)
```

但 `RecoverSubComm` 快照恢复路径只创建并接管子图，未补充 `BuildRankDescVec`。恢复后的子通信域可能出现 `rankDescVec_` 为空，`HcclGetRankDescList` 返回 `descNum = 0` 的风险。

建议：在 `RecoverSubComm` 中补齐与 `CreateSubComm` 一致的 `BuildRankDescVec(*ranktableInfo, &rankIds)` 调用，并增加恢复路径的 API 回归测试。

### 8.2 同类 NetType 多层覆盖

`BuildRankDescVec` 遍历 `rankLevelInfos` 时，对 `elecGroupId` 与 `superPodId` 采用后值覆盖策略：

```text
if OCS_MESH -> d.elecGroupId = lv.elecGroupId
if CLOS     -> d.superPodId  = lv.netInstId
```

当前拓扑假设下，一个 rank 通常只有一个 OCS_MESH 层和一个 CLOS 层，风险较低。但如果未来允许同一 rank 出现多个同类型网络层，当前实现会静默保留最后一个匹配项。

建议：若产品语义要求唯一，应在 RankTable 校验阶段显式约束；若允许多层，应扩展 `RankDesc` 表达能力，而不是依赖覆盖顺序。

### 8.3 C API const 语义不足

`HcclGetRankDescList` 返回 `RankDesc*` 而非 `const RankDesc*`，与内部只读模型不完全一致。当前通过文档约束“库内管理内存、调用者不得释放或修改”控制风险。

建议：保持现有 ABI 不变的前提下，在 API 文档中强化只读契约；如未来允许新增接口，可提供 const 语义更明确的 V2 查询形式。

## 9. 关键文件索引

| 文件 | 关键内容 |
|------|----------|
| `include/hccl/hccl_rank_graph.h` | `COMM_TOPO_OCS_MESH`、`RankDesc`、`HcclGetRankDescList` 声明。 |
| `src/legacy/framework/topo/new_topo_builder/common/topo_common_types.h` | `NetType::OCS_MESH` 定义。 |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/rank_level_info.h/.cc` | `RankLevelInfo::elecGroupId`、JSON 反序列化、BinaryStream 读写。 |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/new_rank_info.h` | `NewRankInfo::serverIdx`。 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h` | `rankDescVec_`、`BuildRankDescVec`、`ReparseGroupedPlaneForOcsMesh`、`GetRankDescVec` 声明。 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc` | `BuildRankDescVec` 与 `ReparseGroupedPlaneForOcsMesh` 实现，OCS_MESH NetInstance 创建。 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/net_instance.h/.cc` | `OcsMeshNetInstance` 与 OCS 双跳路径查找。 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc` | `BuildRankGraph` 阶段编排、`BuildRankDescVec` 父域调用、OCS 构建门控。 |
| `src/legacy/framework/communicator/communicator_impl.cc` | RootInfo/ClusterInfo 汇合、父域保存 `ranktableInfo`、子域构建 `rankDescVec_`。 |
| `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_base.h` | `hccl::RankGraph` V2 抽象接口。 |
| `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.h/.cc` | V2 RankGraph 包装与 pImpl 委托。 |
| `src/legacy/interface/rank_graph_interface.h/.cc` | `IRankGraph` 适配层，零拷贝返回 `rankDescVec_.data()`。 |
| `src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc` | `HcclGetRankDescList` C API 实现。 |

## 10. 结论

OCS RankDesc 设计选择的是“初始化期归一化、运行期零拷贝查询”的模型。它把 RankTable 的拓扑属性、RankGraph 的 Peer 层级信息、OCS_MESH 的平面分组语义集中固化到 `rankDescVec_`，避免在外部 API 查询时重复遍历拓扑图。

该模型的架构收益是边界清晰、查询低成本、父域子域逻辑复用；代价是所有会产生或恢复 RankGraph 的路径都必须显式保证 `rankDescVec_` 完整构建。后续演进应优先守住这一不变量：凡是一个通信域可以对外暴露 RankGraph 查询，它就必须拥有与自身拓扑范围一致的 `rankDescVec_`。
