/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempAllReduceMesh1DTwoShotMeshChunk类头文件
 * Author: wangyuqing
 * Create: 2025-11-07
 */
 
#ifndef HCCLV2_INS_TEMP_ALL_REDUCE_1D_MESH_TWO_SHOT_MESH_CHUNK
#define HCCLV2_INS_TEMP_ALL_REDUCE_1D_MESH_TWO_SHOT_MESH_CHUNK
 
#include "string_util.h"
#include "ins_alg_template_base.h"
#include "executor_utils.h"
 
namespace Hccl {
 
class InsTempAllReduceMesh1DTwoShotMeshChunk : public InsAlgTemplateBase {
public:
    explicit InsTempAllReduceMesh1DTwoShotMeshChunk(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~InsTempAllReduceMesh1DTwoShotMeshChunk() override;
 
    std::string Describe() const override
    {
        return StringFormat("Template of all reduce 1D mesh chunk with tempRankSize [%u].", tempRankSize_);
    }
 
    u32 CalcScratchMultiple(BufferType input, BufferType output) const;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    RankId GetRankFromMap(const u32 rankIdx);
    HcclResult PreCopy(const TemplateDataParams &tempAlgParams, const RankSliceInfo &sliceInfoVec, std::vector<InsQuePtr> &tempInsQues);
    HcclResult ReduceScatterMeshChunk(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues, 
        const TemplateDataParams &tempAlgParams, const std::vector<vector<u64>> &sliceSize, const u32 &stepIndex, const u32 &myAlgRank);
 
private:
    HcclResult RunReduceScatter(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    HcclResult RunAllgather(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
};
 
}  // namespace Hccl
 
#endif  // !HCCLV2_INS_TEMP_ALL_REDUCE_1D_MESH_TWO_SHOT_MESH_CHUNK
