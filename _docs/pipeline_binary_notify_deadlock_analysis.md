# ReduceScatter Pipeline 二进制 Notify 死锁分析报告

| 日期 | 作者 | 关联代码 |
|------|------|----------|
| 2026-04-29 | p_ch | `coll_reduce_scatter_pipeline_for_910_93_executor.cc` (commit e0353adf 之后) |

## 1 问题描述

ReduceScatter Pipeline executor 在真机连续执行 30 次 ReduceScatter 时，第 15 次出现挂死：RunL0L1Phase 完成后卡住，RunL2Phase 未执行。

本报告分析根因：`LocalNotify` 使用的 `aclrtNotify` 为**二进制通知**而非计数器通知，Pipeline 主循环中存在连续两次 Post 无 Wait 间隔的窗口，在设备侧时序竞态下导致信号丢失和死锁。

## 2 LocalNotify 是二进制通知

### 2.1 证据链

**底层 API 调用链**：

```
LocalNotify::Post
  → DispatcherPub::SignalRecord        (dispatcher.cc:262)
    → hrtNotifyRecord                  (adapter_rts.cc:1559)
      → aclrtRecordNotify              (ACL runtime)

LocalNotify::Wait
  → DispatcherPub::SignalWait          (dispatcher.cc:308)
    → hrtNotifyWaitWithTimeOut         (adapter_rts.cc:1576)
      → aclrtWaitAndResetNotify        (ACL runtime)
```

**证据 1：API 命名**

`aclrtWaitAndResetNotify` — "**WaitAndReset**" 表示等待后**重置**（清零），而非递减。

**证据 2：Ascend ACL 有独立的计数器通知类型**

`acl_rt.h` 中定义了独立的 `aclrtCntNotify` 类型及配套枚举（`acl_rt.h:695-721`）：

```c
// 计数器通知 Record 模式
typedef enum {
    ACL_RT_CNT_NOTIFY_RECORD_SET_VALUE_MODE = 0,  // 直接设值
    ACL_RT_CNT_NOTIFY_RECORD_ADD_MODE = 1,         // 累加
    ACL_RT_CNT_NOTIFY_RECORD_BIT_OR_MODE = 2,      // 按位或
    ACL_RT_CNT_NOTIFY_RECORD_BIT_AND_MODE = 4,     // 按位与
} aclrtCntNotifyRecordMode;

// 计数器通知 Wait 模式
typedef enum {
    ACL_RT_CNT_NOTIFY_WAIT_LESS_MODE = 0,
    ACL_RT_CNT_NOTIFY_WAIT_EQUAL_MODE = 1,
    ACL_RT_CNT_NOTIFY_WAIT_BIGGER_MODE = 2,
    ACL_RT_CNT_NOTIFY_WAIT_BIGGER_OR_EQUAL_MODE = 3,
    ACL_RT_CNT_NOTIFY_WAIT_EQUAL_WITH_BITMASK_MODE = 4,
} aclrtCntNotifyWaitMode;

typedef struct {
    aclrtCntNotifyWaitMode mode;
    uint32_t value;
    uint32_t timeout;
    bool isClear;     // 是否消费后清除
} aclrtCntNotifyWaitInfo;
```

如果 `aclrtNotify` 本身就是计数器语义，就不需要另造 `aclrtCntNotify` 类型。两者独立存在证明 `aclrtNotify` 是二进制通知。

**证据 3：Event fallback 路径也是二进制**

`adapter_rts.cc:1584-1588` 中 event fallback 使用 `aclrtStreamWaitEvent` + `aclrtResetEvent`，明确是二进制语义：

```cpp
auto eventWaitFuncPtr = [](rtNotify_t notify, rtStream_t stream) -> s32 {
    CHK_RT_RET(aclrtStreamWaitEvent(stream, notify));
    CHK_RT_RET(aclrtResetEvent(notify, stream));
    return 0;
};
```

### 2.2 二进制通知的语义模型

| 操作 | 行为 |
|------|------|
| `Record`（Post）| flag 0→1；若 flag 已为 1 则**无效果**（信号丢失） |
| `WaitAndReset`（Wait）| 等待 flag=1，然后 flag 1→0 |

关键差异：**两次 Post 之间若无 Wait 消费，第二次 Post 被静默丢弃**。

## 3 死锁机制分析

### 3.1 RunLoop 信号编排

