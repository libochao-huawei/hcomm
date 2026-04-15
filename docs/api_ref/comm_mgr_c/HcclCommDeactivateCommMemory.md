# HcclCommDeactivateCommMemory

> [!CAUTION]注意
> 本接口为试用接口，后续可能存在变更，暂不支持应用于商用产品。

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：不支持

> [!CAUTION]注意
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

将已经激活的虚拟内存反激活，反激活后如果再使用该地址进行集合通信，将不会使能零拷贝功能。

## 函数原型

```c
HcclResult HcclCommDeactivateCommMemory(HcclComm comm, void *virPtr)
```

## 参数说明

s| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | HCCL通信域，建议使用Server内最大的通信域，即覆盖最大卡数的通信域。 |
| virPtr | 输入 | 需要反激活的虚拟地址的起始地址，即[HcclCommActivateCommMemory](HcclCommActivateCommMemory.md)接口“virPtr”参数指定的虚拟内存地址。<br>需要注意，指定的虚拟内存必须是已成功激活的内存，且仅支持整个地址块反激活。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无
