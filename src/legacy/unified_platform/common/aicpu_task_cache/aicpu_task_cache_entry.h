/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_AICPU_TASK_CACHE_ENTRY_H
#define HCCLV2_AICPU_TASK_CACHE_ENTRY_H

#include <cstdint>
#include <vector>

#include "stream_lite.h"
#include "ub_conn_lite.h"
#include "udma_data_struct.h"

namespace Hccl {

// 注意: 不需要额外维护wqeType指定struct, 因为所有struct开头都是UdmaSqeCommon, 可以利用opCode判断wqe类型
union __attribute__((packed)) WqeTask {
    struct UdmaSqeRead wqeRead; // 64B (48 + 16); ub_conn_lite.cc中暂不使用UdmaSqeRead
    struct UdmaSqeWrite wqeWrite; // 64B (48 + 16)
    struct UdmaSqeWriteWithNotify wqeWriteWithNotify; // 96B (48 + 32 + 16)
};

struct DbSqeLocation {
    uint32_t sqeArrayIdx = 0; // 第几个SQE数组
    uint32_t dbSqeIdx = 0; // 数组中第几个SQE是DbSqe
};

enum class TaskArrayType : uint8_t {
    kTaskArrayTypeInvalid = 0,
    kTaskArrayTypeSqe = 1,
    kTaskArrayTypeWqe = 2,
};

struct AddrRefreshInfo {
    explicit AddrRefreshInfo();
    explicit AddrRefreshInfo(const uint32_t curMemIdx);
    explicit AddrRefreshInfo(const AddrRefreshInfo& other);
    ~AddrRefreshInfo();

    const AddrRefreshInfo& operator=(const AddrRefreshInfo& other); // 拷贝赋值操作符

    bool needRefresh = false; // false: fixed memory (例如硬件地址, ccl buffer); true: dynamic memory (e.g., user memory)
    uint32_t memIdx = 0; // 第几个memory range (cachedBaseAddrs_ + cachedSizes_)
};

class AicpuTaskCacheEntry {
public:
    explicit AicpuTaskCacheEntry();
    ~AicpuTaskCacheEntry();

    // Cache admission (cache miss)
    HcclResult AddSqeArray(RtsqA5* rtsqPtr, const uint64_t sqeCount, const uint8_t* sqeArray);
    HcclResult AddWqeArray(UbConnLite* ubConnLitePtr, const std::vector<WqeTask>& wqeTasks, const uint32_t dbSqeIdx);
    HcclResult SubmitCacheEntry(const uint64_t* baseAddrs, const uint64_t* memSizes, const uint64_t count);

    // Cache hit
    // 注意: 如果需要支持profiling, 参考AicpuTsThread和UbTransportLiteImpl构建TaskParam并调用profiling callback
    // 注意: inplace刷新缓存的task, 下发完成后需要更新缓存的user input/output memory range
    HcclResult RefreshAndLaunch(const uint64_t* baseAddrs, const uint64_t* memSizes, const uint64_t count,
        const bool profilingEnable);

private:
    inline static void CombineUint32ToUint64(uint64_t& addr, const uint32_t high, const uint32_t low)
    {
        constexpr uint64_t uintBitWidth = 32;
        addr = (static_cast<uint64_t>(high) << uintBitWidth) | static_cast<uint64_t>(low);
        return;
    }

    inline static void SplitUint64ToUint32(const uint64_t addr, uint32_t& high, uint32_t& low)
    {
        constexpr uint64_t uintBitWidth = 32;
        high = static_cast<uint32_t>(addr >> uintBitWidth);
        low = static_cast<uint32_t>(addr & 0xFFFFFFFFULL);
        return;
    }

    inline static HcclResult InRange(const uint64_t baseAddr, const uint64_t memSize,
        const uint64_t addr, bool& isInRange)
    {
        // 获得endAddr
        uint64_t endAddr = 0;
        CHK_PRT_RET(baseAddr + memSize < baseAddr,
            HCCL_ERROR("[AicpuTaskCacheEntry][InRange] baseAddr[0x%016llx] + memSize[%llu] overflows", baseAddr, memSize),
            HCCL_E_INTERNAL);
        endAddr = baseAddr + memSize;

        isInRange = (addr >= baseAddr && addr < endAddr);
        return HCCL_SUCCESS;
    }

