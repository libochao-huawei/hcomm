---
id: ocs-mesh-hierarchy/2026-05-18-ocs-rankdesc-full-list
type: design
status: draft
created: 2026-05-18
---

# SPEC: HcclGetRankDescList 全量返回 & 存储下沉

## 一、元数据

| 项目 | 说明 |
|------|------|
| **创建日期** | 2026-05-18 |
| **状态** | 待实施 |
| **关联** | `docs/superpowers/specs/2026-05-08-ocs-mesh-design.md` |

## 二、背景

当前 `HcclGetRankDescList` 存在三个设计问题：

**问题 1：返回数量固定为 1。** API 命名为 "List"，实际硬编码 `*descNum = 1`，只返回调用者自己的 rank 描述。RankGraph 中已有全量数据（`ocsMeshAttrMap_` 可按任意 rankId 查询），但无对外出口。

**问题 2：存储碎片化。** 同一个 RankDesc 的 8 个字段散落在四处——`localId` 在 `peers_`、`serverIdx` 在 `NewRankInfo`、`ocsPlaneId/Num` 在 `ocsMeshAttrMap_`、`elecGroupId` 在 `RankLevelInfo`。每次调用 `HcclGetRankDescList` 都要跨 4 个数据源临时组装。

**问题 3：`cachedRankDesc_` 本质是 API 设计缺陷的补丁。** 因为 API 返回 `RankDesc **`（二级指针），内部必须有块内存来"接住"结果。当前用 `hcclComm` 成员变量充当这个角色，但这不是业务需要——如果 RankGraph 在构建阶段就灌好全量 vector，API 直接返回 `data()` 即可，不需要 comm 层面的缓存。

## 三、目标

1. `HcclGetRankDescList` 返回通信域内**全部 rank** 的 RankDesc，按 rankId 顺序排列
2. RankDesc 全量数据在 `BuildRankGraph` 阶段一次性灌入 `RankGraph::rankDescVec_`，消除碎片化组装
3. 删除 `cachedRankDesc_`、`OcsMeshAttr`、`ocsMeshAttrMap_` 及其 7 个 accessor 链路
4. 遵循现有 C API 内存管理惯例：库内管理，调用者禁止释放

## 四、范围

### 纳入

- `RankGraph` 新增 `rankDescVec_` 成员 ( `std::vector<RankDesc>` ) 及 `BuildRankDescVec()` 方法
- `RankGraphBuilder::BuildRankGraph()` 末尾调用 `BuildRankDescVec` 灌入全量数据
- `ReparseGroupedPlaneForOcsMesh` 写入目标从 `ocsMeshAttrMap_` 改为 `rankDescVec_`
- `HcclGetRankDescList` 实现改为直接返回 `rankDescVec_.data()`
- 子通信域 `CreateSubComm` 中调用 `BuildRankDescVec` 重建子通信域 RankDesc
- 删除 `cachedRankDesc_`、`OcsMeshAttr`、`ocsMeshAttrMap_` 及全套 accessor 链路（见第九节删除清单）
- 补齐 `serverIdx`、`elecGroupId`、`localId`、`superPodId` 四条数据链路

### 不纳入

- `RankDesc` 结构体定义变更（8 字段保留不变）
- C API 签名变更（`RankDesc **descList, uint32_t *descNum` 保留）
- HCCL 侧调用者适配
- OCS_MESH 之外的通信层级扩展

## 五、设计

### 5.1 数据流

```
BuildRankGraph 阶段（一次性灌入）
═══════════════════════════════

  rankTable.ranks[i].serverIdx ──────────┐
  rankTable.ranks[i].rankLevelInfos ─────┤  elecGroupId / superPodId
  rankGraph.GetLocalId(i) ───────────────┤  localId
  rankGraph.GetLevels(i) ────────────────┤  netLayers / netLayerNum
  ReparseGroupedPlaneForOcsMesh ─────────┤  ocsPlaneId / ocsPlaneNum
                                         │
                                         ▼
                                rankDescVec_[i] 全部字段就绪


GetRankDescList 调用时（零组装开销）
═══════════════════════════════

  *descList = rankDescVec_.data();
  *descNum  = rankDescVec_.size();
```

### 5.2 存储模型

```cpp
// rank_gph.h — RankGraph 新增成员
class RankGraph {
    // ...
    std::vector<RankDesc> rankDescVec_;          // ★ 新增，全量 RankDesc
public:
    const std::vector<RankDesc>& GetRankDescVec() const { return rankDescVec_; }
    void BuildRankDescVec(const RankTableInfo &rankTable,
                          const std::vector<u32> *globalRankIds = nullptr);
    // 删除：
    // std::unordered_map<RankId, OcsMeshAttr> ocsMeshAttrMap_;  // 字段迁移至 rankDescVec_
    // struct OcsMeshAttr { u32 ocsPlaneId; u32 ocsPlaneNum; };  // 不再需要中间结构体
};
```

