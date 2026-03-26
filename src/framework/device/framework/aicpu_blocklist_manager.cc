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
#include "driver/ascend_hal_define.h"
#include "hccl_common.h"
#include "log.h"

namespace hccl {
    // struct StreamPartialOpRetryInfo

    AicpuBlocklistManager::StreamPartialOpRetryInfo::StreamPartialOpRetryInfo(const uint32_t curSqDepth)
        : sqDepth(curSqDepth), beginSqTail(0), totalSqeCount(0), firstFlipPlaceholderSqIdx(-1),
        secondFlipPlaceholderSqIdx(-1), execNonFlipSqeCount(0) {
    }
    
    AicpuBlocklistManager::StreamPartialOpRetryInfo::StreamPartialOpRetryInfo(const StreamPartialOpRetryInfo& other)
        : sqDepth(other.sqDepth), beginSqTail(other.beginSqTail), totalSqeCount(other.totalSqeCount),
        firstFlipPlaceholderSqIdx(other.firstFlipPlaceholderSqIdx),
        secondFlipPlaceholderSqIdx(other.secondFlipPlaceholderSqIdx),
        execNonFlipSqeCount(other.execNonFlipSqeCount) {}

    AicpuBlocklistManager::StreamPartialOpRetryInfo::~StreamPartialOpRetryInfo() {}

    void AicpuBlocklistManager::StreamPartialOpRetryInfo::ResetAndBackup(const uint32_t sqTail, const bool isRetry)
    {
        // 备份上一算子展开信息 (用于故障停流时, 计算给定stream已执行的SQE count)
        beginSqTail = sqTail; // 当前算子展开前, 给定stream的RTSQ tail

        // 重置当前算子展开信息 (用于故障停流时, 判断局部重执行约束, 并计算给定stream已执行的non-flip SQE count)
        totalSqeCount = 0; // 当前算子在给定stream实际下发的SQE count (含flip placeholder)
        firstFlipPlaceholderSqIdx = -1; // 当前算子在给定stream第一个flip placeholder的RTSQ index
        secondFlipPlaceholderSqIdx = -1; // 当前算子在给定stream第二个flip placeholder的RTSQ index

        // 黑名单信息只有非故障算子才能重置, 故障算子重执行需要使用
        if (!isRetry) { // 非故障算子正常展开
            // 重置黑名单-拷贝类信息 (故障停流时--kStopExec--会更新)
            execNonFlipSqeCount = 0; // 当前算子在给定stream实际执行的non-flip SQE count

            // 重置黑名单-同步类信息 (故障重执行时--kRetry--会更新)
            waitNotifyIdBlockSet_.clear();
            recordSignalAddrBlockSet_.clear();
        } else { // 故障算子重执行
            // 注意: execNonFlipSqeCount在故障算子正常展开触发故障停流时已经设置过, 重执行时需要沿用之前设置的值
            // 如果故障算子重执行过程中, 又触发了故障停流, execNonFlipSqeCount会被CalcExecSqeCount重新计算

            // 注意: waitNotifyIdBlockSet_和recordSignalAddrBlockSet_在故障算子收到重执行命令后已经设置过,
            // 重执行时需要沿用之前设置的值; 如果重执行再次故障, 它们会被CalcBlockSet重新设置
        }

        // 重置故障算子局部重执行信息
        // 注意: 只有故障算子重执行时才会使用, 因此非故障算子正常展开时重置不影响正确性
        opRetryNonFlipSqeCount = 0; // 故障算子在给定stream下发的非flip placeholder SQE的个数

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
        HCCL_INFO("[AicpuBlocklistManager][InitBlocklistManager] 1 mainStream, %u slaveStreams", slaveStreams.size());

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

        HCCL_INFO("[AicpuBlocklistManager][InitBlocklistManager] perStreamPartialOpRetryInfoMap.size[%d]",
            perStreamPartialOpRetryInfoMap_.size());

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::ResetAndBackupBeforeUnfold(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, const uint32_t devId, Stream& mainStream, std::vector<Stream>& slaveStreams,
        const bool isRetry)
    {
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        static double checkTotalUs = 0.0;
        static double checkCount = 0;
        auto checkStartTime = std::chrono::steady_clock::now();
#endif

        HCCL_INFO("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] algName[%s] opType[%u] isRetry[%d]",
            algName.c_str(), param.opType, isRetry);

        if (!isRetry) { // 非故障算子正常展开
            // 重置isEnablePartialOpRetry_
            isEnablePartialOpRetry_ = false;

            // 检查局部重执行约束 (不含totalSqeCount约束); 如果满足约束, 则将isEnablePartialOpRetry_设为true
            CHK_RET(CheckPartialOpRetryConstraints_(algName, param, topoinfo));
        } else { // 故障算子重执行
            // 注意: isEnablePartialOpRetry_在故障算子正常展开时已经设置过, 重执行时需要沿用之前设置的值
        }
    
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        auto checkEndTime = std::chrono::steady_clock::now();
        double checkUs = std::chrono::duration<double, std::micro>(checkEndTime - checkStartTime).count();
        checkTotalUs += checkUs;
        checkCount++;
        HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] avg checkUs[%.2f] w/ %u checks",
            checkTotalUs / checkCount, uint32_t(checkCount));
#endif

