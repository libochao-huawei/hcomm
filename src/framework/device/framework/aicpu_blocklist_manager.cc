/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_blocklist_manager.h"

#include "aicpu_hccl_sqcq.h"
#include "comm_utils.h" // algorithm/COMM_COMBINE_ORDER
#include "driver/ascend_hal_define.h"
#include "hccl_common.h"
#include "hccl_impl.h" // algorithm/COMM_INDEX_0
#include "log.h"

namespace hccl {
    // struct StreamPartialOpRetryInfo

    AicpuBlocklistManager::StreamPartialOpRetryInfo::StreamPartialOpRetryInfo(const uint32_t curSqDepth)
        : sqDepth(curSqDepth), beginSqTail(0), totalSqeCount(0),
        firstFlipPlaceholderSqIdx(-1), secondFlipPlaceholderSqIdx(-1), alltoallvPlaceholderSqIdxVec(),
        execNonPlaceholderSqeCount(0), opRetryNonPlaceholderSqeCount(0) {
    }
    
    AicpuBlocklistManager::StreamPartialOpRetryInfo::StreamPartialOpRetryInfo(const StreamPartialOpRetryInfo& other)
        : sqDepth(other.sqDepth), beginSqTail(other.beginSqTail), totalSqeCount(other.totalSqeCount),
        firstFlipPlaceholderSqIdx(other.firstFlipPlaceholderSqIdx),
        secondFlipPlaceholderSqIdx(other.secondFlipPlaceholderSqIdx),
        alltoallvPlaceholderSqIdxVec(other.alltoallvPlaceholderSqIdxVec),
        execNonPlaceholderSqeCount(other.execNonPlaceholderSqeCount),
        opRetryNonPlaceholderSqeCount(other.opRetryNonPlaceholderSqeCount) {}

    AicpuBlocklistManager::StreamPartialOpRetryInfo::~StreamPartialOpRetryInfo() {}

    void AicpuBlocklistManager::StreamPartialOpRetryInfo::ResetAndBackup(const uint32_t sqTail)
    {
        // 备份上一算子展开信息 (用于故障停流时, 计算给定stream已执行的SQE count)
        beginSqTail = sqTail; // 当前算子展开前, 给定stream的RTSQ tail

        // 重置当前算子展开信息 (用于故障停流时, 判断局部重执行约束, 并计算给定stream已执行的non-placeholder SQE count)
        totalSqeCount = 0; // 当前算子在给定stream实际下发的SQE count (含flip/alltoallv placeholder)
        firstFlipPlaceholderSqIdx = -1; // 当前算子在给定stream第一个flip placeholder的RTSQ index
        secondFlipPlaceholderSqIdx = -1; // 当前算子在给定stream第二个flip placeholder的RTSQ index
        alltoallvPlaceholderSqIdxVec.clear(); // 清空alltoallv placeholder SQE indexes

        // 重置黑名单-拷贝类信息
        // 注意: 理论上无需重置黑名单, 因为拷贝类信息在故障停流时 (即收到kStopExec命令) 会通过CalcExecSqeCount更新
        execNonPlaceholderSqeCount = 0; // 当前算子在给定stream实际执行的non-placeholder SQE count

        // 重置故障算子局部重执行信息
        // 注意: 只有故障算子重执行时才会使用, 因此非故障算子正常展开时重置不影响正确性
        opRetryNonPlaceholderSqeCount = 0; // 故障算子在给定stream下发的非placeholder SQE的个数

        return;
    }

    // class AicpuBlocklistManager

    AicpuBlocklistManager::AicpuBlocklistManager()
    {
        HCCL_RUN_INFO("Construct AicpuBlocklistManager complete.");
    }

    AicpuBlocklistManager::~AicpuBlocklistManager() {}

    HcclResult AicpuBlocklistManager::InitBlocklistManager(Stream& mainStream, std::vector<Stream>& slaveStreams)
    {
        HCCL_RUN_INFO("[AicpuBlocklistManager][InitBlocklistManager] 1 mainStream, %u slaveStreams", slaveStreams.size());

        isRetry_ = false;
        isEnablePartialOpRetry_ = false;

        // 初始化aicpu main stream的局部重执行相关信息
        perStreamPartialOpRetryInfoMap_.clear();
        const HcclComStreamInfo& mainStreamInfo = mainStream.GetHcclStreamInfo();
        perStreamPartialOpRetryInfoMap_.emplace(mainStreamInfo.actualStreamId,
            StreamPartialOpRetryInfo(mainStreamInfo.sqDepth));
        HCCL_DEBUG("[AicpuBlocklistManager][InitBlocklistManager] main streamId[%d] sqDepth[%d]",
            mainStreamInfo.actualStreamId, mainStreamInfo.sqDepth);
        
        // 初始化aicpu slave streams的局部重执行相关信息
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            const HcclComStreamInfo& slaveStreamInfo = slaveStreams[i].GetHcclStreamInfo();
            perStreamPartialOpRetryInfoMap_.emplace(slaveStreamInfo.actualStreamId,
                StreamPartialOpRetryInfo(slaveStreamInfo.sqDepth));
            HCCL_DEBUG("[AicpuBlocklistManager][InitBlocklistManager] slave streamId[%d] sqDepth[%d]",
                slaveStreamInfo.actualStreamId, slaveStreamInfo.sqDepth);
        }

