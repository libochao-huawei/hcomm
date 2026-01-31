/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法模板InsTempAllGatherMesh1D类头文件
 * Author: shenyutian
 * Create: 2024-04-30
 */

#ifndef HCCLV2_INS_TEMP_ALL_GATHER_MESH
#define HCCLV2_INS_TEMP_ALL_GATHER_MESH

#include "string_util.h"
#include "executor_utils.h"
#include "ins_alg_template_base.h"

namespace Hccl {

class InsTempAllGatherMesh1D : public InsAlgTemplateBase {
public:
    explicit InsTempAllGatherMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                  const std::vector<std::vector<RankId>> &tempVTopo,
                                  const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempAllGatherMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of all gather mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    u32 CalcScratchMultiple(const BufferType &inBufferTpye, const BufferType &outBufferTpye) const
    {
        (void) inBufferTpye;
        (void) outBufferTpye;
        HCCL_INFO(
            "[InsTempAllGatherMesh1D][CalcScratchMultiple] templateScratchMultiplier[%llu]", tempRankSize_);
        return tempRankSize_;
    }
private:
    HcclResult LocalDataCopy(std::vector<InsQuePtr> &tempInsQues);
    HcclResult PostLocalCopy(std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunMesh(const u32 myAlgRank, const std::vector<RankId> &vTopo, std::vector<InsQuePtr> &tempInsQues);

    u32 majorQueNum_       = 0;
    u32 queNumPerNeighbor_ = 1;
    bool enableInterRankCounterNotify_ = false;
    TemplateDataParams tempAlgParams_;
    ResLinks tempLinks_;
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_ALL_GATHER_MESH