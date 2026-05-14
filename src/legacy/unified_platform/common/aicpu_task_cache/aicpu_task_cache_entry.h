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

struct AicpuTaskMemRange {
    explicit AicpuTaskMemRange();
    explicit AicpuTaskMemRange(const uint64_t curBaseAddr, const uint64_t curMemSize);
    explicit AicpuTaskMemRange(const AicpuTaskMemRange& other);
    ~AicpuTaskMemRange();

    const AicpuTaskMemRange& operator=(const AicpuTaskMemRange& other);

    HcclResult GetEndAddr(uint64_t& endAddr) const;
    HcclResult InRange(const uint64_t addr, bool& isInRange) const;

    bool isValid;
    uint64_t baseAddr;
    uint64_t memSize;
};

struct AddrRefreshInfo {
    explicit AddrRefreshInfo();
    explicit AddrRefreshInfo(const uint32_t curRankId, const uint8_t curMemType);
    explicit AddrRefreshInfo(const AddrRefreshInfo& other);
    ~AddrRefreshInfo();

    const AddrRefreshInfo& operator=(const AddrRefreshInfo& other); // 拷贝赋值操作符

    static constexpr uint8_t CCL_MEMTYPE = 0;
    static constexpr uint8_t USER_INPUT_MEMTYPE = 1;
    static constexpr uint8_t USER_OUTPUT_MEMTYPE = 2;

    uint32_t rankId; // 当前只考虑单算子场景, 即远端只访问ccl buffer且地址不变 (无需刷新), 因此rankId一定为localRank (此字段暂时保持为0)
    uint8_t memType; // 0: ccl buffer; 1: user input; 2: user output
};

// 注意: 不需要额外维护wqeType指定struct, 因为所有struct开头都是UdmaSqeCommon, 可以利用opCode判断wqe类型
union __attribute__((packed)) WqeTask {
    struct UdmaSqeRead wqeRead; // 64B (48 + 16)
    struct UdmaSqeWrite wqeWrite; // 64B (48 + 16)
    struct UdmaSqeWriteWithNotify wqeWriteWithNotify; // 96B (48 + 32 + 16)
};

struct DbSqeLocation {
    uint32_t sqeArrayIdx; // 第几个SQE数组
    uint32_t dbSqeIdx; // 数组中第几个SQE是DbSqe
};

enum class TaskArrayType : uint8_t {
    kTaskArrayTypeInvalid = 0,
    kTaskArrayTypeSqe = 1,
    kTaskArrayTypeWqe = 2,
};

class AicpuTaskCacheEntry {
public:
    AicpuTaskCacheEntry() = delete;
    explicit AicpuTaskCacheEntry(const AicpuTaskMemRange& localUserInputMemRange,
        const AicpuTaskMemRange& localUserOutputMemRange);
    ~AicpuTaskCacheEntry();

    // Cache admission (cache miss)
    // 注意: 目前WQE通过ProcessOneWqe逐个下发, WQE数组大小一定为1
    HcclResult AddSqeArray(RtsqA5* rtsqPtr, const size_t sqeCount, const uint8_t* sqeArray);
    HcclResult AddWqeArray(UbConnLite* ubConnLitePtr, const UdmaSqeRead& wqeRead, const uint32_t dbSqeIdx);
    HcclResult AddWqeArray(UbConnLite* ubConnLitePtr, const UdmaSqeWrite& wqeWrite, const uint32_t dbSqeIdx);
    HcclResult AddWqeArray(UbConnLite* ubConnLitePtr, const UdmaSqeWriteWithNotify& wqeWriteWithNotify,
        const uint32_t dbSqeIdx);

    // Cache hit
    // 注意: 如果需要支持profiling, 参考AicpuTsThread和UbTransportLiteImpl构建TaskParam并调用profiling callback
    // 注意: inplace刷新缓存的task, 下发完成后需要更新缓存的user input/output memory range
    HcclResult RefreshAndLaunch(const AicpuTaskMemRange& localUserInputMemRange,
        const AicpuTaskMemRange& localUserOutputMemRange, const bool profilingEnable);

private:
    inline void CombineUint32ToUint64(uint64_t& addr, const uint32_t high, const uint32_t low) const
    {
        constexpr uint64_t uintBitWidth = 32;
        addr = (static_cast<uint64_t>(high) << uintBitWidth) | static_cast<uint64_t>(low);
        return;
    }

    inline void SplitUint64ToUint32(const uint64_t addr, uint32_t& high, uint32_t& low) const
    {
        constexpr uint64_t uintBitWidth = 32;
        high = static_cast<uint32_t>(addr >> uintBitWidth);
        low = static_cast<uint32_t>(addr & 0xFFFFFFFFULL);
        return;
    }

    HcclResult RefreshTaskAddr(uint32_t& addrLow, uint32_t& addrHigh, AddrRefreshInfo& addrRefreshInfo,
        const AicpuTaskMemRange& localUserInputMemRange, const AicpuTaskMemRange& localUserOutputMemRange) const;

    // Current rank's user input/output memory range
    AicpuTaskMemRange localUserInputMemRange_;
    AicpuTaskMemRange localUserOutputMemRange_;

    // 多段SQE数组: 每段SQE数组对应一次LaunchTask, 以及相应的RtsqA5指针
    std::vector<uint8_t*> sqeArrays_;
    std::vector<RtsqA5*> rtsqPtrs_;
    std::vector<std::vector<AddrRefreshInfo>> sqeSrcInfoArrays_; // 每段每个SQE中srcAddr (if any)对应的刷新信息
    std::vector<std::vector<AddrRefreshInfo>> sqeDstInfoArrays_; // 每段每个SQE中dstAddr (if any)对应的刷新信息

    // 多段WQE数组: 每段WQE数组对应一次ProcessOneWqe, 以及相应的ubConnLite指针和DbSqeLocation
    // 注意: 目前WQE逐个下发, 因此每段WQE数组大小一定为1
    std::vector<std::vector<WqeTask>> wqeTaskArrays_;
    std::vector<UbConnLite*> ubConnLitePtrs_;
    std::vector<DbSqeLocation> dbSqeLocations_;
    std::vector<std::vector<AddrRefreshInfo>> wqeSrcInfoArrays_; // 每段每个WQE中srcAddr (if any)对应的刷新信息
    std::vector<std::vector<AddrRefreshInfo>> wqeDstInfoArrays_; // 每段每个WQE中dstAddr (if any)对应的刷新信息

    // 下发顺序
    std::vector<TaskArrayType> launchOrder_; // 大小一定为SQE+WQE数组之和
};

} // namespace Hccl

#endif // HCCLV2_AICPU_TASK_CACHE_ENTRY_H
