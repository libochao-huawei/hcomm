# 提交说明：A5 QOS（第一次提交）

本文档基于 Git 提交 `4207554491b972c98c152ccd9ade4408b30920ea` 自动生成，用于记录变更范围、设计意图与代码层面的实现细节。

---

## 1. 提交元信息

| 字段 | 内容 |
|------|------|
| **Commit** | `4207554491b972c98c152ccd9ade4408b30920ea` |
| **作者** | Klayyy \<wanglei886@h-partners.com\> |
| **提交时间** | 2026-04-08 10:37:31 +0800 |
| **说明（原始）** | A5 QOS<br>第一次提交<br>等待 hccltest 日志验证 |

---

## 2. 变更概览

- **统计**：12 个文件变更，**+47 / -14** 行。
- **主题**：将通信域上的 **HCCL QoS 配置**（`hcclQos`）贯通到 **CCU UB Jetty** 创建路径，作为 **JFS 优先级**（`jfsPriority` / `ubJettyJfsPriority`）下发至底层 Orion/HCCP 适配层，替代原先写死的默认优先级 `2`。
- **状态**：提交说明中注明需通过 **hccltest 日志** 进一步验证行为是否符合预期。

---

## 3. 设计意图（从代码归纳）

1. **单一事实来源**：优先级由 `CommunicatorImpl::GetUbJettyJfsPriority()` 从 `config.hcclQos` 推导，避免在 Jetty 管理或平台层重复硬编码。
2. **默认值与“未配置”语义**：当 `hcclQos` 等于 `HCCL_COMM_QOS_CONFIG_NOT_SET` 时，返回优先级 `2`，与历史“CTP 默认优先级 2”行为对齐。
3. **合法范围**：有效优先级限制在 **0–15**；大于 15 的值在 `CommunicatorImpl` 与 `GetQpCreateAttr` 中均被钳位到 15。
4. **端到端传递**：`CcuChannelPara` / `ChannelPara` → 设备管理分配 → `CcuChannelMgrV1` 写入各 `jettyInfos` → `CcuJetty` 填入 `HrtRaUbCreateJettyParam` → `orion_adapter_hccp` 设置 `attr.ub.priority`。

---

## 4. 涉及文件与职责

| 路径 | 角色 |
|------|------|
| `src/legacy/framework/communicator/communicator_impl.h` | 声明 `GetUbJettyJfsPriority()`。 |
| `src/legacy/framework/communicator/communicator_impl.cc` | 实现 QoS → `u8` 优先级映射与钳位。 |
| `src/legacy/framework/ccu/ccu_communicator.h` | 构造 `CcuJettyMgr` 时传入 `CommunicatorImpl*`。 |
| `src/legacy/framework/ccu/ccu_manager/ccu_jetty_mgr.h/.cpp` | `CcuJettyMgr` 持有 `comm_`，申请 channel 时填充 `ubJettyJfsPriority`；日志增加该字段。 |
| `src/legacy/unified_platform/pub_inc/ccu/ccu_dev_mgr.h` | `CcuChannelPara` 增加 `ubJettyJfsPriority`；`CcuJettyInfo` 增加 `jfsPriority`；`ToString` 扩展。 |
| `src/legacy/unified_platform/ccu/ccu_device/ccu_device_manager.h/.cpp` | `ChannelPara` 增加字段；`CcuAllocChannels` 日志与向组件传参。 |
| `src/legacy/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_channel_v1/ccu_channel_mgr_v1.cpp` | 分配后为每个 `jettyInfos` 项设置 `jfsPriority`。 |
| `src/legacy/unified_platform/resource/ccu_transport/ccu_jetty.cpp` | 构造 `HrtRaUbCreateJettyParam` 时设置 `jfsPriority`。 |
| `src/legacy/unified_platform/external_system/orion_adapter_hccp.h/.cc` | `HrtRaUbCreateJettyParam` 增加 `jfsPriority`；`GetQpCreateAttr` 用其设置 `attr.ub.priority`（并钳位 ≤15）。 |

---

## 5. 关键实现细节

### 5.1 `CommunicatorImpl::GetUbJettyJfsPriority()`

- 读取 `config.hcclQos`。
- 若等于 `HCCL_COMM_QOS_CONFIG_NOT_SET`：返回 `2`。
- 否则：返回 `static_cast<u8>(qos > 15u ? 15u : qos)`。

### 5.2 `CcuJettyMgr`

- 构造函数由 `(devLogicId)` 扩展为 `(devLogicId, CommunicatorImpl *comm = nullptr)`，保持默认可不传（`comm_` 为 `nullptr`）。
- `GetAvailableBatch` 中：  
  `jfsPrio = comm_ != nullptr ? comm_->GetUbJettyJfsPriority() : 2u`，再写入 `CcuChannelPara`。

### 5.3 数据结构默认值

- `CcuChannelPara::ubJettyJfsPriority`、`ChannelPara::ubJettyJfsPriority`、`CcuJettyInfo::jfsPriority`、`HrtRaUbCreateJettyParam::jfsPriority` 默认均为 **2**，与旧行为一致。

### 5.4 Orion 适配层

- 原注释：“CTP 默认优先级使用 2, TP/UBG 等模式后续 QoS 特性统一适配”。
- 现改为使用 `in.jfsPriority`，并在 `pri > 15u` 时置为 `15u`，再赋给 `attr.ub.priority`。

---

## 6. 数据流简图

```text
CommunicatorImpl.config.hcclQos
        │
        ▼
GetUbJettyJfsPriority()
        │
        ▼
CcuJettyMgr → CcuChannelPara.ubJettyJfsPriority
        │
        ▼
CcuAllocChannels / ChannelPara
        │
        ▼
CcuChannelMgrV1 → jettyInfos[].jfsPriority
        │
        ▼
CcuJetty → HrtRaUbCreateJettyParam.jfsPriority
        │
        ▼
GetQpCreateAttr → attr.ub.priority
```

---

## 7. 风险与验证建议

- **回归**：未配置 QoS 时路径应仍等价于原默认优先级 2（含 `comm_ == nullptr` 的构造场景）。
- **边界**：`hcclQos` 大于 15 时全链路应表现为 15，与 `GetQpCreateAttr` 二次钳位一致（冗余但安全）。
- **验证**：按提交说明，建议结合 **hccltest** 抓日志，确认分配/创建 Jetty 时优先级与配置一致。

---

## 8. 如何在本仓库重新查看该提交

```bash
git show 4207554491b972c98c152ccd9ade4408b30920ea
```

---

*文档生成说明：内容与提交 diff 对应；若后续有 amend 或追加提交，请重新执行 `git show` 并更新本文档。*
