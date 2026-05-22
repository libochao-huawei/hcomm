# HcommChannelNotifyWaitOnThreadWithDefaultTimeout

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持

## 功能说明

等待同步信号，阻塞等待指定Channel上的Notify完成。**使用通过HcommSetNotifyWaitTimeOut设置的默认超时时间**，无需手动传入超时参数。

## 函数原型

```c
int32_t HcommChannelNotifyWaitOnThreadWithDefaultTimeout(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| thread | 输入 | 通信线程句柄。针对Ascend 950PR/Ascend 950DT的CPU引擎RoCE场景，该参数无作用，传入0即可；CPU_TS/AICPU_TS场景下，为通过[HcclThreadAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclThreadAcquire.md)接口获取到的threads。<br>ThreadHandle类型的定义可参见[ThreadHandle](../../../datatype_definition/ThreadHandle.md)。 |
| channel | 输入 | 通信通道句柄，为通过[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口获取到的channels。关于channel的约束参见约束说明。<br>ChannelHandle类型的定义可参见[ChannelHandle](../../../datatype_definition/ChannelHandle.md)。 |
| localNotifyIdx | 输入 | 本地Notify索引。<br>取值范围：[0, notifyNum)。<br>notifyNum为[HcommChannelCreate](../../../control_plane_api/basic_resource_mgmt/HcommChannelCreate.md)或[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口传入的channelDesc参数中的notifyNum。 |

## 返回值

int32_t：接口成功返回0，其他失败。

## 默认超时时间说明

- 默认超时时间通过[HcommSetNotifyWaitTimeOut](./HcommSetNotifyWaitTimeOut.md)接口设置
- 如果未手动设置，在非AICPU模式下会自动在默认值基础上增加50秒的偏移量
- 设置为0表示永久等待
- 设置大于0表示具体的超时时间（单位：秒）

## 约束说明

- 该接口需要配合[HcommChannelNotifyRecordOnThread](HcommChannelNotifyRecordOnThread.md)使用。
- 针对Ascend 950PR/Ascend 950DT，支持AICPU_TS场景在Device侧调用，也支持CPU引擎RoCE场景在Host CPU侧调用。
- 针对Ascend 950PR/Ascend 950DT的CPU引擎RoCE场景，调用[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)申请入参channel时，需传入`engine = COMM_ENGINE_CPU`，且`channelDesc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE`。URMA/UBC等协议通道当前不支持该接口。
- Host CPU侧调用时，`thread`参数无作用，可传入0。
- 针对Ascend 950PR/Ascend 950DT的CPU引擎RoCE场景，`localNotifyIdx`必须小于本端通信通道的Notify数量，且通信通道创建时的`notifyNum`需大于0；`timeout`需大于0。

## 调用示例

```c
// 1. 设置默认超时时间（可选，未设置则使用默认值）
uint32_t defaultTimeout = 1800000;  // 30分钟
HcommSetNotifyWaitTimeOut(defaultTimeout);

// 2. 申请通信线程资源
CommEngine engine = COMM_ENGINE_AICPU_TS;  // Ascend 950PR/Ascend 950DT时配置
uint32_t threadNum = 1;
uint32_t notifyNumPerThread = 1;
ThreadHandle thread;
HcommThreadAcquire(engine, threadNum, notifyNumPerThread, &thread);

// 3. 申请通信通道资源
uint32_t channelNum = 1;
HcclChannelDesc channelDesc;
HcommChannelDescInit(&channelDesc, channelNum);
HcclComm comm;
ChannelHandle channel;
HcommChannelAcquire(comm, engine, &channelDesc, channelNum, &channel);

// 4. 在Device侧记录通知
HcommChannelNotifyRecordOnThread(thread, channel, 0);

// 5. 数据面操作
// ...

// 6. 等待对端通知（使用默认超时时间，无需手动传入）
HcommChannelNotifyWaitOnThreadWithDefaultTimeout(thread, channel, 0);
```