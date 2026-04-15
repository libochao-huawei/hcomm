# HcclGetRankSize

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持

> [!CAUTION]注意
>针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

查询指定通信域的rank数量。

## 函数原型

```c
HcclResult HcclGetRankSize(HcclComm comm, uint32_t *rankSize)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 集合通信操作所在的通信域。<br>HcclComm类型的定义如下：<br>typedef void *HcclComm; |
| rankSize | 输出 | 通信域的rank数量。 |

## 返回值

[HcclResult](../../../comm_mgr_c/data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

以4机8卡的通信域为例，rank总数为32：

```c
uint32_t rankSize;
HcclComm comm;
HcclGetRankSize(comm, &rankSize);
// rankSize=32
```
