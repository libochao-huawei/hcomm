# HcclSetConfig

## 产品支持情况

<cann-filter npu-type="950">

- Ascend 950PR/Ascend 950DT：不支持</cann-filter>
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

进行集合通信相关配置，当前仅支持配置是否支持确定性计算。

在不开启确定性计算的场景下，多次执行的结果可能不同。这个差异的来源，一般是因为在算子实现中存在异步的多线程执行，会导致浮点数累加的顺序变化。当开启确定性计算后，算子在相同的硬件和输入下，多次执行将产生相同的输出。

默认情况下，无需开启确定性计算或保序功能，但当发现模型执行多次结果不同或者精度调优时，可以开启确定性计算或保序功能辅助进行调试调优，但开启后，算子执行时间会变慢，导致性能下降。

## 函数原型

```c
HcclResult HcclSetConfig(HcclConfig config, HcclConfigValue configValue)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| config | 输入 | config中可配置的参数。<br>[HcclConfig](./data_type_definition/HcclConfig.md)类型，当前版本仅支持配置“HCCL_DETERMINISTIC”。 |
| configValue | 输入 | config中所配置参数的取值。<br>请参见[HcclConfigValue](./data_type_definition/HcclConfigValue.md)类型。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

```c
// 开启确定性计算
HcclConfig config = HCCL_DETERMINISTIC;
union HcclConfigValue configValue;
configValue.value = 1;
HcclSetConfig(config, configValue);
```
