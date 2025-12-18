/*
 * Copyright (c) 2024-2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "symmetric_memory.h"
#include <algorithm> // for std::max
#include <cstddef>
#include <list>      // for SimpleVaAllocator
#include "hccl_comm.h"

namespace hccl 
{
/**
 * @brief (内部) 简单的VA空间分配器
 * (... 此处省略 SimpleVaAllocator 的实现，与上一版相同 ...)
 */
class SymmetricMemory::SimpleVaAllocator {
    struct FreeBlock {
        size_t offset;
        size_t size;
    };
    std::list<FreeBlock> freeList_; // 按offset排序的空闲块
    std::mutex mutex_;
    size_t totalSize_;

public:
    SimpleVaAllocator() : totalSize_(0) {}
    ~SimpleVaAllocator() { Destroy(); }

    // 增加调试打印函数
    void Dump(const char* tag) {
        HCCL_ERROR("[%s] === VA Allocator Dump (Total: %zu) ===", tag, totalSize_);
        size_t freeSum = 0;
        int i = 0;
        for (auto &block : freeList_) {
            HCCL_ERROR("  Block[%d]: offset %zu (0x%zx) -> size %zu (0x%zx) | end: %zu", 
                i++, block.offset, block.offset, block.size, block.size, block.offset + block.size);
            freeSum += block.size;
        }
        HCCL_ERROR("  Total Free: %zu (%.2f%%)", freeSum, (double)freeSum / totalSize_ * 100.0);
        HCCL_ERROR("==========================================");
    }

    HcclResult Init(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size == 0) return HCCL_E_PARA;
        totalSize_ = size;
        freeList_.push_back({0, (size_t)size});
        return HCCL_SUCCESS;
    }

    void Destroy() {
        std::lock_guard<std::mutex> lock(mutex_);
        freeList_.clear();
        totalSize_ = 0;
    }

    HcclResult Reserve(size_t size, size_t align, size_t &offset) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Debug: 打印请求信息
        HCCL_INFO("[VAAllocator] Request Reserve: size %zu, align %zu", size, align);

        for (auto it = freeList_.begin(); it != freeList_.end(); ++it) {
            size_t start = it->offset;
            size_t end = it->offset + it->size;

            // 计算对齐后的offset
            size_t alignedOffset = (start + align - 1) & ~(align - 1);
            
            // 检查对齐后的空间是否足够
            if (alignedOffset < end && (end - alignedOffset) >= size) {
                // 找到了
                offset = alignedOffset;
                
                size_t frontPad = alignedOffset - start;
                size_t backPad = (end) - (alignedOffset + size);
                
                HCCL_INFO("[VAAllocator] Found Block: [0x%zx, 0x%zx], Need aligned: 0x%zx. FrontPad: %zu, BackPad: %zu",
                    start, end, alignedOffset, frontPad, backPad);

                auto to_erase = it;
                // 先插入后部碎片（如果存在）
                if (backPad > 0) {
                    // 后部碎片应该插入在to_erase之后
                    // std::list::insert inserts BEFORE the iterator. 
                    // std::next(to_erase) points to the element AFTER current. 
                    // So inserting before next element puts it after current. Correct.
                    freeList_.insert(std::next(to_erase), 
                                    {alignedOffset + (size_t)size, backPad});
                }
                
                // 再插入前部碎片（如果存在）
                if (frontPad > 0) {
                    // 前部碎片插入在to_erase之前
                    freeList_.insert(to_erase, {start, frontPad});
                }
                
                // 最后删除原空闲块
                freeList_.erase(to_erase);
                return HCCL_SUCCESS;
            } else {
                // 增加调试：为什么这个块不满足？
                // 只有当块看起来比较大但因为对齐无法满足时才打印，避免刷屏
                if (it->size >= size) {
                    HCCL_DEBUG("[VAAllocator] Block [0x%zx, 0x%zx] size %zu skipped. AlignedOffset 0x%zx overlaps end or insufficient.",
                        start, end, it->size, alignedOffset);
                }
            }
        }

        // 失败时打印当前内存布局，极大概率是碎片化导致
        HCCL_ERROR("[VAAllocator] Failed to reserve size %zu with align %zu. No suitable block found.", size, align);
        Dump("Reserve Failed");
        
        return HCCL_E_MEMORY;
    }

    HcclResult Release(size_t offset, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... Release 代码保持不变，也可以加上日志 ...
        // 边界检查
        if (offset + size > totalSize_) {
            HCCL_ERROR("[VAAllocator] Release out of range. off %zu + size %zu > total %zu", offset, size, totalSize_);
            return HCCL_E_PARA;
        }

        // 找到插入位置并合并
        auto it = freeList_.begin();
        while (it != freeList_.end() && it->offset < offset) {
            ++it;
        }
        
	    // 检查重叠,直接报错
 	    // 与前一块重叠
 	    if (it != freeList_.begin()) {
 	        auto prevIt = std::prev(it);
 	        if (prevIt->offset <=  offset && prevIt->offset + prevIt->size >= offset + size) { //判断释放区被前一块空闲区域覆盖
 	            return HCCL_SUCCESS;     
 	        }
 	        if (prevIt->offset + prevIt->size > offset) {
                HCCL_ERROR("[VAAllocator] Releasing block[0x%zx, size %zu] overlaps with the previous block.", offset, size);
 	            return HCCL_E_PARA;
 	        }
 	    }
 	    // 与后一块重叠
 	    if (it != freeList_.end() && it->offset < offset + size) {
 	        if (it->offset <= offset && it->offset + it->size >= offset + size) { //判断释放区完全与后一块空闲区域覆盖
 	            return HCCL_SUCCESS;
 	        }
            HCCL_ERROR("[VAAllocator] Releasing block[0x%zx, size %zu] overlaps with the next block.", offset, size);
 	        return HCCL_E_PARA;
 	    }
        
        // 插入新释放的块
        auto newIt = freeList_.insert(it, {offset, size});
        HCCL_INFO("[VAAllocator] Releasing [0x%zx, size %zu]", offset, size);

        // 尝试与后一块合并
        if (std::next(newIt) != freeList_.end()) {
            auto nextIt = std::next(newIt);
            if (newIt->offset + newIt->size == nextIt->offset) {
                newIt->size += nextIt->size;
                freeList_.erase(nextIt);
            }
        }
        // 尝试与前一块合并
        if (newIt != freeList_.begin()) {
            auto prevIt = std::prev(newIt);
            if (prevIt->offset + prevIt->size == newIt->offset) {
                prevIt->size += newIt->size;
                freeList_.erase(newIt);
            }
        }
        return HCCL_SUCCESS;
    }
};

