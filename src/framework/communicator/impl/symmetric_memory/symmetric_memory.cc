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
        for (auto it = freeList_.begin(); it != freeList_.end(); ++it) {
            size_t start = it->offset;
            size_t end = it->offset + it->size;

            // 计算对齐后的offset
            size_t alignedOffset = (start + align - 1) & ~(align - 1);
            
            if (alignedOffset + size <= end) {
                // 找到了
                offset = alignedOffset;
                
                size_t frontPad = alignedOffset - start;
                size_t backPad = (end) - (alignedOffset + size);

                it = freeList_.erase(it); // 移除旧块
                
                if (backPad > 0) {
                    freeList_.push_front({alignedOffset + size, backPad});
                }
                if (frontPad > 0) {
                    freeList_.push_front({start, frontPad});
                }
                // 重新排序 (push_front可能破坏顺序，简单起见sort)
                freeList_.sort([](const FreeBlock& a, const FreeBlock& b) {
                    return a.offset < b.offset;
                });
                
                return HCCL_SUCCESS;
            }
        }
        return HCCL_E_MEMORY;
    }

    HcclResult Release(size_t offset, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 找到插入位置并合并
        auto it = freeList_.begin();
        while (it != freeList_.end() && it->offset < offset) {
            ++it;
        }
        
        // 插入新释放的块
        auto newIt = freeList_.insert(it, {offset, size});

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

SymmetricMemory::SymmetricMemory(HcclComm &comm, u32 rank, u32 rankSize, size_t stride, u32 devicePhyId)
    : commHandle_(comm), 
      rank_(rank),
      rankSize_(rankSize),
      devicePhyId_(devicePhyId),
      stride_(stride), 
      heapBase_(nullptr), 
      granularity_(0),
      vaAllocator_(new (std::nothrow) SimpleVaAllocator()),
      initResult_(HCCL_E_INTERNAL) {}

SymmetricMemory::~SymmetricMemory() 
{
    for (auto& pair : windowMap_) {
        // pair.first is void* devWin
        if (pair.first) hrtFree(pair.first);
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

    if (stream_) {
        aclrtDestroyStream(stream_);
    }

    HcclCommDestroy(subCommHandle_);
}

HcclResult SymmetricMemory::EnsureInit() {
    std::call_once(init_flag_, [this]() {
        initResult_ = Init();
    });
    return initResult_;
}

HcclResult SymmetricMemory::SymmetricMemory::Init() 
{
    // 0. 检查Pimpl是否构造成功
    CHK_SMART_PTR_NULL(vaAllocator_);

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
    
    // 默认大页对齐
    if (aclrtReserveMemAddress(&heapBase_, totalHeapSize, granularity_, nullptr, 1) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][Init] aclrtReserveMemAddress failed to reserve %zu bytes.", totalHeapSize);
        return HCCL_E_INTERNAL;
    }

    // 5. 初始化VA分配器 (管理本地rank的stride_大小空间，即管理偏移量)
    // 这是一个集合调用，所有rank上的vaAllocator_状态将保持一致（前提是 SimpleVaAllocator 是确定性的）
    CHK_RET(vaAllocator_->Init(stride_));

    CHK_RET(CreateSubComm());

    HCCL_INFO("[SymmetricMemory][Init] SymmetricMemory initialized. Rank[%u], Local Heap Base: %p, Stride: %u, Ranks: %u.",
               rank_, heapBase_, stride_, rankSize_);
    return HCCL_SUCCESS;
}

// ==================================================================
//
//           *** NO OTHER CHANGES REQUIRED BELOW ***
//
// ==================================================================

HcclResult HcclMemAlloc(void **ptr, size_t size)
{
    return HCCL_E_NOT_SUPPORT;
}
HcclResult HcclMemFree(void *ptr)
{
    return HCCL_E_NOT_SUPPORT;
}

void* SymmetricMemory::AllocSymmetricMem(size_t size)
{
    void* devWin = nullptr;
    void *ptr = nullptr;
    HcclResult ret = HcclMemAlloc(&ptr, size);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][AllocSymmetricMem] HcclMemAlloc failed for size[%u].", size);
        return NULL;
    }

    ret = RegisterSymmetricMem(ptr, size, &devWin);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][AllocSymmetricMem] RegisterSymmetricMem failed for ptr[%p], size[%u].", ptr, size);
        (void)HcclMemFree(ptr);
        return NULL;
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

