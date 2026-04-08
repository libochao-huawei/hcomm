# 通信域 QoS → CCU UB Jetty Priority 传递说明

本文描述 **legacy CCU 路径**下，从 **`HcclCommConfig.hcclQos`（通信域配置）** 到 **`HCCP/QP` 创建属性中的 `attr.ub.priority`（UB Jetty 优先级）** 的端到端数据流，以及与该链路相关的代码位置与约定。

---

## 1. 概念与常量

| 名称 | 含义 |
|------|------|
| **`hcclQos`** | `HcclCommConfig` 中的 `uint32_t` 字段，表示用户在通信域侧配置的 QoS 数值。 |
| **`HCCL_COMM_QOS_CONFIG_NOT_SET`** | `0xffffffff`，表示用户**未**在配置中指定 QoS（见 `include/hccl/hccl_types.h`）。 |
| **`HCCL_COMM_QOS_CONFIG_DEFAULT_UB`** | 未配置 QoS 时，UB/CCU 侧采用的默认数值（当前为 **4**，与头文件中常量一致）。 |
| **通道侧 `qos`** | `CcuChannelPara::qos`、`ChannelPara::qos`、`CcuJettyInfo::qos` 等，在 CCU 分配与 Jetty 描述中传递的 `uint8_t`。 |
| **`HrtRaUbCreateJettyParam::qos`** | 调用 HCCP 创建 UB Jetty 时的入参字段。 |
| **`attr.ub.priority`** | `GetQpCreateAttr` 中映射到底层创建 QP/Jetty 的优先级字段。 |

> 说明：业务上「通信域 QoS」与硬件「UB priority」在同一链路中一一传递；具体合法范围以 HCCP/芯片约定为准，`GetCommQos` 将 `uint32_t` 转为 `u8` 供 UB 路径使用。

---

## 2. 端到端数据流（总览）

```mermaid
flowchart LR
  subgraph comm["通信域"]
    A[HcclCommConfig.hcclQos]
    B[CommunicatorImpl::config]
    C[GetCommQos → u8]
  end
  subgraph ccu_alloc["CCU 申请"]
    D[CcuChannelPara.qos]
    E[ChannelPara.qos]
    F[CcuJettyInfo.qos]
  end
  subgraph create["创建 Jetty"]
    G[HrtRaUbCreateJettyParam.qos]
    H[attr.ub.priority]
  end
  A --> B
  B --> C
  C --> D
  D --> E
  E --> F
  F --> G
  G --> H
```

---

## 3. 分阶段说明

### 3.1 配置进入通信域（`HcclCommConfig` → `CommunicatorImpl`）

1. **对外配置结构**  
   `HcclCommConfig` 含 `uint32_t hcclQos`（`include/hccl/hccl_types.h`）。

2. **V2 建域：拷贝 `hcclQos`**  
   在 `CreateCommConfig`、`CreateCommConfigRootInfo`（`src/legacy/framework/entrance/op_base/op_base_v2.cc`）中，构造内部 `hcclConf` 时增加：  
   **`hcclConf->hcclQos = config->hcclQos`**，保证用户传入的 QoS 进入后续 `HcclCommunicator` 持有的配置，而不会在 `make_shared<HcclCommConfig>()` 后残留为 0。

3. **`HcclCommunicator` 持有配置**  
   - 带 `HcclCommConfig*` 的构造函数：`config(*config)`，含完整 `hcclQos`。  
   - 仅 `CommParams` 的构造函数：在 `src/legacy/framework/communicator/hccl_communicator.cc` 中将 **`config.hcclQos = HCCL_COMM_QOS_CONFIG_NOT_SET`**，避免 `HcclCommConfig{}` 零初始化导致 `hcclQos == 0` 被误当成「有效配置」。

4. **下沉到 `CommunicatorImpl`**  
   `InitCommonDataNotInitDevType` 中 **`config = commConfig`**（`src/legacy/framework/communicator/communicator_impl.cc`），此后 **`CommunicatorImpl::GetCommQos()`** 只读该份 `config.hcclQos`。

---

### 3.2 通信域 QoS → CCU 申请参数（`GetCommQos` → `CcuChannelPara`）