SymmetricMemory::SymmetricMemory(u32 rank, u32 rankSize, size_t stride, std::shared_ptr<AllGatherManager> allGatherManager)
    : rank_(rank),
      rankSize_(rankSize),
      stride_(stride), 
      vaAllocator_(new (std::nothrow) SimpleVaAllocator()),
      allGatherManager_(std::move(allGatherManager))
{
    remoteShareablePids.resize(rankSize_, 0);
}

SymmetricMemory::~SymmetricMemory() 
{
    HCCL_INFO("[SymmetricMemory][~SymmetricMemory] begin");
    for (auto& pair : windowMap_) {
        DeregisterSymmetricMem(pair.first);
    }
    windowMap_.clear();
    sortedWindows_.clear();

    if (vaAllocator_) {
        vaAllocator_->Destroy();
    }

    if (heapBase_) {
        if (aclrtReleaseMemAddress(heapBase_) != ACL_SUCCESS) {
            HCCL_ERROR("[SymmetricMemory][~SymmetricMemory] Failed to release symmetric heap VA: %p", heapBase_);
        }
    }

    HCCL_INFO("[SymmetricMemory][~SymmetricMemory] end");
}

HcclResult SymmetricMemory::EnsureInit() {
    std::call_once(init_flag_, [this]() {
        initResult_ = Init();
    });
    return initResult_;
}

