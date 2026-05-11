# HcclGetConfig

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

获取集合通信相关配置。

## 函数原型

```c
HcclResult HcclGetConfig(HcclConfig config, HcclConfigValue *configValue)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| config | 输入 | 集合通信配置参数。<br>[HcclConfig](./data_type_definition/HcclConfig.md)类型，当前版本仅支持配置为“HCCL_DETERMINISTIC”。 |
| configValue | 输出 | 获取config中配置的参数的值。<br>请参见[HcclConfigValue](./data_type_definition/HcclConfigValue.md)类型。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

无

## 调用示例

```c
// 查询确定性计算开关
HcclConfig config = HCCL_DETERMINISTIC;
union HcclConfigValue configValue;
HcclGetConfig(HCCL_DETERMINISTIC, &configValue);
```