        // 如果isEnablePartialOpRetry_为false, 则无需重置与备份局部重执行相关信息
        if (!isEnablePartialOpRetry_) {
            HCCL_INFO("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] isEnablePartialOpRetry_[%d]: no need to "
                "reset and backup partial opretry info", isEnablePartialOpRetry_);
            return HCCL_SUCCESS;
        }

        HCCL_INFO("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] isEnablePartialOpRetry_[%d]",
            isEnablePartialOpRetry_);
        
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        static double resetTotalUs = 0.0;
        static double resetCount = 0;
        auto resetStartTime = std::chrono::steady_clock::now();
#endif

        // 针对aicpu main stream, 重置局部重执行相关信息
        HCCL_DEBUG("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] reset and backup partial opretry info "
            "for main stream[%d]", mainStream.GetHcclStreamInfo().actualStreamId);
        CHK_RET(ResetAndBackupForStream_(devId, mainStream, isRetry));

        // 针对aicpu slave streams, 重置局部重执行相关信息
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            Stream& slaveStream = slaveStreams[i];
            HCCL_DEBUG("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] reset and backup partial opretry info "
                "for slave stream[%d]", slaveStream.GetHcclStreamInfo().actualStreamId);
            CHK_RET(ResetAndBackupForStream_(devId, slaveStream, isRetry));
        }
    
#ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
        auto resetEndTime = std::chrono::steady_clock::now();
        double resetUs = std::chrono::duration<double, std::micro>(resetEndTime - resetStartTime).count();
        resetTotalUs += resetUs;
        resetCount++;
        HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] avg resetUs[%.2f] w/ %u resets",
            resetTotalUs / resetCount, uint32_t(resetCount));