HcclResult SymmetricMemory::Init() 
{
    // 0. 检查Pimpl是否构造成功
    CHK_SMART_PTR_NULL(vaAllocator_);
    CHK_SMART_PTR_NULL(allGatherManager_);

    CHK_PRT_RET(rankSize_ < 2, HCCL_ERROR("[SymmetricMemory][Init] single rank communicator"), HCCL_E_PARA);

    // 1. 获取内存映射的粒度
    aclrtPhysicalMemProp prop = {
        ACL_MEM_HANDLE_TYPE_NONE,
        ACL_MEM_ALLOCATION_TYPE_PINNED,
        ACL_HBM_MEM_HUGE,
        {0, ACL_MEM_LOCATION_TYPE_DEVICE},
        0
    };

    if (aclrtMemGetAllocationGranularity(&prop, ACL_RT_MEM_ALLOC_GRANULARITY_RECOMMENDED, &granularity_) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][Init] aclrtMemGetAllocationGranularity failed.");
        return HCCL_E_INTERNAL;
    }
    if (granularity_ == 0) {
        HCCL_ERROR("[SymmetricMemory][Init] Invalid memory granularity: 0");
        return HCCL_E_INTERNAL;
    }
    
    // 2. 预留总的VA空间
    // 每个rank都预留一个总大小为 totalHeapSize 的VA空间。
    // heapBase_ 在不同rank上可能是不同的，这是预期行为。
    size_t totalHeapSize = (size_t)stride_ * rankSize_;
    if (stride_ % granularity_ != 0) {
        HCCL_ERROR("[SymmetricMemory][Init] Stride %u is not a multiple of granularity %zu.", stride_, granularity_);
        return HCCL_E_PARA;
    }

    size_t targetStartTB = 40ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    void* hintPtr = reinterpret_cast<void*>(targetStartTB);
    //aclrtReserveMemAddressNoUCMemory(&heapBase_, totalHeapSize, 0, hintPtr, 0);
    // 默认大页对齐
    if (aclrtReserveMemAddress(&heapBase_, totalHeapSize, 0, nullptr, 1) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][Init] aclrtReserveMemAddress failed to reserve %zu bytes. stride: %u, rankSize: %u.",
                   totalHeapSize, stride_, rankSize_);
        return HCCL_E_INTERNAL;
    }

    // 5. 初始化VA分配器 (管理本地rank的stride_大小空间，即管理偏移量)
    // 这是一个集合调用，所有rank上的vaAllocator_状态将保持一致（前提是 SimpleVaAllocator 是确定性的）
    CHK_RET(vaAllocator_->Init(stride_));

    CHK_RET(allGatherManager_->Init());

    int32_t localPid{0};    // 当前进程号
    if (aclrtDeviceGetBareTgid(&localPid) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][Init] Failed to get pid");
        return HCCL_E_DRV;
    }
    HCCL_INFO("[SymmetricMemory][Init] Local pid: %d.", localPid);

    CHK_RET(allGatherManager_->AllGather((void*)&localPid, (void*)remoteShareablePids.data(), sizeof(localPid)));

    std::string pidStr;
    for (u32 i = 0; i < remoteShareablePids.size(); i++) {
        pidStr += std::to_string(remoteShareablePids[i]);
        pidStr += "; ";
    }
    HCCL_INFO("[SymmetricMemory][Init] remote pids: %s", pidStr.c_str());

    HCCL_INFO("[SymmetricMemory][Init] SymmetricMemory initialized. Rank[%u], Local Heap Base: %p, Stride: %u, Ranks: %u.",
               rank_, heapBase_, stride_, rankSize_);
    return HCCL_SUCCESS;
}

void* SymmetricMemory::AllocSymmetricMem(size_t size)
{
    void* devWin = nullptr;
    void *ptr = nullptr;
    HcclResult ret = HcclMemAlloc(&ptr, size);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][AllocSymmetricMem] HcclMemAlloc failed for size[%u].", size);
        return nullptr;
    }

    ret = RegisterSymmetricMem(ptr, size, &devWin);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][AllocSymmetricMem] RegisterSymmetricMem failed for ptr[%p], size[%u].", ptr, size);
        (void)HcclMemFree(ptr);
        return nullptr;
    }
    return devWin;
}

