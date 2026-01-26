/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: selector执行器实现
 * Author: yinding
 * Create: 2024-02-04
 */
#include "execute_selector.h"

#include "base_selector.h"
#include "selector_registry.h"

namespace Hccl {
ExecuteSelector &ExecuteSelector::SetVirtualTopo(RankGraph *rankGraph)
{
    rankGraph_ = rankGraph;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetDevType(DevType devType)
{
    devType_ = devType;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetMyRank(RankId myRank)
{
    myRank_ = myRank;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetRankSize(u32 rankSize)
{
    rankSize_ = rankSize;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetSeverId(std::string severId)
{
    severId_ = severId;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetDeviceNumPerSever(u32 deviceNumPerSever)
{
    deviceNumPerSever_ = deviceNumPerSever;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetServerNum(u32 serverNum)
{
    serverNum_ = serverNum;
    return *this;
}

ExecuteSelector &ExecuteSelector::SetOpConfig(OpExecuteConfig opConfig)
{
    opConfig_ = opConfig;
    return *this;
}

HcclResult ExecuteSelector::Run(const CollAlgOperator &op, CollAlgParams &params, std::string &primQueueGenName)
{
    HCCL_DEBUG("[Algo][Selector] Run.");
    if (rankGraph_ == nullptr) {
        HCCL_ERROR("[Algo][ExecuteSelector] rankGraph_ is nullptr.");
        return HcclResult::HCCL_E_PTR;
    }
    std::map<u32, BaseSelector *> selectors = SelectorRegistry::Global()->GetAllSelectors();

    if (params.isMc2) {
        auto iter = selectors.find(18);
        if (iter == selectors.end()) {
            HCCL_ERROR("[Algo][Selector] CCU selector is not registried.");
            return HcclResult::HCCL_E_NOT_SUPPORT;
        }
        iter->second->SetVirtualTopo(rankGraph_)
            .SetDevType(devType_)
            .SetMyRank(myRank_)
            .SetRankSize(rankSize_)
            .SetSeverId(severId_)
            .SetDeviceNumPerSever(deviceNumPerSever_)
            .SetServerNum(serverNum_);
        if(iter->second->Select(op, params, primQueueGenName) == SelectorStatus::MATCH) {
            HCCL_INFO("[Algo][Selector] The ccu selector[priority of %u] is matched, the selected algo type is %s",
                iter->first, primQueueGenName.c_str());
            return HcclResult::HCCL_SUCCESS;
        }
        HCCL_ERROR("[Algo][Selector] CCU selector can not match for optype[%d].", op.opType);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    selectors = SelectorRegistry::Global()->GetSelectorsByOpType(op.opType);
    HCCL_INFO("[Algo][Selector] The selector nums of optype[%d] is [%zu].", op.opType, selectors.size());
    for (auto iter : selectors) {
        HCCL_DEBUG("[Algo][Selector] The selector[priority of %llu] is running.", iter.first);
        iter.second->SetVirtualTopo(rankGraph_)
            .SetDevType(devType_)
            .SetMyRank(myRank_)
            .SetRankSize(rankSize_)
            .SetSeverId(severId_)
            .SetDeviceNumPerSever(deviceNumPerSever_)
            .SetServerNum(serverNum_)
            .SetOpConfig(opConfig_);
        if (iter.second->Select(op, params, primQueueGenName) == SelectorStatus::MATCH) {
            HCCL_INFO("[Algo][Selector] The selector[priority of %llu] is matched, the selected algo type is %s",
                      iter.first, primQueueGenName.c_str());
            return HcclResult::HCCL_SUCCESS;
        }
    }

    HCCL_WARNING("[Algo][Selector] No selector is matched.");
    return HcclResult::HCCL_E_NOT_SUPPORT;
}

} // namespace Hccl
