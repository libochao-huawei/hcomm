# OCS 功能新增数据结构与枚举汇总

> 分析日期：2026-05-19 | 分支：develop | 首 commit `5c9c2a45` → 末 commit `288ab6ef`

## 1. 枚举

### 1.1 NetType::OCS_MESH

**文件**：`src/legacy/framework/topo/new_topo_builder/common/topo_common_types.h:36`

```cpp
MAKE_ENUM(NetType, CLOS, MESH_1D, MESH_2D, A3_SERVER, A2_AX_SERVER, TOPO_FILE_DESC, OCS_MESH)
```

作为第 8 个网络类型，驱动 RankTable JSON `"net_type": "OCS_MESH"` → C++ 反序列化，以及 NetInstance 工厂和 OCS 平面分组逻辑。

### 1.2 CommTopo::COMM_TOPO_OCS_MESH + COMM_TOPO_MAX

**文件**：`include/hccl/hccl_rank_graph.h:46-47`

```cpp
COMM_TOPO_OCS_MESH = 6,   ///< OCS光互联拓扑
COMM_TOPO_MAX = 7
```

对外 API 的通信拓扑类型枚举。`NetType → CommTopo` 映射表中 `{OCS_MESH, COMM_TOPO_OCS_MESH}`。

---

## 2. 类

### 2.1 OcsMeshNetInstance

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_graph/net_instance.h:259-267`

```cpp
class OcsMeshNetInstance : public NetInstance {
public:
    OcsMeshNetInstance(const u32 netLayer, const std::string &netInstId)
        : NetInstance(netLayer, netInstId, NetType::OCS_MESH) {};
    ~OcsMeshNetInstance() override = default;
    std::vector<Path> GetPaths(const RankId srcRankId, const RankId dstRankId) const override;
};
```

继承 `NetInstance`，重写 `GetPaths`（实现在 `net_instance.cc:630`）。OCS 光交换路由逻辑：遍历所有 Fabric，在 vGraph 中查找 `srcPeer → Fabric → dstPeer` 的双跳路径（两段 Link 组成一条 Path），与 CLOS 的交换机转发模型一致。

---

## 3. 数据结构字段

### 3.1 RankDesc 结构体

**文件**：`include/hccl/hccl_rank_graph.h:387-396`

```cpp
#define RANK_DESC_MAX_NET_LAYER 8

typedef struct {
    uint32_t localId;                                 // 本卡物理端口 ID
    char     superPodId[128];                         // 超节点标识（CLOS 层 netInstId）
    uint32_t serverIdx;                               // Server 自然顺序索引     ★ 新增
    uint32_t elecGroupId;                             // 电互联组 ID              ★ 新增
    uint32_t ocsPlaneId;                              // OCS 并行平面 ID          ★ 新增
    uint32_t ocsPlaneNum;                             // OCS 并行平面总数         ★ 新增
    uint32_t netLayerNum;                             // 通信层级数量
    uint32_t netLayers[RANK_DESC_MAX_NET_LAYER];      // 通信层级列表
} RankDesc;
```

`RankGraph` 通过 `std::vector<RankDesc> rankDescVec_` 持有全量 RankDesc 数组。对外 C API `HcclGetRankDescList` 直接返回 `rankDescVec_.data() + size()`，库内管理内存，调用者只读。

### 3.2 RankLevelInfo::elecGroupId

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_table_info/rank_level_info.h:39`

```cpp
u32 elecGroupId{0};
```

- JSON 键名：`"elec_group_id"`，默认值 0
- 反序列化：`rank_level_info.cc:50` — `elecGroupId = json.value<u32>("elec_group_id", 0)`
- BinaryStream 序列化/反序列化：`rank_level_info.cc:95, 116`
- 消费点：仅 `ReparseGroupedPlaneForOcsMesh`（`rank_graph.cc:928-929`），作为 OCS 并行平面分组键

### 3.3 NewRankInfo::serverIdx

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_table_info/new_rank_info.h:38`

```cpp
u32 serverIdx{0};
```

不含 2 个平面字段（`ocsPlaneId/ocsPlaneNum` 仅存于 `RankDesc`）。含 BinaryStream 序列化/反序列化支持。

### 3.4 RankGraph::rankDescVec_

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h:97`

```cpp
std::vector<RankDesc> rankDescVec_;
```

在父域 `BuildRankGraph` 或子域 `CreateSubComm` 中通过 `BuildRankDescVec` 一次性填充，之后通过 `GetRankDescVec()` 对内对外零拷贝访问。

---

## 4. 函数

### 4.1 OcsMeshNetInstance::GetPaths

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_graph/net_instance.cc:630`

双跳路径查找（Peer → Fabric → Peer），结果含 `CheckPortGroupSize` 校验。

### 4.2 RankGraph::BuildRankDescVec

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc:959-1017`

```cpp
void BuildRankDescVec(const RankTableInfo &rankTable,
                      const std::vector<u32> *globalRankIds = nullptr)
```

逐次填充 8 字段：`localId`（Peers）、`serverIdx`（NewRankInfo）、`elecGroupId`（RankLevelInfo，仅 OCS_MESH 层）、`superPodId`（RankLevelInfo，仅 CLOS 层）、`netLayerNum/Levers`（Peers）。最后调用 `ReparseGroupedPlaneForOcsMesh` 计算 `ocsPlaneId/Num`。

