/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_local_gather_executor.h"

#include "dispatcher.h"
#include "ffts_common_pub.h"
#include "stream_active_manager.h"

namespace hccl {

constexpr u32 MAX_LOCAL_GATHER_STREAMS = 8;

CollLocalGatherExecutor::CollLocalGatherExecutor(const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollNativeExecutorBase(dispatcher, topoMatcher)
{
}

HcclResult CollLocalGatherExecutor::CalcStreamNum(u32& streamNum)
{
    streamNum = std::min(splitNum_, MAX_LOCAL_GATHER_STREAMS);
    if (streamNum == 0) {
        streamNum = 1;
    }
    HCCL_INFO("[CollLocalGatherExecutor][CalcStreamNum] tag[%s] splitNum[%u] streamNum[%u].",
        tag_.c_str(), splitNum_, streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollLocalGatherExecutor::CalcNotifyNum(u32 streamNum, u32 &notifyNum)
{
    notifyNum = 0;
    HCCL_INFO("[CollLocalGatherExecutor][CalcNotifyNum] tag[%s] notifyNum[%u].",
        tag_.c_str(), notifyNum);
    return HCCL_SUCCESS;
}

HcclResult CollLocalGatherExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[CollLocalGatherExecutor][KernelRun] Local gather begins. "
        "numBufs[%u] splitNum[%u]", numBufs_, splitNum_);

    void* const* sendBufs  = param.LocalGatherDataDes.sendBufs;
    const u64* counts      = param.LocalGatherDataDes.counts;
    u32 numBufs            = param.LocalGatherDataDes.numBufs;
    void* gatheredBuf      = param.LocalGatherDataDes.gatheredBuf;
    u32 splitNum           = param.LocalGatherDataDes.splitNum;
    HcclDataType dataType  = param.LocalGatherDataDes.dataType;

    if (numBufs == 0 || splitNum == 0) {
        HCCL_WARNING("[CollLocalGatherExecutor][KernelRun] numBufs[%u] or splitNum[%u] is zero, nothing to do.",
            numBufs, splitNum);
        return HCCL_SUCCESS;
    }

    u32 unitSize = SIZE_TABLE[dataType];

    std::vector<u64> partSize(numBufs);
    u64 rowSize = 0;
    for (u32 i = 0; i < numBufs; ++i) {
        partSize[i] = counts[i] * unitSize;
        rowSize += partSize[i];
    }

    std::vector<u64> prefixPartOffset(numBufs + 1, 0);
    for (u32 i = 0; i < numBufs; ++i) {
        prefixPartOffset[i + 1] = prefixPartOffset[i] + partSize[i];
    }

    auto originalAlgTypeLevel1 = static_cast<u32>(algType_.algoLevel1);
    auto opMeta = HcclOpMetaInfo::GetOneForAllGather(originalAlgTypeLevel1, false, false, CopyPattern::BCOPY);
    CHK_RET(InitTask(dispatcher_, const_cast<Stream&>(param.stream),
        opMeta.isEnableCache, opMeta.GetCacheKey()));

    CHK_RET(ActiveSlaveStreams(param.stream));

    u32 numStreams = algResResp_->slaveStreams.size();
    if (numStreams == 0) {
        numStreams = 1;
    }

    for (u32 s = 0; s < splitNum; ++s) {
        u32 streamIdx = s % numStreams;
        Stream& targetStream = (algResResp_->slaveStreams.empty())
            ? const_cast<Stream&>(param.stream)
            : algResResp_->slaveStreams[streamIdx];

        u64 rowOffset = s * rowSize;

        for (u32 i = 0; i < numBufs; ++i) {
            u64 srcOffset = s * partSize[i];
            DeviceMem srcMem = DeviceMem::create(
                static_cast<u8*>(sendBufs[i]) + srcOffset, partSize[i]);
            u64 dstOffset = rowOffset + prefixPartOffset[i];
            DeviceMem dstMem = DeviceMem::create(
                static_cast<u8*>(gatheredBuf) + dstOffset, partSize[i]);

            CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, targetStream));
        }
    }

    CHK_RET(LaunchTaskExtend(dispatcher_, const_cast<Stream&>(param.stream),
        algResResp_->slaveStreams));

    HCCL_CONFIG_INFO(HCCL_ALG, "[CollLocalGatherExecutor][KernelRun] Local gather done. "
        "rowSize[%llu] splitNum[%u]", rowSize, splitNum);
    return HCCL_SUCCESS;
}

HcclResult CollLocalGatherExecutor::Orchestrate(OpParam& param, AlgResourceResponse& algRes)
{
    HcclUs startut = TIME_NOW();
    tag_ = param.tag;
    algResResp_ = &algRes;

    numBufs_  = param.LocalGatherDataDes.numBufs;
    splitNum_ = param.LocalGatherDataDes.splitNum;

    u32 unitSize = SIZE_TABLE[param.LocalGatherDataDes.dataType];
    u64 gatheredSize = 0;
    for (u32 i = 0; i < numBufs_; ++i) {
        gatheredSize += param.LocalGatherDataDes.counts[i] * unitSize;
    }
    gatheredSize *= splitNum_;

    ExecMem execMem;
    execMem.count = 0;
    execMem.inputMem = algRes.cclInputMem;
    execMem.outputMem = algRes.cclOutputMem;
    execMem.scratchMem = algRes.scratchMem;
    execMem.inputPtr = param.LocalGatherDataDes.gatheredBuf;
    execMem.outputPtr = param.LocalGatherDataDes.gatheredBuf;

    HcclResult ret = KernelRun(param, execMem);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollLocalGatherExecutor][Orchestrate] errNo[0x%016llx] KernelRun failed",
            HCCL_ERROR_CODE(ret)), ret);

    CHK_RET(LaunchTaskExtend(dispatcher_, param.stream, algResResp_->slaveStreams));

    HCCL_INFO("tag[%s] LocalGather executor orchestrate success, took [%lld] us.",
        param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
    return HCCL_SUCCESS;
}

REGISTER_EXEC("LocalGatherExecutor", LocalGather, CollLocalGatherExecutor);

} // namespace hccl
