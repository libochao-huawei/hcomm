# HcommChannelFenceOnThread

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

在指定通信线程和通信通道上插入内存屏障操作，确保屏障前的通道读写操作在屏障后的通道读写操作之前完成。

## 函数原型

```c
int32_t HcommChannelFenceOnThread(ThreadHandle thread, ChannelHandle channel)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| thread | 输入 | 通信线程句柄，为通过[HcclThreadAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclThreadAcquire.md)接口获取到的threads。<br>ThreadHandle类型的定义可参见[ThreadHandle](../../../datatype_definition/ThreadHandle.md)。 |
| channel | 输入 | 通信通道句柄，为通过[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口获取到的channels。<br>ChannelHandle类型的定义可参见[ChannelHandle](../../../datatype_definition/ChannelHandle.md)。 |

## 返回值

int32_t：接口成功返回0，其他失败。

## 约束说明

在  Ascend 950PR/Ascend 950DT  上，仅支持通信协议 UBC_TP、UBC_CTP、UBoE。

## 调用示例

```c
// 申请通信线程资源
CommEngine engine = CommEngine::COMM_ENGINE_CPU_TS; // Atlas A3 训练系列产品/Atlas A3 推理系列产品使用
CommEngine engine = CommEngine::COMM_ENGINE_AICPU_TS; // Ascend 950PR/Ascend 950DT使用
uint32_t threadNum = 1;
uint32_t notifyNumPerThread = 1;
ThreadHandle thread;
HcclThreadAcquire(engine, threadNum, notifyNumPerThread, &thread);

// 申请通信通道资源
HcclChannelDesc channelDesc;
HcclChannelDescInit(&channelDesc, channelNum);
HcclComm comm;
uint32_t channelNum = 1;
ChannelHandle channel;
HcclChannelAcquire(comm, engine, &channelDesc, channelNum, &channel);

// 获取本端通信内存信息
void * localBuffer;
uint64_t localBufferSize;
HcclGetHcclBuffer(comm, &localBuffer, &localBufferSize);

// 获取对端通信内存信息
void * remoteBuffer;
uint64_t remoteBufferSize;
HcclChannelGetHcclBuffer(comm, channel, &remoteBuffer, &remoteBufferSize);
uint64_t len = std::min(localBufferSize, remoteBufferSize);

// 将本端内存的内容写到对端内存上
HcommWriteOnThread(thread, channel, remoteBuffer, localBuffer, len);
// 申请通信线程资源
CommEngine engine = CommEngine::COMM_ENGINE_CPU_TS; // Atlas A3 训练系列产品/Atlas A3 推理系列产品使用
CommEngine engine = CommEngine::COMM_ENGINE_AICPU_TS; // Ascend 950PR/Ascend 950DT使用
uint32_t threadNum = 1;
uint32_t notifyNumPerThread = 1;
ThreadHandle thread;
HcclThreadAcquire(engine, threadNum, notifyNumPerThread, &thread);

// 申请通信通道资源
HcclChannelDesc channelDesc;
HcclChannelDescInit(&channelDesc, channelNum);
HcclComm comm;
uint32_t channelNum = 1;
ChannelHandle channel;
HcclChannelAcquire(comm, engine, &channelDesc, channelNum, &channel);

// 获取本端通信内存信息
void * localBuffer;
uint64_t localBufferSize;
HcclGetHcclBuffer(comm, &localBuffer, &localBufferSize);

// 获取对端通信内存信息
void * remoteBuffer;
uint64_t remoteBufferSize;
HcclChannelGetHcclBuffer(comm, channel, &remoteBuffer, &remoteBufferSize);
uint64_t len = std::min(localBufferSize, remoteBufferSize);

// 将本端内存的内容写到对端内存上
HcommWriteOnThread(thread, channel, remoteBuffer, localBuffer, len);
HcommChannelFenceOnThread(thread, channel);
HcommReadOnThread(thread, channel, localBuffer, remoteBuffer, len);
```
