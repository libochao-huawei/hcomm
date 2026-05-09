# HcclRankGraphGetTopoType

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

给定通信域和拓扑层级编号，查询当前rank在指定拓扑实例中对应的拓扑类型。

## 函数原型

```c
HcclResult HcclRankGraphGetTopoType(HcclComm comm, uint32_t netLayer, uint32_t topoInstId, CommTopo *topoType)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 通信域。<br>HcclComm类型的定义如下：<br>typedef void *HcclComm; |
| netLayer | 输入 | 拓扑层级编号。 |
| topoInstId | 输入 | 拓扑实例ID。 |
| topoType | 输出 | 拓扑类型。<br>CommTopo类型的定义请参见[CommTopo](../../datatype_definition/CommTopo.md)。 |

## 返回值

[HcclResult](../../../comm_mgr_c/data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

```c
HcclComm comm;
CommTopo topoType;
HcclRankGraphGetTopoType(comm, netLayer=0, topoInstId=0, &topoType); // topoType=1 (1DMesh)
```
