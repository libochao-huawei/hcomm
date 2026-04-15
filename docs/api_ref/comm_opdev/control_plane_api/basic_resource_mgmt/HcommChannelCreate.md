# HcommChannelCreate

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

该接口为创建通信通道的资源管理接口，基于已创建的网络端点（Endpoint），根据给定的通道描述信息批量创建通信通道，为点对点通信或集合通信提供数据传输的基础设施。

## 函数原型

```c
HcommResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine, HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels);
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| endpointHandle | 输入 | 网络设备端点句柄，标识一个已创建的本地网络设备端点。<br>EndpointHandle类型的定义请参见[EndpointHandle](../../datatype_definition/EndpointHandle.md)，该句柄必须通过[HcommEndpointCreate](HcommEndpointCreate.md)成功创建，且未销毁。 |
| engine | 输入 | 通信引擎类型，指定通道的执行位置。<br>CommEngine类型的定义请参见[CommEngine](../../datatype_definition/CommEngine.md)。<br>需要注意：必须是有效的引擎类型。 |
| channelDescs | 输入 | 通道描述符数组，每个元素描述一个待创建通道的属性信息。<br>HcommChannelDesc类型的定义请参见[HcommChannelDesc](../../datatype_definition/HcommChannelDesc.md)。<br>数组元素数量必须等于channelNum，每个元素需正确填充必要字段。 |
| channelNum | 输入 | 待创建的通道数量。<br>单位：个，取值范围：[1, 1048576]。<br>该参数需要大于 0。 |
| channels | 输出 | 通道句柄数组，用于返回创建成功的通道句柄列表。<br>ChannelHandle类型的定义请参见[ChannelHandle](../../datatype_definition/ChannelHandle.md)。<br>调用者分配的数组，需要至少包含channelNum个元素的空间。 |

## 返回值

HcommResult：接口成功返回0，其他失败。

## 约束说明

- channelDescs数组长度必须与channelNum参数一致。
- HcommChannelDesc中的remoteEndpoint必须正确填充远端端点信息。
- 当HcommChannelDesc中exchangeAllMems为false时，必须配置memHandles和memHandleNum。
- 当前CommEngine配置为CCU时，仅支持交换1份memHandle。
- 当前CommEngine配置为CCU时，不支持外部配置NotifyNum，默认为8个CCU Notify。

## 调用示例

```c
// 1. 调用 HcclRankGraphGetLinks 获取链路信息
CommLink *linkList = nullptr;
uint32_t listSize;
CHK_RET(HcclRankGraphGetLinks(comm, netLayer, myRank, rank, &linkList, &listSize));

// 2. 遍历每个 CommLink，填充 HcommChannelDesc
uint32_t channelNum = listSize;
std::vector<HcommChannelDesc> hcommDescVec(channelNum);
HcommChannelDescInit(hcommDescVec.data(), hcommDescVec.size());
for (uint32_t idx = 0; idx < listSize; idx++) {
  HcommChannelDesc channelDesc;
  CommLink link = linkList[idx];

  //  核心映射：从 CommLink 提取 Endpoint 信息
  channelDesc.localEndpoint.protocol = link.srcEndpointDesc.protocol;
  channelDesc.localEndpoint.commAddr = link.srcEndpointDesc.commAddr;
  channelDesc.localEndpoint.loc    = link.srcEndpointDesc.loc;
  channelDesc.remoteEndpoint.protocol = link.dstEndpointDesc.protocol;
  channelDesc.remoteEndpoint.commAddr = link.dstEndpointDesc.commAddr;
  channelDesc.remoteEndpoint.loc   = link.dstEndpointDesc.loc;
  channelDesc.channelProtocol     = link.linkAttr.linkProtocol;
  channelDesc.notifyNum = NORMAL_NOTIFY_NUM;

  hcommDescVec[idx] = channelDesc;

  // socket配置为空
  HcommSocket hcommSocket = nullptr;
  hcommDescVec[idx].socket = hcommSocket;
}

// 参考 MyRank 下 BatchCreateChannels 获取 EndpointHandle
EndpointHandle epHandle = nullptr;
...

// 3. 批量创建 Channel
CommEngine engine = CommEngine::COMM_ENGINE_CPU_TS;
std::vector<ChannelHandle> channels(channelNum);
HcommChannelCreate(epHandle, engine, hcommDescVec.data(), channelNum, channels.data());
```