**实现：`CommunicatorImpl::GetCommQos()`**（`communicator_impl.cc`）

- 若 **`config.hcclQos == HCCL_COMM_QOS_CONFIG_NOT_SET`**：返回 **`static_cast<u8>(HCCL_COMM_QOS_CONFIG_DEFAULT_UB)`**（默认 UB QoS）。  
- 否则：返回 **`static_cast<u8>(config.hcclQos)`**。

**注入点：`CcuJettyMgr::GetAvailableBatch`**（`src/legacy/framework/ccu/ccu_manager/ccu_jetty_mgr.cpp`）

- `CcuCommunicator` 构造 **`CcuJettyMgr(devLogicId, comm)`**，传入 **`CommunicatorImpl*`**（`src/legacy/framework/ccu/ccu_communicator.h`）。  
- 申请新 batch 时：  
  - **`comm_ != nullptr`**：`qos = comm_->GetCommQos()`；  
  - **`comm_ == nullptr`**：`qos = static_cast<u8>(HCCL_COMM_QOS_CONFIG_DEFAULT_UB)`。  
- 将 `qos` 写入 **`CcuChannelPara`** 的构造函数第五参（即 **`CcuChannelPara::qos`**）。

---

### 3.3 设备与 Channel 层（`CcuAllocChannels` → `ChannelPara` → `CcuJettyInfo`）

1. **`CcuAllocChannels`**（`src/legacy/unified_platform/ccu/ccu_device/ccu_device_manager.cpp`）  
   - 日志打印 **`ccuChannelPara.qos`**。  
   - 填充 **`ChannelPara`**：**`para.qos = ccuChannelPara.qos`**，再调用 `CcuComponent::AllocChannels`。

2. **`CcuChannelMgrV1::Alloc`**（`src/legacy/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_channel_v1/ccu_channel_mgr_v1.cpp`）  
   - Jetty 上下文分配完成后，对每个 **`jettyInfo`**：**`jettyInfo.qos = channelPara.qos`**。  
   这样每个 **`CcuJettyInfo`** 携带与通信域一致的 QoS。

3. **环回（loop）通道**（`CcuComponent::CreateLoopChannel`）  
   - 历史上 **`ChannelPara` 仅用三字段初始化**，**`qos` 固定为结构体默认 `2`**，与通信域无关。  
   - 现与 **`CcuJettyMgr` 取 `comm_->GetCommQos()`** 同一思路：在 **`CommunicatorImpl::TryInitCcuFeature`** 里、调用 **`GetCcuDriver()`** 之前，执行 **`CcuComponent::GetInstance(devLogicId).SetLoopChannelUbQos(GetCommQos())`**，再拉 CCU 驱动；**`CreateLoopChannel`** 使用 **`ChannelPara{..., loopChannelUbQos_}`**，环回 **`attr.ub.priority`** 与当前通信域一致。  
   - **不**走 `TryInitCcuFeature`、仅直接 **`CcuComponent::Init()`** 的单测等：仍用成员默认 **`loopChannelUbQos_{2}`**（与旧行为兼容）。

---

### 3.4 创建 UB Jetty（`CcuJetty` 异步 与 `CcuComponent` 同步）

**异步路径（CCU Transport 常用）** — `src/legacy/unified_platform/resource/ccu_transport/ccu_jetty.cpp`

- 构造函数中先按 10 参数构造 **`HrtRaUbCreateJettyParam`**（不含 `qos` 形参），再赋值：  
  **`inParam_.qos = jettyInfo.qos`**。  
- 后续通过 **`RaUbCreateJettyAsync(rdmaHandle_, inParam_, ...)`** 创建；异步与同步创建均经同一 **`GetQpCreateAttr(in)`**。

**同步路径** — `src/legacy/unified_platform/ccu/ccu_device/ccu_component/ccu_component.cpp`

- 对每个 **`jettyInfo`** 构造 **`HrtRaUbCreateJettyParam req{...}`** 后：  
  **`req.qos = jettyInfo.qos`**，再 **`HrtRaUbCreateJetty(rdmaHandle, req)`**。  
