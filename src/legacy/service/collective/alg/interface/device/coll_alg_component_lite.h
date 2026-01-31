/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 对HCCL框架提供的AICPU执行类文件头
 * Author: shenyutian
 * Create: 2024-07-06
 */
#ifndef HCCLV2_COLL_ALG_COMPONENT_LITE
#define HCCLV2_COLL_ALG_COMPONENT_LITE

#include <string>
#include "dev_type.h"
#include "connected_link_mgr.h"
#include "base_config.h"
#include "coll_alg_params.h"
#include "coll_operator.h"
#include "prim_queue.h"
#include "coll_alg_component.h"
#include "rmt_data_buffer_mgr.h"

namespace Hccl {

using PrimQuePtr = std::shared_ptr<PrimQueue>;

class CollAlgComponentLite {
public:
    CollAlgComponentLite(RankId myRank, u32 rankSize, DevType devType, u64 scratchBufferSize, ConnectedLinkMgr *linkMgr,
        RmtDataBufferMgr *rmaDataBufferMgr) : myRank_(myRank), rankSize_(rankSize), devType_(devType),
        scratchBufferSize_(scratchBufferSize), linkMgr_(linkMgr), rmaDataBufferMgr_(rmaDataBufferMgr)
    {
    }
    virtual ~CollAlgComponentLite() = default;

    void EnableDetour(bool enableDetour);
    void EnableDataAllign(bool enableAllign);
    void SetAllignSize(u64 allignSize);
    void SetDmaMode(const DmaMode dmaMode);
    void UpdateScratchBufferSize(u64 bufferSize);

    HcclResult ParsePackedData(std::vector<char> packedData);

    virtual HcclResult Orchestrate(const CollAlgOperator &op, const std::string &algName,
                                   const AlgTopoInfo &algTopoInfo, PrimQuePtr queue);
    virtual HcclResult Orchestrate(const CollAlgOperator &op, const std::string &algName,
                                   const AlgTopoInfo &algTopoInfo, InsQuePtr queue);

protected:
    u32               myRank_            = INVALID_RANKID;
    u32               rankSize_          = 0;
    DevType           devType_           = DevType::DEV_TYPE_NOSOC;
    u64               scratchBufferSize_ = 0;
    ConnectedLinkMgr *linkMgr_           = nullptr;
    RmtDataBufferMgr *rmaDataBufferMgr_{ nullptr };

    bool enableDetour_ = false;
    bool enableAllign_ = false;
    u64  allignSize_   = 0;

    DmaMode dmaMode_ = DmaMode::DEFAULT;
};

using CollAlgComponentLitePtr = std::shared_ptr<CollAlgComponentLite>;
} // namespace Hccl
#endif