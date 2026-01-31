/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceAicpuReduceMesh2D类头文件
 * Author: zhangzuyu
 * Create: 2025-10-17
 */

#ifndef HCCLV2_INS_TEMP_REDUCE_AICPU_REDUCE_MESH_2D
#define HCCLV2_INS_TEMP_REDUCE_AICPU_REDUCE_MESH_2D

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempReduceAicpuReduceMesh2D : public InsAlgTemplateBase {
public:
    explicit InsTempReduceAicpuReduceMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempReduceAicpuReduceMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of aicpu reduce reduce 2D Mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;
private:
    HcclResult RunAicpuLocalReduce(const TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunGatherToRootXY(const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                        std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunGatherToRootX(const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                        std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunGatherToRootY(const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                        std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunXYGatherToRoot(const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                        std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult RunXGatherToRoot(const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                        std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult RunYGatherToRoot(const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                        std::vector<InsQuePtr> &tempInsQues) const;
    u32 sizeX_{0};
    u32 sizeY_{0};
    u32 rootX_{0};
    u32 rootY_{0};
    u32 curX_{0};
    u32 curY_{0};
    u64 dataTypeSize_{0};
    u64 dataSizeX_{0};
    u64 dataSizeY_{0};
    u64 rankOffsetY_{0};
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_REDUCE_AICPU_REDUCE_MESH_2D