/*
 * Copyright (c) 2024-2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_all_gatherv_mesh_aiv_for_910_93_executor.h"

namespace hccl {

constexpr uint32_t BUFFER_IDX_ONE = 1;

CollAllGatherVMeshAivFor91093Executor::CollAllGatherVMeshAivFor91093Executor(const HcclDispatcher dispatcher,
                                                           std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollAllGatherVExecutor(dispatcher, topoMatcher)
{
    DMAReduceFlag_ = false;
    desc_.isAivMode = true;
    desc_.isAivCrossNode = true;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::PrepareCommInfoToDevice(AlgResourceResponse& algResource)
{
    HCCL_INFO("[CollAllGatherVMeshAivFor91093Executor][PrepareCommInfoToDevice]AllGather aiv copy comm info to device.");
    CHK_RET(CopyAivCommInfoToDevice(COMM_COMBINE_ORDER, COMM_INDEX_0, algResource));
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel0CommInfo(inputType, outputType, opTransport));
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::CalcTransportMemType(TransportMemType &inputType, TransportMemType &outputType)
{
    inputType = TransportMemType::CCL_INPUT;
    outputType = TransportMemType::AIV_OUTPUT;
    HCCL_INFO("[CollAllGatherVMeshAivFor91093Executor][CalcTransportMemType] tag[%s] inputType[%d], outputType[%d].",
        tag_.c_str(), inputType, outputType);
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::CalcLevel0CommInfo(TransportMemType inputType,
    TransportMemType outputType, std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commCombinePara(COMM_COMBINE_ORDER, CommType::COMM_TAG_MESH);
    commCombinePara.meshSinglePlane = true;
    CHK_RET(CalcCommPlaneInfo(tag_, commCombinePara, opTransport[COMM_COMBINE_ORDER], inputType, outputType));

    LevelNSubCommTransport &commTransportLevel0 = opTransport[COMM_COMBINE_ORDER];
    for (u32 subCommIndex = 0; subCommIndex < commTransportLevel0.size(); subCommIndex++) {
        for (auto &transportRequest : commTransportLevel0[subCommIndex].transportRequests) {
            transportRequest.isUsedRdma = false;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::CalNumBlocks(u32& numBlocks, u32 rankSize, u64 dataSize, HcclCMDType cmdType)
{
    // Step1. Calculate the best block dimension
    const u32 bestNumBlocks = (rankSize + 1 < MAX_NUM_BLOCKS ? rankSize + 1 : MAX_NUM_BLOCKS);
    const u32 minNumBlocks = (rankSize + MAX_TARGET_NUM) / MAX_TARGET_NUM;

    // Step2. Compare User Given numBlocks_ with bestNumBlocks
    const u32 originUserLimit = numBlocks_;
    CHK_PRT_RET(originUserLimit < NUM_BLOCKS_FACTOR_TWO,
        HCCL_ERROR("[CollAllGatherVMeshAivFor91093Executor][CalNumBlocks]aivCore[%u] is invalid, at least need [2].",
        originUserLimit), HCCL_E_PARA);

    if (numBlocks_ > bestNumBlocks){
        numBlocks_ = bestNumBlocks;
    }

    if (numBlocks_ < minNumBlocks){
        HCCL_ERROR("[CollAllGatherVMeshAivFor91093Executor][CalNumBlocks]aivCore[%u] is invalid, at least need [%u].",
            originUserLimit, minNumBlocks);
        return HCCL_E_PARA;
    }
    numBlocks = numBlocks_;
    HCCL_INFO("[CollAllGatherVMeshAivFor91093Executor][CalNumBlocks] numBlocks is set to [%u], limit[%u], best[%u]",
        numBlocks_, originUserLimit, bestNumBlocks);
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::Orchestrate(OpParam& param, AlgResourceResponse& algRes)
{
    HcclUs startut = TIME_NOW();
    tag_ = param.tag;
    algResResp_ = &algRes;

    HcclResult ret = HCCL_SUCCESS;
    ExecMem execMem;

    execMem.inputPtr = param.inputPtr;
    execMem.outputPtr = param.outputPtr;
    execMem.count = (static_cast<const u64 *>(param.VDataDes.counts))[topoAttr_.userRank];

    execMem.inputMem = algRes.cclInputMem;
    execMem.outputMem = algRes.aivOutputMem;
    ret = KernelRun(param, execMem);

    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollAllGatherVMeshAivFor91093Executor][Orchestrate]errNo[0x%016llx] tag[%s] executor kernel run failed",
            HCCL_ERROR_CODE(ret), param.tag.c_str()), ret);

    HCCL_INFO("tag[%s], CollAllGatherVMeshAivFor91093Executor orchestrate success, take time [%lld]us",
        param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherVMeshAivFor91093Executor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_INFO("[CollAllGatherVMeshAivFor91093Executor][KernelRun]AllGatherV aiv enter.");

    CHK_RET(CheckCommSize(COMM_COMBINE_ORDER, COMM_INDEX_0 + 1));
    SubCommInfo level0CommInfo = GetSubCommInfo(COMM_COMBINE_ORDER, COMM_INDEX_0);

    void *buffersIn[MAX_RANK_SIZE];
    void *buffersOut[MAX_RANK_SIZE];

    u32 localRank = level0CommInfo.localRank;
    u32 localRankSize = level0CommInfo.localRankSize;
    HCCL_DEBUG("[CollAllGatherVMeshAivFor91093Executor][KernelRun] userRank [%u] localRank [%u]", topoAttr_.userRank, localRank);

    buffersIn[0] = execMem.inputMem.ptr();
    buffersOut[0] = execMem.outputMem.ptr();
    buffersOut[BUFFER_IDX_ONE] = algResResp_->aivCommInfoMem.ptr();

    ExtraArgsV2 extraArgs;
    for (u32 i = 0; i < localRankSize; i++) {
        extraArgs.recvCounts[i] = *(static_cast<const u64 *>(param.VDataDes.counts) + i);
        extraArgs.recvDispls[i] = *(static_cast<const u64 *>(param.VDataDes.displs) + i);
    }

    bool isOpbase = (workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    AivOpArgs opArgs {
        HcclCMDType::HCCL_CMD_ALLGATHER_V, execMem.inputPtr, execMem.outputPtr, execMem.count,
        param.VDataDes.dataType, param.reduceType, param.root, isOpbase
    };

    AivTopoArgs topoArgs { localRank, localRankSize, MAX_RANK_SIZE, 0, topoAttr_.serverNum, topoAttr_.deviceType, algoAttr_.identifier};

    u32 numBlocks;
    CHK_RET(CalNumBlocks(numBlocks, localRankSize));

    AivResourceArgs resourceArgs {
        param.tag, param.stream.ptr(), buffersIn, buffersOut, execMem.inputMem.size(), numBlocks_, param.aivTag
    };
    AivAlgArgs algArgs {};
    algArgs.argsType = KernelArgsType::ARGS_TYPE_SIMPLE;

    struct AivProfilingInfo aivProfilingInfo;
    aivProfilingInfo.counter = opCounter_;

    if (aivClearEnable_) {
        ClearAivSyncBuf(buffersOut, resourceArgs, topoArgs, algArgs);
    }

    HcclResult ret = ExecuteKernelLaunch(opArgs, topoArgs, resourceArgs, algArgs, extraArgs, aivProfilingInfo);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollAllGatherVMeshAivFor91093Executor][KernelRun]AllGatherV aiv failed, return[%d]", ret), ret);

    HCCL_INFO("[CollAllGatherVMeshAivFor91093Executor][KernelRun]AllGatherV aiv run success.");
    return HCCL_SUCCESS;
}

REGISTER_EXEC("AllGatherVMeshAivFor91093Executor", AllGatherVMeshAivFor91093, CollAllGatherVMeshAivFor91093Executor);

} // namespace hccl