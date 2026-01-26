/*
 * Copyright (c) 2024-2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_reduce_scatter_v_mesh_aiv_for_910_93_executor.h"

namespace hccl {
constexpr u32 BUFFER_IDX_ONE = 1;

CollReduceScatterVMeshAivFor91093Executor::CollReduceScatterVMeshAivFor91093Executor(
    const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollReduceScatterVExecutor(dispatcher, topoMatcher)
{
    DMAReduceFlag_ = false;
    desc_.isAivMode = true;
    desc_.isAivCrossNode = true;
}

HcclResult CollReduceScatterVMeshAivFor91093Executor::PrepareCommInfoToDevice(AlgResourceResponse& algResource)
{
    HCCL_INFO("[CollReduceScatterVMeshAivFor91093Executor][PrepareCommInfoToDevice] aiv copy comm info to device.");
    CHK_RET(CopyAivCommInfoToDevice(COMM_COMBINE_ORDER, COMM_INDEX_0, algResource));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVMeshAivFor91093Executor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel0CommInfo(inputType, outputType, opTransport));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVMeshAivFor91093Executor::CalcTransportMemType(TransportMemType &inputType,
    TransportMemType &outputType)
{
    inputType = TransportMemType::CCL_INPUT;
    outputType = TransportMemType::AIV_OUTPUT;
    HCCL_INFO("[CollReduceScatterVMeshAivFor91093Executor][CalcTransportMemType] tag[%s] inputType[%d],"
        " outputType[%d].", tag_.c_str(), inputType, outputType);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVMeshAivFor91093Executor::CalcLevel0CommInfo(TransportMemType inputType,
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

HcclResult CollReduceScatterVMeshAivFor91093Executor::CalNumBlocks(u32& numBlocks, u32 rankSize, u64 dataSize, HcclCMDType cmdType)
{
    // Step1. Calculate the best block dimension
    const u32 bestNumBlocks = (rankSize < MAX_NUM_BLOCKS ? rankSize : MAX_NUM_BLOCKS);
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
        HCCL_ERROR("[CollAllGatherVMeshAivFor91093Executor][CalNumBlocks]aivCore[%u] is invalid, at least need [%u]",
            originUserLimit, minNumBlocks);
        return HCCL_E_PARA;
    }
    numBlocks = numBlocks_;
    HCCL_INFO("[CollAllGatherVMeshAivFor91093Executor][CalNumBlocks] numBlocks is set to [%u], limit[%u], best[%u]",
        numBlocks_, originUserLimit, bestNumBlocks);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVMeshAivFor91093Executor::Orchestrate(OpParam& param, AlgResourceResponse& algRes)
{
    HcclUs startut = TIME_NOW();
    algResResp_ = &algRes;

    HcclResult ret = HCCL_SUCCESS;
    ExecMem execMem;
    execMem.inputPtr = param.inputPtr;
    execMem.outputPtr = param.outputPtr;

    execMem.inputMem = algRes.cclInputMem;
    execMem.outputMem = algRes.aivOutputMem;
    execMem.count = (static_cast<const u64 *>(param.VDataDes.counts))[topoAttr_.userRank];
    ret = KernelRun(param, execMem);

    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollReduceScatterVMeshAivFor91093Executor][Orchestrate]errNo[0x%016llx] tag[%s] executor kernel "
            "run failed.", HCCL_ERROR_CODE(ret), param.tag.c_str()), ret);

    HCCL_INFO("tag[%s], ReduceScatterV executor orchestrate success, take time [%lld]us.",
        param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVMeshAivFor91093Executor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[CollReduceScatterVMeshAivFor91093Executor][KernelRun]ReduceScatterV aiv enter.");

    CHK_RET(CheckCommSize(COMM_COMBINE_ORDER, COMM_INDEX_0 + 1));
    SubCommInfo level0CommInfo = GetSubCommInfo(COMM_COMBINE_ORDER, COMM_INDEX_0);

    void *buffersIn[MAX_RANK_SIZE];
    void *buffersOut[MAX_RANK_SIZE];

    u32 localRank = level0CommInfo.localRank;
    u32 localRankSize = level0CommInfo.localRankSize;
    HCCL_DEBUG("[CollReduceScatterVMeshAivFor91093Executor][KernelRun] userRank [%d] localRank [%d]",
        topoAttr_.userRank, localRank);

    buffersIn[0] = execMem.inputMem.ptr();
    buffersOut[0] = execMem.outputMem.ptr();
    buffersOut[BUFFER_IDX_ONE] = algResResp_->aivCommInfoMem.ptr();

    ExtraArgsV2 extraArgs;
    for (u32 i = 0; i < localRankSize; i++) {
        extraArgs.sendCounts[i] = *(static_cast<const u64 *>(param.VDataDes.counts) + i);
        extraArgs.sendDispls[i] = *(static_cast<const u64 *>(param.VDataDes.displs) + i);
        HCCL_DEBUG("[CollReduceScatterVMeshAivFor91093Executor][KernelRun]rank[%u], sendCount[%llu], sendDispl[%llu]",
            i, extraArgs.sendCounts[i], extraArgs.sendDispls[i]);
    }

    bool isOpbase = (GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    AivOpArgs opArgs {
        HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, execMem.inputPtr, execMem.outputPtr, execMem.count,
        param.VDataDes.dataType, param.reduceType, 0, isOpbase
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
        ClearAivSyncBuf(buffersOut, resourceArgs, topoArgs);
    }

    HcclResult ret = ExecuteKernelLaunch(opArgs, topoArgs, resourceArgs, algArgs, extraArgs, aivProfilingInfo);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollReduceScatterVMeshAivFor91093Executor][KernelRun]ReduceScatterV aiv failed, return[%d].",
        ret), ret);

    HCCL_INFO("[CollReduceScatterVMeshAivFor91093Executor][KernelRun]ReduceScatterV aiv run success.");
    return HCCL_SUCCESS;
}

REGISTER_EXEC("ReduceScatterVMeshAivFor91093Executor", ReduceScatterVMeshAivFor91093,
    CollReduceScatterVMeshAivFor91093Executor);

} // namespace hccl