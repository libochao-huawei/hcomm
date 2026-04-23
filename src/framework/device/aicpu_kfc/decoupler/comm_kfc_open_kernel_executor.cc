/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <memory>
#include <string>
#include <vector>

#include "log.h"
#include "alg_param.h"
#include "coll_alg_v2_exec_registry.h"

namespace {
ops_hccl::OpParam *AsOpenOpParam(std::vector<uint8_t> &opParam)
{
    return reinterpret_cast<ops_hccl::OpParam *>(opParam.data());
}

u64 GetDataTypeSize(HcclDataType dataType)
{
    switch (dataType) {
        case HCCL_DATA_TYPE_INT8:
        case HCCL_DATA_TYPE_UINT8:
        case HCCL_DATA_TYPE_HIF8:
        case HCCL_DATA_TYPE_FP8E5M2:
        case HCCL_DATA_TYPE_FP8E4M3:
        case HCCL_DATA_TYPE_FP8E8M0:
            return 1U;
        case HCCL_DATA_TYPE_INT16:
        case HCCL_DATA_TYPE_UINT16:
        case HCCL_DATA_TYPE_FP16:
        case HCCL_DATA_TYPE_BFP16:
            return 2U;
        case HCCL_DATA_TYPE_INT32:
        case HCCL_DATA_TYPE_UINT32:
        case HCCL_DATA_TYPE_FP32:
            return 4U;
        case HCCL_DATA_TYPE_INT64:
        case HCCL_DATA_TYPE_UINT64:
        case HCCL_DATA_TYPE_FP64:
            return 8U;
        case HCCL_DATA_TYPE_INT128:
            return 16U;
        default:
            return 0U;
    }
}

HcclResult RestoreVarDataBatchSendRecv(ops_hccl::OpParam &param)
{
    u64 sendRecvItemSize = static_cast<u64>(sizeof(HcclSendRecvItem));
    u64 itemNum = static_cast<u64>(param.batchSendRecvDataDes.itemNum);
    if (param.varMemSize != itemNum * sendRecvItemSize) {
        HCCL_ERROR("param.varMemSize[%lu] is not equal to itemNum[%lu] multiply [HcclSendRecvItem] size[%lu]."
                   "Failed to restore end recv info for BatchSendRecv!",
            param.varMemSize,
            itemNum,
            sendRecvItemSize);
        return HCCL_E_PARA;
    }
    param.batchSendRecvDataDes.sendRecvItemsPtr = reinterpret_cast<HcclSendRecvItem *>(param.varData);
    return HCCL_SUCCESS;
}

HcclResult RestoreVarDataAlltoAllV(ops_hccl::OpParam &param, const ops_hccl::AlgResourceCtxSerializable &resCtx)
{
    u64 rankSize = resCtx.topoInfo.userRankSize;
    CHK_PRT_RET(param.varMemSize != ops_hccl::ALL_TO_ALL_V_VECTOR_NUM * rankSize * sizeof(u64),
        HCCL_ERROR("[RestoreVarDataAlltoAllV] param.varMemSize [%llu] is invalid,"
                   " ALL_TO_ALL_V_VECTOR_NUM is [%u], rankSize is [%u], sizeof(u64) is [%u],",
            param.varMemSize,
            ops_hccl::ALL_TO_ALL_V_VECTOR_NUM,
            rankSize,
            sizeof(u64)),
        HCCL_E_PARA);

    HCCL_INFO("[RestoreVarDataAlltoAllV] param.varMemSize [%llu],"
                " ALL_TO_ALL_V_VECTOR_NUM is [%u], rankSize is [%u], sizeof(u64) is [%u],",
        param.varMemSize,
        ops_hccl::ALL_TO_ALL_V_VECTOR_NUM,
        rankSize,
        sizeof(u64));
    constexpr u64 SEND_COUNT_IDX = 0;
    constexpr u64 RECV_COUNT_IDX = 1;
    constexpr u64 SEND_DISPL_IDX = 2;
    constexpr u64 RECV_DISPL_IDX = 3;

    u64 *data = reinterpret_cast<u64 *>(param.varData);
    for (u64 i = 0; i < ops_hccl::ALL_TO_ALL_V_VECTOR_NUM * rankSize; i++) {
        u64 val = i / rankSize;
        switch(val) {
            case SEND_COUNT_IDX:
                data[i] = reinterpret_cast<u64*>(param.all2AllVDataDes.sendCounts)[i % rankSize];
                break;
            case RECV_COUNT_IDX:
                data[i] = reinterpret_cast<u64*>(param.all2AllVDataDes.recvCounts)[i % rankSize];
                break;
            case SEND_DISPL_IDX:
                data[i] = reinterpret_cast<u64*>(param.all2AllVDataDes.sdispls)[i % rankSize];
                break;
            case RECV_DISPL_IDX:
                data[i] = reinterpret_cast<u64*>(param.all2AllVDataDes.rdispls)[i % rankSize];
                break;
            default:
                break;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult RestoreVarDataReduceScatterV(ops_hccl::OpParam &param,
    const ops_hccl::AlgResourceCtxSerializable &resCtx)
{
    u64 rankSize = resCtx.topoInfo.userRankSize;
    HCCL_INFO("rankSize:%u", rankSize);
    CHK_PRT_RET(param.varMemSize != ops_hccl::REDUCE_SCATTER_V_VECTOR_NUM * rankSize * sizeof(u64),
        HCCL_ERROR("[RestoreVarDataReduceScatterV] param.varMemSize [%llu] is invalid,"
                   "REDUCE_SCATTER_V_VECTOR_NUM is [%u], rankSize is [%u], sizeof(u64) is [%u],",
            param.varMemSize,
            ops_hccl::REDUCE_SCATTER_V_VECTOR_NUM,
            rankSize,
            sizeof(u64)),
        HCCL_E_PARA);

    u64 *data = reinterpret_cast<u64 *>(param.varData);
    param.vDataDes.counts = data;
    param.vDataDes.displs = data + rankSize;
    return HCCL_SUCCESS;
}

HcclResult RestoreVarDataAllGatherV(ops_hccl::OpParam &param, const ops_hccl::AlgResourceCtxSerializable &resCtx)
{
    u64 rankSize = resCtx.topoInfo.userRankSize;
    HCCL_INFO("rankSize:%u", rankSize);
    CHK_PRT_RET(param.varMemSize != ops_hccl::ALL_GATHER_V_VECTOR_NUM * rankSize * sizeof(u64),
        HCCL_ERROR("[RestoreVarDataAllGatherV] param.varMemSize [%llu] is invalid,"
                   "ALL_GATHER_V_VECTOR_NUM is [%u], rankSize is [%u], sizeof(u64) is [%u],",
            param.varMemSize,
            ops_hccl::ALL_GATHER_V_VECTOR_NUM,
            rankSize,
            sizeof(u64)),
        HCCL_E_PARA);

    u64 *data = reinterpret_cast<u64 *>(param.varData);
    param.vDataDes.counts = data;
    for (u64 i = 0; i < rankSize; i++) {
        HCCL_INFO("param.vDataDes.counts[%u]:%u", i, reinterpret_cast<u64 *>(param.vDataDes.counts)[i]);
    }
    param.vDataDes.displs = data + rankSize;
    for (u64 i = 0; i < rankSize; i++) {
        HCCL_INFO("param.vDataDes.displs[%u]:%u", i, reinterpret_cast<u64 *>(param.vDataDes.displs)[i]);
    }
    return HCCL_SUCCESS;
}

HcclResult RestoreVarDataIfNeeded(ops_hccl::OpParam &param, const ops_hccl::AlgResourceCtxSerializable &resCtx)
{
    HcclResult ret = HCCL_SUCCESS;
    if (param.opType == HCCL_CMD_BATCH_SEND_RECV) {
        ret = RestoreVarDataBatchSendRecv(param);
    } else if (param.opType == HCCL_CMD_ALLTOALLV || param.opType == HCCL_CMD_ALLTOALLVC
               || param.opType == HCCL_CMD_ALLTOALL) {
        ret = RestoreVarDataAlltoAllV(param, resCtx);
    } else if (param.opType == HCCL_CMD_REDUCE_SCATTER_V) {
        ret = RestoreVarDataReduceScatterV(param, resCtx);
    } else if (param.opType == HCCL_CMD_ALLGATHER_V) {
        ret = RestoreVarDataAllGatherV(param, resCtx);
    }
    return ret;
}
}

HcclResult LaunchOpenOpParamDataImpl(std::vector<uint8_t> &opParam, uint32_t rankNum, bool isAllToAllV)
{
    auto *param = AsOpenOpParam(opParam);
    CHK_PTR_NULL(param);
    ops_hccl::AlgResourceCtxSerializable resCtx;

    char *ctx = static_cast<char *>(param->resCtx);
    std::vector<char> seq(ctx, ctx + param->ctxSize);
    resCtx.DeSerialize(seq);
    HCCL_INFO("[MC2_OPEN_DIAG][Launch] opType %u, algName[%s], inputPtr %p, outputPtr %p, resCtx %p, ctxSize %llu, stream %p.",
              static_cast<u32>(param->opType), param->algName, param->inputPtr, param->outputPtr,
              param->resCtx, static_cast<unsigned long long>(param->ctxSize), param->stream);
    if (param->opType == HCCL_CMD_ALLTOALLV) {
        HCCL_INFO("[MC2_OPEN_DIAG][Launch][AllToAllV] sendDataType %u, recvDataType %u, sendCounts %p, recvCounts %p, sdispls %p, rdispls %p.",
                static_cast<u32>(param->all2AllVDataDes.sendType), static_cast<u32>(param->all2AllVDataDes.recvType),
                param->all2AllVDataDes.sendCounts, param->all2AllVDataDes.recvCounts,
                param->all2AllVDataDes.sdispls, param->all2AllVDataDes.rdispls);
    } else if (param->opType == HCCL_CMD_ALLTOALL) {
        HCCL_INFO("[MC2_OPEN_DIAG][Launch][AllToAll][V] sendDataType %u, recvDataType %u, sendCounts %p, recvCounts %p, sdispls %p, rdispls %p.",
                static_cast<u32>(param->all2AllVDataDes.sendType), static_cast<u32>(param->all2AllVDataDes.recvType),
                param->all2AllVDataDes.sendCounts, param->all2AllVDataDes.recvCounts,
                param->all2AllVDataDes.sdispls, param->all2AllVDataDes.rdispls);
        HCCL_INFO("[MC2_OPEN_DIAG][Launch][AllToAll] sendCount %llu, sendDataType %u, recvCount %llu, recvDataType %u.",
                static_cast<unsigned long long>(param->all2AllDataDes.sendCount),
                static_cast<u32>(param->all2AllDataDes.sendType), static_cast<unsigned long long>(param->all2AllDataDes.recvCount),
                static_cast<u32>(param->all2AllDataDes.recvType));
    } else {
        HCCL_INFO("[MC2_OPEN_DIAG][Launch][Else] count %llu, dataType %u, outputType %u, strideCount %llu.",
                static_cast<unsigned long long>(param->DataDes.count),
                static_cast<u32>(param->DataDes.dataType), static_cast<u32>(param->DataDes.outputType),
                static_cast<unsigned long long>(param->DataDes.strideCount));
    }
    HCCL_INFO("[MC2_OPEN_DIAG][LaunchResCtx] userRank %u, userRankSize %u, threadNum %zu, cclMemAddr %p, "
              "cclMemSize %llu.",
              resCtx.topoInfo.userRank, resCtx.topoInfo.userRankSize, resCtx.threads.size(), resCtx.cclMem.addr,
              static_cast<unsigned long long>(resCtx.cclMem.size));
    const u64 dataTypeSize = GetDataTypeSize(param->DataDes.dataType);
    const u64 inputBytes = param->DataDes.count * dataTypeSize;
    const bool isAllGather = (param->opType == HCCL_CMD_ALLGATHER);
    const u64 rankStrideCount = (param->DataDes.strideCount == 0U) ?
        param->DataDes.count : param->DataDes.strideCount;
    const u64 outputSpanCount = isAllGather ?
        (rankStrideCount * (resCtx.topoInfo.userRankSize == 0U ? 0U : resCtx.topoInfo.userRankSize - 1U) +
            param->DataDes.count) : param->DataDes.count;
    const u64 outputBytes = outputSpanCount * dataTypeSize;
    const u64 rankStrideBytes = isAllGather ? rankStrideCount * dataTypeSize : 0U;
    const u64 inputBegin = reinterpret_cast<u64>(param->inputPtr);
    const u64 outputBegin = reinterpret_cast<u64>(param->outputPtr);
    HCCL_INFO("[MC2_OPEN_DIAG][LaunchRange] userRank %u, userRankSize %u, dataTypeSize %llu, inputBytes %llu, "
              "rankStrideCount %llu, rankStrideBytes %llu, outputSpanCount %llu, outputBytes %llu, "
              "inputRange [%#llx, %#llx), outputSpan [%#llx, %#llx), outputSpanRankScaled %u.",
              resCtx.topoInfo.userRank, resCtx.topoInfo.userRankSize,
              static_cast<unsigned long long>(dataTypeSize), static_cast<unsigned long long>(inputBytes),
              static_cast<unsigned long long>(rankStrideCount), static_cast<unsigned long long>(rankStrideBytes),
              static_cast<unsigned long long>(outputSpanCount), static_cast<unsigned long long>(outputBytes),
              static_cast<unsigned long long>(inputBegin),
              static_cast<unsigned long long>(inputBegin + inputBytes), static_cast<unsigned long long>(outputBegin),
              static_cast<unsigned long long>(outputBegin + outputBytes), isAllGather);

    CHK_RET(RestoreVarDataIfNeeded(*param, resCtx));

    if (isAllToAllV) {
        for (uint32_t i = 0; i < rankNum; i++) {
            HCCL_INFO("param.all2AllVDataDes.sendCounts[%d]:[%llu]", i , reinterpret_cast<u64*>(param->all2AllVDataDes.sendCounts)[i]);
            HCCL_INFO("param.all2AllVDataDes.recvCounts[%d]:[%llu]", i , reinterpret_cast<u64*>(param->all2AllVDataDes.recvCounts)[i]);
            HCCL_INFO("param.all2AllVDataDes.sdispls[%d]:[%llu]", i , reinterpret_cast<u64*>(param->all2AllVDataDes.sdispls)[i]);
            HCCL_INFO("param.all2AllVDataDes.rdispls[%d]:[%llu]", i , reinterpret_cast<u64*>(param->all2AllVDataDes.rdispls)[i]);
        }
    }

    auto executor = ops_hccl::CollAlgExecRegistryV2::Instance().GetAlgExec(param->opType, std::string(param->algName));
    CHK_SMART_PTR_NULL(executor);

    HcclResult ret = executor->Orchestrate(*param, resCtx);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("orchestrate failed for alg:%s", param->algName), ret);

    return HCCL_SUCCESS;
}
