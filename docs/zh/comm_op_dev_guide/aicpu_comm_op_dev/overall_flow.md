HCCL AICPU 通信算子的开发与执行分为 Host 侧准备和 AICPU 侧执行两个阶段。Host 侧负责解析拓扑、选择算法、申请资源并下发 AICPU Kernel；Kernel 启动后，AICPU 侧才根据资源上下文进行任务编排。因此，***Kernel 下发在前，任务编排在后***。



```mermaid
%%{init: {
  "theme": "base",
  "themeVariables": {
    "fontFamily": "Microsoft YaHei, Arial, sans-serif",
    "primaryColor": "#EAF2FF",
    "primaryBorderColor": "#4F7CAC",
    "primaryTextColor": "#1F2937",
    "actorBorder": "#4F7CAC",
    "actorBkg": "#F8FBFF",
    "activationBkgColor": "#DCEBFA",
    "activationBorderColor": "#4F7CAC",
    "sequenceNumberColor": "#6B7280",
    "noteBkgColor": "#FFF7D6",
    "noteBorderColor": "#D6A700"
  }
}}%%
sequenceDiagram
    title HCCL AICPU 通信算子执行流程

    participant HCCL_H as 通信算子接口(Host)
    participant HCOMM_H as HCOMM(Host API)
    participant RTS as RTS
    participant HCCL_K as AICPU Kernel
    participant HCOMM_D as HCOMM(Device API)

    activate HCCL_H

    Note over HCCL_H: Host 侧准备算子执行信息
    HCCL_H->>HCCL_H: 构造算子参数（输入输出地址、数据量、数据类型等）
    HCCL_H->>+HCOMM_H: 查询 rank、拓扑层级和可用链路
    HCOMM_H-->>-HCCL_H: 返回通信域与拓扑信息

    opt 算子存在多种实现
        HCCL_H->>HCCL_H: 根据算子类型、执行引擎、拓扑和数据量选择算法
    end

    Note over HCCL_H,HCOMM_H: 资源以算子和算法 Tag 为粒度复用
    HCCL_H->>+HCOMM_H: HcclEngineCtxGet(tag, engine)
    alt 资源已存在
        HCOMM_H-->>HCCL_H: 返回 Device Context
    else 首次执行
        HCOMM_H-->>HCCL_H: Context 不存在
        HCCL_H->>HCOMM_H: 创建 Context
        HCCL_H->>HCOMM_H: 申请并导出 Host/AICPU 控制 Thread
        HCCL_H->>HCOMM_H: 申请算法 Thread、Notify、Channel 和通信内存
        HCCL_H->>HCOMM_H: 序列化资源上下文并拷贝到 Device
        HCOMM_H-->>HCCL_H: 返回 Device Context
    end
    deactivate HCOMM_H

    Note over HCCL_H,HCCL_K: Host 先下发 Kernel，任务编排在 Kernel 启动后执行
    HCCL_H->>+HCOMM_H: Host Thread 通知 AICPU 控制 Thread
    HCOMM_H-->>-HCCL_H: HcclResult
    HCCL_H->>+RTS: aclrtLaunchKernelWithConfig 下发 AICPU Kernel
    RTS-->>HCCL_H: aclResult
    RTS->>HCCL_K: 启动 Kernel
    deactivate RTS
    HCCL_H->>+HCOMM_H: Host Thread 等待 AICPU 完成通知

    activate HCCL_K
    HCCL_K->>HCCL_K: 反序列化资源 Context
    HCCL_K->>+HCOMM_D: HcommBatchModeStart(tag)
    HCOMM_D-->>-HCCL_K: HcclResult
    HCCL_K->>+HCOMM_D: AICPU 控制 Thread 等待 Host 启动通知
    HCOMM_D-->>-HCCL_K: HcclResult

    Note over HCCL_K,HCOMM_D: Kernel 内执行算法任务编排
    HCCL_K->>HCCL_K: ExecOp(param, resCtx)
    HCCL_K->>+HCOMM_D: 编排 Thread 同步、Channel 同步和数据搬运任务
    HCOMM_D-->>-HCCL_K: HcclResult

    HCCL_K->>+HCOMM_D: AICPU 控制 Thread 通知 Host Thread
    HCOMM_D-->>-HCCL_K: HcclResult
    HCCL_K->>+HCOMM_D: HcommBatchModeEnd(tag)
    HCOMM_D-->>-HCCL_K: HcclResult
    deactivate HCCL_K

    HCOMM_H-->>-HCCL_H: AICPU 执行完成
    deactivate HCCL_H
```



各阶段的主要职责如下：

1. ***定义算子接口***：明确输入输出、数据量、数据类型、通信域和执行流等信息。
2. ***查询拓扑信息***：获取 rank 数量、拓扑层级、层内连接关系和可用链路，为算法选择和资源计算提供依据。
3. ***算法选择***：根据算子类型、执行引擎、拓扑形态、数据量和数据类型等条件选择已注册的算法实现。只有一种固定实现的自定义算子可以省略该步骤。
4. ***创建资源***：计算并申请 Thread、Notify、Channel、通信内存和资源 Context，并将 AICPU 执行所需的上下文拷贝到 Device。
5. ***下发 Kernel***：Host 与 AICPU 控制 Thread 建立启动同步关系，然后将 AICPU Kernel 下发到执行流。
6. ***任务编排***：AICPU Kernel 启动并取得资源上下文后，调用算法执行逻辑，将 Thread 同步、Channel 同步、数据搬运等操作编排到对应 Thread 上。
7. ***完成同步***：AICPU 侧完成编排后通知 Host，Host 侧等待该通知，保证通信任务与业务流的执行顺序。