# HcclSymWinGetPeerPointer

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<cann-filter npu-type="310p">
- Atlas 推理系列产品：不支持</cann-filter>
<cann-filter npu-type="910">
- Atlas 训练系列产品：不支持</cann-filter>

## 功能说明

根据对称内存窗口资源句柄和偏移，获取指定rank ID在对称内存上的虚拟地址指针。

## 函数原型

```c
HcclResult HcclSymWinGetPeerPointer(HcclCommSymWindow winHandle, size_t offset, uint32_t peerRank, void** ptr)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| winHandle | 输入 | 对称内存窗口资源句柄。 |
| offset | 输入 | 使用[HcclCommSymWinGet](HcclCommSymWinGet.md)获取到的偏移量。 |
| peerRank | 输入 | rank ID，取值范围：[0, rankSize)。 |
| ptr | 输出 | 指向“对称内存上的虚拟地址”的指针。 |

## 返回值

[HcclResult](./data_type_definition/HcclResult.md)：接口成功返回HCCL_SUCCESS，其他失败。

## 约束说明

- 仅支持Atlas A3 训练系列产品/Atlas A3 推理系列产品的超节点内通信。
- 仅支持通信算子展开模式为AI CPU的场景。
- 该接口仅支持在Device侧调用。

## 调用示例

HOST侧注册完对称内存window以后，将window作为AI CPU kernel的参数传递到AI CPU kernel，该函数需要编译到Device AI CPU侧执行，以下是伪代码描述：

```c
AicpuKernelFunc(param)：
// 从param中获取对称window
HcclCommSymWindow temp_win = param.win;
void *src_ptr;
void *dest_ptr;
int srcRankId = 0;
int destRankId = 1;
// 使用win + offset + peerRank获取到peerRank对应的地址
HcclSymWinGetPeerPointer(temp_win, 0, srcRankId, &src_ptr);
HcclSymWinGetPeerPointer(temp_win, 0, rankId, &dest_ptr);
// 获取出来的地址可以使用数据面local copy直接读写，thread和size需调用者准备好
HcommLocalCopyOnThread(thread, dest_ptr, src_ptr, size);
```
