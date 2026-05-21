# RegisterMemory 别名（Alias）机制变更文档

## 概述

修改 `RegisterMemory` 的行为：当传入的 `(addr, size)` 完全包含在某个已注册的更大内存区域内时，不再报错，而是创建一个**别名缓冲区（alias rmaBuffer）**——复用父缓冲区的硬件注册信息，但持有自身的 addr/size。解注册 alias 时只减少父缓冲区的引用计数，不释放真实的硬件注册内存。

## 核心设计

```
RegisterMemory(mem):
  ├── 未注册（Find 未命中）        → 正常创建 buffer + HW 注册
  ├── 精确命中（addr/size 全等）   → 复用已有 buffer，引用计数+1（与原来一致）
  ├── 包含在更大区域内（子集）     → 创建 alias buffer，父引用计数+1
  └── 部分重叠（交集但不包含）     → 仍然返回错误

UnregisterMemory(memHandle):
  ├── 是 alias → 从树中删除 alias key → 递减父引用计数 → 父引用计数归零才释放 HW
  └── 非 alias → 原有逻辑：从树中删除 → 引用计数归零则 HW 解注册
```

## 修改文件清单（共 17 个文件）

### 1. 基类 — 新增 `isAlias_` / `parentBuffer_`

| 文件 | 说明 |
|------|------|
| `src/legacy/unified_platform/resource/buffer/local_rma_buffer.h` | 旧版 `LocalRmaBuffer`：新增 `isAlias_`(bool)、`parentBuffer_`(shared_ptr)、`IsAlias()`、`SetAlias()`、`GetParentBuffer()`、`SetParentBuffer()` |
| `src/pub_inc/inner/rma_buffer.h` | 新版 `RmaBuffer`：同上述字段和方法 |

### 2. 旧版缓冲区类 — 新增 alias 构造函数（跳过 HW 注册，拷贝父注册信息）

| 文件 | 关键行为 |
|------|---------|
| `src/legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.h` | 声明 alias 构造函数 `LocalRdmaRmaBuffer(buf, parent)` |
| `src/legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.cc` | 拷贝父的 `lkey`/`rkey`/`mrHandle`/`key`；析构函数对 alias 跳过 `RaDeregisterMr` |
| `src/legacy/unified_platform/resource/buffer/local_ub_rma_buffer.h` | 声明 alias 构造函数 `LocalUbRmaBuffer(buf, parent)` |
| `src/legacy/unified_platform/resource/buffer/local_ub_rma_buffer.cc` | 拷贝父的 UB 注册信息（`tokenValue`/`tokenId`/`tokenIdHandle`/`memHandle`/`keySize`/`reqReg`/`segVa`/`key`）；析构函数对 alias 跳过 `HrtRaUbLocalMemUnreg` |
| `src/legacy/unified_platform/resource/buffer/local_ipc_rma_buffer.h` | 声明 alias 构造函数 `LocalIpcRmaBuffer(buf, parent)` |
| `src/legacy/unified_platform/resource/buffer/local_ipc_rma_buffer.cc` | 偏移量适配：`ipcOffset = parent.ipcOffset + (aliasAddr - parentBaseAddr)`，拷贝父的 `ipcPtr`/`name`；析构函数对 alias 跳过 `HrtIpcDestroyMemoryName` |

### 3. 新版缓冲区类 — pimpl + alias 支持

| 文件 | 关键行为 |
|------|---------|
| `src/pub_inc/inner/local_rdma_rma_buffer.h` | 新增 alias 构造函数、`parentTyped_`、`aliasSerializeStr_` |
| `src/platform/resource/rma_buffer/local_rdma_rma_buffer.cc` | alias 构造函数存储父指针，`devAddr` 按偏移量计算；`Init()`/`Destroy()` 为空操作；`Serialize()` 用子区间 addr/size + 父的 lkey 序列化；`GetKey()` 委托给父；`Remap()` 为空操作 |
| `src/pub_inc/inner/local_ipc_rma_buffer.h` | 同上模式 |
| `src/platform/resource/rma_buffer/local_ipc_rma_buffer.cc` | alias 构造函数处理同上；`Serialize()` 用自身 addr/size + 父的 IPC memName/memOffset；`Grant()` 委托给父 |

