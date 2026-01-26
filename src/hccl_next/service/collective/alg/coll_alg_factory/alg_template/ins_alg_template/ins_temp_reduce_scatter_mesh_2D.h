/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceScatterMesh2D类头文件
 * Author: chenluyao
 * Create: 2025-07-30
 */

#ifndef INS_TEMP_REDUCE_SCATTER_MESH_2D_H
#define INS_TEMP_REDUCE_SCATTER_MESH_2D_H

#include "string_util.h"
#include "ins_temp_all_gather_nhr.h"
#include "ins_alg_template_base.h"
#include "executor_utils.h"
namespace Hccl {

constexpr u32 PARALLEL_SIZE = 2;
class InsTempReduceScatterMesh2D : public InsAlgTemplateBase {
public:
    explicit InsTempReduceScatterMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                        const std::vector<std::vector<RankId>> &tempVTopo,
                                        const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempReduceScatterMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce scatter Mesh2D with tempRankSize [%u].", tempRankSize_);
    }
    u64 CalcScratchMultiple(const BufferType &inBuffType, const BufferType &outBuffType);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams, const ResLinks &tempLinks,
                         std::vector<InsQuePtr> &tempInsQues);
private:
    HcclResult CalcResLinksMesh2D(const u32 linkNumBtwPeers, AlgTempResReq &tempResReq);
    HcclResult PreCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult SendRecvProcess(const ResLinks &tempLinks, std::vector<std::vector<DataSlice>> allSliceVec,
                               std::vector<InsQuePtr> &tempInsQues, u32 remoteRank, u32 queIdx) const;
    HcclResult RunFirstLevel(const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    HcclResult RunFirstReduce(std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    HcclResult RunSecondLevel(const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    HcclResult RunSecondReduce(std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    RankId GetRankFromMap(const u32 rankIdx);
    u32      xQueNum_ = 0;
    u32      yQueNum_ = 0;
    u32      xRankSize_ = 0;
    u32      yRankSize_ = 0;
    u32      xRankId_ = 0;
    u32      yRankId_ = 0;
    u64      halfDataSize_ = 0;
};

} // namespace Hccl

#endif //INS_TEMP_REDUCE_SCATTER_MESH_2D_H