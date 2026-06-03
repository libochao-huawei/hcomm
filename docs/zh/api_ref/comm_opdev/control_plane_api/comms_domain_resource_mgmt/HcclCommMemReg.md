# HcclCommMemReg

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

向通信域注册已申请的内存，并获取对应的注册内存句柄。

## 函数原型

```c
HcclResult HcclCommMemReg(HcclComm comm, const char *memTag, const CommMem *mem, HcclMemHandle *memHandle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| comm | 输入 | 通信域句柄。<br>HcclComm类型的定义如下：<br>typedef void *HcclComm; |
| memTag | 输入 | 内存字符串标签，最大字符长度为HCCL_OP_TAG_LEN_MAX。<br>const uint32_t HCCL_OP_TAG_LEN_MAX = 255; |
| mem | 输入 | 内存信息，CommMem类型的定义可参见[CommMem](../../datatype_definition/CommMem.md)。 |
| memHandle | 输出 | 注册内存句柄。<br>HcclMemHandle类型的定义如下：<br>typedef void *HcclMemHandle; |

## 返回值

[HcclResult](../../../comm_mgr_c/data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

- 一个通信域内，同一个memTag只允许绑定一块内存。
- 一个通信域内，相同memTag重复调用时，接口返回失败。
- HcclCommMemReg返回的memHandle描述本次传入的CommMem信息。后续创建或更新Channel时，框架会根据该句柄记录的addr、size、type和memTag进行内存交换；调用者不应自行从句柄地址推导这些信息。
- memHandle只保证在创建它的通信域及其相关资源生命周期内有效。解注册后，该句柄失效。

## 调用示例

```c
HcclComm comm; // 通信域句柄，省略获取
char* memTag = "memTag"; // 内存标签
CommMem memInfo; // 内存信息
memInfo.addr = 0x00; // 配置已申请的内存地址     
memInfo.size = 1024; // 配置已申请的内存大小
memInfo.type = COMM_MEM_TYPE_DEVICE; // 配置已申请的内存类型
HcclMemHandle memHandle; // 内存句柄
HcclCommMemReg(comm, memTag, &memInfo, &memHandle);
```
