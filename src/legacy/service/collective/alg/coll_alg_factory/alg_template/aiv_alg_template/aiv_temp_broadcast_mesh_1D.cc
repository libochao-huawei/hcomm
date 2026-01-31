/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板AivTempBroadcastMesh1D类实现
 * Author: zhangzuyu
 * Create: 2025-09-15
 */

#include "hccl_aiv_utils.h"
#include "aiv_ins.h"
#include "aiv_temp_broadcast_mesh_1D.h"

namespace Hccl {

AivTempBroadcastMesh1D::AivTempBroadcastMesh1D(const RankId virtualRank, const u32 tempRankSize,
    const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap)
    : AivAlgTemplateBase(virtualRank, tempRankSize, tempVTopo, tempVirtRankMap)
{
}

AivTempBroadcastMesh1D::~AivTempBroadcastMesh1D()
{
}

HcclResult AivTempBroadcastMesh1D::CalcRes(AlgTempResReq &tempResReq)
{
    tempResReq.queNum = 1;
    tempResReq.streamNum = tempResReq.queNum;
    HCCL_INFO("[CalcRes] tempResReq.queNum[%u]", tempResReq.queNum);
    CHK_RET(CalcResLinksMesh(myRank_, tempRankSize_, tempVTopo_, linkNumBtwPeers_, tempResReq));

    return HcclResult::HCCL_SUCCESS;
}

HcclResult AivTempBroadcastMesh1D::GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, 
                                             const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues)
{
    HCCL_INFO("[AivTempBroadcastMesh1D] GenExtIns start");

    std::vector<LinkData> allLinks;
    for (auto iter = tempLinks.begin(); iter != tempLinks.end(); ++iter) {
        allLinks.emplace_back(iter->second.at(0));
    }

    IncSliceId();  // 自动增长sliceId，传入aivTag

    AivOpArgs aivBroadcastArgs;
    aivBroadcastArgs.cmdType = HcclCMDType::HCCL_CMD_BROADCAST;
    aivBroadcastArgs.input = templateDataParams.buffInfo.inBuffBaseOff;
    aivBroadcastArgs.output = templateDataParams.buffInfo.outBuffBaseOff;
    aivBroadcastArgs.rank = myRank_;
    aivBroadcastArgs.rankSize = tempRankSize_;
    aivBroadcastArgs.count = templateDataParams.sliceSize / DataTypeSizeGet(dataType_);
    aivBroadcastArgs.dataType = dataType_;
    aivBroadcastArgs.root = root_;
    aivBroadcastArgs.aivTag = sliceId_;  // 传入aivTag，Lauch时重新组装为aivTag
    aivBroadcastArgs.isOpBase = (tempFuncs.opMode == OpMode::OPBASE);
    aivBroadcastArgs.xRankSize = tempVTopo_[0].size();
    aivBroadcastArgs.yRankSize = 0;
    aivBroadcastArgs.zRankSize = 0;
    for (u32 i = 0; i < tempVTopo_[0].size(); i++){
        aivBroadcastArgs.topo_[i] = tempVTopo_[0][i];
    }
    if (tempVTopo_.size() > 1){
        aivBroadcastArgs.yRankSize = tempVTopo_[1].size();
        for (u32 i = 0; i < tempVTopo_[1].size(); i++){
            aivBroadcastArgs.topo_[TOPO_LEN_Y_OFFSET + i] = tempVTopo_[1][i];
        }
    }
    if (tempVTopo_.size() > MAX_DIM_NUM){
        aivBroadcastArgs.zRankSize = tempVTopo_[MAX_DIM_NUM].size();
        for (u32 i = 0; i < tempVTopo_[MAX_DIM_NUM].size(); i++){
            aivBroadcastArgs.topo_[TOPO_LEN_Z_OFFSET + i] = tempVTopo_[MAX_DIM_NUM][i];
        }
    }
    aivBroadcastArgs.inputSliceStride = templateDataParams.inputSliceStride;
    aivBroadcastArgs.outputSliceStride = templateDataParams.outputSliceStride;
    aivBroadcastArgs.repeatNum = templateDataParams.repeatNum;
    aivBroadcastArgs.inputRepeatStride = templateDataParams.inputRepeatStride;
    aivBroadcastArgs.outputRepeatStride = templateDataParams.outputRepeatStride;

    std::unique_ptr<Instruction> aivInsBroadcastMesh1D = std::make_unique<AivInstruction>(allLinks, aivBroadcastArgs);

    tempInsQues[0]->Append(std::move(aivInsBroadcastMesh1D));

    HCCL_INFO("[AivTempBroadcastMesh1D] GenExtIns finished");
    return HcclResult::HCCL_SUCCESS;
}

}  // namespace Hccl