# HcclCommInitRootInfo

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持

> [!CAUTION]注意
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
> 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。

## 功能说明

根据rootInfo初始化HCCL，创建HCCL通信域。

该接口在同一进程内支持多线程并发调用，但仅支持单卡单线程的场景，若是单卡多线程，不支持并发调用。

如下图所示，不支持step0与step1并发调用，需要step0执行结束后，再串行执行step1。

![不支持并发调用](figures/comminit_rootinfo.png)

## 函数原型

```c
HcclResult HcclCommInitRootInfo(uint32_t nRanks, const HcclRootInfo *rootInfo, uint32_t rank, HcclComm *comm)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| nRanks | 输入 | 集群中的rank数量。 |
| rootInfo | 输入 | root rank信息，主要包含root rank的ip、id等信息，由[HcclGetRootInfo](HcclGetRootInfo.md)接口生成。 |
| rank | 输入 | 本rank的rank id。 |
| comm | 输出 | 初始化后的通信域指针。<br>HcclComm类型的定义可参见[HcclComm](./data_type_definition/HcclComm.md)。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

同一通信域中所有rank的nRanks、rootInfo均应相同。

## 调用示例

```c
uint32_t rankSize = 8;
uint32_t deviceId = 0;
// 生成 root 节点的 rank 标识信息
HcclRootInfo rootInfo;
HcclGetRootInfo(&rootInfo);
// 初始化通信域
HcclComm hcclComm;
HcclCommInitRootInfo(rankSize, &rootInfo, deviceId, &hcclComm);
// 销毁通信域
HcclCommDestroy(hcclComm);
```
