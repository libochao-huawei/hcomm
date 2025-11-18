# HCCL 代码示例

本目录提供了不同场景下使用 HCCL 接口实现集合通信功能的示例代码。

## 通信域管理

- [每个进程管理一个 NPU 设备（基于 root 节点信息初始化通信域）](./01_communicators/01_one_device_per_process/)
- [每个进程管理一个 NPU 设备（基于 rank table 初始化通信域）](./01_communicators/02_one_device_per_process_rank_table/)
- [每个线程管理一个 NPU 设备（基于 root 节点信息初始化通信域）](./01_communicators/03_one_device_per_pthread/)

## 点对点通信

- [HcclSend/HcclRecv（基础收发功能）](./02_point_to_point/01_send_recv/)
- [HcclBatchSendRecv（实现 Ring 环状通信）](./02_point_to_point/02_batch_send_recv_ring/)

## 集合通信

- [AllReduce](./03_collectives/01_allreduce/)
- [Broadcast](./03_collectives/02_broadcast/)
- [AllGather](./03_collectives/03_allgather/)
- [ReduceScatter](./03_collectives/04_reduce_scatter/)
- [Reduce](./03_collectives/05_reduce/)
- [AlltoAll](./03_collectives/06_alltoall/)
- [AlltoAllV](./03_collectives/07_alltoallv/)
- [AlltoAllVC](./03_collectives/08_alltoallvc/)
- [Scatter](./03_collectives/09_scatter/)

## AI 框架

- [PyTorch](./04_ai_framework/01_pytorch/)
- [Tensorflow](./04_ai_framework/02_tensorflow/)