        HCCL_RUN_INFO("[AicpuBlocklistManager][InitBlocklistManager] perStreamPartialOpRetryInfoMap.size[%d]",
            perStreamPartialOpRetryInfoMap_.size());

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CheckConstraints(const size_t opUnfoldIdx, const std::string& algName,
        const OpParam &param, const HcclTopoInfo& topoinfo, const bool isDeviceMode)
    {
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        static double checkTotalUs = 0.0;
        static double checkCount = 0;
        auto checkStartTime = std::chrono::steady_clock::now();
#endif

        // 注意: 只有非故障算子正常展开前才会调用CheckConstraints

        // 重置isRetry_和isEnablePartialOpRetry_
        isRetry_ = false;
        isEnablePartialOpRetry_ = false;

        // 检查局部重执行约束 (不含totalSqeCount约束); 如果满足约束, 则将isEnablePartialOpRetry_设为true
        CHK_RET(CheckPartialOpRetryConstraints_(algName, param, topoinfo, isDeviceMode));

        // 算子正常展开使用HCCL_INFO, 避免影响性能
        HCCL_INFO("[AicpuBlocklistManager][CheckConstraints] opUnfoldIdx[%u] algName[%s] opType[%u] "
            "isDeviceMode[%d] isRetry_[%d] isEnablePartialOpRetry_[%d]",
            opUnfoldIdx, algName.c_str(), param.opType, isDeviceMode, isRetry_, isEnablePartialOpRetry_);

#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        auto checkEndTime = std::chrono::steady_clock::now();
        double checkUs = std::chrono::duration<double, std::micro>(checkEndTime - checkStartTime).count();
        checkTotalUs += checkUs;
        checkCount++;
        HCCL_ERROR("[AicpuBlocklistManager][CheckConstraints] avg checkUs[%.2f] w/ %u checks",
            checkTotalUs / checkCount, uint32_t(checkCount));
#endif

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::MarkIsRetry(const size_t opUnfoldIdx, const std::string& algName, const OpParam &param)
    {
        // 注意: 只有故障算子重执行前才会调用MarkIsRetry

        isRetry_ = true; // 标记当前算子为故障算子重执行

        // 注意: isEnablePartialOpRetry_在故障算子正常展开时已经设置过, 重执行时需要沿用之前设置的值

        // 故障算子重执行使用HCCL_RUN_INFO (非性能瓶颈)
        // 注意: 本地局部重执行flag isEnablePartialOpRetry_不一定为true (可能是正常重执行流程)
        HCCL_RUN_INFO("[AicpuBlocklistManager][MarkIsRetry] opUnfoldIdx[%u] algName[%s] opType[%u] "
            "isRetry_[%d] isEnablePartialOpRetry_[%d] (before SyncPartialOpRetry)",
            opUnfoldIdx, algName.c_str(), param.opType, isRetry_, isEnablePartialOpRetry_);
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::UpdateTotalSqeCount(const int32_t streamId, const uint64_t sqeCount)
    {
        // 如果isEnablePartialOpRetry_为false, 则没有必要再更新局部重执行相关信息
        if (!isEnablePartialOpRetry_) {
            HCCL_INFO("[AicpuBlocklistManager][UpdateTotalSqeCount] isEnablePartialOpRetry_[%d] no need to "
                "update totalSqeCount for streamId[%d] sqeCount[%llu]",
                isEnablePartialOpRetry_, streamId, sqeCount);
            return HCCL_SUCCESS;
        }

        // 非aicpu main/slave stream (例如aicpu order stream)
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator mapIter = perStreamPartialOpRetryInfoMap_.find(streamId);
        if (mapIter == perStreamPartialOpRetryInfoMap_.end()) {
            HCCL_INFO("[AicpuBlocklistManager][UpdateTotalSqeCount] streamId[%d] not found; sqeCount[%llu]",
                streamId, sqeCount);
            return HCCL_SUCCESS;
        }

        // 判断totalSqeCount integer overflow
        if (UNLIKELY(mapIter->second.totalSqeCount + sqeCount < mapIter->second.totalSqeCount)) {
            HCCL_ERROR("[AicpuBlocklistManager][UpdateTotalSqeCount] existing sqeCount[%llu] + current sqeCount[%llu]"
                "overflow, streamId[%d]", mapIter->second.totalSqeCount, sqeCount, streamId);
            return HCCL_E_INTERNAL;
        }

        // 更新totalSqeCount
        mapIter->second.totalSqeCount += sqeCount;
        HCCL_INFO("[AicpuBlocklistManager][UpdateTotalSqeCount] totalSqeCount[%llu] after streamId[%d] sqeCount[%llu]",
            mapIter->second.totalSqeCount, streamId, sqeCount);

        // 检查totalSqeCount约束, 如果超过sqDepth则将isEnablePartialOpRetry_设置为false
        if (mapIter->second.totalSqeCount > mapIter->second.sqDepth) {
            isEnablePartialOpRetry_ = false;
            HCCL_INFO("[AicpuBlocklistManager][UpdateTotalSqeCount] totalSqeCount[%llu] exceeds sqDepth[%llu]: "
                "not support partial opretry",
                mapIter->second.totalSqeCount, mapIter->second.sqDepth);
        }

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::UpdatePlaceholderSqIdx(const int32_t streamId, const int64_t curPlaceholderSqIdx,
        const bool isFlip)
    {
        // 如果isEnablePartialOpRetry_为false, 则没有必要再更新局部重执行相关信息
        if (!isEnablePartialOpRetry_) {
            HCCL_INFO("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] isEnablePartialOpRetry_[%d]: no need to "
                "update placeholderSqIdx for streamId[%d] curPlaceholderSqIdx[%lld]",
                isEnablePartialOpRetry_, streamId, curPlaceholderSqIdx);
            return HCCL_SUCCESS;
        }

        // 非aicpu main/slave stream (例如aicpu order stream)
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator mapIter = perStreamPartialOpRetryInfoMap_.find(streamId);
        if (mapIter == perStreamPartialOpRetryInfoMap_.end()) {
            HCCL_INFO("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] streamId[%d] not found; "
                "curPlaceholderSqIdx[%lld]", streamId, curPlaceholderSqIdx);
            return HCCL_SUCCESS;
        }

        // 检验入参
        CHK_PRT_RET(curPlaceholderSqIdx < 0,
            HCCL_ERROR("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] curPlaceholderSqIdx[%lld] < 0 "
                "for streamId[%d]", curPlaceholderSqIdx, streamId),
            HCCL_E_INTERNAL);

        if (isFlip) { // flip placeholder SQE
            // 更新firstFlipPlaceholderSqIdx_ if necessary
            // 注意: 正常展开时, 大部分情况下, 当前算子在给定stream上只有一个flip placeholder
            // 注意: 重执行故障算子时, 由于hccl通过AddRetryExecFlipTask->AddRetryPreamble强制下发一个flip placeholder;
            //     此时大部分情况下, 当前算子在给定stream上只有两个flip placeholder
            if (LIKELY(mapIter->second.firstFlipPlaceholderSqIdx == -1)) { // 当前算子在给定stream上的第一个flip placeholder SQE
                mapIter->second.firstFlipPlaceholderSqIdx = curPlaceholderSqIdx;
                HCCL_INFO("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] set firstFlipPlaceholderSqIdx[%lld] "
                    "for streamId[%d]", curPlaceholderSqIdx, streamId);
            } else if (mapIter->second.secondFlipPlaceholderSqIdx == -1) { // 当前算子在给定stream上的第二个flip placeholder SQE
                mapIter->second.secondFlipPlaceholderSqIdx = curPlaceholderSqIdx;
                HCCL_INFO("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] set secondFlipPlaceholderSqIdx[%lld] "
                    "for streamId[%d]", curPlaceholderSqIdx, streamId);
                
                CHK_PRT_RET(mapIter->second.secondFlipPlaceholderSqIdx == mapIter->second.firstFlipPlaceholderSqIdx,
                    HCCL_ERROR("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] secondFlipPlaceholderSqIdx[%lld] "
                        "== firstFlipPlaceholderSqIdx[%lld] for streamId[%d]",
                        curPlaceholderSqIdx, mapIter->second.firstFlipPlaceholderSqIdx, streamId),
                    HCCL_E_INTERNAL);
            } else { // >=3个flip placeholder SQE (即当前算子在给定stream上生成的SQE过多)
                // 注意: 无论正常展开还是故障算子重执行, 单个算子生成这么多SQE的情况几乎不可能, 仅用于警告
                HCCL_WARNING("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] >= thirdflipPlaceholderSqIdx[%lld] "
                    "for streamId[%d] with firstFlipPlaceholderSqIdx[%lld] and secondFlipPlaceholderSqIdx[%lld]",
                    curPlaceholderSqIdx, streamId, mapIter->second.firstFlipPlaceholderSqIdx,
                    mapIter->second.secondFlipPlaceholderSqIdx);

                // 注意: 当前算子在给定stream下发SQE数量超过sqDepth之后, 就一定不会再更新firstFlipPlaceholderSqIdx_;
                //     而sqDepth目前为HCCL_SQE_MAX_CNT=2048个, 一定小于UINT16_MAX (65536), 即一定不会有>=3个flip placeholders
                CHK_PRT_RET(mapIter->second.sqDepth > UINT16_MAX,
                    HCCL_ERROR("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] sqDepth[%llu] > UINT16_MAX "
                        "for streamId[%d] with curPlaceholderSqIdx[%lld]",
                        mapIter->second.sqDepth, streamId, curPlaceholderSqIdx),
                    HCCL_E_INTERNAL);
            }
        } else { // alltoallv placeholder SQE
            // 更新alltoallvPlaceholderSqIdxVec
            // 注意: 正常展开时, alltoallv使能aicpu cache条件下, 会对零长数据相关的拷贝/同步引入多个alltoallv placeholder
            mapIter->second.alltoallvPlaceholderSqIdxVec.push_back(curPlaceholderSqIdx);
            HCCL_INFO("[AicpuBlocklistManager][UpdatePlaceholderSqIdx] add placeholderSqIdx[%lld] "
                "; alltoallvPlaceholderSqIdxVec.size[%u] for streamId[%d]",
                curPlaceholderSqIdx, mapIter->second.alltoallvPlaceholderSqIdxVec.size(), streamId);
        }
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcExecSqeCount(const uint32_t devId, Stream& mainStream,
        std::vector<Stream>& slaveStreams)
    {
        // 注意: CalcExecSqeCount只会在故障触发重执行流程中, 停流时才会调用; 因此直接使用RUN_INFO日志 (非性能瓶颈)

        // 如果isEnablePartialOpRetry_为false, 则没有必要再计算non-placeholder executed SQE count (黑名单-拷贝类信息)
        if (!isEnablePartialOpRetry_) {
            HCCL_RUN_INFO("[AicpuBlocklistManager][CalcExecSqeCount] isEnablePartialOpRetry_[%d]: no need to "
                "calculate non-placeholder executed SQE count", isEnablePartialOpRetry_);
            return HCCL_SUCCESS;
        }

        // 针对aicpu main stream, 计算non-placeholder executed SQE count
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcExecSqeCount] calc non-placeholder executed SQE count "
            "for main stream[%d]", mainStream.GetHcclStreamInfo().actualStreamId);
        CHK_RET(CalcExecSqeCountForStream_(devId, mainStream));
        
        // 针对aicpu slave streams, 计算non-placeholder executed SQE count
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            Stream& slaveStream = slaveStreams[i];
            HCCL_RUN_INFO("[AicpuBlocklistManager][CalcExecSqeCount] calc non-placeholder executed SQE count "
                "for slave stream[%d]", slaveStream.GetHcclStreamInfo().actualStreamId);
            CHK_RET(CalcExecSqeCountForStream_(devId, slaveStream));
        }

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::SyncPartialOpRetry(const bool partialOpRetryFlag)
    {
        // 注意: SyncPartialOpRetry只有重执行故障算子时才会调用; 因此直接使用RUN_INFO日志 (非性能瓶颈)
        CHK_PRT_RET(!isRetry_,
            HCCL_ERROR("[AicpuBlocklistManager][SyncPartialOpRetry] isRetry_[%d] != true", isRetry_),
            HCCL_E_INTERNAL);

        if (partialOpRetryFlag) {
            // 校验本地使能flag (全局使能局部重执行, 则本rank一定使能)
            CHK_PRT_RET(!isEnablePartialOpRetry_,
                HCCL_ERROR("[AicpuBlocklistManager][SyncPartialOpRetry] partialOpRetryFlag[%d] != isEnablePartialOpRetry_[%d]",
                    partialOpRetryFlag, isEnablePartialOpRetry_),
                HCCL_E_INTERNAL);
        } else {
            // 注意: 全局不使能局部重执行, 本rank有可能使能 (其他rank不满足局部重执行约束导致不使能)
            // 因此需要将本rank的使能flag更新为全局flag, 确保后续不走局部重执行逻辑
            HCCL_RUN_INFO("[AicpuBlocklistManager][SyncPartialOpRetry] set isEnablePartialOpRetry_[%d] to false",
                isEnablePartialOpRetry_);
            isEnablePartialOpRetry_ = false;
        }

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcBlockSet(const AlgResourceResponse& algResource, const HcclTopoInfo& topoinfo,
        const PartialRetryInfo& partialRetryInfo, const uint32_t mainWaitParamStreamNotifyId) {
        // 注意: CalcBlockSet只有重执行故障算子时才会调用; 因此直接使用RUN_INFO日志 (非性能瓶颈)
        CHK_PRT_RET(!isRetry_,
            HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] isRetry_[%d] != true", isRetry_),
            HCCL_E_INTERNAL);

        // 注意: CalcBlockSet前已经通过SyncPartialOpRetry同步全局局部重执行使能flag
        // 如果isEnablePartialOpRetry_为false, 则没有必要再计算record/wait blockset (黑名单-同步类信息)
        if (!isEnablePartialOpRetry_) {
            HCCL_RUN_INFO("[AicpuBlocklistManager][CalcBlockSet] isEnablePartialOpRetry_[%d]: no need to "
                "calculate record/wait blockset", isEnablePartialOpRetry_);
            return HCCL_SUCCESS;
        }

        waitNotifyIdBlockSet_.clear();
        recordSignalAddrBlockSet_.clear();

        // 注意: 目前局部重执行针对alltoall类算子 (alltoall/alltoallv/alltoallvc), 其他算子后续再考虑;
        // 该场景下, 涉及跨卡同步的操作主要有:
        // SDMA: TransportP2p::TxAck/RxAck/TxDataSignal/RxDataSignal
        // -> remoteSendDone/localSendDone/remoteSendReady/localSendReady;
        // RDMA (暂不支持局部重执行): TransportIbverbs::TxAck/RxAck/TxAsync/RxAsync/PostFinAck/WaitFinAck (暂不考虑)

        // 注意: alltoall类算子一定会设置COMM_COMBINE_ORDER的transports
        const uint32_t rankSize = topoinfo.userRankSize;
        const uint32_t curRank = topoinfo.userRank;
        const std::vector<LINK>& links = algResource.opTransportResponse[COMM_COMBINE_ORDER][COMM_INDEX_0].links;
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcBlockSet] links.size[%u] fastRankNum[%u] rankSize[%u] curRank[%u]",
            links.size(), partialRetryInfo.fastRankNum, rankSize, curRank);
        CHK_PRT_RET(links.size() != rankSize,
            HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] links.size[%u] != rankSize[%u]",
                links.size(), rankSize),
            HCCL_E_INTERNAL);

        // 注意: 局部重执行最多支持AICPU_MAX_FAST_RANK_NUM快卡信息 (如果超过则应该进约束, 而非使能局部重执行)
        CHK_PRT_RET(partialRetryInfo.fastRankNum > AICPU_MAX_FAST_RANK_NUM,
            HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] fastRankNum[%u] > AICPU_MAX_FAST_RANK_NUM[%u]",
                partialRetryInfo.fastRankNum, AICPU_MAX_FAST_RANK_NUM),
            HCCL_E_INTERNAL);
        
        // 将快卡相关的record/wait信号添加到黑名单-同步类信息
        for (uint32_t i = 0; i < partialRetryInfo.fastRankNum; i++) {
            // 注意: fastRankId一定不超过rankSize
            const uint32_t fastRankId = partialRetryInfo.fastRankIdList[i];
            CHK_PRT_RET(fastRankId >= links.size(),
                HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] fastRankId[%u] >= links.size[%u]",
                    fastRankId, links.size()),
                HCCL_E_INTERNAL);
            
            // 注意: fastRankId一定不等于curRank (当前这张卡既然参与局部重执行, 则一定不是快卡)
            CHK_PRT_RET(fastRankId == curRank,
                HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] fastRankId[%u] == curRank[%u]",
                    fastRankId, curRank),
                HCCL_E_INTERNAL);
            
            // 获取curRank与fastRankId之间的LINK
            const LINK& tmpLink = links[fastRankId];
            CHK_PTR_NULL(tmpLink);

            // 更新waitNotifyIdBlockSet_
            HCCL_RUN_INFO("[AicpuBlocklistManager][CalcBlockSet] update waitNotifyIdBlockSet_ for fastRankId[%u]",
                fastRankId);
            CHK_RET(CalcWaitNotifyIdBlockSet_(tmpLink));
            
            // 更新recordSignalAddrBlockSet_
            HCCL_RUN_INFO("[AicpuBlocklistManager][CalcBlockSet] update recordSignalAddrBlockSet_ for fastRankId[%u]",
                fastRankId);
            CHK_RET(CalcRecordSignalAddrBlockSet_(tmpLink));
        }

        // 将通信主流 (param.stream) 相关的wait信号添加到黑名单-同步类信息
        waitNotifyIdBlockSet_.emplace(mainWaitParamStreamNotifyId);
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcBlockSet] add mainWaitParamStreamNotifyId[%u] into waitNotifyIdBlockSet_",
            mainWaitParamStreamNotifyId);
        
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcBlockSet] waitNotifyIdBlockSet_.size[%u] recordSignalAddrBlockSet_.size[%u]",
            waitNotifyIdBlockSet_.size(), recordSignalAddrBlockSet_.size());

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::ApplyBlocklist(const bool isCopySqe, Stream& stream, const bool isWaitSqe,
        const uint32_t notifyId, const uint64_t signalAddr, bool& isEnable, bool& isFilter) {
        // 注意: ApplyBlocklist由DispatcherAicpu调用, dispatcher层不区分正常算子/故障算子重执行
        // 如果是正常算子, 一定没有必要应用黑名单信息
        isEnable = false;
        if (!isRetry_) {
            // 算子正常展开, 使用HCCL_INFO避免影响性能
            HCCL_INFO("[AicpuBlocklistManager][ApplyBlocklist] isRetry_[%d]: no need to apply blacklist", isRetry_);
            return HCCL_SUCCESS;
        }

        // 注意: 接下来都是故障算子重执行; 直接使用RUN_INFO日志 (非性能瓶颈)

        // 如果isEnablePartialOpRetry_为false, 也没有必要应用黑名单信息
        if (!isEnablePartialOpRetry_) {
            HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] isEnablePartialOpRetry_[%d]: no need to "
                "apply blacklist", isEnablePartialOpRetry_);
            return HCCL_SUCCESS;
        }

        // 注意: 接下来都是故障算子重执行, 且使能局部重执行, 需要应用黑名单信息
        isEnable = true;

        const int32_t streamId = stream.GetHcclStreamInfo().actualStreamId;
        HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] isCopySqe[%d] stream[%d] isWaitSqe[%d] notifyId[%u] "
            "signalAddr[0x%016llx]", isCopySqe, streamId, isWaitSqe, notifyId, signalAddr);
        
        // stream一定是aicpu device main/slave stream
        // 注意: 重执行故障算子前只会清理main/slave streams, 不会清理aicpu order stream;
        //     因此, 再次展开时, 不会在aicpu order stream中重下record sqe; 则应用黑名单时只会感知到主从流SQE
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator mapIter =
            perStreamPartialOpRetryInfoMap_.find(streamId);
        CHK_PRT_RET(mapIter == perStreamPartialOpRetryInfoMap_.end(),
            HCCL_ERROR("[AicpuBlocklistManager][ApplyBlocklist] streamId[%d] not found", streamId),
            HCCL_E_INTERNAL);

        // 判断是否需要过滤当前SQE
        if (isCopySqe) { // 拷贝类SQE
            // 判断当前拷贝类SQE故障前是否执行过
            if (mapIter->second.opRetryNonPlaceholderSqeCount < mapIter->second.execNonPlaceholderSqeCount) { // 执行过, 需要过滤
                HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] execNonPlaceholderSqeCount[%llu] > "
                    "opRetryNonPlaceholderSqeCount[%llu] for streamId[%d] -> filter current SQE",
                    mapIter->second.execNonPlaceholderSqeCount, mapIter->second.opRetryNonPlaceholderSqeCount, streamId);
                isFilter = true;
            } else { // 未执行过, 需要下发
                HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] execNonPlaceholderSqeCount[%llu] <= "
                    "opRetryNonPlaceholderSqeCount[%llu] for streamId[%d] -> not filter current SQE",
                    mapIter->second.execNonPlaceholderSqeCount, mapIter->second.opRetryNonPlaceholderSqeCount, streamId);
                isFilter = false;
            }
        } else { // 同步类SQE
            if (isWaitSqe) { // 同步等待类SQE
                // 校验入参
                CHK_PRT_RET(notifyId == 0,
                    HCCL_ERROR("[AicpuBlocklistManager][ApplyBlocklist] notifyId[%u] == 0", notifyId),
                    HCCL_E_INTERNAL);

                if (waitNotifyIdBlockSet_.find(notifyId) != waitNotifyIdBlockSet_.end()) { // 需要过滤
                    HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] notifyId[%u] is in waitNotifyIdBlockSet_ -> "
                        "filter current SQE", notifyId);
                    isFilter = true;
                } else { // 需要下发
                    HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] notifyId[%u] is not in waitNotifyIdBlockSet_ -> "
                        "not filter current SQE", notifyId);
                    isFilter = false;
                }
            } else { // 同步通知类SQE
                // 校验入参
                CHK_PRT_RET(signalAddr == 0,
                    HCCL_ERROR("[AicpuBlocklistManager][ApplyBlocklist] signalAddr[0x%016llx] == 0", signalAddr),
                    HCCL_E_INTERNAL);

                if (recordSignalAddrBlockSet_.find(signalAddr) != recordSignalAddrBlockSet_.end()) { // 需要过滤
                    HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] signalAddr[0x%016llx] is in "
                        "recordSignalAddrBlockSet_ -> filter current SQE", signalAddr);
                    isFilter = true;
                } else { // 需要下发
                    HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] signalAddr[0x%016llx] is not in "
                        "recordSignalAddrBlockSet_ -> not filter current SQE", signalAddr);
                    isFilter = false;
                }
            }
        }

        // 更新重执行故障算子时, 在当前stream已展开的非placeholder SQE数量
        mapIter->second.opRetryNonPlaceholderSqeCount += 1;
        HCCL_RUN_INFO("[AicpuBlocklistManager][ApplyBlocklist] opRetryNonPlaceholderSqeCount[%llu] after ApplyBlocklist",
            mapIter->second.opRetryNonPlaceholderSqeCount);

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::ResetAndBackupAtEnd(const size_t opUnfoldIdx, const std::string& algName,
        const OpParam &param, const uint32_t devId, Stream& mainStream, std::vector<Stream>& slaveStreams)
    {
        // 注意: 即使isEnablePartialOpRetry_为false, 仍然需要重置和备份 (see aicpu_blocklist_manager.h)

        HCCL_INFO("[AicpuBlocklistManager][ResetAndBackupAtEnd] opUnfoldIdx[%u] algName[%s] opType[%u] "
            "isEnablePartialOpRetry_[%d] isRetry_[%d]",
            opUnfoldIdx, algName.c_str(), param.opType, isEnablePartialOpRetry_, isRetry_);
        
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        static double resetTotalUs = 0.0;
        static double resetCount = 0;
        auto resetStartTime = std::chrono::steady_clock::now();
#endif

        // 重置isRetry_和isEnablePartialOpRetry_
        isRetry_ = false;
        isEnablePartialOpRetry_ = false;

        // 针对aicpu main stream, 重置局部重执行相关信息
        HCCL_DEBUG("[AicpuBlocklistManager][ResetAndBackupAtEnd] reset and backup partial opretry info "
            "for main stream[%d]", mainStream.GetHcclStreamInfo().actualStreamId);
        CHK_RET(ResetAndBackupForStream_(devId, mainStream));

        // 针对aicpu slave streams, 重置局部重执行相关信息
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            Stream& slaveStream = slaveStreams[i];
            HCCL_DEBUG("[AicpuBlocklistManager][ResetAndBackupAtEnd] reset and backup partial opretry info "
                "for slave stream[%d]", slaveStream.GetHcclStreamInfo().actualStreamId);
            CHK_RET(ResetAndBackupForStream_(devId, slaveStream));
        }

        // 重置黑名单-同步类信息
        // 注意: 理论上无需重置黑名单, 因为同步类信息在故障重执行时 (即收到kRetry命令) 会通过CalcBlockSet更新
        waitNotifyIdBlockSet_.clear();
        recordSignalAddrBlockSet_.clear();
    
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        auto resetEndTime = std::chrono::steady_clock::now();
        double resetUs = std::chrono::duration<double, std::micro>(resetEndTime - resetStartTime).count();
        resetTotalUs += resetUs;
        resetCount++;
        HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupAtEnd] avg resetUs[%.2f] w/ %u resets",
            resetTotalUs / resetCount, uint32_t(resetCount));
