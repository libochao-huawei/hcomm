/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __AICPU_BLOCKLIST_MANAGER_H__
#define __AICPU_BLOCKLIST_MANAGER_H__

#include <vector>
#include <string>
#include <stdint.h>
#include <unordered_map>
#include <utility>

#include "coll_alg_param.h"
#include "hccl_types.h"
#include "topo_matcher.h"

namespace hccl {

class AicpuBlocklistManager {
public:
    struct StreamDispatchInfo {
        const uint32_t sqDepth = 0; // 通信域初始化时, 给定stream的RTSQ size (与算子无关)
        uint32_t beginSqTail = 0; // 当前算子展开前, 给定stream的RTSQ tail
        uint64_t totalSqeCount = 0; // 当前算子展开过程中, 给定stream下发的SQE总数 (含flip placeholder)
        int64_t firstFlipPlaceholderSqIdx = -1; // 当前算子展开过程中, 给定stream下发的第一个flip placeholder SQE index

        StreamDispatchInfo() = delete;
        StreamDispatchInfo(const uint32_t sqDepth);
        StreamDispatchInfo(const StreamDispatchInfo& other);
        ~StreamDispatchInfo();
    };

    AicpuBlocklistManager();
    ~AicpuBlocklistManager();

    // Part 1: 通信域初始化阶段

    // 初始化blocklist manager
    HcclResult InitBlocklistManager(Stream& mainStream, std::vector<Stream>& slaveStreams);

    // Part 2: 当前算子展开前阶段

    // 重置与备份各stream的下发信息 (TODO: 确定平均耗时)
    HcclResult ResetAndBackupBeforeUnfold(const uint32_t devId, Stream& mainStream, std::vector<Stream>& slaveStreams);

    // Part 3: 当前算子展开过程中阶段

    // 更新对应stream的totalSqeCount (TODO: 确定平均耗时)
    HcclResult UpdateTotalSqeCount(const int32_t streamId, const uint64_t sqeCount);

    // 更新对应stream的第一个flip placeholder SQE index (TODO: 确定平均耗时)
    HcclResult UpdateFirstFlipPlaceholderSqIdx(const int32_t streamId, const int64_t firstFlipPlaceholderSqIdx);

    // Part 4: 故障触发算子重执行阶段

    // 停流时, 判断是否使能局部重执行
    HcclResult EnablePartialOpretry(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, bool& isEnablePartialOpretry);
private:
    // 是否为inplace场景 (局部重执行不支持inplace)
    static HcclResult IsInplace(const OpParam &param, const HcclTopoInfo& topoinfo, bool& isInplace);

    // 是否为reduce场景 (局部重执行不支持inline reduce)
    static bool IsReduce(const OpParam& param);

    // 从param中解析input/output size
    static HcclResult ParseOpParam(const OpParam &param, const HcclTopoInfo& topoinfo,
        uint64_t& inputSize, uint64_t& outputSize);

    // 当前算子各stream的SQE信息, 用于故障后与各stream RTSQ信息比较, 推导各stream成功执行的non-flip SQE count
    // 注意: 只需要考虑与集合通信执行相关的stream, 即aicpu device main/slave streams
    // 注意: 只有aicpu kfc thread会访问以下变量 (展开前重置, 展开时更新, 故障停流时读取), 不需要维护锁
    bool exceedSqDepth_ = false; // 当前算子是否存在某条stream RTSQ下发的SQE数量超过sqDepth
    std::unordered_map<int32_t, StreamDispatchInfo> perStreamDispatchInfoMap_; // 当前算子各stream的下发信息
};

} // namespace hccl

#endif // __AICPU_BLOCKLIST_MANAGER_H__