### 4. RegedMemMgr 类 — RegisterMemory / UnregisterMemory 核心逻辑

| 文件 | 类 | 说明 |
|------|-----|------|
| `src/framework/next/comms/endpoints/reged_mems/roce_mem.cc` | `RoceRegedMemMgr` | 旧版 RDMA buffer (legacy `Hccl::LocalRdmaRmaBuffer`) |
| `src/framework/next/comms/endpoints/reged_mems/urma_mem.cc` | `UbRegedMemMgr` | 旧版 UB buffer (legacy `Hccl::LocalUbRmaBuffer`) |
| `src/framework/next/comms/endpoints/reged_mems/ub_mem.cc` | `UbMemRegedMemMgr` | 旧版 IPC buffer (legacy `Hccl::LocalIpcRmaBuffer`) |
| `src/framework/next/comms/endpoints/reged_mems/aicpu_ts_roce_mem.cc` + `.h` | `AicpuTsRoceRegedMemMgr` | 新版 RDMA buffer (`hccl::LocalRdmaRmaBuffer`)，`GetOrCreateLocalRdmaRmaBuffer` 签名新增 `bool &isAlias` 参数 |
| `src/framework/next/comms/endpoints/reged_mems/hccs_reged_mem_mgr.cc` | `HccsRegedMemMgr` | 新版 IPC buffer (`hccl::LocalIpcRmaBuffer`) |

### RegisterMemory 统一逻辑

```cpp
// 1. Find(tempKey) 查询是否被已有注册覆盖
auto findPair = mgr->Find(tempKey);
if (findPair.first) {
    auto existing = findPair.second;
    if (existing->addr == addr && existing->size == size) {
        // 精确命中 → 复用，Add() 引用计数+1
    } else {
        // 子集 → 创建 alias buffer（alias 构造函数不做 HW 注册）
        //   → AddWithoutCheck(tempKey, alias) 将 alias 插入树
        //   → AddToTree(parentKey, parent) 父引用计数+1
    }
} else {
    // 未注册 → 正常创建 + HW 注册 + Add() 到树
}
```

### UnregisterMemory 统一逻辑

```cpp
if (buffer->IsAlias()) {
    // 1. Del(aliasKey) 从树中删除 alias
    // 2. Del(parentKey) 递减父引用计数
    // 3. 若父引用计数归零 → 从 allRegisteredBuffers_ 移除父（触发 HW 解注册）
    // 4. 从 allRegisteredBuffers_ 移除 alias
} else {
    // 原有逻辑：Del(key) → 引用计数归零则从 vector 移除并 HW 解注册
}
```

## 对齐（Alignment）处理

- **旧版 RDMA buffer alias**: 直接拷贝父的 `lkey`/`rkey`/`mrHandle`，子区域的 addr 在父注册范围内即可正常访问
- **旧版 IPC buffer alias**: `ipcOffset = parent.ipcOffset + (aliasAddr - parentBaseAddr)`，保持页对齐的相对偏移
- **新版 RDMA/IPC buffer alias**: `devAddr = parentDevAddr + (aliasAddr - parentAddr)`，正确计算子区域的设备虚拟地址

## 关键不变式

1. **父 buffer 的生命周期**: alias 持有 `shared_ptr<LocalRmaBuffer> parentBuffer_`，保证父在 alias 存活期间不被销毁
2. **树的引用计数一致性**: 父注册时 ref=1；每个 alias 创建使父 ref+1；每个 alias 解注册使父 ref-1；父 ref 归零时才真正释放 HW 注册
3. **MemoryExport 正确性**: alias 的 `GetExchangeDto()`/`Serialize()` 返回子区间的 addr/size + 父的注册密钥（lkey/rkey/ipcName），远端可正确访问子区间
4. **上层 API 透明**: `HcommMemReg`/`HcommMemExport`/`HcommMemUnreg` 无需修改，handle 仍为指向 buffer 对象的原始指针
