/*
 * Copyright (c) 2024-2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "log.h"
#include "hccl_communicator.h"
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <memory> // For std::unique_ptr

// HCCL
#include "hccl/hccl_types.h"
#include "hccl/base.h"
#include "hccl/hccl.h" // 假设 HcclCommunicator 在这里

// NPU VMM API
#include "acl/acl_rt.h"

namespace hccl {

struct SymmetricWindow {
    void *userVa;
    size_t userSize;

    size_t heapOffset; // 对应userVa在对称堆上的地址, 是不一定对齐的
    size_t alignedHeapOffset;
    size_t alignedSize;
    u32 localRank;
    u32 rankSize;
    u32 stride;

    void *devWin; // device端结构体
};

/**
 * @brief 对称内存管理器
 * 负责对称VA空间的预留、注册、映射和查找。
 */
class SymmetricMemory {
public:

    SymmetricMemory(HcclComm &comm, u32 rank, u32 rankSize, size_t stride, u32 devicePhyId);
    ~SymmetricMemory();

    // 禁止拷贝和赋值
    SymmetricMemory(const SymmetricMemory&) = delete;
    SymmetricMemory& operator=(const SymmetricMemory&) = delete;
    HcclResult EnsureInit();
    void* AllocSymmetricMem(size_t size);
    HcclResult FreeSymmetricMem(void* devWin);
    HcclResult RegisterSymmetricMem(void* ptr, size_t size, void** devWin);
    HcclResult DeregisterSymmetricMem(void* devWin);
    HcclResult FindSymmetricWindow(void* ptr, size_t size, std::shared_ptr<SymmetricWindow> &win, size_t &offset);

private:
    HcclResult Init();
    HcclResult RegisterInternal(aclrtDrvMemHandle &paHandle, size_t offset, size_t mapSize);
    HcclResult AddSymmetricWindow(std::shared_ptr<SymmetricWindow> &win);
    HcclResult DeleteSymmetricWindow(std::shared_ptr<SymmetricWindow> &win);
    HcclResult DeleteSymmetricWindow(void* devWin);
    HcclResult CreateSubComm();
    HcclResult ExchangePhyAddrHandle(void * inputBuff, void *outputBuff, u64 inputCount, HcclDataType dataType);
private:
    // VA空间分配器 (Pimpl)
    HcclComm commHandle_;
    HcclComm subCommHandle_;
    std::once_flag init_flag_;
    u32 rank_;
    u32 rankSize_;
    u32 devicePhyId_;
    size_t stride_;      // 每个Rank的VA空间大小
    void* heapBase_;  // 对称VA空间的总基地址 (所有rank相同)
    size_t granularity_;
    class SimpleVaAllocator;
    std::unique_ptr<SimpleVaAllocator> vaAllocator_;
    aclrtStream stream_;
    HcclResult initResult_; // 存储Init()的结果
    std::vector<std::shared_ptr<SymmetricWindow>> sortedWindows_;
    std::map<void*, std::shared_ptr<SymmetricWindow>> windowMap_; // device指针到host SymmetricWindow 的映射
    std::unordered_map<void *, SymmetricWindow*> exactMatchMap_; // 用于提升查找性能
};

} // namespace hccl