# HcclGetRankDescList 全量返回 & 存储下沉 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `HcclGetRankDescList` 从返回单个 myRank 改为返回全量 RankDesc 数组，存储从碎片化 map 收敛为 `RankGraph::rankDescVec_`，删除 `OcsMeshAttr`/`ocsMeshAttrMap_`/`cachedRankDesc_` 及全套 accessor 链路。

**Architecture:** 在 `BuildRankGraph` 阶段一次性灌入 `rankDescVec_`（vector<RankDesc>），包含全部 8 字段。`HcclGetRankDescList` 直接返回 `rankDescVec_.data()`，零组装开销。遵循现有 C API 惯例：库内管理内存，调用者禁止释放。

**Tech Stack:** C++14, HCCL/HCOMM legacy framework (Hccl namespace), GoogleTest, GCC 7.3+

---

### Task 1: RankGraph 头文件改动 — 加 rankDescVec_，删 OcsMeshAttr/ocsMeshAttrMap_

**Files:**
- Modify: `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h:32-35,81-88,102`

- [x] **Step 1: 删除 OcsMeshAttr 结构体 (line 32-35)**

```diff
- struct OcsMeshAttr {
-     u32 ocsPlaneId{0};
-     u32 ocsPlaneNum{0};
- };
```

- [x] **Step 2: 替换 ocsMeshAttrMap_ 为 rankDescVec_ (line 102)**

```diff
-     std::unordered_map<RankId, OcsMeshAttr> ocsMeshAttrMap_;
+     std::vector<RankDesc> rankDescVec_;
```

- [x] **Step 3: 删除 SetOcsMeshAttr/GetOcsPlaneId/GetOcsPlaneNum 声明 (line 82-84)**

```diff
-     // OCS mesh 并行平面属性
-     void SetOcsMeshAttr(RankId rankId, u32 ocsPlaneId, u32 ocsPlaneNum);
-     u32  GetOcsPlaneId(RankId rankId) const;
-     u32  GetOcsPlaneNum(RankId rankId) const;
```

- [x] **Step 4: 添加 GetRankDescVec() 和 BuildRankDescVec() (在 ReparseGroupedPlaneForOcsMesh 声明之后)**

```cpp
    // 基于 RankTableInfo 重算 OCS 平面分组，globalRankIds 用于子通信域 rankId 映射（主通信域传 nullptr）
    void ReparseGroupedPlaneForOcsMesh(const RankTableInfo &rankTable,
                                       const std::vector<u32> *globalRankIds = nullptr);

    // RankDesc 全量数据一次性灌入，globalRankIds 为 nullptr 时使用主通信域 rankId 直接索引
    void BuildRankDescVec(const RankTableInfo &rankTable,
                          const std::vector<u32> *globalRankIds = nullptr);
    const std::vector<RankDesc>& GetRankDescVec() const { return rankDescVec_; }
```

- [x] **Step 5: 添加 #include "hccl_rank_graph.h" 到头文件顶部（RankDesc 定义所在）**

```diff
  #include "rank_table_info.h"
+ #include "hccl_rank_graph.h"
```

- [x] **Step 6: Commit**

```bash
git add src/legacy/framework/topo/new_topo_builder/rank_graph/rank_gph.h
git commit -m "refactor(rank_graph): replace OcsMeshAttr/map with rankDescVec_"
```

---

### Task 2: RankGraph 实现 — 删旧方法 + 加 BuildRankDescVec + Reparse 收敛

**Files:**
- Modify: `src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc:883-956`

- [x] **Step 1: 删除 SetOcsMeshAttr/GetOcsPlaneId/GetOcsPlaneNum 实现 (lines 883-898)**

```diff
- void RankGraph::SetOcsMeshAttr(RankId rankId, u32 ocsPlaneId, u32 ocsPlaneNum)
- {
-     ocsMeshAttrMap_[rankId] = {ocsPlaneId, ocsPlaneNum};
- }
- 
- u32 RankGraph::GetOcsPlaneId(RankId rankId) const
- {
-     auto it = ocsMeshAttrMap_.find(rankId);
-     return (it != ocsMeshAttrMap_.end()) ? it->second.ocsPlaneId : 0;
- }
- 
- u32 RankGraph::GetOcsPlaneNum(RankId rankId) const
- {
-     auto it = ocsMeshAttrMap_.find(rankId);
-     return (it != ocsMeshAttrMap_.end()) ? it->second.ocsPlaneNum : 0;
- }
```

