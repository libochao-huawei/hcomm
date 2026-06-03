# 创建资源

## 通信资源计算

通信算子在执行时依赖底层的硬件通信资源，因此在算子任务编排下发前，首先需要申请所需的通信资源。HCCL通信资源主要分为三类：

- **Thread**：用于表达单个rank内的并发关系，相同Thread上的task严格顺序执行，不同Thread上的task可以并发执行。无论何种展开模式，Thread均可分为1个主Thread和若干个从Thread，由该主Thread来控制这些从Thread上的任务的开始和结束，其中主Thread必须申请，从Thread的申请数量由每个通信算法单独计算。
- **Notify**：用于实现同步机制，包括rank内Thread间同步和rank间同步。
- **Channel**：用于跨rank传输数据的通道。

不同的通信算法所需的通信资源数量往往不相同。下面以单机4卡的拓扑组网、使用AI CPU通信引擎为例，分别讲解Ring环状通信算法、Mesh全连接通信算法所需通信资源数量的计算逻辑。

- **Ring环状通信算法**

  Ring算法的硬件拓扑如下图所示，每个rank从上一个rank接收数据，同时将数据发送到下一个rank。

  ![Ring算法拓扑示例](figures/ring.png)

  上述组网中Ring环状通信算法所需要的通信资源数量如下：

  - Thread：每个rank分别跟左右两个rank进行通信，且左右两端通信可以异步执行，因此每个rank需要2个Thread资源。
  - Thread Notify：Thread上的Notify用于Thread之间的同步， 对于1个Thread来说，只需要1个Notify来接受另1个Thread的POST信号，因此2个Thread只需要1个Notify。
  - Channel：每个rank分别跟左右两个rank进行通信，因此每个rank需要建立2条Channel通信链路。
  - Channel Notify：每个rank分别跟左右两个rank进行同步，同步操作分为Record和Wait，二者需要配对使用，因此每个Channel上需要2个Notify资源，每个rank的2条Channel上共需要4个Notify资源。

- **Mesh全连接通信算法**

  Mesh算法的硬件拓扑如下图所示，每个rank与其他所有rank直接通信。

  ![Mesh算法硬件拓扑示例](figures/mesh.png)

  上述组网中Mesh全连接通信算法所需要的通信资源数量如下：

  - Thread：每个rank跟其他所有rank进行通信，且所有通信可以异步执行，因此每个rank一共需要3个Thread资源。
  - Thread Notify：Thread上的Notify用于Thread之间的同步， 对于1个Thread来说，他只需要1个Notify来接受另1个Thread的POST信号，因此3个Thread只需要2个Notify。
  - Channel：每个rank跟其他所有rank进行通信，因此每个rank一共需要建立3条Channel通信通道。
  - Channel Notify：每个rank跟其他所有rank进行同步，同步操作分为前同步和后同步（前同步即是告诉远端rank自己已经准备好数据或者内存，可被远端Rank读写；后同步即是告诉远端Rank自己已经读写完毕），二者需要配对使用，因此每个rank各3条Channel一共需要6个Notify资源。

综上所述，针对Ring环状通信算法、Mesh全连接通信算法，每个rank所需的通信资源数量如下表所示。

单机4卡拓扑中，算法所需各类通信资源的数量对比如下表所示。

| 通信资源 | Ring算法 | Mesh算法 |
| --- | --- | --- |
| Thread | 2 | 3 |
| Notify | 4 | 6 |
| Channel | 2 | 3 |

## 代码示例

下面以自定义点对点Send/Receive通信算子为例，介绍其在Host上的资源创建代码片段。

1. 申请Context内存，用以存储资源信息

    ```c
    uint64_t size = 需要申请的Context内存的大小;
    void *ctx = nullptr;
    char *tag = 保存资源需要用到的tag;
    CommEngine engine = COMM_ENGINE_AICPU_TS; // 选择引擎
    HcclResult ret = HcclEngineCtxGet(comm, tag, engine, &ctx, &size);
    if (ret != HCCL_SUCCESS) {
       // 即之前没有创建过tag表示的资源
       HcclEngineCtxCreate(comm, tag, engine, size, ctx); 
    }else {
       // 说明之前创建过资源，直接用ctx就好
    }
    ```

2. 申请Thread资源。

    ```c
    CommEngine engine = CommEngine::COMM_ENGINE_AICPU;  // AI CPU通信引擎
    uint32_t threadNum = 1;             // 申请一个通信线程
    uint32_t notifyNumPerThread = 0;    // 因为仅一个通信线程，不涉及线程间同步，所以不额外申请通信线程中的同步资源
    ThreadHandle thread;
    HcclThreadAcquire(comm, engine, threadNum, notifyNumPerThread, &thread);
    // 将thread句柄保存进ctx指向的内存中，方便后续使用
    ```

3. 建立通信链路Channel，两个rank之间建立1条通信通道。

    ```c
    HcclChannelDesc channelDesc;
    HcclChannelDescInit(&channelDesc, 1);
    channelDesc.remoteRank = destRank;
    channelDesc.channelProtocol = CommProtocol::COMM_PROTOCOL_HCCS;
    channelDesc.notifyNum = 2; // 前同步和后同步均需要notify，因此需要2个notify
    ChannelHandle channelHandle;
    HcclChannelAcquire(comm, engine, &channelDesc, 1, &channelHandle);
    // 将channel句柄保存进ctx指向的内存中，方便后续使用
    ```

4. 分配通信内存。

    ```c
    // 分配并获取本端通信内存
    void *localAddr;
    uint64_t localSize;
    HcclGetHcclBuffer(comm, &localAddr, &localSize);
    // 获取远端通信内存，用于后续读写操作
    void *remoteAddr;
    uint64_t remoteSize;
    HcclChannelGetHcclBuffer(comm, channelHandle, &remoteAddr, &remoteSize);
    // 将内存地址保存进ctx指向的内存中，方便后续使用
    ```
