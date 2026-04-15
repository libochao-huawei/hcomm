# HcclCommGetStatus

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：不支持

## 功能说明

算子下发时获取通信域状态，判断能否下发算子。

## 函数原型

```c
HcclResult HcclCommGetStatus(const char *commId, HcclCommStatus *status)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| commId | 输入 | 通信域名称。<br>const char*类型，最大长度支持128。 |
| status | 输出 | 通信域状态，HcclCommStatus类型的定义可参见[HcclCommStatus](./data_type_definition/HcclCommStatus.md)。 |