- [x] **Step 2: 修改 ReparseGroupedPlaneForOcsMesh — 写入目标从 ocsMeshAttrMap_ 改为 rankDescVec_**

找到 line 950 的 `SetOcsMeshAttr(rankId, planeIdx, totalGroups);`，替换为：

```diff
-                     SetOcsMeshAttr(rankId, planeIdx, totalGroups);
+                     rankDescVec_[rankId].ocsPlaneId  = planeIdx;
+                     rankDescVec_[rankId].ocsPlaneNum = totalGroups;
```

- [x] **Step 3: 在 ReparseGroupedPlaneForOcsMesh 之后添加 BuildRankDescVec 实现**

```cpp
void RankGraph::BuildRankDescVec(const RankTableInfo &rankTable,
                                 const std::vector<u32> *globalRankIds)
{
    u32 count = (globalRankIds == nullptr) ? static_cast<u32>(rankTable.ranks.size())
                                           : static_cast<u32>(globalRankIds->size());
    rankDescVec_.resize(count);
    (void)memset_s(rankDescVec_.data(), count * sizeof(RankDesc),
                   0, count * sizeof(RankDesc));

    for (u32 i = 0; i < count; i++) {
        RankDesc &d = rankDescVec_[i];
        u32 globalIdx = (globalRankIds == nullptr) ? i : (*globalRankIds)[i];

        if (globalIdx >= rankTable.ranks.size()) {
            THROW<InvalidParamsException>(StringFormat(
                "[RankGraph][BuildRankDescVec] globalIdx[%u] exceeds rankTable size[%zu].",
                globalIdx, rankTable.ranks.size()));
        }

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

    // ocsPlaneId/ocsPlaneNum: 复用分组逻辑
    ReparseGroupedPlaneForOcsMesh(rankTable, globalRankIds);
}
```

- [x] **Step 4: Commit**

```bash
git add src/legacy/framework/topo/new_topo_builder/rank_graph/rank_graph.cc
git commit -m "feat(rank_graph): add BuildRankDescVec, converge Reparse to rankDescVec_"
```

---

### Task 3: RankGraphBuilder — BuildRankGraph 末尾调用 BuildRankDescVec

**Files:**
- Modify: `src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc:610-611`

- [x] **Step 1: 替换 ReparseGroupedPlaneForOcsMesh 调用为 BuildRankDescVec**

```diff
-     rankGraph_->ReparseGroupedPlaneForOcsMesh(*rankTable_);
+     rankGraph_->BuildRankDescVec(*rankTable_);
```

- [x] **Step 2: Commit**

```bash
git add src/legacy/framework/topo/new_topo_builder/rank_graph_builder/rank_graph_builder.cc
git commit -m "refactor(builder): switch from Reparse to BuildRankDescVec in BuildRankGraph"
```

---

### Task 4: HcclGetRankDescList — 简化为两行返回

**Files:**
- Modify: `src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc:378-425`

- [x] **Step 1: 替换整个 HcclGetRankDescList 实现**

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
        HCCL_RUN_INFO("[%s] success, descNum[%u]", __func__, *descNum);
        return HCCL_SUCCESS;
    }());

    // V1 路径暂不支持
    HCCL_ERROR("[%s] V1 path not supported for HcclGetRankDescList", __func__);
    return HCCL_E_NOT_SUPPORT;
}
```

- [x] **Step 2: Commit**

```bash
git add src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc
git commit -m "feat(api): simplify HcclGetRankDescList to return full rankDescVec_"
```

---

### Task 5: 子通信域 — CreateSubComm 调用 BuildRankDescVec

**Files:**
- Modify: `src/legacy/framework/communicator/communicator_impl.cc:324-366`

- [x] **Step 1: 两个 CreateSubComm 重载中替换 Reparse 为 BuildRankDescVec**

**位置 1 (line 330-332)**：

```diff
              std::unique_ptr<RankGraph> subRankGraph = rankGraph->CreateSubRankGraph(rankIds);
              if (ranktableInfo != nullptr) {
-                 subRankGraph->ReparseGroupedPlaneForOcsMesh(*ranktableInfo, &rankIds);
+                 subRankGraph->BuildRankDescVec(*ranktableInfo, &rankIds);
              }
