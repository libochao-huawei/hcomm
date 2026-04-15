# HcclGetErrorString

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持

> [!CAUTION]注意
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
> 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。

## 功能说明

解析HcclResult类型的错误码。

## 函数原型

```c
const char* HcclGetErrorString (HcclResult code)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| code | 输入 | 需要解析的错误码，[HcclResult](./data_type_definition/HcclResult.md)类型。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)类型错误码的解析结果。

## 约束说明

无。
