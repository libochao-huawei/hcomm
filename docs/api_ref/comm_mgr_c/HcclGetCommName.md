# HcclGetCommName

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<cann-filter npu-type="310p">
- Atlas 推理系列产品：支持</cann-filter>
<cann-filter npu-type="910">
- Atlas 训练系列产品：支持</cann-filter>

> [!NOTE]说明
>
> - 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
<cann-filter npu-type="310p">
> - 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。</cann-filter>

## 功能说明

获取指定通信操作所在的通信域的名称。

## 函数原型

```c
HcclResult HcclGetCommName(HcclComm comm, char* commName)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| comm | 输入 | 集合通信操作所在的通信域。<br>HcclComm类型的定义可参见[HcclComm](./data_type_definition/HcclComm.md)。 |
| commName | 输出 | 获取到的通信域名称。<br>char*类型，最大长度支持128。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HcclSuccess，其他失败。

## 约束说明

无

## 调用示例

```c
// 初始化通信域
HcclComm comm;
// 查询通信域名称
char commName[128];
HcclGetCommName(comm, &commName);
```
