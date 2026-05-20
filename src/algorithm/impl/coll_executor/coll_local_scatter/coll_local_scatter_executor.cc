/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_local_scatter_executor.h"

#include "dispatcher.h"
#include "ffts_common_pub.h"
#include "stream_active_manager.h"
#include "alg_template_base_pub.h"

namespace hccl {

constexpr u32 MAX_LOCAL_SCATTER_STREAMS = 8;

CollLocalScatterExecutor::CollLocalScatterExecutor(const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollNativeExecutorBase(dispatcher, topoMatcher)
{
}

HcclResult CollLocalScatterExecutor::CalcStreamNum(u32& streamNum)
{
    streamNum = std::min(splitNum_, MAX_LOCAL_SCATTER_STREAMS);
    if (streamNum == 0) {
        streamNum = 1;
    }
    HCCL_INFO("[CollLocalScatterExecutor][CalcStreamNum] tag[%s] splitNum[%u] streamNum[%u].",
        tag_.c_str(), splitNum_, streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollLocalScatterExecutor::CalcNotifyNum(u32 streamNum, u32 &notifyNum)
{
    notifyNum = 2U * streamNum;
    HCCL_INFO("[CollLocalScatterExecutor][CalcNotifyNum] tag[%s] streamNum[%u] notifyNum[%u].",
        tag_.c_str(), streamNum, notifyNum);
    return HCCL_SUCCESS;
}

HcclResult CollLocalScatterExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[CollLocalScatterExecutor][KernelRun] Local scatter begins. "
        "numBufs[%u] splitNum[%u]", numBufs_, splitNum_);

    void* const* recvBufs  = param.LocalScatterDataDes.recvBufs;
    const u64* counts      = param.LocalScatterDataDes.counts;
    u32 numBufs            = param.LocalScatterDataDes.numBufs;
    void* gatheredBuf      = param.LocalScatterDataDes.gatheredBuf;
    u32 splitNum           = param.LocalScatterDataDes.splitNum;
    HcclDataType dataType  = param.LocalScatterDataDes.dataType;

    if (numBufs == 0 || splitNum == 0) {
        HCCL_WARNING("[CollLocalScatterExecutor][KernelRun] numBufs[%u] or splitNum[%u] is zero, nothing to do.",
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

    if (static_cast<bool>(topoMatcher_->GetExternalInputHcclEnableFfts())) {
        auto meta = HcclOpMetaInfo::GetOneForBatchSendRecv();
        CHK_RET(InitTask(dispatcher_, const_cast<Stream&>(param.stream),
            meta.isEnableCache, meta.GetCacheKey()));
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem,
            const_cast<Stream&>(param.stream), dispatcher_));
    } else {
        auto originalAlgTypeLevel1 = static_cast<u32>(algType_.algoLevel1);
        auto opMeta = HcclOpMetaInfo::GetOneForAllGather(originalAlgTypeLevel1, false, false, CopyPattern::BCOPY);
        opMeta.isEnableCache = false;
        CHK_RET(InitTask(dispatcher_, const_cast<Stream&>(param.stream),
            opMeta.isEnableCache, opMeta.GetCacheKey()));
    }

    CHK_RET(ActiveSlaveStreams(param.stream));

    u32 numStreams = algResResp_->slaveStreams.size();
    if (numStreams == 0) {
        numStreams = 1;
    }

    if (!algResResp_->slaveStreams.empty()) {
        CHK_RET(NotifySubStreamStart(const_cast<Stream&>(param.stream),
            algResResp_->slaveStreams, algResResp_->notifiesAux,
            static_cast<u32>(algResResp_->slaveStreams.size())));
    }

    for (u32 s = 0; s < splitNum; ++s) {
        u32 streamIdx = s % numStreams;
        Stream& targetStream = (algResResp_->slaveStreams.empty())
            ? const_cast<Stream&>(param.stream)
            : algResResp_->slaveStreams[streamIdx];

        u64 rowOffset = s * rowSize;

        for (u32 i = 0; i < numBufs; ++i) {
            u64 srcOffset = rowOffset + prefixPartOffset[i];
            DeviceMem srcMem = DeviceMem::create(
                static_cast<u8*>(gatheredBuf) + srcOffset, partSize[i]);
            u64 dstOffset = s * partSize[i];
            DeviceMem dstMem = DeviceMem::create(
                static_cast<u8*>(recvBufs[i]) + dstOffset, partSize[i]);

            CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, targetStream));
        }
    }

    if (!algResResp_->slaveStreams.empty()) {
        CHK_RET(WaitSubStreamFinish(const_cast<Stream&>(param.stream),
            algResResp_->slaveStreams, algResResp_->notifiesMain,
            static_cast<u32>(algResResp_->slaveStreams.size())));
    }

    if (static_cast<bool>(topoMatcher_->GetExternalInputHcclEnableFfts())) {
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem,
            const_cast<Stream&>(param.stream), dispatcher_));
    }

    CHK_RET(LaunchTaskExtend(dispatcher_, const_cast<Stream&>(param.stream),
        algResResp_->slaveStreams));

    HCCL_CONFIG_INFO(HCCL_ALG, "[CollLocalScatterExecutor][KernelRun] Local scatter done. "
        "rowSize[%llu] splitNum[%u]", rowSize, splitNum);
    return HCCL_SUCCESS;
}

HcclResult CollLocalScatterExecutor::Orchestrate(OpParam& param, AlgResourceResponse& algRes)
{
    HcclUs startut = TIME_NOW();
    tag_ = param.tag;
    algResResp_ = &algRes;

    numBufs_  = param.LocalScatterDataDes.numBufs;
    splitNum_ = param.LocalScatterDataDes.splitNum;

    u32 unitSize = SIZE_TABLE[param.LocalScatterDataDes.dataType];
    u64 gatheredSize = 0;
    for (u32 i = 0; i < numBufs_; ++i) {
        gatheredSize += param.LocalScatterDataDes.counts[i] * unitSize;
    }
    gatheredSize *= splitNum_;

    ExecMem execMem;
    execMem.count = 0;
    execMem.inputMem = algRes.cclInputMem;
    execMem.outputMem = algRes.cclOutputMem;
    execMem.scratchMem = algRes.scratchMem;
    execMem.inputPtr = param.LocalScatterDataDes.gatheredBuf;
    execMem.outputPtr = param.LocalScatterDataDes.gatheredBuf;

    HcclResult ret = KernelRun(param, execMem);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollLocalScatterExecutor][Orchestrate] errNo[0x%016llx] KernelRun failed",
            HCCL_ERROR_CODE(ret)), ret);

    HCCL_INFO("tag[%s] LocalScatter executor orchestrate success, took [%lld] us.",
        param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
    return HCCL_SUCCESS;
}

REGISTER_EXEC("LocalScatterExecutor", LocalScatter, CollLocalScatterExecutor);

} // namespace hccl