当前 RunLoop 代码（`coll_reduce_scatter_pipeline_for_910_93_executor.cc:62-76`）：

```cpp
const u64 numLoopTotal = ctx.numBlockTotal + 1;
for (u64 i = 0; i < numLoopTotal; ++i) {
    // L0+L1 阶段 (StreamL0L1)
    if (i < ctx.numBlockTotal) {
        if (i >= 2) {                                       // ← i=0,1 时无 Wait
            Wait(streamL0L1, notifyL2toL0L1);
        }
        RunL0L1Phase(param, ctx, i, streamL0L1);
        Post(streamL0L1, notifyL0L1toL2);                  // ← 每轮都 Post
    }

    // L2 阶段 (StreamL2)
    if (i >= 1 && i <= ctx.numBlockTotal) {
        Wait(streamL2, notifyL0L1toL2);                     // ← 每轮都 Wait
        RunL2Phase(param, ctx, i - 1, streamL2);
        Post(streamL2, notifyL2toL0L1);
    }
}
```

**问题核心**：i=0 和 i=1 时，StreamL0L1 连续执行两次 `Post(notifyL0L1toL2)` 之间无任何 Wait 阻塞。CPU 侧 Post/Wait 仅为入队操作（立即返回），设备侧 StreamL0L1 可能在 StreamL2 的第一个 Wait 执行前连续完成两次 Post。

### 3.2 信号平衡分析（以 4 blocks 为例）

**`notifyL0L1toL2` 信号收支**：

| 操作 | 次数 | 说明 |
|------|------|------|
| Post | 4（i=0,1,2,3） | StreamL0L1 每轮 Post 1 次 |
| Wait | 4（i=1,2,3,4） | StreamL2 每轮 Wait 1 次 |

计数器语义下完美平衡。但二进制语义下，i=0 和 i=1 的两次 Post 可能合并为 1 次有效信号，导致**只有 3 次有效 Post 对 4 次 Wait**。

### 3.3 完整死锁追踪（4 blocks，二进制 notify，两次 Post 合并）

下表追踪 `notifyL0L1toL2` flag 状态。假设 StreamL0L1 的 chunk0 和 chunk1 在 StreamL2 首次 Wait 前完成（竞态触发）：

| 步骤 | StreamL0L1 | StreamL2 | `notifyL0L1toL2` flag |
|------|-----------|----------|:-----:|
| 1 | L0L1_chunk0, **Post #1** | — | 0→**1** |
| 2 | L0L1_chunk1, **Post #2** | — | 1→**1** (丢失!) |
| 3 | Wait(notifyL2toL0L1) → 阻塞 | **Wait #1** → 消费, L2_chunk0, Post(❶) | 1→**0** |
| 4 | Wait(❶)消费, L0L1_chunk2, **Post #3** | **Wait #2** → 阻塞 (flag=0) | 0→**1** |
| 5 | Wait(notifyL2toL0L1) → 阻塞 | **Wait #2**消费, L2_chunk1, Post(❷) | 1→**0** |
| 6 | Wait(❷)消费, L0L1_chunk3, **Post #4** | **Wait #3** → 阻塞 (flag=0) | 0→**1** |
| 7 | PostSync Wait(❸) → 阻塞 | **Wait #3**消费, L2_chunk2, Post(❸) | 1→**0** |
| 8 | PostSync Wait(❸)消费, PostSync Wait(❹) → 阻塞 | **Wait #4 → 永久阻塞** (flag=0, 无更多 Post) | **0** |
| 9 | **PostSync Wait(❹) → 永久阻塞** | ↑卡住, 无法执行 L2_chunk3, 无法 Post(❹) | — |

**第 8-9 步：双向死锁。** StreamL2 等待 `notifyL0L1toL2`（永远不会来），StreamL0L1 等待 `notifyL2toL0L1`（StreamL2 卡住无法 Post）。

### 3.4 关键观察

- **Post #2 在步骤 2 被丢弃**（flag 已为 1），总有效信号从 4 降为 3
- **最终 Wait #4 无信号可消费**，StreamL2 永久阻塞在 L2_chunk3 的准入等待
- **WaitForRemainingL2Signals（PostSync）本身不是直接触发点**——它等待的 `notifyL2toL0L1` 信号不存在累积问题（❸ 和 ❹ 被 L2 计算时间天然分隔）。死锁发生在主循环的 `notifyL0L1toL2` 信号上，间接导致 PostSync 也卡住

