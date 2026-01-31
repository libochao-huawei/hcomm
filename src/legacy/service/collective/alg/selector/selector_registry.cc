/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: selector注册器实现
 * Author: yinding
 * Create: 2024-02-04
 */
#include "selector_registry.h"

#include <string>
#include <iostream>
#include <map>
#include <mutex>

namespace Hccl {

SelectorRegistry *SelectorRegistry::Global()
{
    static SelectorRegistry *globalSelectorRegistry = new SelectorRegistry;
    return globalSelectorRegistry;
}

HcclResult SelectorRegistry::Register(u32 priority, BaseSelector *selector)
{
    const std::lock_guard<std::mutex> lock(mu_);
    if (impls_.count(priority) != 0) {
        HCCL_ERROR("[Algo][Selector] priority %llu already registered.", priority);
        return HcclResult::HCCL_E_PARA;
    }

    impls_[priority] = selector;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult SelectorRegistry::RegisterByOpType(const OpType opType, u32 priority, BaseSelector *selector)
{
    const std::lock_guard<std::mutex> lock(mu_);
    if (opTypeImpls_[opType].count(priority) != 0) {
        HCCL_ERROR("[Algo][Selector] opType %d priority %llu already registered.", opType, priority);
        return HcclResult::HCCL_E_PARA;
    }
    opTypeImpls_[opType][priority] = selector;
    return HcclResult::HCCL_SUCCESS;
}

std::map<u32, BaseSelector *> SelectorRegistry::GetSelectorsByOpType(const OpType opType)
{
    if (opTypeImpls_.count(opType) == 0) {
        HCCL_WARNING("[Algo][Selector] opType %d has no selector registered.", opType);
        return {};
    }
    return opTypeImpls_[opType];
}

std::map<u32, BaseSelector *> SelectorRegistry::GetAllSelectors()
{
    return impls_;
}

} // namespace Hccl