- 与异步路径一致，保证 **`HrtRaUbCreateJettyParam::qos`** 来自上游 **`CcuJettyInfo::qos`**。

---

### 3.5 HCCP 适配：`qos` → `attr.ub.priority`

**`GetQpCreateAttr`**（`src/legacy/unified_platform/external_system/orion_adapter_hccp.cc`）

- **`attr.ub.priority = in.qos`**。  
- **`HrtRaUbCreateJetty`**（同步）与 **`RaUbCreateJettyAsync`**（异步）均在创建前调用 **`GetQpCreateAttr(in)`**，因此两条 API 对 **priority** 的处理一致，区别仅在于同步/异步调用 HCCP。

---

## 4. 关键文件索引

| 环节 | 文件 |
|------|------|
| 常量与 `hcclQos` 定义 | `include/hccl/hccl_types.h` |
| 建域拷贝 `hcclQos` | `src/legacy/framework/entrance/op_base/op_base_v2.cc` |
| `HcclCommunicator` 默认 `hcclQos` | `src/legacy/framework/communicator/hccl_communicator.cc` |
| `config` 合并、`GetCommQos` | `src/legacy/framework/communicator/communicator_impl.cc` |
| 环回 `ChannelPara::qos`、`SetLoopChannelUbQos` | `ccu_component.h` / `ccu_component.cpp` |
| `TryInitCcuFeature` 在 `GetCcuDriver` 前 `SetLoopChannelUbQos(GetCommQos())` | `src/legacy/framework/communicator/communicator_impl.cc` |
| `CcuJettyMgr` 取 QoS并组 `CcuChannelPara` | `src/legacy/framework/ccu/ccu_manager/ccu_jetty_mgr.cpp` |
| 注入 `CcuJettyMgr` 的 `comm` | `src/legacy/framework/ccu/ccu_communicator.h` |
| `CcuChannelPara` / `CcuJettyInfo` | `src/legacy/unified_platform/pub_inc/ccu/ccu_dev_mgr.h` |
| `ChannelPara` | `src/legacy/unified_platform/ccu/ccu_device/ccu_device_manager.h` |
| `CcuAllocChannels` | `src/legacy/unified_platform/ccu/ccu_device/ccu_device_manager.cpp` |
| Channel 分配写 `jettyInfo.qos` | `ccu_channel_mgr_v1.cpp` |
| `CcuJetty` 填 `inParam_.qos` | `src/legacy/unified_platform/resource/ccu_transport/ccu_jetty.cpp` |
| 同步 `HrtRaUbCreateJetty` 填 `req.qos` | `src/legacy/unified_platform/ccu/ccu_device/ccu_component/ccu_component.cpp` |
| `HrtRaUbCreateJettyParam`、`GetQpCreateAttr` | `orion_adapter_hccp.h` / `orion_adapter_hccp.cc` |

---

## 5. 使用与排查注意点

1. **`GetCommQos` 当前语义**（以 `communicator_impl.cc` 为准）：仅 **`NOT_SET`** 走默认 UB；**`hcclQos == 0`** 会原样 **`static_cast<u8>(0)`**，与「未配置」不同。因此务必通过 **`CreateCommConfig` 拷贝**、**默认构造里设 `NOT_SET`** 等方式，避免把「未配置」落在 **`0`** 上。  
2. **`hcclQos` 为 `uint32_t`**：若配置值过大，**`static_cast<u8>`** 会发生截断，需与产品侧约定合法范围。  
3. **其他构造 `HrtRaUbCreateJettyParam` 的路径**（如非 CCU 的 HOST 场景）：若未显式赋值 **`qos`**，则依赖结构体成员默认值；与本文 CCU 主链路分开看待。  
4. **验证建议**：在 **`GetCommQos`**、**`CcuJettyMgr`**、**`CcuAllocChannels`**、**`GetQpCreateAttr` 日志**（`attr.ub.priority`）处对照打印，确认数值一致。

---

*文档与仓库中 legacy CCU QoS 相关实现对应；若后续修改 `GetCommQos` 或拷贝逻辑，请同步更新本文。*
