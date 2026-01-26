/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: selector注册器文件头
 * Author: yinding
 * Create: 2024-02-04
 */
#ifndef HCCLV2_COLL_ALG_SELECTOR_REGISTRY
#define HCCLV2_COLL_ALG_SELECTOR_REGISTRY

#include <mutex>
#include <map>

#include "base_selector.h"

namespace Hccl {

class SelectorRegistry {
public:
    static SelectorRegistry      *Global();
    HcclResult                    Register(u32 priority, BaseSelector *selector);
    HcclResult RegisterByOpType(const OpType opType, u32 priority, BaseSelector *selector);
    std::map<u32, BaseSelector *> GetAllSelectors();
    std::map<u32, BaseSelector *> GetSelectorsByOpType(const OpType opType);

private:
    std::map<u32, BaseSelector *> impls_;
    std::map<OpType, std::map<u32, BaseSelector *>> opTypeImpls_;
    mutable std::mutex            mu_;
};

#define REGISTER_SELECTOR_HELPER(ctr, priority, name, selector)                                                        \
    static HcclResult g_func_##priority##_##name##_##ctr                                                               \
        = SelectorRegistry::Global()->Register(priority, new selector())

#define REGISTER_SELECTOR_HELPER_1(ctr, priority, name, selector)                                                      \
    REGISTER_SELECTOR_HELPER(ctr, priority, name, selector)

#define REGISTER_SELECTOR(priority, selector) REGISTER_SELECTOR_HELPER_1(__COUNTER__, priority, selector, selector)
}

#define REGISTER_SELECTOR_BY_OPTYPE_HELPER(ctr, optype, priority, name, selector)    \
    static HcclResult g_func_##priority##_##name##_##ctr                                                               \
        = SelectorRegistry::Global()->RegisterByOpType(optype, priority, new selector())

#define REGISTER_SELECTOR_BY_OPTYPE_HELPER_1(ctr, optype, priority, name, selector)  \
    REGISTER_SELECTOR_BY_OPTYPE_HELPER(ctr, optype, priority, name, selector)

#define REGISTER_SELECTOR_BY_OPTYPE(optype, priority, selector)              \
    REGISTER_SELECTOR_BY_OPTYPE_HELPER_1(__COUNTER__, optype, priority, selector, selector)// namespace Hccl

#endif
