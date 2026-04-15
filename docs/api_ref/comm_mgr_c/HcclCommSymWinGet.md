# HcclCommSymWinGet

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：不支持

## 功能说明

根据已注册对称内存的虚拟地址指针，返回对应的窗口资源句柄及其在窗口内的偏移量。

## 函数原型

```c
HcclResult HcclCommSymWinGet(HcclComm comm, void *ptr, size_t size, HcclCommSymWindow *winHandle, size_t *offset)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| comm | 输入 | HCCL通信域。 |
| ptr | 输入 | 虚拟地址指针，该内存需要已使用HcclCommSymWinRegister接口进行注册。 |
| size | 输入 | 对称内存窗口大小。<br>假设对称内存窗口大小为symSize，已注册对称内存的虚拟地址指针为addr，size需要满足以下条件：<br>  - size > 0<br>  - ptr+size <= addr + symSize |
| winHandle | 输出 | 指向“对称内存窗口资源句柄”的指针。 |
| offset | 输出 | 指向偏移量的指针。<br>假设已注册对称内存的虚拟地址指针为addr，则*offset = ptr - addr。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

- 仅支持Atlas A3 训练系列产品/Atlas A3 推理系列产品的超节点内通信。
- 仅支持通信算子展开模式为AI CPU的场景。

## 调用示例

```c
// 创建并初始化通信域配置项
HcclCommConfig config;
HcclCommConfigInit(&config);
// 按需修改通信域配置
config.hcclSymWinMaxMemSizePerRank = 10; //单位GB, 默认值为16。设置对称堆预留的虚拟内存大小 = rankSize * config.hcclSymWinMaxMemSizePerRank;
// 初始化集合通信域
HcclComm hcclComm;
HCCLCHECK(HcclCommInitRootInfoConfig(rankSize, &rootInfo, deviceId, &config, &hcclComm));

// 物理内存属性配置
int32_t deviceId;
aclrtGetDevice(&deviceId);
aclrtPhysicalMemProp prop;
prop.handleType = ACL_MEM_HANDLE_TYPE_NONE;
prop.allocationType = ACL_MEM_ALLOCATION_TYPE_PINNED;
prop.memAttr = ACL_HBM_MEM_HUGE;
prop.location.id = deviceId;
prop.location.type = ACL_MEM_LOCATION_TYPE_DEVICE;
prop.reserve = 0;
// 获取对齐粒度，通常为2M
size_t granularity = 0;
aclrtMemGetAllocationGranularity(&prop, ACL_RT_MEM_ALLOC_GRANULARITY_RECOMMENDED, &granularity);
// size按粒度对齐
size_t size = 2 * 1024 * 1024;
size_t allocSize = (size + granularity - 1) / granularity * granularity;
// 预留虚拟内存
void *virPtr = nullptr;
aclrtReserveMemAddress(&virPtr, allocSize, 0, nullptr, 1);
// 申请物理内存
aclrtDrvMemHandle handle;
aclrtMallocPhysical(&handle, allocSize, &prop, 0);
// 建立物理到虚拟的映射
aclrtMapMem(virPtr, allocSize, 0, handle, 0);

HcclCommSymWindow sym_win;
// 注册对称内存
HcclCommSymWinRegister(hcclComm, virPtr, allocSize, &sym_win, HCCL_WIN_COLL_SYMMETRIC);
// 使用HcclCommSymWinGet获取对称内存资源
HcclCommSymWindow temp_win;
size_t offset = 0;
HcclCommSymWinGet(hcclComm, virPtr + 10, 1024, &temp_win, &offset);
// 解注册对称内存
HcclCommSymWinDeregister(sym_win);
// 释放内存
aclrtUnmapMem(virPtr);
aclrtFreePhysical(handle);
aclrtReleaseMemAddress(virPtr);
// 销毁通信域
HcclCommDestroy(hcclComm);
```
