# HcclGetRankId

## 产品支持情况

<cann-filter npu-type="950">

- Ascend 950PR/Ascend 950DT：支持</cann-filter>
<cann-filter npu-type="A3">
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持</cann-filter>
<cann-filter npu-type="910b">
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持</cann-filter>
<cann-filter npu-type="310p">
- Atlas 推理系列产品：支持</cann-filter>
<cann-filter npu-type="910">
- Atlas 训练系列产品：支持</cann-filter>

<cann-filter npu-type="910b,310P">

> [!NOTE]说明
<cann-filter npu-type="910b">
> - 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。</cann-filter>
<cann-filter npu-type="310p">
> - 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。</cann-filter>
</cann-filter>

## 功能说明

获取device在指定通信域中对应的rank序号。

## 函数原型

```c
HcclResult HcclGetRankId(HcclComm comm, uint32_t *rank)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 集合通信操作所在的通信域。<br>HcclComm类型的定义可参见[HcclComm](./data_type_definition/HcclComm.md)。 |
| rank | 输出 | 指定集合通信域中rank序号的输出地址指针。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

```c
// 初始化通信域
HcclComm comm;
// 获取当前 Device 的 RankId
uint32_t rank;
HcclGetRankId(comm, &rank);
```