// stub
int aclMemRetainHandle(void *ptr, size_t size, aclrtDrvMemHandle *handle)
{
    return 0;
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

    size_t alignedSize = (size + granularity_ - 1) & ~(granularity_ - 1);
    void* alignedPtr = (void*)((uintptr_t)ptr & ~(granularity_ - 1));
    aclrtDrvMemHandle paHandle;
    if (aclMemRetainHandle(alignedPtr, alignedSize, &paHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] aclMemAttainHandle failed for ptr[%p], size[%ld]. ", ptr, size);
        return HCCL_E_PARA;
    }

    size_t offset = 0;
    if (vaAllocator_->Reserve(alignedSize, granularity_, offset) != HCCL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][RegisterSymmetricMem] Failed to reserve VA space in symmetric heap for size %zu.", alignedSize);
        return HCCL_E_MEMORY;
    }

    std::shared_ptr<SymmetricWindow> pWin(new (std::nothrow) SymmetricWindow());
    pWin->userVa = ptr;
    pWin->userSize = size;
    pWin->heapOffset = (uintptr_t)alignedPtr - (uintptr_t)ptr + offset;
    pWin->alignedHeapOffset = offset;
    pWin->alignedSize = alignedSize;
    pWin->localRank = rank_;
    pWin->rankSize = rankSize_;
    pWin->stride = stride_;
    HcclResult ret = RegisterInternal(paHandle, offset, alignedSize);
    if (ret != HCCL_SUCCESS) {
        (void)vaAllocator_->Release(offset, alignedSize);
        return ret;
    }

    ret = AddSymmetricWindow(pWin);
    if (ret != HCCL_SUCCESS) {
        (void)vaAllocator_->Release(offset, alignedSize);
        return ret;
    }
    *devWin = pWin->devWin;
    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::DeregisterSymmetricMem(void* devWin)
{
    HcclResult ret = HCCL_SUCCESS;
    if (devWin == nullptr) {
        return ret;
    }

    for (auto it = sortedWindows_.begin(); it != sortedWindows_.end();) {
        if ((*it)->devWin != devWin) {
            it++;
            continue;
        }

        for (u32 i = 0; i < rankSize_; i++) {
            aclError aclRet = aclrtUnmapMem(static_cast<uint8_t*>(heapBase_) + (stride_ * i) + (*it)->alignedHeapOffset);
            if (aclRet != 0) {
                HCCL_ERROR("[SymmetricMemory][DeregisterSymmetricMem] Failed to unmap mem for rank %u at va %p, ret[%d].", 
                    i, static_cast<uint8_t*>(heapBase_) + (stride_ * i) + (*it)->alignedHeapOffset, aclRet);
                ret = HCCL_E_DRV;
            }
        }
        vaAllocator_->Release((*it)->alignedHeapOffset, (*it)->alignedSize);
        it = sortedWindows_.erase(it);
        windowMap_.erase(devWin);
        break;
    }

    return ret;
}

HcclResult SymmetricMemory::FindSymmetricWindow(void* ptr, size_t size, std::shared_ptr<SymmetricWindow> &win, size_t &offset)
{
    uintptr_t userVaStart = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t userVaEnd = userVaStart + size;

    std::shared_ptr<SymmetricWindow> targetWindow = nullptr;

    // 遍历所有窗口
    for (const auto& pWin : sortedWindows_) {
        uintptr_t winStart = reinterpret_cast<uintptr_t>(pWin->userVa);
        if (winStart > userVaStart) {
            return HCCL_E_NOT_FOUND;
        }

        if (userVaStart >= winStart && userVaEnd <= winStart + pWin->userSize) {
            win = pWin;
            offset = userVaStart - winStart;
            return HCCL_SUCCESS;
        }
    }

    return HCCL_E_NOT_FOUND;
}