### 5.3 BuildRankDescVec 实现

```cpp
void RankGraph::BuildRankDescVec(const RankTableInfo &rankTable,
                                 const std::vector<u32> *globalRankIds = nullptr)
{
    u32 count = (globalRankIds == nullptr) ? rankTable.ranks.size()
                                           : static_cast<u32>(globalRankIds->size());
    rankDescVec_.resize(count);
    (void)memset_s(rankDescVec_.data(), count * sizeof(RankDesc),
                   0, count * sizeof(RankDesc));

    for (u32 i = 0; i < count; i++) {
        RankDesc &d = rankDescVec_[i];
        u32 globalIdx = (globalRankIds == nullptr) ? i : (*globalRankIds)[i];
        const auto &rankInfo = rankTable.ranks[globalIdx];

        // localId: peers_ 已有
        d.localId   = GetLocalId(i);

        // serverIdx: Builder 已计算存入 NewRankInfo
        d.serverIdx = rankInfo.serverIdx;

        // elecGroupId & superPodId: 遍历层级信息
        for (const auto &lv : rankInfo.rankLevelInfos) {
            if (lv.netType == NetType::OCS_MESH) {
                d.elecGroupId = lv.elecGroupId;
            }
            if (lv.netType == NetType::CLOS) {
                (void)strncpy(d.superPodId, lv.netInstId.c_str(),
                              sizeof(d.superPodId) - 1);
            }
        }

        // netLayers: 从 GetLevels 填充
        std::set<u32> levels = GetLevels(i);
        d.netLayerNum = 0;
        for (u32 lv : levels) {
            if (d.netLayerNum < RANK_DESC_MAX_NET_LAYER) {
                d.netLayers[d.netLayerNum++] = lv;
            }
        }
    }

    // ocsPlaneId/ocsPlaneNum: 复用以 elecGroup 分组的平面计算逻辑
    ReparseGroupedPlaneForOcsMesh(rankTable, globalRankIds);
}
```

### 5.4 ReparseGroupedPlaneForOcsMesh 收敛

写入目标从 `ocsMeshAttrMap_` 改为 `rankDescVec_`：

```
改前:
  SetOcsMeshAttr(rankId, planeIdx, totalGroups)
    → ocsMeshAttrMap_[rankId] = {planeIdx, totalGroups}

改后:
  rankDescVec_[rankId].ocsPlaneId  = planeIdx;
  rankDescVec_[rankId].ocsPlaneNum = totalGroups;
```

分组逻辑不变（遍历 OCS_MESH NetInstance → 按 elecGroupId 分组 → 分配 planeIdx）。

### 5.5 HcclGetRankDescList 实现

```cpp
HcclResult HcclGetRankDescList(HcclComm comm, RankDesc **descList, uint32_t *descNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(descList);
    CHK_PTR_NULL(descNum);

    HCCLV2_FUNC_RUN([&]() -> HcclResult {
        hccl::hcclComm *hcclCommObj = static_cast<hccl::hcclComm *>(comm);
        CollComm* collComm = hcclCommObj->GetCollComm();
        CHK_PTR_NULL(collComm);
        RankGraph* rankGraph = collComm->GetRankGraph();
        CHK_PTR_NULL(rankGraph);

        const auto &vec = rankGraph->GetRankDescVec();
        *descList = const_cast<RankDesc *>(vec.data());
        *descNum  = static_cast<uint32_t>(vec.size());
        return HCCL_SUCCESS;
    }());

    // V1 路径不支持
    HCCL_ERROR("[%s] V1 path not supported for HcclGetRankDescList", __func__);
    return HCCL_E_NOT_SUPPORT;
}
```

### 5.6 子通信域

子通信域的 `rankDescVec_` 需要在创建子 RankGraph 后独立构建——子 RankGraph 的 rankId 是局部编号（0, 1, 2...），必须通过 `globalRankIds` 映射回主通信域 `rankTable` 获取原始属性。

`BuildRankDescVec` 的 `globalRankIds` 参数覆盖两条路径：

| 路径 | `globalRankIds` | count | globalIdx 映射 |
|------|----------------|-------|---------------|
| 主通信域 | `nullptr` | `rankTable.ranks.size()` | `globalIdx = i` |
| 子通信域 | `&rankIds` | `rankIds.size()` | `globalIdx = rankIds[i]` |

`CreateSubComm` 中插入点（替代原 `ReparseGroupedPlaneForOcsMesh` 调用）：

```cpp
// communicator_impl.cc — CreateSubComm 两个重载中
std::unique_ptr<RankGraph> subRankGraph = rankGraph->CreateSubRankGraph(rankIds);
if (ranktableInfo != nullptr) {
    subRankGraph->BuildRankDescVec(*ranktableInfo, &rankIds);
}
```