### 3.5 信号丢失的充要条件

信号丢失发生当且仅当：**设备侧 StreamL0L1 的 Post #2（i=1 之后）在 StreamL2 的 Wait #1（i=1）执行前完成**。

这是一个竞态条件，取决于：
- L0+L1 chunk0 和 chunk1 的设备执行时间
- 设备调度器何时开始 drain StreamL2 的任务队列
- 系统负载、cache 状态、任务队列深度等

因此表现为**间歇性复现**（非确定性触发）。

## 4 真机现象与分析的对应关系

| 真机观察 | 分析解释 |
|----------|----------|
| 30 次中第 15 次挂死（非首次） | 竞态条件——前 14 次时序安全（设备调度器在两次 Post 之间执行了 StreamL2 的 Wait），第 15 次时序对齐触发信号合并 |
| RunL0L1Phase 完成后卡住 | StreamL0L1 在后续 Wait(notifyL2toL0L1) 或 PostSync Wait 处阻塞——等待 StreamL2 的 Post 永远不会到来 |
| RunL2Phase 未执行 | StreamL2 在 Wait(notifyL0L1toL2) 处永久阻塞——丢失的信号导致 L2 无法获得准入 |
| 间歇复现 | 硬件调度时序的非确定性——是竞态而非逻辑错误的固定表现 |

## 5 修复方案

### 方案 A：奇偶 Notify 分离（推荐）

将每个方向的 1 个 notify 按 block 索引的奇偶性拆分为 2 个，与 double buffer A/B 天然对应。数据按奇偶用 buffer A/B，notify 也按奇偶走不同信号通道。

**资源**：4 个 notify（原 2 个 × 奇偶），无需引入新资源类型，仍为 `aclrtNotify`。

```
notifyL0L1toL2_A  —— 偶数块 (chunk 0, 2, ...) L0L1→L2 方向
notifyL0L1toL2_B  —— 奇数块 (chunk 1, 3, ...) L0L1→L2 方向
notifyL2toL0L1_A  —— 偶数块 L2→L0L1 方向
notifyL2toL0L1_B  —— 奇数块 L2→L0L1 方向
```

**改动后 RunLoop 示意**：

```cpp
for (u64 i = 0; i < numLoopTotal; ++i) {
    if (i < numBlockTotal) {
        auto& fwdNotify = (i % 2 == 0) ? notifyL0L1toL2_A : notifyL0L1toL2_B;

        if (i >= 2) {
            // 等待同侧 buffer 的 L2 释放（2 轮前的同奇偶 bwd notify）
            auto& bwdWait = (i % 2 == 0) ? notifyL2toL0L1_A : notifyL2toL0L1_B;
            Wait(streamL0L1, bwdWait);
        }
        RunL0L1Phase(i);
        Post(streamL0L1, fwdNotify);
    }

    if (i >= 1 && i <= numBlockTotal) {
        u64 blockIdx = i - 1;
        auto& fwdWait = (blockIdx % 2 == 0) ? notifyL0L1toL2_A : notifyL0L1toL2_B;
        auto& bwdPost = (blockIdx % 2 == 0) ? notifyL2toL0L1_A : notifyL2toL0L1_B;

        Wait(streamL2, fwdWait);
        RunL2Phase(blockIdx);
        Post(streamL2, bwdPost);
    }
}

// PostSync：按最后两块的奇偶性分别消费对应的 notifyL2toL0L1_A/B
```

**同一 notify 上相邻两次 Post 间必有 Wait 消费的证明**：

分离后，同一 notify 的相邻 Post 间隔从 1 轮变为 2 轮。以 `notifyL0L1toL2_A`（偶数）为例：

```
Post_A 在 i=0 —— 下一次 Post_A 在 i=2

i=2 的 L0L1 阶段前有 Wait(notifyL2toL0L1_A)
  → 需要 L2_chunk0 完成并 Post(notifyL2toL0L1_A)
    → 需要 StreamL2 的 Wait(notifyL0L1toL2_A) 已经消费了 i=0 的 Post_A
```

因此 **i=0 的 Post_A 必定在 i=2 的 Post_A 之前被消费**，flag_A 在第二次 Post 时已回到 0。奇数侧同理。归纳可得：对任意 numBlockTotal，同一 notify 上不会出现未消费的信号累积。

**以 4 blocks 追踪奇偶分离信号流**：