HcclResult SymmetricMemory::FreeSymmetricMem(void* devWin)
{
    std::shared_ptr<SymmetricWindow> pWin = windowMap_[devWin];
    if (pWin == nullptr) {
        return HCCL_SUCCESS;
    }

    void* userPtr = pWin->userVa;
    CHK_RET(HcclMemFree(userPtr));
    CHK_RET(DeregisterSymmetricMem(devWin));
    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::AddSymmetricWindow(std::shared_ptr<SymmetricWindow> &win)
{
    CHK_RET(hrtMalloc(&win->devWin, sizeof(SymmetricWindow)));
    CHK_RET(hrtMemSyncCopy(win->devWin, sizeof(SymmetricWindow), 
        win.get(), sizeof(SymmetricWindow), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    sortedWindows_.push_back(win);
    std::sort(sortedWindows_.begin(), sortedWindows_.end(), 
        [](const std::shared_ptr<SymmetricWindow>& a, const std::shared_ptr<SymmetricWindow>& b) {
            return ((uintptr_t)a->userVa < (uintptr_t)b->userVa) || 
                (((uintptr_t)a->userVa == (uintptr_t)b->userVa) && (a->userSize < b->userSize));
    });

    windowMap_[win->devWin] = win;
    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::DeleteSymmetricWindow(std::shared_ptr<SymmetricWindow> &win)
{
    auto it = std::find_if(sortedWindows_.begin(), sortedWindows_.end(),
        [&win](const std::shared_ptr<SymmetricWindow>& w) {
            return w.get() == win.get();
        });
    if (it != sortedWindows_.end()) {
        CHK_PRT(hrtFree(win->devWin));
        windowMap_.erase(win->devWin);
        sortedWindows_.erase(it);
    }

    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::DeleteSymmetricWindow(void* devWin)
{
    auto it = windowMap_.find(devWin);
    if (it != windowMap_.end()) {
        std::shared_ptr<SymmetricWindow> win = it->second;
        CHK_PRT(hrtFree(win->devWin));
        windowMap_.erase(it);

        auto vecIt = std::find_if(sortedWindows_.begin(), sortedWindows_.end(),
            [&win](const std::shared_ptr<SymmetricWindow>& w) {
                return w.get() == win.get();
            });
        if (vecIt != sortedWindows_.end()) {
            sortedWindows_.erase(vecIt);
        }
    }

    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::RegisterSymmetricMem(void* ptr, size_t size, void** devWin)
{
    CHK_RET(EnsureInit());
    if (ptr == nullptr || size == 0) {
        HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] Invalid parameters.");
        return HCCL_E_PARA;
    }

    // 打印当前注册请求的关键信息
    HCCL_INFO("[SymmetricMemory][RegisterSymmetricMem] Request: ptr=%p, size=%zu, granularity=%zu", 
        ptr, size, granularity_);

    void* baseUserVa = ptr;
    size_t baseVaSize = size;
    // if(aclrtMemGetAddressRange(ptr, &baseUserVa, &baseVaSize) != 0) {
    //     HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] aclrtMemGetAddressRange failed for ptr[%p], size[%zu]. ", ptr, size);
    //     return HCCL_E_PARA;
    // }
    CHK_PTR_NULL(baseUserVa);
    aclrtDrvMemHandle paHandle;
    if (aclrtMemRetainAllocationHandle(baseUserVa, &paHandle) != 0) {
        HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] MemRetainAllocationHandle failed for ptr[%p], size[%zu]. ", ptr, size);
        return HCCL_E_PARA;
    }
    size_t alignedBaseSize = (baseVaSize + granularity_ - 1) & ~(granularity_ - 1);

    if (reinterpret_cast<uintptr_t>(ptr) + size > reinterpret_cast<uintptr_t>(baseUserVa) + alignedBaseSize) {
        HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] ptr=%p size=%zu exceeds  block [baseUserVa=%p, size=%zu]", 
           ptr, size, baseUserVa, alignedBaseSize);
        return HCCL_E_PARA;
    }

    HCCL_INFO("[SymmetricMemory][RegisterSymmetricMem] Retained paHandle[%p] for baseUserVa[%p], alignedBaseSize[%zu]. Total Stride: %zu",
        paHandle, baseUserVa, alignedBaseSize, stride_);

    std::shared_ptr<PaMappingInfo> paMapInfo;
    auto it = paMappingMap_.find(paHandle);
    if (it != paMappingMap_.end()) {
        paMapInfo = it->second;
        paMapInfo->refCount++;
        HCCL_INFO("PA handle[%p], refCount[%d]", paHandle, paMapInfo->refCount);
    }else {
        size_t offset = 0;
        // 使用 granularity_ (通常是2MB) 作为对齐参数
        if (vaAllocator_->Reserve(alignedBaseSize, granularity_, offset) != HCCL_SUCCESS) {
            HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] Failed to reserve VA space. "
                "Req alignedSize: %zu (0x%zx), Align: %zu. Total Stride: %zu. "
                "Is fragmentation too high or stride too small?", 
                alignedBaseSize, alignedBaseSize, granularity_, stride_);
            return HCCL_E_MEMORY;
        }
        paMapInfo = std::make_shared<PaMappingInfo>();
        paMapInfo->paHandle = paHandle;
        paMapInfo->origAllocBaseVa = baseUserVa;
        paMapInfo->origAllocSize = baseVaSize;
        paMapInfo->heapBaseOffset = offset;
        paMapInfo->refCount = 1;
        paMappingMap_.emplace(paHandle, paMapInfo);
    }
    
    std::shared_ptr<SymmetricWindow> pWin(new (std::nothrow) SymmetricWindow());
    pWin->userVa = baseUserVa;
    pWin->userSize = baseVaSize;
    pWin->baseVa = static_cast<uint8_t*>(heapBase_) + paMapInfo->heapBaseOffset;
    pWin->alignedHeapOffset = paMapInfo->heapBaseOffset;
    pWin->alignedSize = alignedBaseSize;
    pWin->localRank = rank_;
    pWin->rankSize = rankSize_;
    pWin->stride = stride_;
    pWin->paHandle = paHandle;

    HcclResult ret = RegisterInternal(paHandle, paMapInfo->heapBaseOffset, alignedBaseSize);
    if (ret != HCCL_SUCCESS) {
        if (paMapInfo->refCount == 1) {
            HCCL_ERROR("[SymmetricMemory] RegisterInternal Failed! Releasing offset 0x%zx", paMapInfo->heapBaseOffset);
            (void)vaAllocator_->Release(paMapInfo->heapBaseOffset, alignedBaseSize);
            paMappingMap_.erase(paHandle);
        } else {
            paMapInfo->refCount--;
        }
        return ret;
    }

    ret = AddSymmetricWindow(pWin);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory] AddSymmetricWindow Failed!");
        if (paMapInfo->refCount == 1) {
            HCCL_ERROR("[SymmetricMemory] Releasing offset 0x%zx", paMapInfo->heapBaseOffset);
            (void)vaAllocator_->Release(paMapInfo->heapBaseOffset, alignedBaseSize);
            paMappingMap_.erase(paHandle);
        } else {
            paMapInfo->refCount--;
        }
        return ret;
    }
    *devWin = pWin->devWin;
    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::DeregisterSymmetricMem(void* devWin)
{
    HcclResult ret = HCCL_SUCCESS;
    if (devWin == nullptr) {
        HCCL_INFO("[SymmetricMemory] DeregisterSymmetricMem sucessed devWin[nullptr]");
        return ret;
    }

    for (auto it = sortedWindows_.begin(); it != sortedWindows_.end();) {
        if ((*it)->devWin != devWin) {
            it++;
            continue;
        }

        std::shared_ptr<PaMappingInfo> paMapInfo = paMappingMap_[(*it)->paHandle];
        if (paMapInfo->refCount == 1) {
            for (u32 i = 0; i < rankSize_; i++) {
                void* virPtr = static_cast<uint8_t*>(heapBase_) + (stride_ * i) + (*it)->alignedHeapOffset;
                aclrtDrvMemHandle handle;
                aclError aclRet = aclrtMemRetainAllocationHandle(virPtr, &handle);
                if (aclRet != ACL_SUCCESS) {
                    HCCL_ERROR("[SymmetricMemory][DeregisterSymmetricMem] MemRetainAllocationHandle failed for ptr[%p], rank[%u], ret[%d].", virPtr, i, aclRet);
                    ret = HCCL_E_DRV;
                    continue;
                }
                HCCL_INFO("[SymmetricMemory][DeregisterSymmetricMem] Start to UnmapMem virPtr[%p], handle[%p], rank[%u].", virPtr, handle, i);
                aclRet = aclrtUnmapMem(virPtr);
                if (aclRet != ACL_SUCCESS) {
                    HCCL_ERROR("[SymmetricMemory][DeregisterSymmetricMem] Failed to unmap mem for rank %u at va %p, ret[%d].", i, virPtr, aclRet);
                    ret = HCCL_E_DRV;
                }
                aclRet = aclrtFreePhysical(handle);
                if (aclRet != ACL_SUCCESS) {
                    HCCL_ERROR("[SymmetricMemory][DeregisterSymmetricMem] FreePhysical handle[%p] failed, ret[%d], rank[%u].", handle, aclRet, i);
                    ret = HCCL_E_DRV;
                }
            }
            vaAllocator_->Release((*it)->alignedHeapOffset, (*it)->alignedSize);
            paMappingMap_.erase((*it)->paHandle);
        } else {
            paMapInfo->refCount--;
        }
        
        it = sortedWindows_.erase(it);
        windowMap_.erase(devWin);
        CHK_RET(hrtFree(devWin));
        break;
    }

    return ret;
}

