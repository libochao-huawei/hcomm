/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: CollAlgRegistry算法注册类的头文件
 * Author: yinding
 * Create: 2024-02-04
 */
#ifndef HCCLV2_COLL_ALG_REGISTRY
#define HCCLV2_COLL_ALG_REGISTRY

#include <mutex>
#include <functional>
#include "coll_alg_base.h"
#include "log.h"

namespace Hccl {

using CollAlgCreator = std::function<CollAlgBase *()>;
template <typename P> static CollAlgBase *DefaultCreator()
{
    static_assert(std::is_base_of<CollAlgBase, P>::value, "CollAlg type must derived from Hccl::CollAlgBase");
    return new (std::nothrow) P;
}

class CollAlgRegistry {
public:
    static CollAlgRegistry *Global();
    HcclResult Register(const OpType type, const std::string &funcName, const CollAlgCreator &collAlgCreator);
    void       PrintAllImpls();
    std::shared_ptr<CollAlgBase>               GetAlgImpl(const OpType type, const std::string &funcName);
    std::map<OpType, std::vector<std::string>> GetAvailAlgs();

private:
    std::map<OpType, std::map<std::string, const CollAlgCreator>> impls_;
    mutable std::mutex                                            mu_;
};

#define REGISTER_IMPL_HELPER(ctr, type, name, collAlgBase)                                                             \
    static HcclResult g_func_##name##_##ctr                                                                            \
        = CollAlgRegistry::Global()->Register(type, std::string(#name), DefaultCreator<collAlgBase>)

#define REGISTER_IMPL_HELPER_1(ctr, type, name, collAlgBase) REGISTER_IMPL_HELPER(ctr, type, name, collAlgBase)

#define REGISTER_IMPL(type, name, collAlgBase) REGISTER_IMPL_HELPER_1(__COUNTER__, type, name, collAlgBase)

#define REGISTER_IMPL_BY_TEMP_HELPER(ctr, type, name, collAlgBase, AlgTopoMatch, AlgTemplate)                          \
    static HcclResult g_func_##name##_##ctr = CollAlgRegistry::Global()->Register(                                     \
        type, std::string(#name), DefaultCreator<collAlgBase<AlgTopoMatch, AlgTemplate>>)

#define REGISTER_IMPL_BY_TEMP_HELPER_1(ctr, type, name, collAlgBase, AlgTopoMatch, AlgTemplate)                        \
    REGISTER_IMPL_BY_TEMP_HELPER(ctr, type, name, collAlgBase, AlgTopoMatch, AlgTemplate)

#define REGISTER_IMPL_BY_TEMP(type, name, collAlgBase, AlgTopoMatch, AlgTemplate)                                      \
    REGISTER_IMPL_BY_TEMP_HELPER_1(__COUNTER__, type, name, collAlgBase, AlgTopoMatch, AlgTemplate)

#define REGISTER_IMPL_BY_TWO_TEMPS_HELPER(ctr, type, name, collAlgBase, AlgTopoMatch, AlgTemplateRS, AlgTemplateAG)    \
    static HcclResult g_func_##name##_##ctr = CollAlgRegistry::Global()->Register(                                     \
        type, std::string(#name), DefaultCreator<collAlgBase<AlgTopoMatch, AlgTemplateRS, AlgTemplateAG>>)

#define REGISTER_IMPL_BY_TWO_TEMPS_HELPER_1(ctr, type, name, collAlgBase, AlgTopoMatch, AlgTemplateRS, AlgTemplateAG)  \
    REGISTER_IMPL_BY_TWO_TEMPS_HELPER(ctr, type, name, collAlgBase, AlgTopoMatch, AlgTemplateRS, AlgTemplateAG)

#define REGISTER_IMPL_BY_TWO_TEMPS(type, name, collAlgBase, AlgTopoMatch, AlgTemplateRS, AlgTemplateAG)                \
    REGISTER_IMPL_BY_TWO_TEMPS_HELPER_1(__COUNTER__, type, name, collAlgBase, AlgTopoMatch, AlgTemplateRS,             \
                                        AlgTemplateAG)

} // namespace Hccl

#endif
