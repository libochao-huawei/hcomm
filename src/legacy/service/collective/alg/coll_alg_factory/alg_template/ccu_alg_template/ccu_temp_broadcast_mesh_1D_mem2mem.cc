/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempBroadcastMeshMem2Mem1D类实现
 * Author: hanyan
 * Create: 2025-11-07
 */

#include <ios>
#include <iostream>
#include "log.h"
#include "ccu_temp_broadcast_mesh_1D_mem2mem.h"
#include "alg_data_trans_wrapper.h"
#include "ccu_assist.h"
#include "ccu_rank_group.h"
#include "ccu_ctx_creator_registry.h"
#include "ccu_context_broadcast_mesh1d_mem2mem.h"
#include "ccu_instruction_broadcast_mesh1d_mem2mem.h"
#include "ccu_alg_template_base.h"
namespace Hccl {

static CcuInstRegister<CcuContextBroadcastMesh1DMem2Mem>
    registrarBroadcast(CcuInstType::CCU_BROADCAST_MESH_1D_MEM2MEM);

CcuTempBroadcastMesh1DMem2Mem::CcuTempBroadcastMesh1DMem2Mem(
    const RankId virtualRank, const u32 tempRankSize, const std::vector<std::vector<RankId>> &tempVTopo,
    const std::map<RankId, u32> &tempVirtRankMap)
    : CcuAlgTemplateBase(virtualRank, tempRankSize, tempVTopo, tempVirtRankMap)
{
}

CcuTempBroadcastMesh1DMem2Mem::~CcuTempBroadcastMesh1DMem2Mem()
{
}

HcclResult CcuTempBroadcastMesh1DMem2Mem::CalcRes(AlgTempResReq &tempResReq)
{
    tempResReq.queNum    = 1;
    tempResReq.streamNum = tempResReq.queNum;
    HCCL_DEBUG("[CalcRes] tempResReq.queNum[%u]", tempResReq.queNum);
    CHK_RET(CalcResLinksMesh(myRank_, tempRankSize_, tempVTopo_, linkNumBtwPeers_, tempResReq));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTempBroadcastMesh1DMem2Mem::GenExtIns(const TempFuncs          &tempFuncs,
                                                              const TemplateDataParams &templateDataParams,
                                                              const ResLinks           &tempLinks,
                                                              std::vector<InsQuePtr>   &tempInsQues)
{
    opMode_   = tempFuncs.opMode; // 传递opMode，是opbase还是offload
    buffInfo_ = templateDataParams.buffInfo;
    CcuInstructionBroadcastMesh1DMem2Mem ccuIns;
    std::vector<uint64_t> dimSize;
    dimSize.push_back(tempRankSize_);
    uint32_t                                rankId    = myRank_;
    uint32_t                                rootId    = tempVirtRankMap_[rootId_];
    const CollAlgOperator                  &op        = op_;
    const std::vector<std::vector<RankId>> &tempVTopo = tempVTopo_;
    uint64_t      inputAddr          = BufferTypeToAddr(buffInfo_.inBuffType) + buffInfo_.inBuffBaseOff;
    uint64_t      outputAddr         = BufferTypeToAddr(buffInfo_.outBuffType) + buffInfo_.outBuffBaseOff;
    uint64_t token;
    CHK_RET(GetToken(op_, token));
    uint64_t      inputSliceStride   = templateDataParams.inputSliceStride;
    uint64_t      outputSliceStride  = templateDataParams.outputSliceStride;
    uint64_t      repeatNum          = templateDataParams.repeatNum;
    uint64_t      inputRepeatStride  = templateDataParams.inputRepeatStride;
    uint64_t      outputRepeatStride = templateDataParams.outputRepeatStride;
    uint64_t      curSliceSize       = templateDataParams.sliceSize;
    uint64_t      allgatherOffset    = templateDataParams.sliceSize * tempVirtRankMap_[rankId];
    uint64_t      repeatNumVar       = UINT64_MAX - repeatNum;
    uint64_t      normalSliceSize = curSliceSize / tempRankSize_;
    uint64_t      lastSliceSize   = curSliceSize % tempRankSize_ + normalSliceSize;
    ccuIns.Init(tempVirtRankMap_[rankId], rootId, op, tempVTopo, inputAddr, outputAddr, token, inputSliceStride, outputSliceStride,
                repeatNum, inputRepeatStride, outputRepeatStride, normalSliceSize, lastSliceSize, allgatherOffset,
                repeatNumVar);
    HCCL_DEBUG("[CcuTempBroadcastMesh1DTwoShotMem2Mem] Run Init: rankId[%u], rootId[%u], inputAddr[%llu], "
               "outputAddr[%llu], inputSliceStride[%llu], outputSliceStride[%llu], repeatNum[%llu], "
               "inputRepeatStride[%llu], outputRepeatStride[%llu], normalSliceSize[%llu], "
               "lastSliceSize[%llu],allgatherOffset_[%llu], repeatNumVar[%llu]",
               rankId, rootId, inputAddr, outputAddr, inputSliceStride, outputSliceStride, repeatNum, inputRepeatStride,
               outputRepeatStride, normalSliceSize, lastSliceSize, allgatherOffset, repeatNumVar);
    std::vector<LinkData> links;
    for (auto &pair : tempLinks) {
        if (pair.second.empty()) {
            continue;
        }
        links.push_back(pair.second[0]);
    }
    HCCL_DEBUG("[CcuTempBroadcastMesh1DMem2Mem] links.size[%llu]", links.size());
    ccuIns.SetLinks(links);
    RankGroup rankGroup;
    for (auto &peer : tempVTopo_[0]) {
        rankGroup.AddRank(peer); // 添加RankId到ranks中
    }
    u32 cntCkeNum = 5;
    ccuIns.SetCntCkeNum(cntCkeNum);
    ccuIns.SetRankGroup(rankGroup);
    HCCL_DEBUG("CcuTempBroadcastMesh1DMem2Mem is [%s]", ccuIns.Describe().c_str());
    tempInsQues[0]->Append(std::move(std::make_unique<CcuInstructionBroadcastMesh1DMem2Mem>(ccuIns)));
    return HcclResult::HCCL_SUCCESS;
}

} // namespace Hccl
