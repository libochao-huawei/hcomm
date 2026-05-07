# HcclConfigValue

## 功能说明

定义[HcclConfig](HcclConfig.md)中可配置的参数的值。

## 定义原型

```c
union HcclConfigValue {
    int32_t value;
};
```

value为[HcclConfig](HcclConfig.md)中“HCCL_DETERMINISTIC”参数的值。

此参数仅在如下型号中支持配置：

- Atlas A3 训练系列产品/Atlas A3 推理系列产品
- Atlas A2 训练系列产品/Atlas A2 推理系列产品
- Atlas 推理系列产品
- Atlas 训练系列产品
