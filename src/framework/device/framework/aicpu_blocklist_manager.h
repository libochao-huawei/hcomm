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

#include <mutex>
#include <string>
#include <stdint.h>
#include <unordered_map>

#include "coll_alg_param.h"
#include "hccl_types.h"
#include "topo_matcher.h"

namespace hccl {

class AicpuBlocklistManager {
public:
    static HcclResult EnablePartialOpretry(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, bool& isEnablePartialOpretry);

    AicpuBlocklistManager();
    ~AicpuBlocklistManager();

    HcclResult InitBlocklistManager();

    HcclResult BackupSqeCounts();
private:
    // 是否为inplace场景 (局部重执行不支持inplace)
    static HcclResult IsInplace(const OpParam &param, const HcclTopoInfo& topoinfo, bool& isInplace);

    // 是否为reduce场景 (局部重执行不支持inline reduce)
    static bool IsReduce(const OpParam& param);

    // 从param中解析input/output size
    static HcclResult ParseOpParam(const OpParam &param, const HcclTopoInfo& topoinfo,
        uint64_t& inputSize, uint64_t& outputSize);

    // 各stream累计下发的SQE count, 用于推导当前算子各stream成功执行的SQE count
    // 注意: 只需要考虑与集合通信执行相关的stream, 即aicpu device main/slave streams
    std::unordered_map<int32_t, uint64_t> streamIdSqeCountMap_; // Aicpu kfc线程展开时更新
    mutable std::mutex backupMutex_; // streamIdSqeCountMapBackup_的互斥锁 (只有1 reader + 1 writer, 不需要使用读写锁)
    std::unordered_map<int32_t, uint64_t> streamIdSqeCountMapBackup_; // Aicpu kfc线程展开前备份; aicpu dfx线程故障时读取
};

} // namespace hccl

#endif // __AICPU_BLOCKLIST_MANAGER_H__