HcclResult SymmetricMemory::GetSymmetricMemPtr(void* ptr, size_t size, void** win, void *symPtr)
{
    uintptr_t userVaStart = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t userVaEnd = userVaStart + size;

    // 遍历所有窗口
    for (const auto& pWin : sortedWindows_) {
        uintptr_t winStart = reinterpret_cast<uintptr_t>(pWin->userVa);
        if (winStart > userVaStart) {
            return HCCL_E_NOT_FOUND;
        }

        if (userVaStart >= winStart && userVaEnd <= winStart + pWin->userSize) {
            *win = pWin->devWin;
            symPtr = pWin->userVa;
            return HCCL_SUCCESS;
        }
    }

    return HCCL_E_NOT_FOUND;
}

HcclResult SymmetricMemory::FindSymmetricWindow(void* ptr, size_t size, void** win, u64 &offset)
{
    uintptr_t userVaStart = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t userVaEnd = userVaStart + size;

    // 遍历所有窗口
    for (const auto& pWin : sortedWindows_) {
        uintptr_t winStart = reinterpret_cast<uintptr_t>(pWin->userVa);
        if (winStart > userVaStart) {
            return HCCL_E_NOT_FOUND;
        }

        if (userVaStart >= winStart && userVaEnd <= winStart + pWin->userSize) {
            *win = pWin->devWin;
            offset = userVaStart - winStart;
            return HCCL_SUCCESS;
        }
    }

    return HCCL_E_NOT_FOUND;
}

