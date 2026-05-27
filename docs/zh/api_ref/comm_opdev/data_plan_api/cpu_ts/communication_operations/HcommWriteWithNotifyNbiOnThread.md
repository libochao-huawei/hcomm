# HcommWriteWithNotifyNbiOnThread

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

向channel上的指定内存写数据，将src中长度为len的内存数据写入dst所指向的相同长度的内存区域，并向dst所在节点发送同步信号。接口调用方为src所在节点，该接口为非阻塞接口。

## 函数原型

```c
int32_t HcommWriteWithNotifyNbiOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| thread | 输入 | 通信线程句柄，当前无作用，传入 0 即可。 |
| channel | 输入 | 通信通道句柄，为通过[HcommChannelCreate](../../../control_plane_api/basic_resource_mgmt/HcommChannelCreate.md)或[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口获取到的channels。<br>ChannelHandle类型的定义可参见[ChannelHandle](../../../datatype_definition/ChannelHandle.md)。 |
| dst | 输出 | 目的内存地址。基础通信场景下，写发起端应使用通过[HcommMemImport](../../../control_plane_api/basic_resource_mgmt/HcommMemImport.md)导入的对端内存地址；集合通信场景下，为使用[HcclChannelGetHcclBuffer](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelGetHcclBuffer.md)获取到的对端通信内存地址。 |
| src | 输入 | 源内存地址。基础通信场景下，为通过[HcommMemReg](../../../control_plane_api/basic_resource_mgmt/HcommMemReg.md)注册的本端内存地址；集合通信场景下，为使用[HcclGetHcclBuffer](../../../control_plane_api/comms_domain_resource_mgmt/HcclGetHcclBuffer.md)、[HcclChannelGetHcclBuffer](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelGetHcclBuffer.md)获取到的本端内存地址。 |
| len | 输入 | 数据长度（字节），需大于0。 |
| remoteNotifyIdx | 输入 | 通信通道另一端的Notify索引。<br>取值范围：[0, notifyNum)。<br>notifyNum为[HcommChannelCreate](../../../control_plane_api/basic_resource_mgmt/HcommChannelCreate.md)或[HcclChannelAcquire](../../../control_plane_api/comms_domain_resource_mgmt/HcclChannelAcquire.md)接口传入的channelDesc参数中的notifyNum。 |

## 返回值

int32_t：接口成功返回0，其他失败。

## 约束说明

- 针对Ascend 950PR/Ascend 950DT，仅支持通信引擎CPU，仅支持通信协议RoCE，仅支持在Host CPU上调用。
- `[src, src + len)`必须落在本端已注册或获取的内存范围内，`[dst, dst + len)`必须落在对端已导入或获取的内存范围内。
- `remoteNotifyIdx`必须小于通信通道另一端的Notify数量，且通信通道创建时的`notifyNum`需大于0。
- 该接口需要配合[HcommChannelNotifyWaitOnThread](HcommChannelNotifyWaitOnThread.md)使用，写入目标端应调用该接口等待同步信号。
- 该接口返回成功仅表示带通知写请求提交成功。如需确认本端通道上已提交的写操作完成，调用方应调用[HcommChannelFenceOnThread](HcommChannelFenceOnThread.md)等待通道上已提交的写操作完成。

## 调用示例

### 集合通信示例

以下代码在写发起端调用，将本端通信内存写入对端通信内存，并通知对端。

```c
// 省略：创建通信域句柄 comm

// 申请通信通道资源
CommEngine engine = CommEngine::COMM_ENGINE_CPU;
uint32_t channelNum = 1;
HcclChannelDesc channelDesc;
HcclResult result = HcclChannelDescInit(&channelDesc, channelNum);
channelDesc.channelProtocol = COMM_PROTOCOL_ROCE;
channelDesc.localEndpoint.protocol = COMM_PROTOCOL_ROCE;
channelDesc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
channelDesc.notifyNum = 1;
// 省略：channelDesc填充其他信息
ChannelHandle channel;
result = HcclChannelAcquire(comm, engine, &channelDesc, channelNum, &channel);

// 获取本端通信内存信息
void * localBuffer;
uint64_t localBufferSize;
result = HcclGetHcclBuffer(comm, &localBuffer, &localBufferSize);

// 获取对端通信内存信息
void * remoteBuffer;
uint64_t remoteBufferSize;
result = HcclChannelGetHcclBuffer(comm, channel, &remoteBuffer, &remoteBufferSize);
uint64_t len = std::min(localBufferSize, remoteBufferSize);

// 将本端内存的内容写到对端内存上并通知对端
uint32_t rmtNotifyIdx = 0;
int32_t ret = HcommWriteWithNotifyNbiOnThread(0, channel, remoteBuffer, localBuffer, len, rmtNotifyIdx);

// 等待通信通道上已提交的写操作完成
ret = HcommChannelFenceOnThread(0, channel);
```

以下代码在写入目标端调用，等待写发起端发送的同步信号。

```c
// 省略：资源创建

