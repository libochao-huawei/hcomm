/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: InsInsCollAlgRegistry算法注册类的头文件
 * Author: shenyutian
 * Create: 2024-04-30
 */

#ifndef HCCLV2_INS_COLL_ALG_REGISTRY
#define HCCLV2_INS_COLL_ALG_REGISTRY

#include <mutex>
#include <functional>
#include "ins_coll_alg_base.h"
#include "log.h"

namespace Hccl {

using InsCollAlgCreator = std::function<InsCollAlgBase *()>;
template <typename P> static InsCollAlgBase *DefaultCreator()
{
    static_assert(std::is_base_of<InsCollAlgBase, P>::value, "InsCollAlg type must derived from Hccl::InsCollAlgBase");
    return new (std::nothrow) P;
}

class InsCollAlgRegistry {
public:
    static InsCollAlgRegistry *Global();
    HcclResult Register(const OpType type, const std::string &funcName, const InsCollAlgCreator &collAlgCreator);
    void       PrintAllImpls();
    std::shared_ptr<InsCollAlgBase>            GetAlgImpl(const OpType type, const std::string &funcName);
    std::map<OpType, std::vector<std::string>> GetAvailAlgs();

private:
    std::map<OpType, std::map<std::string, const InsCollAlgCreator>> impls_;
    mutable std::mutex                                               mu_;
};

#define INS_REGISTER_IMPL_HELPER(ctr, type, name, insCollAlgBase)                                                      \
    static HcclResult g_func_##name##_##ctr                                                                            \
        = InsCollAlgRegistry::Global()->Register(type, std::string(#name), DefaultCreator<insCollAlgBase>)

#define INS_REGISTER_IMPL_HELPER_1(ctr, type, name, insCollAlgBase)                                                    \
    INS_REGISTER_IMPL_HELPER(ctr, type, name, insCollAlgBase)

#define INS_REGISTER_IMPL(type, name, insCollAlgBase)                                                                  \
    INS_REGISTER_IMPL_HELPER_1(__COUNTER__, type, name, insCollAlgBase)

#define INS_REGISTER_IMPL_BY_TEMP_HELPER(ctr, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate)                \
    static HcclResult g_func_##name##_##ctr = InsCollAlgRegistry::Global()->Register(                                  \
        type, std::string(#name), DefaultCreator<insCollAlgBase<AlgTopoMatch, InsAlgTemplate>>)

#define INS_REGISTER_IMPL_BY_TEMP_HELPER_1(ctr, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate)              \
    INS_REGISTER_IMPL_BY_TEMP_HELPER(ctr, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate)

#define INS_REGISTER_IMPL_BY_TEMP(type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate)                            \
    INS_REGISTER_IMPL_BY_TEMP_HELPER_1(__COUNTER__, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate)

#define INS_REGISTER_IMPL_BY_TWO_TEMPS_HELPER(ctr, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1)    \
    static HcclResult g_func_##name##_##ctr = InsCollAlgRegistry::Global()->Register(                                     \
        type, std::string(#name), DefaultCreator<insCollAlgBase<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1>>)

#define INS_REGISTER_IMPL_BY_TWO_TEMPS_HELPER_1(ctr, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1)  \
    INS_REGISTER_IMPL_BY_TWO_TEMPS_HELPER(ctr, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1)

#define INS_REGISTER_IMPL_BY_TWO_TEMPS(type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1)              \
    INS_REGISTER_IMPL_BY_TWO_TEMPS_HELPER_1(__COUNTER__, type, name, insCollAlgBase, AlgTopoMatch, InsAlgTemplate0,             \
        InsAlgTemplate1)

#define INS_REGISTER_IMPL_BY_TOPO_HELPER(ctr, type, name, insCollAlgBase, AlgTopoMatch)                \
    static HcclResult g_func_##name##_##ctr = InsCollAlgRegistry::Global()->Register(                                  \
        type, std::string(#name), DefaultCreator<insCollAlgBase<AlgTopoMatch>>)

#define INS_REGISTER_IMPL_BY_TOPO_HELPER_1(ctr, type, name, insCollAlgBase, AlgTopoMatch)                              \
    INS_REGISTER_IMPL_BY_TOPO_HELPER(ctr, type, name, insCollAlgBase, AlgTopoMatch)

#define INS_REGISTER_IMPL_BY_TOPO(type, name, insCollAlgBase, AlgTopoMatch)                                            \
    INS_REGISTER_IMPL_BY_TOPO_HELPER_1(__COUNTER__, type, name, insCollAlgBase, AlgTopoMatch)

} // namespace Hccl

#endif