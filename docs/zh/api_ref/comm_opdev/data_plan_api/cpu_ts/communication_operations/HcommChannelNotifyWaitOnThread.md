# HcommChannelNotifyWaitOnThread

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持

> [!NOTE]说明
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

等待同步信号，阻塞等待Thread的运行，直到指定的Notify完成。

## 函数原型

```c
int32_t HcommChannelNotifyWaitOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| thread | 输入 | 通信线程句柄，为通过[HcclThreadAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclThreadAcquire.md)接口获取到的threads。<br>ThreadHandle类型的定义可参见[ThreadHandle](../../../datatype_definition/ThreadHandle.md)。 |
| channel | 输入 | 通信通道句柄，为通过[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口获取到的channels。<br>ChannelHandle类型的定义可参见[ChannelHandle](../../../datatype_definition/ChannelHandle.md)。 |
| localNotifyIdx | 输入 | 本地Notify索引。<br>取值范围：[0, [HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口传入的channelDescs参数中的notifyNum)。 |
| timeout | 输入 | 超时时间，单位：毫秒。<br>  - 0：表示永久等待。<br>  - >0：配置的具体超时时间。<br>说明：针对 Ascend 950PR/Ascend 950DT ，暂不支持自定义超时功能，固定为 1080000 毫秒。 |

## 返回值

int32_t：接口成功返回0，其他失败。

## 约束说明

该接口需要配合[HcommChannelNotifyRecordOnThread](HcommChannelNotifyRecordOnThread.md)使用。

在 Ascend 950PR/Ascend 950DT 上，仅支持 AICPU_TS 模式下、在 Device 侧调用该接口。

## 调用示例

```c
// 申请通信线程资源
CommEngine engine = CommEngine::COMM_ENGINE_CPU_TS;
CommEngine engine = CommEngine::COMM_ENGINE_AICPU_TS;  // Ascend 950PR/Ascend 950DT时配置
uint32_t threadNum = 1;
uint32_t notifyNumPerThread = 1;
ThreadHandle thread;
HcclThreadAcquire(engine, threadNum, notifyNumPerThread, &thread);

// 申请通信通道资源
uint32_t channelNum = 1;
HcclChannelDesc channelDesc;
HcclChannelDescInit(&channelDesc, channelNum);
HcclComm comm;
ChannelHandle channel;
HcclChannelAcquire(comm, engine, &channelDesc, channelNum, &channel);

// 针对Ascend 950PR/Ascend 950DT，需要在 Device 侧调用以下接口

// 通知对端
HcommChannelNotifyRecordOnThread(thread, channel, 0);

// 数据面操作
// ...

// 等待对端通知本端
uint32_t notifyTimeout = 1800;
HcommChannelNotifyWaitOnThread(thread, channel, 0, notifyTimeout);
```
