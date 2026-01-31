/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempBroadcastMesh2DTwoShot类头文件
 * Author: zhangzuyu
 * Create: 2025-08-01
 */

#ifndef HCCLV2_INS_TEMP_BROADCAST_MESH_2D_TWO_SHOT
#define HCCLV2_INS_TEMP_BROADCAST_MESH_2D_TWO_SHOT

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {


class InsTempBroadcastMesh2DTwoShot : public InsAlgTemplateBase {
public:
    explicit InsTempBroadcastMesh2DTwoShot(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempBroadcastMesh2DTwoShot() override;

    std::string Describe() const override
    {
        return StringFormat("Template of broadcast mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType);

private:
    std::pair<u32, u32> GetRankPos(u32 rank) const { return make_pair(rank / sizeX_, rank % sizeX_); }
    HcclResult RunScatter(const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues, u64 baseOffset, u64 dataSize, u32 root, bool isRootX);
    HcclResult RunAllgather(const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues, u64 baseOffset, u64 dataSize, bool isX, bool isDma);
    HcclResult PreCopy(const TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PostCopy(const TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult Run1RootScatterXY(u64 dataSize, const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunNRootScatterYX(u64 dataSize, const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunAllgatherYX(u64 dataSize, const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunAllgatherXY(u64 dataSize, const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    // 列数
    u32 sizeX_{0};
    // 行数
    u32 sizeY_{0};
    //当前rank行号
    u32 curX_{0};
    //当前rank列号
    u32 curY_;
    u64 dataTypeSize_{0};
    u64 inputOffset_{0};
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_BROADCAST_MESH