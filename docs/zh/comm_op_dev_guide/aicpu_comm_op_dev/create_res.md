# 创建资源

## 通信资源计算

通信算子在任务编排前，需要根据算法的并发方式和通信对象计算资源。AICPU 通信主要使用以下资源：

- **Context**：保存 Thread、Channel、通信内存等资源信息。Host 侧完成资源创建后，将 Context 序列化并拷贝到 Device，供 AICPU Kernel 反序列化使用。
- **Thread**：表达单个 rank 内任务的串行和并发关系。同一 Thread 上的任务顺序执行，不同 Thread 上的任务可以并发执行。算法 Thread 通常由 1 个主 Thread 和若干个从 Thread 组成。
- **Thread Notify**：用于同一 rank 内主从 Thread 之间的同步。
- **Channel**：连接本 rank 与远端 rank，用于跨 rank 数据传输和同步。
- **Channel Notify**：属于 Channel 的同步资源，用于通知远端数据已准备、传输已完成等状态。
- **通信内存**：包括本端 CCL Buffer 和通过 Channel 获取的远端 CCL Buffer。

Thread Notify 和 Channel Notify 的作用域不同，计算资源时应分别统计，不能只用一个 Notify 总数表示。

## Mesh 全连接资源示例

以当前自定义 AllGather AICPU 样例的单机 4 卡 Mesh 全连接为例，每个 rank 与其余 3 个 rank 直接通信：

![Mesh算法硬件拓扑示例](./figures/mesh.png)

### Thread 与 Thread Notify

样例为每条并发通信路径分配一个算法 Thread，因此每个 rank 需要 3 个算法 Thread：

- 1 个主 Thread，负责本地拷贝以及协调从 Thread。
- 2 个从 Thread，负责其余并发通信路径。

主从 Thread 同步是成对组织的：

1. 开始阶段，主 Thread 分别通知两个从 Thread 开始执行。
2. 结束阶段，两个从 Thread 分别通知主 Thread 已完成。

因此，从逻辑需求看：

| Thread 类型 | 数量 | 每个 Thread 所需 Notify | Notify 小计 |
| --- | ---: | ---: | ---: |
| 主 Thread | 1 | 2 | 2 |
| 从 Thread | 2 | 1 | 2 |
| **合计** | **3** | - | **4** |

一般化后，若算法共有 `T` 个 Thread，则有 1 个主 Thread 和 `T - 1` 个从 Thread：

- 主 Thread 需要 `T - 1` 个 Notify，分别对应每个从 Thread。
- 每个从 Thread 需要 1 个 Notify，用于与主 Thread 同步。
- 算法逻辑所需的 Thread Notify 总数为 `2 * (T - 1)`。

资源接口可能按“每个 Thread 的最大 Notify 数”统一申请，因此物理申请数量可能大于上述逻辑使用数量。应以算法实际使用的 Notify 索引为准，避免把接口的统一申请上限误认为每个 Thread 都会使用相同数量的 Notify。

### Channel 与 Channel Notify

4 卡 Mesh 中，每个 rank 需要连接另外 3 个 rank，因此需要 3 条 Channel。当前样例为每条 Channel 申请 3 个 Notify：

- `ACK`：双方确认数据已经准备完成。
- `DATA_SIGNAL`：双方确认本轮数据传输已经完成。
- `FIN_ACK`：预留的完成确认 Notify，便于后续扩展完整的结束同步。

因此每个 rank 的 Channel Notify 申请数量为 `3 条 Channel * 3 = 9`。当前任务编排主要使用前两个 Notify 索引，资源申请仍以代码中的 Channel 描述为准。

### Host 与 AICPU 控制同步

除算法 Thread 外，样例还创建了一对用于 Host 与 AICPU Kernel 控制同步的 Thread：

- Host Thread 在 Kernel 下发前通知 AICPU 控制 Thread 可以开始执行。
- AICPU 控制 Thread 完成任务编排后通知 Host Thread。
- 两侧各使用 1 个控制 Notify，该资源不计入算法主从 Thread 的 4 个逻辑 Notify 中。

单机 4 卡样例的资源汇总如下：

| 通信资源 | 每个 rank 的数量 | 说明 |
| --- | ---: | --- |
| 算法 Thread | 3 | 1 个主 Thread，2 个从 Thread |
| 算法 Thread Notify | 4 | 主 Thread 2 个，两个从 Thread 各 1 个 |
| Channel | 3 | 分别连接其余 3 个 rank |
| Channel Notify | 9 | 每条 Channel 申请 3 个 |
| Host/AICPU 控制 Notify | 2 | Host 和 AICPU 控制 Thread 各 1 个 |

## 代码示例

下面的代码片段展示当前 AICPU 自定义 AllGather 样例的主要资源计算方式。

1. 计算算法 Thread 和 Thread Notify。

    ```cpp
    uint32_t threadNum = rankSize > 1 ? rankSize - 1 : 1;
    resource.slaveThreadNum = threadNum - 1;
    resource.notifyNumOnMainThread = resource.slaveThreadNum;
    resource.notifyNumPerThread =
        std::vector<uint32_t>(resource.slaveThreadNum, 1);
    ```

2. 申请算法 Thread。接口按每个 Thread 的最大 Notify 数统一申请；样例额外预留 1 个 Notify 用于控制同步。

    ```cpp
    uint32_t maxNotifyNum = resource.notifyNumOnMainThread;
    std::vector<ThreadHandle> threads(threadNum);
    HcclThreadAcquire(comm, COMM_ENGINE_AICPU_TS,
                      threadNum, maxNotifyNum + 1, threads.data());
    ```

3. 为每个远端 rank 建立 Channel，并设置 Channel Notify 数量。

    ```cpp
    constexpr uint32_t channelNotifyNum = 3;
    HcclChannelDesc desc;
    HcclChannelDescInit(&desc, 1);
    desc.remoteRank = remoteRank;
    desc.notifyNum = channelNotifyNum;
    desc.channelProtocol = selectedProtocol;
    HcclChannelAcquire(comm, COMM_ENGINE_AICPU_TS,
                       &desc, 1, &channelHandle);
    ```

4. 获取本端和远端通信内存，并写入资源 Context。

    ```cpp
    void *localAddr = nullptr;
    uint64_t localSize = 0;
    HcclGetHcclBuffer(comm, &localAddr, &localSize);

    void *remoteAddr = nullptr;
    uint64_t remoteSize = 0;
    HcclChannelGetHcclBuffer(comm, channelHandle,
                             &remoteAddr, &remoteSize);
    ```

资源创建完成后，Host 将 Context 序列化并拷贝到 Device。AICPU Kernel 启动后反序列化该 Context，再使用其中的 Thread、Notify、Channel 和通信内存进行任务编排。