#endif

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

    HcclResult AicpuBlocklistManager::UpdateFlipPlaceholderSqIdx(const int32_t streamId,
        const int64_t curFlipPlaceholderSqIdx)
    {
        // 如果isEnablePartialOpRetry_为false, 则没有必要再更新局部重执行相关信息
        if (!isEnablePartialOpRetry_) {
            HCCL_INFO("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] isEnablePartialOpRetry_[%d]: no need to "
                "update flipPlaceholderSqIdx for streamId[%d] curFlipPlaceholderSqIdx[%lld]",
                isEnablePartialOpRetry_, streamId, curFlipPlaceholderSqIdx);
            return HCCL_SUCCESS;
        }

        // 非aicpu main/slave stream (例如aicpu order stream)
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator mapIter = perStreamPartialOpRetryInfoMap_.find(streamId);
        if (mapIter == perStreamPartialOpRetryInfoMap_.end()) {
            HCCL_INFO("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] streamId[%d] not found; "
                "curFlipPlaceholderSqIdx[%lld]", streamId, curFlipPlaceholderSqIdx);
            return HCCL_SUCCESS;
        }

        // 检验入参
        CHK_PRT_RET(curFlipPlaceholderSqIdx < 0,
            HCCL_ERROR("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] curFlipPlaceholderSqIdx[%lld] < 0 "
                "for streamId[%d]", curFlipPlaceholderSqIdx, streamId),
            HCCL_E_INTERNAL);

        // 更新firstFlipPlaceholderSqIdx_ if necessary
        // 注意: 正常展开时, 大部分情况下, 当前算子在给定stream上只有一个flip placeholder
        // 注意: 重执行故障算子时, 由于hccl通过AddRetryExecFlipTask->AddRetryPreamble强制下发一个flip placeholder;
        //     此时大部分情况下, 当前算子在给定stream上只有两个flip placeholder
        if (LIKELY(mapIter->second.firstFlipPlaceholderSqIdx == -1)) { // 当前算子在给定stream上的第一个flip placeholder SQE
            mapIter->second.firstFlipPlaceholderSqIdx = curFlipPlaceholderSqIdx;
            HCCL_INFO("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] set firstFlipPlaceholderSqIdx[%lld] "
                "for streamId[%d]", curFlipPlaceholderSqIdx, streamId);
        } else if (mapIter->second.secondFlipPlaceholderSqIdx == -1) { // 当前算子在给定stream上的第二个flip placeholder SQE
            mapIter->second.secondFlipPlaceholderSqIdx = curFlipPlaceholderSqIdx;
            HCCL_INFO("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] set secondFlipPlaceholderSqIdx[%lld] "
                "for streamId[%d]", curFlipPlaceholderSqIdx, streamId);
            
            CHK_PRT_RET(mapIter->second.secondFlipPlaceholderSqIdx == mapIter->second.firstFlipPlaceholderSqIdx,
                HCCL_ERROR("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] secondFlipPlaceholderSqIdx[%lld] "
                    "== firstFlipPlaceholderSqIdx[%lld] for streamId[%d]",
                    curFlipPlaceholderSqIdx, mapIter->second.firstFlipPlaceholderSqIdx, streamId),
                HCCL_E_INTERNAL);
        } else { // >=3个flip placeholder SQE (即当前算子在给定stream上生成的SQE过多)
            // 注意: 无论正常展开还是故障算子重执行, 单个算子生成这么多SQE的情况几乎不可能, 仅用于警告
            HCCL_WARNING("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] >= thirdflipPlaceholderSqIdx[%lld] "
                "for streamId[%d] with firstFlipPlaceholderSqIdx[%lld] and secondFlipPlaceholderSqIdx[%lld]",
                curFlipPlaceholderSqIdx, streamId, mapIter->second.firstFlipPlaceholderSqIdx,
                mapIter->second.secondFlipPlaceholderSqIdx);

            // 注意: 当前算子在给定stream下发SQE数量超过sqDepth之后, 就一定不会再更新firstFlipPlaceholderSqIdx_;
            //     而sqDepth目前为HCCL_SQE_MAX_CNT=2048个, 一定小于UINT16_MAX (65536), 即一定不会有>=3个flip placeholders
            CHK_PRT_RET(mapIter->second.sqDepth > UINT16_MAX,
                HCCL_ERROR("[AicpuBlocklistManager][UpdateFlipPlaceholderSqIdx] sqDepth[%llu] > UINT16_MAX "
                    "for streamId[%d] with flipPlaceholderSqIdx[%lld]",
                    mapIter->second.sqDepth, streamId, curFlipPlaceholderSqIdx),
                HCCL_E_INTERNAL);
        }
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcExecSqeCount(const uint32_t devId, Stream& mainStream,
        std::vector<Stream>& slaveStreams)
    {
        // 如果isEnablePartialOpRetry_为false, 则没有必要再计算non-flip executed SQE count
        if (!isEnablePartialOpRetry_) {
            HCCL_INFO("[AicpuBlocklistManager][CalcExecSqeCount] isEnablePartialOpRetry_[%d]: no need to "
                "calculate non-flip executed SQE count", isEnablePartialOpRetry_);
            return HCCL_SUCCESS;
        }

        // 针对aicpu main stream, 计算non-flip executed SQE count
        HCCL_INFO("[AicpuBlocklistManager][CalcExecSqeCount] calc non-flip executed SQE count "
            "for main stream[%d]", mainStream.GetHcclStreamInfo().actualStreamId);
        CHK_RET(CalcExecSqeCountForStream_(devId, mainStream));

        // 针对aicpu slave streams, 重置局部重执行相关信息
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            Stream& slaveStream = slaveStreams[i];
            HCCL_INFO("[AicpuBlocklistManager][CalcExecSqeCount] calc non-flip executed SQE count "
                "for slave stream[%d]", slaveStream.GetHcclStreamInfo().actualStreamId);
            CHK_RET(CalcExecSqeCountForStream_(devId, slaveStream));
        }

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcBlockSet(const AlgResourceResponse& algResource,
        const PartialRetryInfo& partialRetryInfo, const uint32_t mainWaitParamStreamNotifyId) {
        // 注意: 目前局部重执行针对alltoall/alltoallv(c)算子, 其他算子后续再考虑;
        // 该场景下, 涉及跨卡同步的操作主要有:
        // SDMA: TransportP2p::TxAck/RxAck/TxDataSignal/RxDataSignal
        // -> remoteSendDone/localSendDone/remoteSendReady/localSendReady;
        // RDMA (暂不支持局部重执行): TransportIbverbs::TxAck/RxAck/TxAsync/RxAsync/PostFinAck/WaitFinAck (暂不考虑)

        // 注意: alltoall类算子一定会设置COMM_COMBINE_ORDER的transports
        const std::vector<LINK>& links = algResource.opTransportResponse[COMM_COMBINE_ORDER][COMM_INDEX_0].links;
        HCCL_INFO("[AicpuBlocklistManager][CalcBlockSet] links.size[%u] fastRankNum[%u]",
            links.size(), partialRetryInfo.fastRankNum);

        // 注意: 局部重执行最多支持AICPU_MAX_FAST_RANK_NUM快卡信息 (如果超过则应该进约束, 而非使能局部重执行)
        CHK_PRT_RET(partialRetryInfo.fastRankNum > AICPU_MAX_FAST_RANK_NUM,
            HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] fastRankNum[%u] > AICPU_MAX_FAST_RANK_NUM[%u]",
                partialRetryInfo.fastRankNum, AICPU_MAX_FAST_RANK_NUM),
            HCCL_E_INTERNAL);
        for (uint32_t i = 0; i < partialRetryInfo.fastRankNum; i++) {
            // 注意: fastRankId一定不超过rankSize
            const uint32_t fastRankId = partialRetryInfo.fastRankIdList[i];
            CHK_PRT_RET(fastRankId >= links.size(),
                HCCL_ERROR("[AicpuBlocklistManager][CalcBlockSet] fastRankId[%u] >= links.size[%u]",
                    fastRankId, links.size()),
                HCCL_E_INTERNAL);
        }

        // TODO: === END HERE ===
    }

    HcclResult AicpuBlocklistManager::CheckPartialOpRetryConstraints_(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo) {
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

        // Part 2: 黑名单过滤inplace场景, inline reduce算子, 以及跨超场景

        // 过滤inline reduce算子
        bool isInlineReduce = IsReduce(param);
        if (isInlineReduce) {
            HCCL_INFO("[AicpuBlocklistManager][CheckPartialOpRetryConstraints_] opType[%u] w/ inline reduce: not support "
                "partial opretry", param.opType);
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

    HcclResult AicpuBlocklistManager::ResetAndBackupForStream_(const uint32_t devId, Stream& stream, const bool isRetry)
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
//         HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] avg lookupUs[%.2f] w/ %u looks",
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
//         HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] avg getSqTailUs[%.2f] w/ %u gets",
//             getSqTailTotalUs / count, uint32_t(count));
//         auto invokeResetStartTime = std::chrono::steady_clock::now();
// #endif
        
        // 备份并重置aicpu main/slave stream的局部重执行相关信息
        StreamPartialOpRetryInfo& partialOpRetryInfo = iter->second;
        partialOpRetryInfo.ResetAndBackup(sqTail, isRetry);
        HCCL_DEBUG("[AicpuBlocklistManager][ResetAndBackupForStream_] streamId[%d] beginSqTail[%u] "
            "totalSqeCount[%llu] firstFlipPlaceholderSqIdx[%d] secondFlipPlaceholderSqIdx[%d], execNonFlipSqeCount[%llu]",
            streamId, sqTail, partialOpRetryInfo.totalSqeCount, partialOpRetryInfo.firstFlipPlaceholderSqIdx,
            partialOpRetryInfo.secondFlipPlaceholderSqIdx, partialOpRetryInfo.execNonFlipSqeCount);

// #ifdef ENABLE_PARTIAL_OPRETRY_BREAKDOWN
//         auto invokeResetEndTime = std::chrono::steady_clock::now();
//         double invokeResetUs = std::chrono::duration<double, std::micro>(invokeResetEndTime - invokeResetStartTime).count();
//         invokeResetTotalUs += invokeResetUs;
//         HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] avg invokeResetUs[%.2f] w/ %u invokes",
//             invokeResetTotalUs / count, uint32_t(count));
// #endif
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::CalcExecSqeCountForStream_(const uint32_t devId, Stream& stream)
    {
        // aicpu main/slave stream在InitBlocklistManager时, 一定已经初始化了StreamPartialOpRetryInfo
        const int32_t streamId = stream.GetHcclStreamInfo().actualStreamId;
        std::unordered_map<int32_t, StreamPartialOpRetryInfo>::iterator iter =perStreamPartialOpRetryInfoMap_.find(streamId);
        CHK_PRT_RET(iter == perStreamPartialOpRetryInfoMap_.end(),
            HCCL_ERROR("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] "
                "not found in perStreamPartialOpRetryInfoMap", streamId),
            HCCL_E_INTERNAL);
        StreamPartialOpRetryInfo& partialOpRetryInfo = iter->second;
        
        // 计算aicpu main/slave stream's已经执行的SQE count, 即[beginSqTail, sqHead) (包括flip placeholder SQE if any)
        // 注意: 最新的sqHead只能通过driver API获取, 但本函数只会在故障处理阶段使用, 不影响常态化性能
        uint32_t sqHead = 0xFFFFFFFF;
        CHK_RET(QuerySqStatusByType(devId, stream.sqId(), DRV_SQCQ_PROP_SQ_HEAD, sqHead));
        uint32_t execSqeCount = 0;
        if (sqHead >= partialOpRetryInfo.beginSqTail) {
            execSqeCount = sqHead - partialOpRetryInfo.beginSqTail;
        } else {
            uint32_t newSqHead = sqHead + partialOpRetryInfo.sqDepth;
            CHK_PRT_RET(newSqHead < partialOpRetryInfo.beginSqTail,
                HCCL_ERROR("[AicpuBlocklistManager][CalcExecSqeCountForStream_] sqHead[%u] + sqDepth[%u] "
                    "= newSqHead[%u] < beginSqTail[%u]",
                    sqHead, partialOpRetryInfo.sqDepth, newSqHead, partialOpRetryInfo.beginSqTail),
                HCCL_E_INTERNAL);
            execSqeCount = newSqHead - partialOpRetryInfo.beginSqTail;
        }

        if (UNLIKELY(execSqeCount == 0)) {
            partialOpRetryInfo.execNonFlipSqeCount = 0;
            HCCL_INFO("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] execSqeCount[%u] "
                "execNonFlipSqeCount[%u]", streamId, execSqeCount, partialOpRetryInfo.execNonFlipSqeCount);
            return HCCL_SUCCESS;
        }

        // 计算[beginSqTail, sqHead)包含的flip placeholder SQE的个数
        uint32_t flipPlaceholderCnt = 0;
        if (partialOpRetryInfo.firstFlipPlaceholderSqIdx != -1) {
            if (partialOpRetryInfo.firstFlipPlaceholderSqIdx >= partialOpRetryInfo.beginSqTail &&
                partialOpRetryInfo.firstFlipPlaceholderSqIdx < sqHead) {
                flipPlaceholderCnt++;
            }
        }
        if (partialOpRetryInfo.secondFlipPlaceholderSqIdx != -1) {
            if (partialOpRetryInfo.secondFlipPlaceholderSqIdx >= partialOpRetryInfo.beginSqTail &&
                partialOpRetryInfo.secondFlipPlaceholderSqIdx < sqHead) {
                flipPlaceholderCnt++;
            }
        }

        // 计算aicpu main/slave stream's已经执行的non-flip SQE count
        CHK_PRT_RET(execSqeCount < flipPlaceholderCnt,
            HCCL_ERROR("[AicpuBlocklistManager][CalcExecSqeCountForStream_] execSqeCount[%u] < flipPlaceholderCnt[%u] "
                "for streamId[%d]", execSqeCount, flipPlaceholderCnt, streamId),
            HCCL_E_INTERNAL);
        partialOpRetryInfo.execNonFlipSqeCount = execSqeCount - flipPlaceholderCnt;
        HCCL_INFO("[AicpuBlocklistManager][CalcExecSqeCountForStream_] streamId[%d] execSqeCount[%u] "
            "flipPlaceholderCnt[%u] execNonFlipSqeCount[%u]",
            streamId, execSqeCount, flipPlaceholderCnt, partialOpRetryInfo.execNonFlipSqeCount);
        
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
}