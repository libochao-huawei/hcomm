# HcommBatchTransferOnThread

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持

> [!NOTE]说明
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

在指定线程和通道上异步提交一组传输任务。每个传输任务由[HcommBatchTransferDesc](../../../datatype_definition/HcommBatchTransferDesc.md)描述，支持单边写、单边读、写规约、带通知的写以及通知记录/等待等操作类型。

该接口将所有描述符合并为一批RDMA Work Request统一提交到网卡，减少PCIe Doorbell提交次数，适用于小包聚合、流水线传输等需要批量提交的场景，可显著提升传输效率。

## 函数原型

```c
int32_t HcommBatchTransferOnThread(ThreadHandle thread, ChannelHandle channel,
    const HcommBatchTransferDesc *transferDescs, uint32_t transferDescNum)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| thread | 输入 | 通信线程句柄，为通过[HcclThreadAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclThreadAcquire.md)接口获取到的threads。<br>ThreadHandle类型的定义可参见[ThreadHandle](../../../datatype_definition/ThreadHandle.md)。 |
| channel | 输入 | 通信通道句柄，为通过[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口获取到的channels。<br>ChannelHandle类型的定义可参见[ChannelHandle](../../../datatype_definition/ChannelHandle.md)。 |
| transferDescs | 输入 | 批量传输描述符数组，数组中每个元素描述一个传输任务，按数组顺序依次提交。描述符定义参见[HcommBatchTransferDesc](../../../datatype_definition/HcommBatchTransferDesc.md)。 |
| transferDescNum | 输入 | 描述符数量，须大于0。 |

## 返回值

int32_t：接口成功返回0，其他失败。

| 返回值 | 描述 |
| --- | --- |
| 0 | 执行成功。 |
| HCCL_E_PTR | thread或transferDescs为空指针。 |
| HCCL_E_PARA | transferDescNum为0。 |
| HCCL_E_NOT_SUPPORT | A5路径或不支持的传输类型。 |

## 约束说明

1. 该接口为异步提交，调用返回不代表传输完成，需配合通知机制（notifyRecord/notifyWait）或流同步来确认传输完成。
2. 当前实现中，仅HCOMM_TRANSFER_TYPE_WRITE和HCOMM_TRANSFER_TYPE_READ类型被底层ibverbs传输层完整支持，其他类型（reduce、notify等）返回HCCL_E_NOT_SUPPORT。
3. transferDescs数组中的描述符按顺序提交，但不保证依次完成的顺序。
4. 调用前须确保thread和channel已正确初始化，且涉及的内存地址已通过内存注册机制获取。

## 调用示例

```c
// 申请通信线程资源和通道资源（略）
// 获取本端和对端通信内存信息（略）

// 构造批量传输描述符
HcommBatchTransferDesc descs[3];

// 描述符1：单边写
descs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
descs[0].transferInfo.write.len = 4096;
descs[0].transferInfo.write.dst = remoteBuffer;      // 远端地址
descs[0].transferInfo.write.src = localBuffer;        // 本地地址

// 描述符2：记录通知
descs[1].transType = HCOMM_TRANSFER_TYPE_NOTIFY_RECORD;
descs[1].transferInfo.notifyRecord.notifyIdx = 0;

// 描述符3：带通知的单边写
descs[2].transType = HCOMM_TRANSFER_TYPE_WRITE_WITH_NOTIFY;
descs[2].transferInfo.writeWithNotify.len = 8192;
descs[2].transferInfo.writeWithNotify.dst = remoteBuffer2;  // 远端地址
descs[2].transferInfo.writeWithNotify.src = localBuffer2;    // 本地地址
descs[2].transferInfo.writeWithNotify.notifyIdx = 1;

// 提交批量传输
HcommBatchTransferOnThread(thread, channel, descs, 3);
```
