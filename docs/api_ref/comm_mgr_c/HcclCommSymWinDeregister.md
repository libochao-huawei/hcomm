# HcclCommSymWinDeregister

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<cann-filter npu-type="310p">
- Atlas 推理系列产品：不支持</cann-filter>
<cann-filter npu-type="910">
- Atlas 训练系列产品：不支持</cann-filter>

## 功能说明

将注册过的对称窗口对应的虚拟内存解除注册，释放对称窗口资源。

## 函数原型

```c
HcclCommSymWinDeregister(HcclCommSymWindow winHandle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| winHandle | 输入 | 注册过的对称窗口句柄。<br>HcclCommSymWindow类型的定义请参见[HcclCommSymWindow](./data_type_definition/HcclResult.md)。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

- 该接口需要配套[HcclCommSymWinRegister](HcclCommSymWinRegister.md)使用。
- 确保通信域中的所有rank同时调用该接口释放对称窗口资源。

## 调用示例

请参见[调用示例](HcclCommSymWinRegister.md#调用示例)。
