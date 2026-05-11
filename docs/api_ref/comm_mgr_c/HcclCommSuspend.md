# HcclCommSuspend

> [!NOTE]说明
> 本接口为预留接口，后续有可能变更，不支持开发者使用。

## 产品支持情况

<cann-filter npu-type="950">

- Ascend 950PR/Ascend 950DT：不支持</cann-filter>
<cann-filter npu-type="A3">
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持</cann-filter>
<cann-filter npu-type="910b">
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持</cann-filter>
<cann-filter npu-type="310p">
- Atlas 推理系列产品：不支持</cann-filter>
<cann-filter npu-type="910">
- Atlas 训练系列产品：不支持</cann-filter>

<cann-filter npu-type="910b">

> [!NOTE]说明
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
</cann-filter>

## 功能说明

当片上内存UCE（uncorrect error）故障时（ACL接口返回ACL_ERROR_RT_DEVICE_MTE_ERROR错误码），可调用本接口将通信域置为挂起状态。

使用此接口挂起通信域，无需退出Host侧进程，后续故障修复后，可调用[HcclCommResume](HcclCommResume.md)接口恢复通信域状态。

## 函数原型

```c
HcclResult HcclCommSuspend(HcclComm comm)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 需要将状态置为挂起的通信域。<br>HcclComm类型的定义可参见[HcclComm](./data_type_definition/HcclComm.md)。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

- 本接口需要与[HcclCommResume](HcclCommResume.md)接口配对使用。
- 本接口不能与集合通信、点对点通信的相关接口并发执行。
