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
#include <unordered_set>
#include <utility>

#include "aicpu_operator_pub.h"
#include "coll_alg_param.h" // algorithm/HcclTopoInfo
#include "hccl_types.h"
#include "topo_matcher.h" // algorithm/AlgResourceResponse

namespace hccl {

class AicpuBlocklistManager {
public:
    // A3 AICPU局部重执行stream-level相关信息
    struct StreamPartialOpRetryInfo {
        // 通信域初始化时 (配置信息)
        const uint32_t sqDepth = 0; // 给定stream的RTSQ size (与算子无关)

        // 当前算子展开前 (上一算子展开信息)
        uint32_t beginSqTail = 0; // 给定stream的RTSQ tail

        // 当前算子展开过程中 (当前算子展开信息)
        uint64_t totalSqeCount = 0; // 给定stream下发的SQE总数 (含flip/alltoallv placeholder)
        int64_t firstFlipPlaceholderSqIdx = -1; // 给定stream下发的第一个flip placeholder SQE index
        int64_t secondFlipPlaceholderSqIdx = -1; // 给定stream下发的第二个flip placeholder SQE index
        std::vector<int64_t> alltoallvPlaceholderSqIdxVec = {}; // 给定stream下发的所有alltoallv placeholder SQE indexes

        // 当前算子故障停流时 (当前算子执行信息)
        uint64_t execNonPlaceholderSqeCount = 0 ; // 给定stream成功执行的非placeholder SQE的个数 (黑名单-拷贝类信息)

        // 故障算子重执行时 (故障算子局部重执行信息)
        uint64_t opRetryNonPlaceholderSqeCount = 0; // 故障算子在给定stream已展开的非placeholder SQE的个数

        StreamPartialOpRetryInfo() = delete;
        StreamPartialOpRetryInfo(const uint32_t sqDepth);
        StreamPartialOpRetryInfo(const StreamPartialOpRetryInfo& other);
        ~StreamPartialOpRetryInfo();

        void ResetAndBackup(const uint32_t sqTail, const bool isRetry);
    };

    AicpuBlocklistManager();
    ~AicpuBlocklistManager();

    // Part 1: 通信域初始化阶段

    // 初始化blocklist manager
    HcclResult InitBlocklistManager(Stream& mainStream, std::vector<Stream>& slaveStreams);

    // Part 2: 当前算子展开前阶段

    // 重置与备份各stream的局部重执行相关信息
    // 注意: 属于黑名单信息的变量, 在故障算子重执行展开前 (即isRetry = true), 不能被清理
    HcclResult ResetAndBackupBeforeUnfold(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, const bool isDeviceMode, const uint32_t devId,
        Stream& mainStream, std::vector<Stream>& slaveStreams, const bool isRetry);

    // Part 3: 当前算子展开过程中阶段

    // 更新对应stream的totalSqeCount, 并检查是否超过sqDepth (局部重执行totalSqeCount约束)
    HcclResult UpdateTotalSqeCount(const int32_t streamId, const uint64_t sqeCount);

    // 更新对应stream的placeholder SQE index
    HcclResult UpdatePlaceholderSqIdx(const int32_t streamId, const int64_t curPlaceholderSqIdx, const bool isFlip);

    // Part 4: 故障触发算子重执行阶段

    // 停流时, 根据RTSQ索引计算成功执行的SQE count, 构建黑名单-拷贝类信息
    HcclResult CalcExecSqeCount(const uint32_t devId, Stream& mainStream, std::vector<Stream>& slaveStreams);

    // 停流时, 判断是否使能局部重执行
    inline bool isRetry() const { return isRetry_; }
    inline bool IsEnablePartialOpRetry() const { return isEnablePartialOpRetry_; }

    // Part 5: 重执行故障算子阶段 (接收KfcCommand:kRetry后)

    // 重执行故障算子时, 根据全局使能flag更新本地使能flag
    HcclResult SyncPartialOpRetry(const bool partialOpRetryFlag);