```

**位置 2 (line 352-356)**：

```diff
              std::unique_ptr<RankGraph> subRankGraph = rankGraph->CreateSubRankGraph(rankIds);
              subCommImpl->rankIdsVec = rankIds;
              if (ranktableInfo != nullptr) {
-                 subRankGraph->ReparseGroupedPlaneForOcsMesh(*ranktableInfo, &rankIds);
+                 subRankGraph->BuildRankDescVec(*ranktableInfo, &rankIds);
              }
```

- [x] **Step 2: Commit**

```bash
git add src/legacy/framework/communicator/communicator_impl.cc
git commit -m "feat(subcomm): replace Reparse with BuildRankDescVec in CreateSubComm"
```

---

### Task 6: accessor 链路清理 — IRankGraph + RankGraphV2 + rank_graph_base

**Files:**
- Modify: `src/legacy/interface/rank_graph_interface.h:43-44`
- Modify: `src/legacy/interface/rank_graph_interface.cc:455-473`
- Modify: `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.h:38-39`
- Modify: `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.cc:106-114`
- Modify: `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_base.h:47-48`

- [ ] **Step 1: 删除 IRankGraph 中 GetOcsPlaneId/Num 声明**

`rank_graph_interface.h`:

```diff
-         u32 GetOcsPlaneId() const;
-         u32 GetOcsPlaneNum() const;
```

- [ ] **Step 2: 删除 IRankGraph 中 GetOcsPlaneId/Num 实现**

`rank_graph_interface.cc`:

```diff
-     u32 IRankGraph::GetOcsPlaneId() const
-     {
-         if (rankGraphPtr_ == nullptr) {
-             HCCL_ERROR("[IRankGraph::GetOcsPlaneId] rankGraphPtr_ is null");
-             return 0;
-         }
-         const RankGraph *rankGraph = static_cast<const RankGraph *>(rankGraphPtr_);
-         return rankGraph->GetOcsPlaneId(rankGraph->GetMyRank());
-     }
- 
-     u32 IRankGraph::GetOcsPlaneNum() const
-     {
-         if (rankGraphPtr_ == nullptr) {
-             HCCL_ERROR("[IRankGraph::GetOcsPlaneNum] rankGraphPtr_ is null");
-             return 0;
-         }
-         const RankGraph *rankGraph = static_cast<const RankGraph *>(rankGraphPtr_);
-         return rankGraph->GetOcsPlaneNum(rankGraph->GetMyRank());
-     }
```

- [ ] **Step 3: 删除 RankGraphV2 中 GetOcsPlaneId/Num 声明和实现**

`rank_graph_v2.h`:

```diff
-     u32 GetOcsPlaneId() const;
-     u32 GetOcsPlaneNum() const;
```

`rank_graph_v2.cc`:

```diff
- u32 RankGraphV2::GetOcsPlaneId() const
- {
-     return pImpl->GetOcsPlaneId();
- }
- 
- u32 RankGraphV2::GetOcsPlaneNum() const
- {
-     return pImpl->GetOcsPlaneNum();
- }
```

- [ ] **Step 4: 删除 rank_graph_base.h 中虚函数默认实现**

```diff
-     virtual u32 GetOcsPlaneId() const { return 0; }
-     virtual u32 GetOcsPlaneNum() const { return 0; }
```

- [ ] **Step 5: Commit**

```bash
git add src/legacy/interface/rank_graph_interface.h \
        src/legacy/interface/rank_graph_interface.cc \
        src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.h \
        src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.cc \
        src/framework/communicator/impl/independent_op/rank_graph/rank_graph_base.h
git commit -m "refactor: remove OcsPlaneId/Num accessor chain (superseded by rankDescVec_)"
```

---

### Task 7: hcclComm + NewRankInfo 清理 — cachedRankDesc_ + 死字段

**Files:**
- Modify: `src/framework/inc/hccl_comm_pub.h:409,458`
- Modify: `src/legacy/framework/topo/new_topo_builder/rank_table_info/new_rank_info.h:39-40`

- [ ] **Step 1: 删除 hcclComm::GetCachedRankDesc() 和 cachedRankDesc_**

`hccl_comm_pub.h`:

```diff
-     RankDesc& GetCachedRankDesc() { return cachedRankDesc_; }
```

和私有成员区域:

```diff
-     RankDesc cachedRankDesc_;
```

- [ ] **Step 2: 删除 NewRankInfo 中 ocsPlaneId/ocsPlaneNum 死字段**

`new_rank_info.h`:

```diff
      u32                        serverIdx{0};