#endif

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CheckPartialOpRetryConstraints_(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, const bool isDeviceMode) {
        // Part 1: 白名单控制影响范围 (暂时只支持alltoall类DirectFullmesh算法, 其他算子算法后续再考虑)

        // 校验alltoall类算子
        const HcclCMDType opType = param.opType;
        bool isValidOp = false;
        if (opType == HcclCMDType::HCCL_CMD_ALLTOALL || opType == HcclCMDType::HCCL_CMD_ALLTOALLV ||
            opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) {
            isValidOp = true;
        }
        if (!isValidOp) {
            HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] opType[%u]: not support partial opretry",
                opType);
            isEnablePartialOpRetry_ = false;
            return HCCL_SUCCESS;
        }

        // 校验DirectFullmesh算法
        bool isValidAlg = false;
        if (algName == "RunAlltoAllDirectFullmesh") {
            isValidAlg = true;
        }
        if (!isValidAlg) {
            HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] algName[%s]: not support partial opretry",
                algName.c_str());
            isEnablePartialOpRetry_ = false;
            return HCCL_SUCCESS;
        }

        // Part 2: 黑名单过滤inline reduce算子, MC2场景, 跨超场景, 以及inplace场景

        // 过滤inline reduce算子
        bool isInlineReduce = IsReduce(param);
        if (isInlineReduce) {
            HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] opType[%u] w/ inline reduce: not support "
                "partial opretry", param.opType);
            isEnablePartialOpRetry_ = false;
            return HCCL_SUCCESS;
        }

        // 过滤MC2场景
        if (isDeviceMode) {
            HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] isDeviceMode[%d]: not support partial opretry",
                isDeviceMode);
            isEnablePartialOpRetry_ = false;
            return HCCL_SUCCESS;
        }

        // 过滤跨超场景 (即是否使用RDMA)
        const std::unordered_map<u32, bool>& isUsedRdmaMap = topoinfo.isUsedRdmaMap;
        for (std::unordered_map<u32, bool>::const_iterator map_iter = isUsedRdmaMap.cbegin();
            map_iter != isUsedRdmaMap.end(); ++map_iter) {
            if (map_iter->second) {
                HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] rank[%u] uses RDMA: not support "
                    "partial opretry", map_iter->first);
                isEnablePartialOpRetry_ = false;
                return HCCL_SUCCESS;
            }
        }

        // 过滤inplace算子
        bool isInplace = false;
        CHK_RET(IsInplace(param, topoinfo, isInplace));
        if (isInplace) {
            HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] inplace: not support partial opretry");
            isEnablePartialOpRetry_ = false;
            return HCCL_SUCCESS;
        }

        isEnablePartialOpRetry_ = true;
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::ResetAndBackupForStream_(const uint32_t devId, Stream& stream)
    {
// #ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
//         static double lookupTotalUs = 0.0;
//         static double getSqTailTotalUs = 0.0;
//         static double invokeResetTotalUs = 0.0;
//         static double count = 0;
//         auto lookupStartTime = std::chrono::steady_clock::now();
// #endif

        // aicpu main/slave stream在InitBlocklistManager时, 一定已经初始化了StreamPartialOpRetryInfo
        const int32_t streamId = stream.GetHcclStreamInfo().actualStreamId;
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator iter = perStreamPartialOpRetryInfoMap_.find(streamId);
        CHK_PRT_RET(iter == perStreamPartialOpRetryInfoMap_.end(),
            HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupForStream_] streamId[%d] "
                "not found in perStreamPartialOpRetryInfoMap", streamId),
            HCCL_E_INTERNAL);

