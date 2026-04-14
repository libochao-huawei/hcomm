# hcomm 8.5.0 OXC 一二阶段移植 MR 文档

## 描述

本 MR 聚焦 **hcomm 8.5.0 对 OXC 组网 RankTable 2.0 的一、二阶段移植主线**，目标是在不扩大范围的前提下，逐步把 OXC 输入资产从“可解析”推进到“可在 subcomm 链路中形成内部闭环”。

本 MR **只包含移植开发主线内容**，不包含后续为 `op_base_api` 轻量桩稳定性所做的风险治理/UT 桩修复。

---

### 第一阶段：OXC RankTable 2.0 基础解析支持

第一阶段目标是：

> 让 hcomm 8.5.0 能够识别并解析 OXC 2.0 RankTable，产出内部 `params + rankTable`，但不进入下游拓扑/算法/通信平面消费链。

#### 主要改动
1. 新增独立 OXC 解析器：
   - `TopoinfoRanktableOxc`
2. 在配置加载路径中接入 OXC 2.0 parser 分派
3. 扩展 `RankTable_t / RankInfo_t` 的 OXC 承载字段，保留：
   - `task_id`
   - `level_list`
   - `rank_addr_list`
   - `net_type`
   - `net_instance_id`
   - `plane_id`
   等 OXC 结构信息
4. 新增 OXC parser 单测，验证：
   - `CfgGetClusterInfo`
   - `CfgGetClusterInfoWithoutDev`
   能成功解析 OXC 2.0 输入

#### 当前边界
- 只到 parser / config / struct / test
- 不进入：
  - netplane 计算
  - RankGraph
  - CommPlane
  - HCCL 消费链

---

### 第二阶段：并行平面信息支持（受限 TopoinfoPlaneTransformer 子集）

第二阶段目标是：

> 参照 `TopoinfoPlaneTransformer` 体系，在 hcomm 主仓受限落地其最小必要子集，只打通 **subcomm 初始化闭环**，让 OXC ranktable 可进一步形成内部 `netPlaneId / netPlaneNum` 信息。

#### 主要改动
1. 新增受限 `TopoinfoPlaneTransformer`：
   - `ParsePlane`
   - `ParseUniformPlane`
   - 必要 helper
2. 在 `CommConfig` 中增加内部 netplane 运行时字段：
   - `netPlaneId_`
   - `netPlaneNum_`
   - `netPlaneInfoSet_`
3. 在 `HcclCommunicator / hcclComm` 中增加 netplane 持有与 getter
4. 在 `HcclCreateSubCommConfigInner` 中接入：
   - `globalRankTable + subRankTable + subCommRankId`
   - `netPlaneId / netPlaneNum`
   - `CommConfig`
5. 新增对外 API：
   - `HcclGetNetPlaneId`
   - `HcclGetNetPlaneNum`
6. 新增 UT：
   - API 空指针与正向查询测试
   - subcomm netplane 注入/消费测试

#### 当前边界
- 只覆盖 **subcomm 初始化闭环**
- 不进入：
  - world comm 主初始化路径
  - RootInfo
  - CommPlane / RankGraph 深化消费
  - HCCL 算子层消费

---

### 本 MR 额外补充：OXC_Mesh JSON 正向样例覆盖

在现有第一阶段 OXC parser UT 基础上，进一步补充与设计文档一致的 OXC 2.0 四层样例，覆盖：

- L0: `TOPO_FILE_DESC`
- L1: `CLOS`
- **L2: `OXC_Mesh`**
- L3: `CLOS`（参数面）

并在现有测试文件中增加断言，证明：
- `net_type = "OXC_Mesh"` 已被 parser 正确读取
- 其结果已保存到 `rankTable.rankList[*].levelList[*].netType`
- `net_instance_id = "0"` 等关键信息被正确保留

该补充仍属于 **parser/json/样例/UT 主线**，未进入后续层级语义消费。

---

## 主要改动文件

### 第一阶段核心文件
- `src/framework/common/src/config.cc`
- `src/framework/common/src/topo/CMakeLists.txt`
- `src/framework/common/src/topo/topoinfo_ranktableOxc.cc`
- `src/framework/common/src/topo/topoinfo_ranktableOxc.h`
- `src/framework/common/src/topo/topoinfo_ranktableParser_pub.h`
- `src/framework/inc/topoinfo_struct.h`
- `test/ut/framework/op_base_api/CMakeLists.txt`
- `test/ut/framework/op_base_api/ut_HcclCommInitClusterInfo_Oxc_API_test.cc`

