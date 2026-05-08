# HcclBarrier

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：支持

> [!NOTE]说明
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

将指定通信域内所有rank的stream阻塞，直到所有rank都下发执行该操作为止。

## 函数原型

```c
HcclResult HcclBarrier(HcclComm comm, aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 集合通信操作所在的通信域。 |
| stream | 输入 | 本rank所使用的stream。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

```c
HcclComm comm;
aclrtStream stream;
aclrtCreateStream(&stream);

// 下发通信任务到该stream，如HcclAllReduce
// ...

// 阻塞等待所有rank均执行Barrier操作
HcclBarrier(comm, stream);
```