    // 重执行故障算子时, 更新黑名单-同步类信息
    HcclResult CalcBlockSet(const AlgResourceResponse& algResource, const HcclTopoInfo& topoinfo,
        const PartialRetryInfo& partialRetryInfo, const uint32_t mainWaitParamStreamNotifyId);
    
    // 重执行故障算子时, 应用黑名单 (拷贝/同步类信息) 决定是否过滤给定stream下当前生成的SQE
    // Rule 0: placeholder SQE不需要进行过滤, 正常下发 (理论上也不会被ApplyBlocklist处理);
    // Rule 1: 拷贝类SQE, 如果故障前已经执行过 (即opRetryNonPlaceholderSqeCount < execNonPlaceholderSqeCount), 则过滤; 否则正常下发;
    // Rule 2: 远程同步类SQE, 如果与快卡相关, 则过滤; 否则正常下发;
    // Rule 3: 本地同步类SQE, 如果是wait通信主流, 则过滤 (理论上故障算子重执行时不会展开该SQE); 否则正常下发;
    HcclResult ApplyBlocklist(const bool isCopySqe, Stream& stream, const bool isWaitSqe,
        const uint32_t notifyId, const uint64_t signalAddr, bool& isEnable, bool& isFilter);
private:
    // Part 2: 当前算子展开前阶段

    // 检查局部重执行约束 (不含totalSqeCount约束)
    HcclResult CheckPartialOpRetryConstraints_(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, const bool isDeviceMode);

    // 重置与备份给定stream的局部重执行相关信息
    HcclResult ResetAndBackupForStream_(const uint32_t devId, Stream& stream, const bool isRetry);

    // Part 4: 故障触发算子重执行阶段

    // 停流时, 根据RTSQ索引计算成功执行的SQE count
    HcclResult CalcExecSqeCountForStream_(const uint32_t devId, Stream& stream);

    // 停流时, 判断是否使能局部重执行
    static HcclResult IsInplace(const OpParam &param, const HcclTopoInfo& topoinfo, bool& isInplace); // 是否为inplace场景
    static bool IsReduce(const OpParam& param); // 是否为reduce场景 (inline reduce)
    static HcclResult ParseOpParam(const OpParam &param, const HcclTopoInfo& topoinfo,
        uint64_t& inputSize, uint64_t& outputSize); // 从param中解析input/output size

    // Part 5: 重执行故障算子阶段 (接收KfcCommand:kRetry后)

    // 针对wait信号更新黑名单-同步类信息
    HcclResult CalcWaitNotifyIdBlockSet_(const LINK& tmpLink);

    // 针对record信号更新黑名单-同步类信息
    HcclResult CalcRecordSignalAddrBlockSet_(const LINK& tmpLink);

    // 当前算子各stream的SQE信息, 用于故障后与各stream RTSQ信息比较, 推导各stream成功执行的non-placeholder SQE count
    // 注意: 只需要考虑与集合通信执行相关的stream, 即aicpu device main/slave streams
    // 注意: 只有aicpu kfc thread会访问以下变量 (展开前重置, 展开时更新, 故障停流时读取), 不需要维护锁
    bool isRetry_ = false; // 当前算子是否是重执行故障算子 (属于黑名单信息)
    bool isEnablePartialOpRetry_ = false; // 当前算子是否使能局部重执行 (属于黑名单信息)
    std::unordered_map<int32_t, StreamPartialOpRetryInfo> perStreamPartialOpRetryInfoMap_; // 当前算子各stream的局部重执行相关信息

    // 黑名单-同步类信息: 需要屏蔽的同步信号
    std::unordered_set<uint32_t> waitNotifyIdBlockSet_; // 快卡对应的remote wait notify id, 通信主流对应的local wait notify id
    std::unordered_set<uint64_t> recordSignalAddrBlockSet_; // 快卡对应的remote record signal addr
};

} // namespace hccl

#endif // __AICPU_BLOCKLIST_MANAGER_H__