-     u32                        ocsPlaneId{0};
-     u32                        ocsPlaneNum{0};
```

确认 `.cc` 中 BinaryStream 序列化代码未引用这两个字段（已确认无引用，仅声明处删除即可）。

- [ ] **Step 3: Commit**

```bash
git add src/framework/inc/hccl_comm_pub.h \
        src/legacy/framework/topo/new_topo_builder/rank_table_info/new_rank_info.h
git commit -m "refactor: remove cachedRankDesc_ and NewRankInfo dead ocsPlane fields"
```

---

### Task 8: 测试适配 — ut_ocs_mesh_plane.cc

**Files:**
- Modify: `test/legacy/ut/framework/topo/new_topo_builder/rank_graph/ut_ocs_mesh_plane.cc`

- [ ] **Step 1: 将所有 `graph.GetOcsPlaneId(N)` 替换为 `graph.GetRankDescVec()[N].ocsPlaneId`**

```cpp
// 改前
EXPECT_EQ(graph.GetOcsPlaneId(0), 0u);
EXPECT_EQ(graph.GetOcsPlaneNum(0), 2u);

// 改后
EXPECT_EQ(graph.GetRankDescVec()[0].ocsPlaneId, 0u);
EXPECT_EQ(graph.GetRankDescVec()[0].ocsPlaneNum, 2u);
```

受影响的测试用例（共 6 个测试函数，约 18 处断言）：
- `ReparseGroupedPlaneForOcsMesh_MultiElecGroup_MainComm` (lines 119-126)
- `ReparseGroupedPlaneForOcsMesh_SingleElecGroup_MainComm` (lines 140-141)
- `ReparseGroupedPlaneForOcsMesh_NoOcsMeshNetInst_DoesNotSetAttrs` (lines 156-157)
- `ReparseGroupedPlaneForOcsMesh_AllZeroElecGroup_MainComm` (lines 172-173)
- `ReparseGroupedPlaneForOcsMesh_UsesGlobalRankIdsForSubComm` (lines 188-193)

- [ ] **Step 2: 测试用例需在 Reparse/BuildRankDescVec 调用后验证**

`ReparseGroupedPlaneForOcsMesh` 测试用例改为调用 `BuildRankDescVec`：

```diff
-     graph.ReparseGroupedPlaneForOcsMesh(table, nullptr);
+     graph.BuildRankDescVec(table, nullptr);
```

或保留 Reparse 调用（Reparse 仍然存在，只是从 BuildRankDescVec 内部调用），但断言从 `GetRankDescVec()` 读取。对于只测 Reparse 分组逻辑的用例，需在 Reparse 前确保 `rankDescVec_` 已 resize（添加 `graph.BuildRankDescVec(table, nullptr)` 调用），因为 Reparse 现在写入 `rankDescVec_[rankId]`，依赖 vector 已有 size。

- [ ] **Step 3: Commit**

```bash
git add test/legacy/ut/framework/topo/new_topo_builder/rank_graph/ut_ocs_mesh_plane.cc
git commit -m "test: update ut_ocs_mesh_plane for rankDescVec_ access pattern"
```

---

### Task 9: 编译验证

- [ ] **Step 1: 增量编译**

```bash
cd /data/ccl_workspace/toolsh_dir
bash hcomm_build.sh
```

**预期**: 增量编译成功，退出码 0。

- [ ] **Step 2: 运行单元测试**

```bash
bash hcomm_build.sh --ut
```

**预期**: 所有测试通过，尤其是 `ut_ocs_mesh_plane` 7 个测试用例。

- [ ] **Step 3: 最终检查 — 确认删除项全部清除**

```bash
grep -rn "OcsMeshAttr\|GetOcsPlaneId\|GetOcsPlaneNum\|SetOcsMeshAttr\|ocsMeshAttrMap_\|cachedRankDesc_\|GetCachedRankDesc" src/ --include="*.h" --include="*.cc"
```

**预期**: 无任何匹配（所有删除项已清理干净）。
