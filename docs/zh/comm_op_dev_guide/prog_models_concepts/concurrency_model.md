# 并发模型

通信算子由不同通信任务组成。如果某些通信步骤没有资源冲突，则可以并发执行。HCCL在算子编程模型中提供了并发模型，并发模型需要定义并发单元以及并发单元之间的同步行为，如下图所示。

![并发模型](figures/concurrency_model.png)

- 并发单元：提供抽象为Thread的并发单元，通信任务与Thread绑定，不同Thread间的操作可以并发执行。
- 并发单元间的同步：
  - 一个Thread可以包含多个Notify实例，创建Thread时可以指定包含的Notify实例数量。
  - 在一个通信对象内，一个Thread可以向另一个Thread发送同步信号，一个Thread可以等待另一个Thread的同步信号，具体接口请参见[本地操作](../../api_ref/comm_opdev/data_plan_api/cpu-cpu_ts-aicpu_ts/local_operations/README.md)。
