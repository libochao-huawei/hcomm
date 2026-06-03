# HcommMemReg

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持

## 功能说明

注册内存到指定EndPoint。

## 函数原型

```c
HcommResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, const CommMem *mem, HcommMemHandle *memHandle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| endpointHandle | 输入 | Endpoint句柄。<br>EndpointHandle类型的定义请参见[EndpointHandle](../../datatype_definition/EndpointHandle.md)。 |
| memTag | 输入 | 内存字符串标识。 |
| mem | 输入 | 内存描述信息，包含内存物理位置类型、内存地址、内存区域字节数。<br>CommMem类型的定义请参见[CommMem](../../datatype_definition/CommMem.md)。 |
| memHandle | 输出 | 注册内存句柄 |

## 返回值

HcommResult：接口成功返回0，其他失败。

## 约束说明

- 支持的通信协议包括：RoCE、UBC_TP、UBC_CTP、UBoE。
- UBoE、UBC_TP、UBC_CTP协议下，若本次注册的内存区域是同一Endpoint下已注册内存区域的子集，接口复用已注册内存区域的底层注册资源，并返回可用于本次注册内存的注册内存句柄；调用者仍应按本次返回的句柄进行后续导出、建链、更新和解注册。
- RoCE、HCCS等使用重叠检测的协议不支持子集、超集或交叠内存区域重复注册，出现该类场景时接口返回失败。
- memHandle只保证在创建它的Endpoint及其相关资源生命周期内有效。调用HcommMemUnreg后，该句柄失效。

## 调用示例

```c
const EndpointDesc endpointDesc = {
    .protocol = COMM_PROTOCOL_ROCE,
    .commAddr = {
        .type = COMM_ADDR_TYPE_IP_V4,
        .addr = {{192, 168, 1, 100}}
    },
    .loc = {
        .locType = ENDPOINT_LOC_TYPE_DEVICE,
        .device = {
            .devPhyId = 0,
            .superDevId = 0,
            .serverIdx = 0,
            .superPodIdx = 0
        }
    },
    .raws = {0}
};
EndpointHandle endpointHandle = nullptr;
HcommResult result = HcommEndpointCreate(&endpointDesc, &endpointHandle);
const char *memTag = "HcclBuffer";
CommMem mem = {
    .type = COMM_MEM_TYPE_DEVICE,
    .addr = reinterpret_cast<void*>(0x1111),
    .size = 100
};
HcommMemHandle memHandle;
result = HcommMemReg(endpointHandle, memTag, &mem, &memHandle);
```
