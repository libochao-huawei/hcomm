# HcommChannelGetPtrByHandle

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

该接口用于获取通信通道在Device侧的通道实体指针，支持批量查询。

通过已创建的通道句柄，解析并获取对应通道在Device侧的通道实体（ChannelEntity）指针，供Device侧通信栈使用。

该接口通常在[HcommChannelCreate](HcommChannelCreate.md)创建通道且[HcommChannelGetStatus](HcommChannelGetStatus.md)确认通道就绪后调用，用于将通道实体指针传递给Device侧算子或内核使用。

## 函数原型

```c
HcommResult HcommChannelGetPtrByHandle(const ChannelHandle *channelList, uint32_t listNum, uint64_t *channelPtr);
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| channelList | 输入 | 待查询的通道句柄数组，每个元素标识一个已创建的通信通道。<br>ChannelHandle类型的定义请参见[ChannelHandle](../../datatype_definition/ChannelHandle.md)。<br>该参数不能为空指针，数组中每个通道句柄必须是通过[HcommChannelCreate](HcommChannelCreate.md)创建的有效句柄。 |
| listNum | 输入 | 待查询的通道数量。<br>单位为"个"，取值范围：[1, 1048576]。<br>该参数必须大于 0。 |
| channelPtr | 输出 | Device侧通道实体指针数组，用于返回每个通道对应的Device侧通道实体指针，与channelList一一对应。<br>该参数不能为空指针。<br>调用者分配的数组，至少包含listNum个元素的空间。<br>每个元素的值为uint64_t类型，表示Device侧ChannelEntity结构的地址。 |

## 返回值

HcommResult：接口成功返回0，其他失败。

## 约束说明

- channelList数组长度需要与listNum参数一致。
- channelPtr\[i\]与channelList\[i\]一一对应，表示第i个通道的Device侧实体指针。
- 当前支持的通道类型为：1. 基于AIV引擎ROCE协议的通道 2. 基于AIV引擎UBC_TP、UBC_CTP协议的通道。

## 调用示例

```c
uint32_t channelNum = 4;
ChannelHandle channels[4] = {0};
// 参考 HcommChannelCreate 进行 Channel 创建
// 参考 HcommChannelGetStatus 确认通道就绪
...

// 获取通道的Device侧实体指针
uint64_t devChannelPtrs[4] = {0};
HcommResult ret = HcommChannelGetPtrByHandle(channels, channelNum, devChannelPtrs);
if (ret != 0) {
    printf("Failed to get channel ptr, ret = %d\n", ret);
    return ret;
}

// devChannelPtrs 可传递给Device侧算子或内核使用
// ...
```