// --- Private Methods ---
HcclResult SymmetricMemory::RegisterInternal(aclrtDrvMemHandle &paHandle, size_t offset, size_t mapSize)
{
    aclrtMemFabricHandle shareableHandle;
    if (aclrtMemExportToShareableHandleV2(paHandle, 0, 
        ACL_MEM_SHARE_HANDLE_TYPE_FABRIC, (void*)&shareableHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to export shareable handle. offset: %zu, size: %zu",
            offset, mapSize);
        return HCCL_E_DRV;
    }
    if(aclrtMemSetPidToShareableHandleV2((void*)&shareableHandle, ACL_MEM_SHARE_HANDLE_TYPE_FABRIC,
        remoteShareablePids.data(), remoteShareablePids.size()) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to aclrtMemSetPidToShareableHandleV2");
        return HCCL_E_DRV;
    }

    ShareableInfo shareableInfo{offset, mapSize, shareableHandle};
    std::vector<ShareableInfo> remoteShareableInfos(rankSize_);

    CHK_RET(allGatherManager_->AllGather((void*)&shareableInfo, (void*)remoteShareableInfos.data(), sizeof(ShareableInfo)));
    for (u32 i = 0; i < rankSize_; i++) {
        if (remoteShareableInfos[i].offset != offset || remoteShareableInfos[i].size != mapSize) {
            HCCL_ERROR("[SymmetricMemory][RegisterInternal] rank[%u]:[offset: %llu, mapSize: %llu] is not equal to "
            "rank[%u]:[offset: %llu, mapSize: %llu]. Please ensure collective invocation!", rank_, offset, mapSize,
            i, remoteShareableInfos[i].offset, remoteShareableInfos[i].size);
            return HCCL_E_INTERNAL;
        }
    }

    u32 i = 0;
    if(paMappingMap_[paHandle]->refCount == 1) {
        aclrtDrvMemHandle importedHandle;
        for (; i < rankSize_; i++) {
            void* targetVa = static_cast<uint8_t*>(heapBase_) + (stride_ * i) + offset;
            if (i == rank_) {
                importedHandle = paHandle;
            } else if (aclrtMemImportFromShareableHandleV2((void*)&(remoteShareableInfos[i].handle), ACL_MEM_SHARE_HANDLE_TYPE_FABRIC, 0,
                &importedHandle) != ACL_SUCCESS) {
                HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to import handle from rank %u.", i);
                goto MAP_ERROR;
            }

            if (aclrtMapMem(targetVa, mapSize, 0, importedHandle, 0) != ACL_SUCCESS) {
                HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to map mem for rank %u at va %p.", i, targetVa);
                goto MAP_ERROR;
            }
            HCCL_INFO("[SymmetricMemory][RegisterInternal] success to Mapmem for rank %u at va %p to handle[%p].", i, targetVa, importedHandle);
        }
    }
    return HCCL_SUCCESS;

MAP_ERROR:
    for (u32 j = 0; j < i; j++) {
        (void)aclrtUnmapMem(static_cast<uint8_t*>(heapBase_) + (stride_ * j) + offset);
    }
    return HCCL_E_DRV;
}

} // namespace hccl