// #ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
//         auto lookupEndTime = std::chrono::steady_clock::now();
//         double lookupUs = std::chrono::duration<double, std::micro>(lookupEndTime - lookupStartTime).count();
//         lookupTotalUs += lookupUs;
//         count++;
//         HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupForStream_] avg lookupUs[%.2f] w/ %u looks",
//             lookupTotalUs / count, uint32_t(count));
//         auto getSqTailStartTime = std::chrono::steady_clock::now();
// #endif
        
        // 获得aicpu main/slave stream当前的sqTail
        // 注意: 假设hccl每次在launch SQE时, 会保证SqeRingBuffer.sqTail与driver侧的tail一致
        // 注意: 由于QuerySqStatusByType单次调用在~0.5 us, 仅在debug模式下用于校验, 降低常态化开销
        uint32_t sqTail = stream.GetSqeContextPtr()->buffer.sqTail;
        if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_DEBUG))) {
            uint32_t sqTailFromDriver = 0xFFFFFFFF;
            CHK_RET(QuerySqStatusByType(devId, stream.sqId(), DRV_SQCQ_PROP_SQ_TAIL, sqTailFromDriver));
            CHK_PRT_RET(sqTailFromDriver != sqTail,
                HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupForStream_] sqTail[%u] != sqTailFromDriver[%u] "
                    "for streamId[%d]", sqTail, sqTailFromDriver, streamId),
                HCCL_E_INTERNAL);
        }

