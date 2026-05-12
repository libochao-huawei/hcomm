# RegisterMemory 虚拟注册机制

## 问题背景

`RegedMemMgr` 的各子类管理着已注册内存的引用计数。原有实现存在以下问题：

1. **ub_mem / roce_mem / hccs_reged_mem_mgr / aicpu_ts_roce_mem**：当传入的 `[addr, size)` 被某个更大的已注册区域包含（子区间）时，`RmaBufferMgr::Add()` 检测到重叠直接返回 `HCCL_E_INTERNAL`，导致上层无法注册子区间内存。
2. **urma_mem**：虽然通过 `DirectFind` + `AddWithoutCheck` 避免了报错，但直接用父buffer作为`memHandle`返回，丢失了子区间的 `addr/size` 信息，导致 `MemoryExport` 导出的地址范围是父区间的完整范围。

## 设计思路

核心思想：**当传入内存是某个已注册区域的子区间时，不重复注册硬件，而是构造一个"虚拟"的 buffer 条目，共享父区间的注册 token/key，同时增加父区间的引用计数。**

### 三种注册场景

```
RegisterMemory([addr, size))
  │
  ├─ 精确匹配 ──→ RmaBufferMgr::Add (ref++)，返回真实 buffer 指针
  │
  ├─ 子区间 ──→ 创建 VirtualRegEntry {parentBuffer, childAddr, childSize}
  │              AddWithoutCheck(childKey) + AddWithoutCheck(parentKey)
  │              返回 VirtualRegEntry 地址作为 memHandle
  │
  └─ 未注册 ──→ 构造新 buffer → 硬件注册 → Add → 返回 buffer 指针
```

### 解注册

```
UnregisterMemory(memHandle)
  │
  ├─ 虚拟句柄 ──→ Del(childKey) → Del(parentKey)
  │               parent引用归零时清理 allRegisteredBuffers_
  │               删除 VirtualRegEntry
  │
  └─ 真实句柄 ──→ 原有逻辑：Del → 引用归零时擦除 vector 条目
```

### MemoryExport 对虚拟句柄的支持

对于使用 DTO 导出方式的类（urma_mem / roce_mem）：
- 从父 buffer 获取 DTO（包含注册 token/key）
- 将 DTO 的 `addr/size` 覆写为子区间的值
- 序列化后返回，使远端看到的是子区间地址范围

对于使用 opaque serialization 的类（hccs / aicpu_ts_roce），直接导出父 buffer 的序列化数据。

## 修改的文件

### 新增数据结构

每个 `RegedMemMgr` 子类新增了 `VirtualRegEntry` 结构体和对应的 `virtualRegs_` 列表：

```cpp
struct VirtualRegEntry {
    std::shared_ptr<XxxBuffer> parentBuffer;  // 父 buffer
    uintptr_t childAddr;                       // 子区间起始地址
    uint64_t childSize;                        // 子区间大小
    std::vector<char> exportDesc;              // MemoryExport 暂存（部分类）
};
std::list<VirtualRegEntry> virtualRegs_;
```

### 各文件变更

| 文件 | 主要变更 |
|------|---------|
| [ub_mem.h](ub_mem.h) | 添加 `<list>` include、VirtualRegEntry 结构体、virtualRegs_ 成员 |
| [ub_mem.cc](ub_mem.cc) | RegisterMemory 三分支重写、UnregisterMemory 虚拟句柄检测 + 父清理 |
| [urma_mem.h](urma_mem.h) | 添加 `<list>` include、VirtualRegEntry（含 exportDesc）、virtualRegs_ |
| [urma_mem.cc](urma_mem.cc) | RegisterMemory 三分支重写、UnregisterMemory 虚拟句柄 + 父清理、MemoryExport DTO 覆写 |
| [roce_mem.h](roce_mem.h) | 添加 `<list>` include、VirtualRegEntry（含 exportDesc）、virtualRegs_ |
| [roce_mem.cc](roce_mem.cc) | RegisterMemory 三分支重写、UnregisterMemory 虚拟句柄 + 父清理、MemoryExport DTO 覆写 |
| [hccs_reged_mem_mgr.h](hccs_reged_mem_mgr.h) | 添加 `<list>` include、VirtualRegEntry、virtualRegs_ |
| [hccs_reged_mem_mgr.cc](hccs_reged_mem_mgr.cc) | RegisterMemory 三分支重写、UnregisterMemory 虚拟句柄 + 父清理 |
| [aicpu_ts_roce_mem.h](aicpu_ts_roce_mem.h) | 添加 `<list>` include、VirtualRegEntry（含 exportDesc）、virtualRegs_ |
| [aicpu_ts_roce_mem.cc](aicpu_ts_roce_mem.cc) | RegisterMemory 三分支重写（内联 GetOrCreateLocalRdmaRmaBuffer 的 Find 逻辑）、UnregisterMemory 虚拟句柄 + 父清理 + hcclBufRecords_ 清理、MemoryExport 虚拟支持 |

## 引用计数规则

- **父区间在 mgr 中的 ref**：每个虚拟注册通过 `AddWithoutCheck(parentKey)` 增加一次，解注册时通过 `Del(parentKey)` 减少一次。所有虚拟注册解除后，父区间 ref 恢复为仅真实用户的计数。
- **子区间在 mgr 中的 ref**：用于跟踪子区间条目本身，同一子区间可多次注册多次引用。
- **allRegisteredBuffers_ 清理**：仅在父区间 ref 归零（`parentDeleted == true`）时，才从 vector 中移除父 buffer 的 `shared_ptr`，避免内存泄漏。