    // 插入WQE/SQE数组时, 获取AddrRefreshInfo
    HcclResult GetSqeAddrRefreshInfo_(const uint8_t* sqePtr, AddrRefreshInfo& srcAddrRefreshInfo,
        AddrRefreshInfo& dstAddrRefreshInfo);
    HcclResult GetWqeAddrRefreshInfo_(const WqeTask& wqeTask, AddrRefreshInfo& srcAddrRefreshInfo,
        AddrRefreshInfo& dstAddrRefreshInfo);
    HcclResult GetAddrRefreshInfo_(const uint32_t addrLow, const uint32_t addrHigh, AddrRefreshInfo& addrRefreshInfo);

    // 刷新下发WQE, 并刷新对应的DbSqe
    HcclResult RefreshWqeTasks_(const size_t arrayIdx, const uint64_t* baseAddrs, const uint64_t* memSizes,
        const uint64_t count);
    HcclResult LaunchWqeTasks_(const size_t arrayIdx);
    HcclResult RefreshDbSqe_(const size_t arrayIdx);

    // 根据AddrRefreshInfo刷新task地址字段
    inline HcclResult RefreshTaskAddr_(uint32_t& addrLow, uint32_t& addrHigh, AddrRefreshInfo& addrRefreshInfo,
        const uint64_t* baseAddrs, const uint64_t* memSizes, const uint64_t count) const
    {
        // 拼接地址
        uint64_t addr = 0;
        AicpuTaskCacheEntry::CombineUint32ToUint64(addr, addrHigh, addrLow);
        CHK_RET(RefreshTaskAddr_(addrLow, addrHigh, addr, addrRefreshInfo, baseAddrs, memSizes, count));
        return HCCL_SUCCESS;
    }
    HcclResult RefreshTaskAddr_(uint32_t& addrLow, uint32_t& addrHigh, const uint64_t addr, AddrRefreshInfo& addrRefreshInfo,
        const uint64_t* baseAddrs, const uint64_t* memSizes, const uint64_t count) const;

    // 多段SQE数组: 每段SQE数组对应一次LaunchTask, 以及相应的RtsqA5指针
    std::vector<uint8_t*> sqeArrays_;
    std::vector<RtsqA5*> rtsqPtrs_;
    std::vector<uint64_t> sqeCounts_;
    std::vector<std::vector<AddrRefreshInfo>> sqeSrcAddrRefreshInfoArrays_; // 每段每个SQE中srcAddr (if any)对应的刷新信息
    std::vector<std::vector<AddrRefreshInfo>> sqeDstAddrRefreshInfoArrays_; // 每段每个SQE中dstAddr (if any)对应的刷新信息

    // 多段WQE数组: 每段WQE数组对应多次ProcessOneWqe/ProcessOneWqeWithNotify (按256MiB切分, 但始终只对应**一个**DbSqe),
    //     以及相应的ubConnLite指针和DbSqeLocation
    std::vector<std::vector<WqeTask>> wqeTaskArrays_;
    std::vector<UbConnLite*> ubConnLitePtrs_;
    std::vector<DbSqeLocation> dbSqeLocations_;
    std::vector<std::vector<AddrRefreshInfo>> wqeSrcAddrRefreshInfoArrays_; // 每段每个WQE中srcAddr (if any)对应的刷新信息
    std::vector<std::vector<AddrRefreshInfo>> wqeDstAddrRefreshInfoArrays_; // 每段每个WQE中dstAddr (if any)对应的刷新信息

    // 下发顺序
    std::vector<TaskArrayType> launchOrder_; // 大小一定为SQE+WQE数组之和

    // Cached memory ranges
    std::vector<uint64_t> cachedBaseAddrs_;
    std::vector<uint64_t> cachedMemSizes_;
};

} // namespace Hccl

#endif // HCCLV2_AICPU_TASK_CACHE_ENTRY_H