| 步骤 | StreamL0L1 | StreamL2 | flag_A | flag_B |
|------|-----------|----------|:---:|:---:|
| 1 | L0L1_chunk0, Post(fwd_**A**) | — | 0→**1** | 0 |
| 2 | L0L1_chunk1, Post(fwd_**B**) | — | 1 | 0→**1** |
| 3 | Wait(bwd_A) → 阻塞 | Wait(fwd_**A**) → 消费 | **1→0** | 1 |
| 4 | — | L2_chunk0, Post(bwd_A) | 0 | 1 |
| 5 | Wait(bwd_A)消费 | Wait(fwd_**B**) → 消费 | 0 | **1→0** |
| 6 | L0L1_chunk2, Post(fwd_**A**) | L2_chunk1, Post(bwd_B) | 0→**1** | 0 |
| 7 | Wait(bwd_B) → 消费 | Wait(fwd_**A**) → 消费 | **1→0** | 0 |
| 8 | L0L1_chunk3, Post(fwd_**B**) | L2_chunk2, Post(bwd_A) | 0 | 0→**1** |
| 9 | PostSync Wait(bwd_A) → 消费 | Wait(fwd_**B**) → 消费 | 0 | **1→0** |
| 10 | PostSync Wait(bwd_B) → 阻塞 | L2_chunk3, Post(bwd_B) | 0 | 0 |
| 11 | PostSync Wait(bwd_B) → 消费 | — | 0 | 0 |

**全程无信号丢失，无死锁。** 步骤 1-2 的连续两次 Post 分别落在 flag_A 和 flag_B 上，互不干扰。

**优势**：保持 2-block 预取并行度；不引入新资源类型（仍为 `aclrtNotify`）；与 double buffer A/B 天然对称。

**劣势**：多申请 2 个 notify 资源（共 4 个）；循环体内按奇偶索引增加少量复杂度；PostSync 需按最后两块的奇偶分别消费。

### 方案 B：改用计数器通知 `aclrtCntNotify`

将 `notifyL0L1toL2` 和 `notifyL2toL0L1` 从 `aclrtNotify`（二进制）替换为 `aclrtCntNotify`（计数器），使用 `ADD_MODE` Record 和 `BIGGER_OR_EQUAL_MODE` Wait。每次 Post 累加计数，每次 Wait 在计数达到预期值时消费，不会丢失信号。

**优势**：保持现有流水编排不变，Pipeline 的 2-block 预取窗口完整保留。

**劣势**：需要引入新的 `aclrtCntNotify` 资源管理路径（创建/销毁/注册），修改 `LocalNotify` 封装或新增封装类；仓库内无 `aclrtCntNotify` 使用先例，需验证 910_93 硬件支持情况。

### 方案 C：消除连续 Post 窗口

在 i=1 的 L0L1 阶段前插入对 i=0 的同步等待，确保同一 notify 上任何时刻最多只有 1 个未消费的 Post：

```cpp
if (i < ctx.numBlockTotal) {
    if (i >= 1) {  // 从 i>=2 改为 i>=1
        Wait(streamL0L1, notifyL2toL0L1);
    }
    RunL0L1Phase(param, ctx, i, streamL0L1);
    Post(streamL0L1, notifyL0L1toL2);
}
```

**优势**：改动最小（一行条件），不引入新资源类型。

**劣势**：Pipeline 从 2-block 预取退化为 1-block 预取——i=1 的 L0L1 必须等待 i=0 的 L2 完成后才能开始，预热阶段丧失并行度。PostSync 也需对应调整为消费 `min(1, numBlockTotal)` 个剩余信号。

### 方案对比

| | 方案 A (奇偶分离) | 方案 B (CntNotify) | 方案 C (i>=1 Wait) |
|------|:---:|:---:|:---:|
| 改动范围 | 中（循环体 + notify 分配） | 中（新资源类型 + 封装） | 小（一行条件） |
| 新资源类型 | 无（仍为 aclrtNotify） | 需引入 aclrtCntNotify | 无 |
| 额外 notify 数 | +2（共 4 个） | 0 | 0 |
| 预热并行度 | **保持 2-block** | **保持 2-block** | 退化为 1-block |
| 稳态并行度 | 保持 2-block | 保持 2-block | 保持 2-block |
| 硬件兼容性 | 高（已有 API） | 需验证 910_93 支持 | 高 |
| 正确性风险 | 低（可形式化证明） | 低 | 低 |
