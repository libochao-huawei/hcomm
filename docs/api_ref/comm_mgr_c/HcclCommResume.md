# HcclCommResume

> [!NOTE]说明
> 本接口为预留接口，后续有可能变更，不支持开发者使用。

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：不支持

> [!NOTE]说明
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

本接口用于恢复通信域的状态。

若开发者调用[HcclCommSuspend](HcclCommSuspend.md)接口或者acl提供的aclrtDeviceTaskAbort接口挂起了通信域，故障恢复后，需要调用本接口将通信域恢复为正常状态。

## 函数原型

```c
HcclResult HcclCommResume(HcclComm comm)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 需要从挂起状态恢复为正常状态的通信域。<br>HcclComm类型的定义可参见[HcclComm](./data_type_definition/HcclComm.md)。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

- 调用本接口前，需要调用acl提供的aclrtDeviceTaskAbort接口停止本Device上的任务执行。
- 调用本接口恢复通信域状态前，需要进行一次集群同步操作。
