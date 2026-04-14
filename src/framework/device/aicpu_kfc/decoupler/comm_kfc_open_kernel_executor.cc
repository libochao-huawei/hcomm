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

    constexpr u32 ALL_TO_ALL_V_OFFSET_RECV_COUNTS = 1;
    constexpr u32 ALL_TO_ALL_V_OFFSET_SDISPLS = 2;
    constexpr u32 ALL_TO_ALL_V_OFFSET_RDISPLS = 3;

    u64 *data = reinterpret_cast<u64 *>(param.varData);
    param.all2AllVDataDes.sendCounts = data;
    param.all2AllVDataDes.recvCounts = data + ALL_TO_ALL_V_OFFSET_RECV_COUNTS * rankSize;
    param.all2AllVDataDes.sdispls = data + ALL_TO_ALL_V_OFFSET_SDISPLS * rankSize;
    param.all2AllVDataDes.rdispls = data + ALL_TO_ALL_V_OFFSET_RDISPLS * rankSize;

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

HcclResult LaunchOpenOpParamDataImpl(std::vector<uint8_t> &opParam)
{
    auto *param = AsOpenOpParam(opParam);
    CHK_PTR_NULL(param);
    ops_hccl::AlgResourceCtxSerializable resCtx;

    char *ctx = static_cast<char *>(param->resCtx);
    std::vector<char> seq(ctx, ctx + param->ctxSize);
    resCtx.DeSerialize(seq);

    CHK_RET(RestoreVarDataIfNeeded(*param, resCtx));

    auto executor = ops_hccl::CollAlgExecRegistryV2::Instance().GetAlgExec(param->opType, std::string(param->algName));
    CHK_SMART_PTR_NULL(executor);

    HcclResult ret = executor->Orchestrate(*param, resCtx);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("orchestrate failed for alg:%s", param->algName), ret);

    return HCCL_SUCCESS;
}
