/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_batch_send_recv_direct_fullmesh_executor.h"
#include "alg_template_register.h"
#include "stream_utils.h"

namespace hccl {

CollBatchSendRecvDirectFullmeshExecutor::CollBatchSendRecvDirectFullmeshExecutor(
    const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollBatchSendRecvExecutor(dispatcher, topoMatcher)
{
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::GetLocalSDMAGroupInfo(const u32 userRank,
    u32& devNumInlocalPod, u32& rankIdxInPod)
{
    (void) userRank;
    bool isA2MultiModule = topoAttr_.deviceType == DevType::DEV_TYPE_910B &&
                            !topoAttr_.isSingleMeshAggregation;
    if (static_cast<bool>(topoMatcher_->GetExternalInputInterHccsDisable()) || isA2MultiModule) {
        CHK_RET(topoMatcher_->GetLocalServerRankSize(topoAttr_.userRank, devNumInlocalPod, rankIdxInPod));
    } else {
        CHK_RET(topoMatcher_->GetLocalSuperPodRankSize(topoAttr_.userRank, devNumInlocalPod, rankIdxInPod));
    }
    CHK_PRT_RET(devNumInlocalPod == INVALID_VALUE_RANKSIZE,
        HCCL_ERROR("[CollBatchSendRecvDirectFullmeshExecutor][GetLocalSDMAGroupInfo]"
            "get local superPod total ranksize failed."), HCCL_E_PARA);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::CalcStreamNum(u32& streamNum)
{
    u32 devNumInlocalPod = INVALID_VALUE_RANKSIZE;
    u32 rankIdxInPod = INVALID_VALUE_RANKID;
    CHK_RET(GetLocalSDMAGroupInfo(topoAttr_.userRank, devNumInlocalPod, rankIdxInPod));

    streamNum = (devNumInlocalPod > BATCH_SEND_RECV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE) ?
        (BATCH_SEND_RECV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE * RANK_SET_COMPUTE_CONST) :
        (devNumInlocalPod * RANK_SET_COMPUTE_CONST);

    u32 totalRdmaRankNum = topoAttr_.userRankSize - devNumInlocalPod;
    if (totalRdmaRankNum > 0) {
        streamNum += 1;
        u32 rdmaConcurrentNum = (totalRdmaRankNum > BATCH_SEND_RECV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE) ?
            (BATCH_SEND_RECV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE) : (totalRdmaRankNum);
        streamNum += rdmaConcurrentNum;
    }

    HCCL_INFO("[CollBatchSendRecvDirectFullmeshExecutor][CalcStreamNum] tag[%s] streamNum[%u]",
        tag_.c_str(), streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::CalcTransportMemType(
    TransportMemType &inputType, TransportMemType &outputType)
{
    inputType = TransportMemType::CCL_INPUT;
    outputType = TransportMemType::CCL_OUTPUT;

    HCCL_INFO("[CollBatchSendRecvDirectFullmeshExecutor][CalcTransportMemType] tag[%s] inputType[%d], outputType[%d]",
        tag_.c_str(), inputType, outputType);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::CalcLevel0CommInfo(
    TransportMemType inputType, TransportMemType outputType,
    std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commCombinePara(COMM_COMBINE_ORDER, CommType::COMM_TAG_MESH);
    CHK_RET(CalcCommPlaneInfo(tag_, commCombinePara, opTransport[COMM_COMBINE_ORDER], inputType, outputType));

    LevelNSubCommTransport &commTransportLevel0 = opTransport[COMM_COMBINE_ORDER];
    for (u32 subCommIndex = 0; subCommIndex < commTransportLevel0.size(); subCommIndex++) {
        for (auto &transportRequest : commTransportLevel0[subCommIndex].transportRequests) {
            transportRequest.isUsedRdma = topoAttr_.isUsedRdmaMap.at(transportRequest.remoteUserRank);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::CalcCommInfo(
    std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;

    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel0CommInfo(inputType, outputType, opTransport));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::ParseBatchSendRecvItems(const OpParam &param)
{
    HcclSendRecvItem* itemPtr = param.BatchSendRecvDataDes.sendRecvItemsPtr;
    u32 itemNum = param.BatchSendRecvDataDes.itemNum;
    CHK_PTR_NULL(itemPtr);

    batchSendRecvInfo_.items.clear();
    batchSendRecvInfo_.remoteRanks.clear();
    batchSendRecvInfo_.maxRemoteRankNum = 0;

    std::unordered_map<u32, BatchSendRecvItem> rankItemMap;

    for (u32 i = 0; i < itemNum; i++) {
        u32 remoteRank = (itemPtr + i)->remoteRank;
        u32 unitSize = SIZE_TABLE[(itemPtr + i)->dataType];
        u64 dataLen = (itemPtr + i)->count * unitSize;

        if (rankItemMap.find(remoteRank) == rankItemMap.end()) {
            BatchSendRecvItem item;
            item.remoteRank = remoteRank;
            item.sendLength = 0;
            item.sendOffset = 0;
            item.recvLength = 0;
            item.recvOffset = 0;
            item.sendBuf = nullptr;
            item.recvBuf = nullptr;
            rankItemMap[remoteRank] = item;
        }

        if ((itemPtr + i)->sendRecvType == HcclSendRecvType::HCCL_SEND) {
            rankItemMap[remoteRank].sendLength = dataLen;
            rankItemMap[remoteRank].sendBuf = (itemPtr + i)->buf;
        } else if ((itemPtr + i)->sendRecvType == HcclSendRecvType::HCCL_RECV) {
            rankItemMap[remoteRank].recvLength = dataLen;
            rankItemMap[remoteRank].recvBuf = (itemPtr + i)->buf;
        }
    }

    for (auto& pair : rankItemMap) {
        if (pair.first != topoAttr_.userRank) {
            batchSendRecvInfo_.items.push_back(pair.second);
            batchSendRecvInfo_.remoteRanks.insert(pair.first);
        }
    }

    batchSendRecvInfo_.maxRemoteRankNum = batchSendRecvInfo_.remoteRanks.size();

    HCCL_INFO("[CollBatchSendRecvDirectFullmeshExecutor][ParseBatchSendRecvItems] "
        "total items[%zu], remoteRanks[%zu]",
        batchSendRecvInfo_.items.size(), batchSendRecvInfo_.remoteRanks.size());
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::Orchestrate(OpParam& param, AlgResourceResponse& algRes)
{
    HcclUs startut = TIME_NOW();
    HcclResult ret = HCCL_SUCCESS;
    tag_ = param.tag;
    algResResp_ = &algRes;

    CHK_RET(ParseBatchSendRecvItems(param));
    CHK_RET(GetPairWiseList(param.BatchSendRecvDataDes.sendRecvItemsPtr, param.BatchSendRecvDataDes.itemNum));
    CHK_RET(ProcessSelfSendRecvTasks(param.stream));

    if (topoAttr_.userRankSize == 1 || batchSendRecvInfo_.items.empty()) {
        HCCL_INFO("tag[%s] BatchSendRecvDirectFullmesh Excutor orchestrate success, take time [%lld]us.",
            param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
        return HCCL_SUCCESS;
    }

    ExecMem execMem;
    execMem.count = 0;
    execMem.inputMem = algRes.cclInputMem;
    execMem.outputMem = algRes.cclOutputMem;
    ret = KernelRun(param, execMem);

    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollBatchSendRecvDirectFullmeshExecutor][Orchestrate]errNo[0x%016llx]executor run failed",
            HCCL_ERROR_CODE(ret)), ret);

    CHK_RET(LaunchTaskExtend(dispatcher_, param.stream, algResResp_->slaveStreams));

    HCCL_INFO("tag[%s] BatchSendRecvDirectFullmesh Excutor orchestrate success, take time [%lld]us.",
        param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvDirectFullmeshExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] BatchSendRecvDirectFullmesh start.", __func__);

    CHK_RET(ActiveSlaveStreams(param.stream));

    u32 devNumInlocalPod = INVALID_VALUE_RANKSIZE;
    u32 rankIdxInPod = INVALID_VALUE_RANKID;
    CHK_RET(GetLocalSDMAGroupInfo(topoAttr_.userRank, devNumInlocalPod, rankIdxInPod));

    CHK_RET(CheckCommSize(COMM_COMBINE_ORDER, COMM_INDEX_0 + 1));
    SubCommInfo level0CommInfo = GetSubCommInfo(COMM_COMBINE_ORDER, COMM_INDEX_0);

    bool isA2MultiModule = topoAttr_.deviceType == DevType::DEV_TYPE_910B &&
                            !topoAttr_.isSingleMeshAggregation;
    bool isSuPodAsym = false;
    if (topoAttr_.superPodNum > 1) {
        isSuPodAsym = (static_cast<bool>(topoAttr_.multiModuleDiffDeviceNumMode) ||
            static_cast<bool>(topoAttr_.multiSuperPodDiffServerNumMode));
    } else {
        isSuPodAsym = (static_cast<bool>(topoMatcher_->GetExternalInputInterHccsDisable()) || isA2MultiModule) &&
            static_cast<bool>(topoAttr_.multiModuleDiffDeviceNumMode);
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BATCH_SEND_RECV_DIRECT_FULL_MESH, dispatcher_);
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_BATCH_SEND_RECV_DIRECT_FULL_MESH in COMM_COMBINE_ORDER", __func__);
    CHK_SMART_PTR_NULL(tempAlg);

    PrepareData prepareData;
    prepareData.stream = param.stream;
    prepareData.userRank = topoAttr_.userRank;
    prepareData.userRankSize = topoAttr_.userRankSize;
    prepareData.linksPtr = &level0CommInfo.links;
    prepareData.devNumInlocalPod = devNumInlocalPod;
    prepareData.rankIdxInPod = rankIdxInPod;

    prepareData.inputMem = algResResp_->paramInputMem;
    prepareData.outputMem = algResResp_->paramOutputMem;
    prepareData.cclInMem = execMem.inputMem;
    prepareData.cclOutMem = execMem.outputMem;
    prepareData.workMode = workflowMode_;
    prepareData.subStreamsPtr = &algResResp_->slaveStreams;
    prepareData.signalPtr = &algResResp_->notifiesMain;
    prepareData.signalAuxPtr = &algResResp_->notifiesAux;
    prepareData.isSuPodAsym = isSuPodAsym;
    prepareData.opType = param.opType;

    CHK_RET(tempAlg->Prepare(prepareData));

    CHK_RET(tempAlg->RunAsync());

    HCCL_INFO("[CollBatchSendRecvDirectFullmeshExecutor] executor run success.");
    return HCCL_SUCCESS;
}

REGISTER_EXEC("BatchSendRecvDirectFullmesh", BatchSendRecvDirectFullmeshExecutor, CollBatchSendRecvDirectFullmeshExecutor);
} // namespace hccl