调用点：
- 父域：`rank_graph_builder.cc:610` — `globalRankIds = nullptr`
- 子域：`communicator_impl.cc:332,355` — `globalRankIds = &rankIds`

### 4.3 RankGraph::ReparseGroupedPlaneForOcsMesh

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc:885-957`

```cpp
void ReparseGroupedPlaneForOcsMesh(const RankTableInfo &rankTable,
                                   const std::vector<u32> *globalRankIds)
```

仅处理 `NetType::OCS_MESH` 的 NetInstance：

```
遍历 OCS_MESH NetInstance → 获取 rankIds
  → 按 elecGroupId 分组：std::map<u32, vector<RankId>> elecGroups
    → 分配：rankDescVec_[rankId].ocsPlaneId  = planeIdx   // 同 elecGroupId 共享
            rankDescVec_[rankId].ocsPlaneNum = totalGroups // 全实例统一
```

仅被 `BuildRankDescVec` 尾调（`rank_graph.cc:1016`），不独立对外。

### 4.4 RankGraph::GetRankDescVec

**文件**：`src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h:83`

```cpp
const std::vector<RankDesc>& GetRankDescVec() const { return rankDescVec_; }
```

零拷贝返回，供 `IRankGraph::GetRankDescList` 使用。

### 4.5 IRankGraph::GetRankDescList

**文件**：`src/legacy/interface/rank_graph_interface.cc:455-465`

```cpp
HcclResult GetRankDescList(RankDesc **descList, uint32_t *descNum) {
    *descList = const_cast<RankDesc *>(rankGraph->GetRankDescVec().data());
    *descNum = static_cast<uint32_t>(rankGraph->GetRankDescVec().size());
}
```

### 4.6 HcclGetRankDescList

**文件**：`src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc:378-399`

C API，V2 路径通过 `CollComm → RankGraph → GetRankDescList` 返回全量数组；V1 返回 `HCCL_E_NOT_SUPPORT`。

---

## 5. 工厂与门控

| 位置 | 文件:行号 | 修改 |
|------|----------|------|
| `GetOrCreateNetInstance` | `rank_graph.cc:513` | `else if (type == OCS_MESH) → make_shared<OcsMeshNetInstance>` |
| `CreateNetInstance` | `rank_graph_builder.cc:389` | `else if (levelInfo.netType == OCS_MESH) → make_shared<OcsMeshNetInstance>` |
| `NetInstance::AddFabric` | `net_instance.cc:109-110` | 门控 `CLOS` → `CLOS \|\| OCS_MESH` |
| `RankGraphBuilder::AddFabricInfo` | `rank_graph_builder.cc:133-135` | 门控 `CLOS` → `CLOS \|\| OCS_MESH` |
| `BuildFromRankTable` | `rank_graph_builder.cc:298-299` | OCS_MESH 跳过 `CheckNetLayerFromPhyTopo`（复用 CLOS 物理端口） |
| `serverIdx` 计算块 | `rank_graph_builder.cc:330+` | 按 A3_SERVER 层 `netInstId` 首次出现顺序分配 |
| `BuildRankGraph` | `rank_graph_builder.cc:610` | `rankGraph_->BuildRankDescVec(*rankTable_)` |
| `CreateSubComm`（2 重载） | `communicator_impl.cc:332,355` | `subRankGraph->BuildRankDescVec(*ranktableInfo, &rankIds)` |

---

## 6. 映射表条目

| 文件:行号 | 映射 | 条目 |
|-----------|------|------|
| `rank_level_info.cc:32` | `string → NetType` | `{"OCS_MESH", NetType::OCS_MESH}` |
| `communicator_impl.cc:3408` | `NetType → CommTopo` | `{OCS_MESH, COMM_TOPO_OCS_MESH}` |
| `rank_graph_interface.cc:85` | `NetType → CommTopo` | `{OCS_MESH, COMM_TOPO_OCS_MESH}` |

---

## 7. 关键文件索引

| 文件 | 关键内容 |
|------|---------|
| `include/hccl/hccl_rank_graph.h:46-47, 387-407` | `COMM_TOPO_OCS_MESH`, `RankDesc`, `HcclGetRankDescList` |
| `src/legacy/framework/topo/new_topo_builder/common/topo_common_types.h:36` | `NetType::OCS_MESH` |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/rank_level_info.h:39` `cc:50` | `elecGroupId` 声明与反序列化 |
| `src/legacy/framework/topo/new_topo_builder/rank_table_info/new_rank_info.h:38` | `serverIdx` |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/net_instance.h:259-267` `cc:630` | `OcsMeshNetInstance` 类与 `GetPaths` |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h:78-83,97` | `ReparseGroupedPlaneForOcsMesh`, `BuildRankDescVec`, `GetRankDescVec`, `rankDescVec_` |
| `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc:885-1017` | `ReparseGroupedPlaneForOcsMesh` + `BuildRankDescVec` 完整实现 |
| `src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc:330+, 610` | `serverIdx` 计算 + `BuildRankDescVec` 调用 |
| `src/legacy/framework/communicator/communicator_impl.cc:332,355` | 子域 `BuildRankDescVec` 调用 |
| `src/legacy/interface/rank_graph_interface.cc:455-465` | `IRankGraph::GetRankDescList` |
| `src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc:378-399` | `HcclGetRankDescList` C API |
