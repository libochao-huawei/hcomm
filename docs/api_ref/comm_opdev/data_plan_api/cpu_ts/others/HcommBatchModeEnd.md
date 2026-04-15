# HcommBatchModeEnd

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持

> [!CAUTION]注意
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。

## 功能说明

该接口用于提交并触发批量模式下缓存的所有操作的执行。所有在HcommBatchModeStart和HcommBatchModeEnd之间的数据面接口调用操作将在此时统一执行。

## 函数原型

```c
int32_t HcommBatchModeEnd(const char *batchTag)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| batchTag | 输入 | 批量任务标识符，需要与HcommBatchModeStart中传入的batchTag保持一致。 |

## 返回值

int32_t：接口成功返回0，其他失败。

## 约束说明

HcommBatchModeStart和HcommBatchModeEnd必须成对调用，且需要在同一线程中执行。

仅Ascend 950PR/Ascend 950DT支持批量模式和立即执行模式（不调用批量接口），其他产品必须使用批量模式。

## 调用示例

```c
char *tag = "";
// 启动批量模式（临时批量任务）
HcommBatchModeStart(tag);

// 在批量模式中调用数据面接口不会立即执行
// ...

// 结束批量模式并触发执行
HcommBatchModeEnd(tag);
```