注意：`BuildRankDescVec` 内部已调用 `ReparseGroupedPlaneForOcsMesh`，因此 `CreateSubComm` 中**不需要再单独调用 Reparse**。

## 六、调用者迁移

```c
// 改前
RankDesc *desc;
uint32_t num;
HcclGetRankDescList(comm, &desc, &num);
// num == 1
uint32_t myElecGroup = desc[0].elecGroupId;

// 改后
RankDesc *descList;
uint32_t rankCount;
HcclGetRankDescList(comm, &descList, &rankCount);
// rankCount == 通信域总 rank 数
// descList[i] 对应 rankId = i
for (uint32_t i = 0; i < rankCount; i++) {
    if (descList[i].ocsPlaneId == targetPlane) {
        // ...
    }
}
```

## 七、影响面

| 受影响方 | 影响 | 风险 |
|---------|------|------|
| `HcclGetRankDescList` 实现 | 从临时组装变为返回 vector data() | 零风险，代码量大幅减少 |
| `HcclGetRankDescList` 调用者 | 返回数量从 1 变为 rankCount | **调用者必须适配**循环遍历 |
| `ReparseGroupedPlaneForOcsMesh` | 写入目标从 map 改为 vector 元素 | 低风险，分组逻辑不变 |
| `BuildRankGraph` | 末尾新增 `BuildRankDescVec` 调用 | 低风险，纯新增 |
| `CreateSubComm` | 子通信域需调用 `BuildRankDescVec` | 低风险，沿用已有插入点 |
| 旧芯片路径（910B/910_93） | V1 路径不访问 `rankDescVec_` | 零影响 |
| 已删除的 accessor 链路 | 7 个方法 + 1 个结构体 + 1 个 map | 编译时检测所有引用 |

## 八、不变更项

| 文件 | 内容 | 原因 |
|------|------|------|
| `include/hccl/hccl_rank_graph.h` | `RankDesc` 结构体定义 | 8 字段不变 |
| `include/hccl/hccl_rank_graph.h` | `HcclGetRankDescList` API 声明 | 签名不变 |
| `include/hccl/hccl_rank_graph.h` | `RANK_DESC_MAX_NET_LAYER = 8` | 常量不变 |
| `topo_common_types.h` | `NetType::OCS_MESH` | 保留 |
| `rank_level_info.h/.cc` | `elecGroupId` | 保留 |
| `new_rank_info.h/.cc` | `serverIdx` | 保留 |
| `net_instance.h/.cc` | `OcsMeshNetInstance` | 保留 |
| `rank_graph_builder.h/.cc` | 工厂/门控/serverIdx 计算 | 保留 |
| `comm.h` / `hccl_common_v2.h` | `HcclTopoLevel` | 保留 |
| `hccl_communicator.h` | `COMM_LAYER_NUM_MAX = 3` | 保留 |

## 九、删除清单

| 删除项 | 文件 | 原因 |
|--------|------|------|
| `struct OcsMeshAttr` | `rank_gph.h` | 字段写入 RankDesc，无存在必要 |
| `ocsMeshAttrMap_` 成员 | `rank_gph.h` | 无调用者 |
| `SetOcsMeshAttr()` | `rank_gph.h` / `rank_graph.cc` | 改直接写 `rankDescVec_[i]` |
| `GetOcsPlaneId(rankId)` | `rank_gph.h` / `rank_graph.cc` | 无调用者 |
| `GetOcsPlaneNum(rankId)` | `rank_gph.h` / `rank_graph.cc` | 无调用者 |
| `IRankGraph::GetOcsPlaneId()` | `rank_graph_interface.h/.cc` | 无调用者 |
| `IRankGraph::GetOcsPlaneNum()` | `rank_graph_interface.h/.cc` | 无调用者 |
| `RankGraphV2::GetOcsPlaneId()` | `rank_graph_v2.h/.cc` | 无调用者 |
| `RankGraphV2::GetOcsPlaneNum()` | `rank_graph_v2.h/.cc` | 无调用者 |
| 基类 `GetOcsPlaneId/Num()` 虚函数 | `rank_graph_base.h` | V1 路径无 OCS_MESH |
| `hcclComm::cachedRankDesc_` | `hccl_comm_pub.h` | 缓存下沉至 `rankDescVec_` |
| `hcclComm::GetCachedRankDesc()` | `hccl_comm_pub.h` | 同上 |
| `NewRankInfo::ocsPlaneId` 字段 | `new_rank_info.h` | 已是死代码 |
| `NewRankInfo::ocsPlaneNum` 字段 | `new_rank_info.h` | 已是死代码 |

## 十、验证方式

```bash
cd /data/ccl_workspace/toolsh_dir
bash hcomm_build.sh --pkg --full
```

编译成功即为验证通过。逻辑正确性验证：

```bash
bash hcomm_build.sh --ut
# 重点关注 ut_ocs_mesh_plane 测试通过
```