// --- Private Methods ---
HcclResult SymmetricMemory::RegisterInternal(aclrtDrvMemHandle &paHandle, size_t offset, size_t mapSize)
{
    uint64_t shareableHandle;
    std::vector<uint64_t> remoteShareableHandles(rankSize_, 0);

    if (aclrtMemExportToShareableHandle(paHandle, ACL_MEM_HANDLE_TYPE_NONE, 
        ACL_RT_VMM_EXPORT_FLAG_DEFAULT, &shareableHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to export shareable handle. offset: %zu, size: %zu",
            offset, mapSize);
        return HCCL_E_DRV;
    }

    if (ExchangePhyAddrHandle((void*)&shareableHandle, (void*)remoteShareableHandles.data(), sizeof(uint64_t),
        HCCL_DATA_TYPE_INT8) != HCCL_SUCCESS) {
        return HCCL_E_INTERNAL;
    }

    u32 i = 0;
    aclrtDrvMemHandle importedHandle;
    for (; i < rankSize_; i++) {
        void* targetVa = static_cast<uint8_t*>(heapBase_) + (stride_ * i) + offset;
        if (aclrtMemImportFromShareableHandle(remoteShareableHandles[i], devicePhyId_, 
            &importedHandle) != ACL_SUCCESS) {
            HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to import handle from rank %u.", i);
            goto MAP_ERROR;
        }

        if (aclrtMapMem(targetVa, mapSize, 0, importedHandle, 0) != ACL_SUCCESS) {
            HCCL_ERROR("[SymmetricMemory][RegisterInternal] Failed to map mem for rank %u at va %p.", i, targetVa);
            goto MAP_ERROR;
        }
    }
    return HCCL_SUCCESS;

MAP_ERROR:
    for (u32 j = 0; j < i; j++) {
        (void)aclrtUnmapMem(static_cast<uint8_t*>(heapBase_) + (stride_ * j) + offset);
    }
    return HCCL_E_DRV;
}

HcclResult SymmetricMemory::CreateSubComm()
{
    // aclrtStream stream;
    HcclCommConfig config;
    HcclCommConfigInit(&config);
    CHK_RET(hrtStreamCreate(&stream_));

    config.hcclBufferSize = 1;  // 1MByte
    std::vector<u32> rankIds(rankSize_);
    for (u32 i = 0; i < rankSize_; i++) {
        rankIds[i] = i;
    }
    CHK_RET(HcclCreateSubCommConfig(&commHandle_, rankSize_, rankIds.data(), 1, rank_, &config, &subCommHandle_));

    return HCCL_SUCCESS;
}

HcclResult SymmetricMemory::ExchangePhyAddrHandle(void* inputBuff, void *outputBuff, u64 inputCount, HcclDataType dataType)
{
    u32 unitSize = SIZE_TABLE[dataType];
    u64 inputSize = inputCount * unitSize;
    u64 outputSize = inputSize * rankSize_;
    void* sendBuff = nullptr;
    void* recvBuff = nullptr;

    CHK_RET(hrtMalloc((void**)&sendBuff, inputSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHK_RET(hrtMalloc((void**)&recvBuff, outputSize, ACL_MEM_MALLOC_HUGE_FIRST));
    CHK_RET(hrtMemcpy((void*)sendBuff, inputSize, inputBuff, inputSize, ACL_MEMCPY_HOST_TO_DEVICE));

    CHK_RET(HcclAllGather(sendBuff, recvBuff, inputCount, dataType, subCommHandle_, stream_));
    aclSynchronizeStream(stream_); // TODO，增加hrt接口判断返回值
    CHK_RET(hrtMemcpy((void*)outputBuff, outputSize, (void*)recvBuff, outputSize, ACL_MEMCPY_DEVICE_TO_HOST));

    CHK_RET(hrtFree(sendBuff));
    CHK_RET(hrtFree(recvBuff));

    return HCCL_SUCCESS;
}

} // namespace hccl