// #ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
//         auto getSqTailEndTime = std::chrono::steady_clock::now();
//         double getSqTailUs = std::chrono::duration<double, std::micro>(getSqTailEndTime - getSqTailStartTime).count();
//         getSqTailTotalUs += getSqTailUs;
//         HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupForStream_] avg getSqTailUs[%.2f] w/ %u gets",
//             getSqTailTotalUs / count, uint32_t(count));
//         auto invokeResetStartTime = std::chrono::steady_clock::now();
// #endif
        
        // 备份并重置aicpu main/slave stream的局部重执行相关信息
        StreamPartialOpRetryInfo& partialOpRetryInfo = iter->second;
        partialOpRetryInfo.ResetAndBackup(sqTail);
        HCCL_DEBUG("[AicpuBlocklistManager][ResetAndBackupForStream_] streamId[%d] beginSqTail[%u] "
            "totalSqeCount[%llu] firstFlipPlaceholderSqIdx[%d] secondFlipPlaceholderSqIdx[%d], "
            "alltoallvPlaceholderSqIdxVec.size[%u], execNonPlaceholderSqeCount[%llu], opRetryNonPlaceholderSqeCount[%llu]",
            streamId, sqTail, partialOpRetryInfo.totalSqeCount, partialOpRetryInfo.firstFlipPlaceholderSqIdx,
            partialOpRetryInfo.secondFlipPlaceholderSqIdx, partialOpRetryInfo.alltoallvPlaceholderSqIdxVec.size(),
            partialOpRetryInfo.execNonPlaceholderSqeCount, partialOpRetryInfo.opRetryNonPlaceholderSqeCount);

