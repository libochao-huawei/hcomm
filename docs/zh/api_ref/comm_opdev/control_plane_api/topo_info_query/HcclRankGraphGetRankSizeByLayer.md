# HcclRankGraphGetRankSizeByLayer

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持

## 功能说明

给定通信域和拓扑层级编号，返回该层级下本rank所在拓扑实例的rank数量。

![拓扑模型](figures/topo_model.png)

以上述拓扑模型为例：

- Layer 0中包含两个拓扑实例，为方便理解，定义拓扑实例ID分别为0和1。
- Layer1中包含1个拓扑实例。

假设在rank 0上调用此接口，如果指定layer层级为0，则此接口返回的rank数量为3；如果指定layer层级为1，则此接口返回的rank数量为6。

## 函数原型

```c
HcclResult HcclRankGraphGetRankSizeByLayer(HcclComm comm, uint32_t netLayer, uint32_t *rankNum)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 通信域。<br>HcclComm类型的定义如下：<br>typedef void *HcclComm; |
| netLayer | 输入 | 拓扑层级编号。 |
| rankNum | 输出 | rank数量。 |

## 返回值

[HcclResult](../../../comm_mgr_c/data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

以[功能说明](#功能说明)的拓扑模型为例：

```c
HcclComm comm;
uint32_t rankNum;
HcclRankGraphGetRankSizeByLayer(comm, 0, &rankNum);
// rankNum=3
HcclRankGraphGetRankSizeByLayer(comm, 1, &rankNum);
// rankNum=6
```