// 使用本地Notify索引等待写发起端发送的同步信号
uint32_t lclNotifyIdx = 0;
uint32_t notifyTimeout = 0;
int32_t ret = HcommChannelNotifyWaitOnThread(0, channel, lclNotifyIdx, notifyTimeout);
```

### 基础通信示例

以Host RoCE H2D场景为例。
Client端为写发起端，提供Host源内存；Server端为写入目标端，提供Device目的内存，并等待Client端发送的Notify。

#### Server 端

```c
// 申请Server端Endpoint资源
const EndpointDesc serverEndpointDesc = {
    .protocol = COMM_PROTOCOL_ROCE,
    .commAddr = {
        .type = COMM_ADDR_TYPE_IP_V4,
        .addr = {{192, 168, 1, 101}}
    },
    .loc = {
        .locType = ENDPOINT_LOC_TYPE_HOST
    },
    .raws = {0}
};
EndpointHandle serverEndpointHandle = nullptr;
HcommResult result = HcommEndpointCreate(&serverEndpointDesc, &serverEndpointHandle);

// 注册Server端内存
CommMem serverDeviceMem = {
    .type = COMM_MEM_TYPE_DEVICE,
    .addr = serverDeviceAddr,
    .size = dataLen
};
HcommMemHandle serverMemHandle = nullptr;
result = HcommMemReg(serverEndpointHandle, "server_device_mem", &serverDeviceMem, &serverMemHandle);

// 导出Server端内存描述
void *serverMemDesc = nullptr;
uint32_t serverMemDescLen = 0;
result = HcommMemExport(serverEndpointHandle, serverMemHandle, &serverMemDesc, &serverMemDescLen);

// 省略：
// Server端将serverEndpointDesc、serverMemDesc、serverMemDescLen发送给Client端
// Server端获取clientEndpointDesc

// 申请Server端通信通道资源
HcommChannelDesc serverChannelDesc = {};
result = HcommChannelDescInit(&serverChannelDesc, 1);
serverChannelDesc.remoteEndpoint = clientEndpointDesc;
serverChannelDesc.notifyNum = 1;
serverChannelDesc.exchangeAllMems = true; // 让Channel创建时自动关联本端memHandle。
serverChannelDesc.role = HCOMM_SOCKET_ROLE_SERVER;
serverChannelDesc.roceAttr.queueNum = 1;
ChannelHandle serverChannel = 0;
result = HcommChannelCreate(serverEndpointHandle, COMM_ENGINE_CPU, &serverChannelDesc, 1, &serverChannel);

// 等待Client端写入数据并发送Notify
uint32_t lclNotifyIdx = 0;
uint32_t notifyTimeout = 0;
int32_t ret = HcommChannelNotifyWaitOnThread(0, serverChannel, lclNotifyIdx, notifyTimeout);
```

#### Client 端

```c
// 申请Client端Endpoint资源
const EndpointDesc clientEndpointDesc = {
    .protocol = COMM_PROTOCOL_ROCE,
    .commAddr = {
        .type = COMM_ADDR_TYPE_IP_V4,
        .addr = {{192, 168, 1, 100}}
    },
    .loc = {
        .locType = ENDPOINT_LOC_TYPE_HOST
    },
    .raws = {0}
};
EndpointHandle clientEndpointHandle = nullptr;
result = HcommEndpointCreate(&clientEndpointDesc, &clientEndpointHandle);

// 注册Client端内存
CommMem clientHostMem = {
    .type = COMM_MEM_TYPE_HOST,
    .addr = clientHostAddr,
    .size = dataLen
};
HcommMemHandle clientMemHandle = nullptr;
result = HcommMemReg(clientEndpointHandle, "client_host_mem", &clientHostMem, &clientMemHandle);

// 省略：
// Client端将clientEndpointDesc发送给Server端
// Client端获取serverEndpointDesc、serverMemDesc、serverMemDescLen

// 导入Server端内存描述
CommMem importedServerDeviceMem = {};
result = HcommMemImport(clientEndpointHandle, serverMemDesc, serverMemDescLen, &importedServerDeviceMem);

// 申请Client端通信通道资源
HcommChannelDesc clientChannelDesc = {};
result = HcommChannelDescInit(&clientChannelDesc, 1);
clientChannelDesc.remoteEndpoint = serverEndpointDesc;
clientChannelDesc.notifyNum = 1;
clientChannelDesc.exchangeAllMems = true; // 让Channel创建时自动关联本端memHandle。
clientChannelDesc.role = HCOMM_SOCKET_ROLE_CLIENT;
clientChannelDesc.roceAttr.queueNum = 1;
ChannelHandle clientChannel = 0;
result = HcommChannelCreate(clientEndpointHandle, COMM_ENGINE_CPU, &clientChannelDesc, 1, &clientChannel);

// 将Client端内存写入Server端内存，并通知Server端
uint32_t rmtNotifyIdx = 0;
int32_t ret = HcommWriteWithNotifyNbiOnThread(0, clientChannel, importedServerDeviceMem.addr,
    clientHostMem.addr, dataLen, rmtNotifyIdx);

// 等待通信通道上已提交的写操作完成
ret = HcommChannelFenceOnThread(0, clientChannel);
```