// #ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
//         auto invokeResetEndTime = std::chrono::steady_clock::now();
//         double invokeResetUs = std::chrono::duration<double, std::micro>(invokeResetEndTime - invokeResetStartTime).count();
//         invokeResetTotalUs += invokeResetUs;
//         HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupForStream_] avg invokeResetUs[%.2f] w/ %u invokes",
//             invokeResetTotalUs / count, uint32_t(count));
// #endif
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcExecSqeCountForStream_(const uint32_t devId, Stream& stream)
    {
        // 注意: CalcExecSqeCountForStream_只会在故障触发重执行流程中, 停流时才会调用; 因此直接使用RUN_INFO日志 (非性能瓶颈)

        // aicpu main/slave stream在InitBlocklistManager时, 一定已经初始化了StreamPartialOpRetryInfo
        const int32_t streamId = stream.GetHcclStreamInfo().actualStreamId;
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator iter = perStreamPartialOpRetryInfoMap_.find(streamId);
        CHK_PRT_RET(iter == perStreamPartialOpRetryInfoMap_.end(),
            HCCL_ERROR("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] "
                "not found in perStreamPartialOpRetryInfoMap", streamId),
            HCCL_E_INTERNAL);
        StreamPartialOpRetryInfo& partialOpRetryInfo = iter->second;
        
        // 计算aicpu main/slave stream's已经执行的SQE count, 即[beginSqTail, sqHead) (包括flip/alltoallv placeholder SQE if any)
        // 注意: 最新的sqHead只能通过driver API获取, 但本函数只会在故障处理阶段使用, 不影响常态化性能
        uint32_t sqHead = 0xFFFFFFFF;
        CHK_RET(QuerySqStatusByType(devId, stream.sqId(), DRV_SQCQ_PROP_SQ_HEAD, sqHead));
        if (sqHead < partialOpRetryInfo.beginSqTail) {
            uint32_t newSqHead = sqHead + partialOpRetryInfo.sqDepth;
            CHK_PRT_RET(newSqHead < partialOpRetryInfo.beginSqTail,
                HCCL_ERROR("[AicpuBlocklistManager][CalcExecSqeCountForStream_] sqHead[%u] + sqDepth[%u] "
                    "= newSqHead[%u] < beginSqTail[%u]",
                    sqHead, partialOpRetryInfo.sqDepth, newSqHead, partialOpRetryInfo.beginSqTail),
                HCCL_E_INTERNAL);
            sqHead = newSqHead;
        }
        // 注意: by now, sqHead must >= beginSqTail
        uint32_t execSqeCount = sqHead - partialOpRetryInfo.beginSqTail;
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] devId[%u] sqHead[%u] "
            "beginSqTail[%u] execSqeCount[%u]",
            streamId, devId, sqHead, partialOpRetryInfo.beginSqTail, execSqeCount);

        if (UNLIKELY(execSqeCount == 0)) {
            partialOpRetryInfo.execNonPlaceholderSqeCount = 0;
            HCCL_RUN_INFO("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] execSqeCount[%u] "
                "execNonPlaceholderSqeCount[%u]", streamId, execSqeCount, partialOpRetryInfo.execNonPlaceholderSqeCount);
            return HCCL_SUCCESS;
        }

        // 计算[beginSqTail, sqHead)包含的flip/alltoallv placeholder SQE的个数
        uint32_t placeholderCnt = 0;
        if (partialOpRetryInfo.firstFlipPlaceholderSqIdx != -1) {
            if (partialOpRetryInfo.firstFlipPlaceholderSqIdx >= partialOpRetryInfo.beginSqTail &&
                partialOpRetryInfo.firstFlipPlaceholderSqIdx < sqHead) {
                placeholderCnt++;
            }
        }
        if (partialOpRetryInfo.secondFlipPlaceholderSqIdx != -1) {
            if (partialOpRetryInfo.secondFlipPlaceholderSqIdx >= partialOpRetryInfo.beginSqTail &&
                partialOpRetryInfo.secondFlipPlaceholderSqIdx < sqHead) {
                placeholderCnt++;
            }
        }
        for (size_t i = 0; i < partialOpRetryInfo.alltoallvPlaceholderSqIdxVec.size(); i++) {
            if (partialOpRetryInfo.alltoallvPlaceholderSqIdxVec[i] >= partialOpRetryInfo.beginSqTail &&
                partialOpRetryInfo.alltoallvPlaceholderSqIdxVec[i] < sqHead) {
                placeholderCnt++;
            }
        }

        // 计算aicpu main/slave stream's已经执行的non-placeholder SQE count
        CHK_PRT_RET(execSqeCount < placeholderCnt,
            HCCL_ERROR("[AicpuBlocklistManager][CalcExecSqeCountForStream_] execSqeCount[%u] < placeholderCnt[%u] "
                "for streamId[%d]", execSqeCount, placeholderCnt, streamId),
            HCCL_E_INTERNAL);
        partialOpRetryInfo.execNonPlaceholderSqeCount = execSqeCount - placeholderCnt;
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] execSqeCount[%u] "
            "placeholderCnt[%u] execNonPlaceholderSqeCount[%u]",
            streamId, execSqeCount, placeholderCnt, partialOpRetryInfo.execNonPlaceholderSqeCount);
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::IsInplace(const OpParam &param, const HcclTopoInfo& topoinfo, bool& isInplace)
    {
        // 准备input/output size
        uint64_t inputSize = 0;
        uint64_t outputSize = 0;
        CHK_RET(ParseOpParam(param, topoinfo, inputSize, outputSize));

        // 注意: alltoall/alltoallv/alltoallvc可能存在inputSize/outputSize为0的情况, 导致不分配user input/output
        // 但会使用tinySendRecvMem_更新algResource.paramInput/OutputMem用于建链, 相当于param input/output为同一块内存
        // 参考aicpu_communicator.cc中的SetAlltoAllInputAndOutPutMem
        if (inputSize == 0 && outputSize == 0) {
            isInplace = true;
            HCCL_INFO("[AicpuCacheManager][IsInplace] inputSize[%u] is overlapping with outputSize[%u]",
                inputSize, outputSize);
            return HCCL_SUCCESS;
        }

        if (inputSize == 0 || outputSize == 0) {
            isInplace = false;
            HCCL_INFO("[AicpuCacheManager][IsInplace] inputSize[%u] is not overlapping with outputSize[%u]",
                inputSize, outputSize);
            return HCCL_SUCCESS;
        }

        const uint64_t inputStart = reinterpret_cast<uint64_t>(param.inputPtr);
        const uint64_t inputEnd = inputStart + inputSize - 1;
        const uint64_t outputStart = reinterpret_cast<uint64_t>(param.outputPtr);
        const uint64_t outputEnd = outputStart + outputSize - 1;

        if (inputStart <= outputEnd && outputStart <= inputEnd) {
            isInplace = true;
            HCCL_INFO("[AicpuCacheManager][IsInplace] input[0x%016llx, 0x%016llx] is overlapping with output[0x%016llx, 0x%016llx]",
                inputStart, inputEnd, outputStart, outputEnd);
        } else {
            isInplace = false;
            HCCL_INFO("[AicpuCacheManager][IsInplace] input[0x%016llx, 0x%016llx] is not overlapping with output[0x%016llx, 0x%016llx]",
                inputStart, inputEnd, outputStart, outputEnd);
        }

        return HCCL_SUCCESS;
    }

    bool AicpuBlocklistManager::IsReduce(const OpParam& param)
    {
        const HcclCMDType opType = param.opType;
        return opType == HcclCMDType::HCCL_CMD_ALLREDUCE || opType == HcclCMDType::HCCL_CMD_REDUCE ||
            opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER || opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V;
    }

    HcclResult AicpuBlocklistManager::ParseOpParam(const OpParam &param, const HcclTopoInfo& topoinfo,
        uint64_t& inputSize, uint64_t& outputSize)
    {
        const HcclCMDType opType = param.opType;
        const uint32_t rankSize = topoinfo.userRankSize;

        // 准备input/output size
        // NOTE: 非V类算子 (DataRes), V类算子 (VDataDes), All2All类算子 (All2AllDataDes), batch类算子 (BatchSendRecvDataDes/BatchWriteDataDes)
        if (opType == HcclCMDType::HCCL_CMD_ALLTOALL) { // alltoall算子
            // 注意: sendType和recvType一定相同
            const HcclDataType sendType = param.All2AllDataDes.sendType;

            // 注意: 对于alltoall算子, inputSize和outputSize一定相同 (但不能直接使用param.input/outputSize, alltoall算子不会设置这两个字段)
            // 注意: outputSize的计算不能使用param.All2AllDataDes.recvCount * rankSize * SIZE_TABLE[recvType]
            // 因为alltoall使用sendCount来表示send/recvCount, 而recvCount本身为0
            inputSize = param.All2AllDataDes.sendCount * rankSize * SIZE_TABLE[sendType];
            outputSize = inputSize;
        } else if (opType == HcclCMDType::HCCL_CMD_ALLTOALLV) { // alltoallv算子
            // 注意: sendType和recvType一定相同
            const HcclDataType sendType = param.All2AllDataDes.sendType;
            const HcclDataType recvType = param.All2AllDataDes.recvType;

            // 注意: 对于alltoallv算子, inputSize和outputSize不一定相同 (但不能直接使用param.input/outputSize, alltoallv算子不会设置这两个字段)
            // 参考coll_all_to_all_v_direct_fullmesh_executor.cc下的CollRunAlltoAllDirectFullmesh::GetLocalSendRecvInfoforAlltoallV
            HCCL_DEBUG("[AicpuBlocklistManager][ParseOpParam] sum %u send/recv counts for input/output size", rankSize);
            for (uint32_t tmpRank = 0; tmpRank < rankSize; ++tmpRank) {
                // curRank发送到tmpRank的数据量
                const uint64_t curSendCounts = *(static_cast<const u64 *>(param.All2AllDataDes.sendCounts) + tmpRank);
                const uint64_t curSendLength = curSendCounts * SIZE_TABLE[sendType];
                inputSize += curSendLength;

                // curRank从tmpRank接收的数据量
                const uint64_t curRecvCounts = *(static_cast<const u64 *>(param.All2AllDataDes.recvCounts) + tmpRank);
                const uint64_t curRecvLength = curRecvCounts * SIZE_TABLE[recvType];
                outputSize += curRecvLength;
            }
        } else if (opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) { // alltoallvc算子
            // 注意: sendType和recvType一定相同
            const HcclDataType sendType = param.All2AllDataDes.sendType;
            const HcclDataType recvType = param.All2AllDataDes.recvType;

            // 注意: 对于alltoallvc算子, inputSize和outputSize不一定相同 (但不能直接使用param.input/outputSize, alltoallvc算子不会设置这两个字段)
            // 参考coll_all_to_all_v_direct_fullmesh_executor.cc下的CollRunAlltoAllDirectFullmesh::GetLocalSendRecvInfoforAlltoallV
            const uint32_t curRank = topoinfo.userRank;
            HCCL_DEBUG("[AicpuBlocklistManager][ParseOpParam] sum %u-size sendCountMatrix for input/output size", rankSize);
            for (uint32_t tmpRank = 0; tmpRank < rankSize; ++tmpRank) {
                // curRank发送到tmpRank的数据量
                const uint64_t curSendCounts = *(static_cast<const u64 *>(param.All2AllDataDes.sendCountMatrix)
                    + curRank * rankSize + tmpRank); // sendCountMatrix[curRank][tmpRank]
                const uint64_t curSendLength = curSendCounts * SIZE_TABLE[sendType];
                inputSize += curSendLength;

                // curRank从tmpRank接收到的数据量
                const uint64_t curRecvCounts = *(static_cast<const u64 *>(param.All2AllDataDes.sendCountMatrix)
                    + tmpRank * topoinfo.userRankSize + curRank); // sendCountMatrix[tmpRank][curRank]
                const uint64_t curRecvLength = curRecvCounts * SIZE_TABLE[recvType];
                outputSize += curRecvLength;
            }
        } else { // 非V类算子
            inputSize = param.inputSize;
            outputSize = param.outputSize;
        }

        HCCL_INFO("[AicpuBlocklistManager][ParseOpParam] opType[%u] rankSize[%u] inputSize[%llu] outputSize[%llu]",
            opType, rankSize, inputSize, outputSize);

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcWaitNotifyIdBlockSet_(const LINK& tmpLink)
    {
        // 注意: CalcWaitNotifyIdBlockSet_只有重执行故障算子时才会调用; 因此直接使用RUN_INFO日志 (非性能瓶颈)

        CHK_PTR_NULL(tmpLink);

        // 获取RxAck相关的NotifyId (for alltoall类 recv)
        HcclSignalInfo recvNotifyInfo;
        bool recvIsValid = false;
        CHK_RET(tmpLink->GetSpecificNotify(recvNotifyInfo, recvIsValid, "localSendDone"));
        CHK_PRT_RET(!recvIsValid,
            HCCL_ERROR("[AicpuBlocklistManager][CalcWaitNotifyIdBlockSet_] invalid localSendDoneNotify_"),
            HCCL_E_INTERNAL);
        const uint32_t recvNotifyId = static_cast<uint32_t>(recvNotifyInfo.resId);
        waitNotifyIdBlockSet_.emplace(recvNotifyId);
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcWaitNotifyIdBlockSet_] "
            "add localSendDone.notifyId[%u] into waitNotifyIdBlockSet_", recvNotifyId);

        // 获取RxDataSignal相关的NotifyId (for alltoall类 send)
        HcclSignalInfo sendNotifyInfo;
        bool sendIsValid = false;
        CHK_RET(tmpLink->GetSpecificNotify(sendNotifyInfo, sendIsValid, "localSendReady"));
        CHK_PRT_RET(!sendIsValid,
            HCCL_ERROR("[AicpuBlocklistManager][CalcWaitNotifyIdBlockSet_] invalid localSendReadyNotify_"),
            HCCL_E_INTERNAL);
        const uint32_t sendNotifyId = static_cast<uint32_t>(sendNotifyInfo.resId);
        waitNotifyIdBlockSet_.emplace(sendNotifyId);
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcWaitNotifyIdBlockSet_] "
            "add localSendReady.notifyId[%u] into waitNotifyIdBlockSet_", sendNotifyId);

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcRecordSignalAddrBlockSet_(const LINK& tmpLink)
    {
        // 注意: CalcRecordSignalAddrBlockSet_只有重执行故障算子时才会调用; 因此直接使用RUN_INFO日志 (非性能瓶颈)

        CHK_PTR_NULL(tmpLink);
        
        // 获取TxDataSignal相关的SignalAddr (for alltoall类 recv)
        HcclSignalInfo recvNotifyInfo;
        bool recvIsValid = false;
        CHK_RET(tmpLink->GetSpecificNotify(recvNotifyInfo, recvIsValid, "remoteSendReady"));
        CHK_PRT_RET(!recvIsValid,
            HCCL_ERROR("[AicpuBlocklistManager][CalcRecordSignalAddrBlockSet_] invalid remoteSendReadyNotify_"),
            HCCL_E_INTERNAL);
        const uint64_t recvSignalAddr = recvNotifyInfo.addr;
        recordSignalAddrBlockSet_.emplace(recvSignalAddr);
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcRecordSignalAddrBlockSet_] "
            "add remoteSendReady.signalAddr[0x%016llx] into recordSignalAddrBlockSet_", recvSignalAddr);

        // 获取TxAck相关的SignalAddr (与send count相关)
        HcclSignalInfo sendNotifyInfo;
        bool sendIsValid = false;
        CHK_RET(tmpLink->GetSpecificNotify(sendNotifyInfo, sendIsValid, "remoteSendDone"));
        CHK_PRT_RET(!sendIsValid,
            HCCL_ERROR("[AicpuBlocklistManager][CalcRecordSignalAddrBlockSet_] invalid remoteSendDoneNotify_"),
            HCCL_E_INTERNAL);
        const uint64_t sendSignalAddr = sendNotifyInfo.addr;
        recordSignalAddrBlockSet_.emplace(sendSignalAddr);
        HCCL_RUN_INFO("[AicpuBlocklistManager][CalcRecordSignalAddrBlockSet_] "
            "add remoteSendDone.signalAddr[0x%016llx] into recordSignalAddrBlockSet_", sendSignalAddr);

        return HCCL_SUCCESS;
    }
}