### 第二阶段核心文件
- `include/hccl/hccl_comm.h`
- `src/framework/common/src/topo/topoinfo_plane_transformer.cc`
- `src/framework/common/src/topo/topoinfo_plane_transformer.h`
- `src/framework/communicator/comm_config.cc`
- `src/framework/communicator/hccl_comm.cc`
- `src/framework/communicator/impl/hccl_communicator.h`
- `src/framework/communicator/impl/hccl_communicator_device.cc`
- `src/framework/communicator/impl/hccl_communicator_host.cc`
- `src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc`
- `src/framework/inc/comm_config_pub.h`
- `src/framework/inc/hccl_comm_pub.h`
- `src/framework/op_base/src/op_base.cc`
- `test/ut/framework/communicator/CMakeLists.txt`
- `test/ut/framework/communicator/ut_HcclGetNetPlaneInfo_API_test.cc`
- `test/ut/framework/op_base_api/ut_HcclCreateSubCommConfig_NetPlane_API_test.cc`

### 编译验证顺手修复
- `src/platform/common/launch_aicpu.cc`
- `src/platform/resource/notify/notify_pool_impl.h`

---

## 关联的Issue

当前 MR 文档中未绑定具体 Issue 编号。
如提交 MR 时有对应问题单/Issue，可在正式 MR 描述中补充：
- 第一阶段：OXC RankTable 2.0 基础解析支持
- 第二阶段：subcomm netplane 内部闭环支持

---

## 测试

### 第一阶段相关验证
1. OXC parser 定向测试：
```bash
./build/test/ut/framework/op_base_api/hccl_utest_framework_op_base_api \
  --gtest_filter=HcclCommInitClusterInfoOxcTest.*
```
结果：**通过**

### 第二阶段相关验证
2. netplane API 定向测试：
```bash
./build/test/ut/framework/communicator/hccl_utest_framework_communicator \
  --gtest_filter=HcclGetNetPlaneInfoTest.*
```
结果：**5/5 通过**

3. subcomm netplane 定向测试：
```bash
./build/test/ut/framework/op_base_api/hccl_utest_framework_op_base_api \
  --gtest_filter=HcclCreateSubCommNetPlaneTest.*
```
结果：**2/2 通过**

4. OXC parser + netplane 联合验证：
```bash
./build/test/ut/framework/op_base_api/hccl_utest_framework_op_base_api \
  --gtest_filter=HcclCreateSubCommNetPlaneTest.*:HcclCommInitClusterInfoOxcTest.*
```
结果：**7/7 通过**

5. `OXC_Mesh` 正向样例最小验证：
```bash
./build/test/ut/framework/op_base_api/hccl_utest_framework_op_base_api \
  --gtest_filter=HcclCommInitClusterInfoOxcTest.Ut_CfgGetClusterInfo_When_OxcRankTable2_0_Expect_ParseSuccess:\
HcclCommInitClusterInfoOxcTest.Ut_CfgGetClusterInfoWithoutDev_When_OxcRankTable2_0_Expect_ParseSuccess
```
结果：**2/2 通过**

### 说明
- 当前只列出与一、二阶段移植主线直接相关的验证证据。
- 不包含后续 UT 风险治理修复引入的全量 `op_base_api` 稳定性工作内容。

---

## 文档更新

本 MR 对应的设计/方案文档包括：
- `wiki_dir/task_0_topoinfo_plane_transformer/03_OXC组网RankTable设计.md`
- `wiki_dir/task_0_topoinfo_plane_transformer/08_hcomm8.5.0_并行平面信息支持方案.md`
- `wiki_dir/task_0_topoinfo_plane_transformer/09_hcomm8.5.0_OXC一二阶段MR文档.md`

---

## 类型标签

- [ ] Bug修复
- [x] 新特性
- [ ] 性能优化
- [x] 文档更新
- [ ] 其他，请描述：

---

## 备注

1. 本 MR 仅代表 **OXC 一、二阶段移植主线**。
2. 后续为 `op_base_api` 轻量桩稳定性所做的风险治理修复，建议单独提交，不与本 MR 混合。
3. 当前对 `net_type` 的支持层级为：
   - **已支持解析与保存**（含 `OXC_Mesh`）
   - **尚未进入后续内部语义消费**
