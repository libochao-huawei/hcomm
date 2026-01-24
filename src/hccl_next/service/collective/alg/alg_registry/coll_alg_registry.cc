/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: CollAlgRegistry算法注册类的实现
 * Author: yinding
 * Create: 2024-02-04
 */
#include "coll_alg_registry.h"

#include <string>
#include <iostream>
#include <map>
#include <mutex>

namespace Hccl {

CollAlgRegistry *CollAlgRegistry::Global()
{
    static CollAlgRegistry *globalAlgImplRegistry = new CollAlgRegistry;
    return globalAlgImplRegistry;
}

HcclResult CollAlgRegistry::Register(const OpType type, const std::string &funcName,
                                     const CollAlgCreator &collAlgCreator)
{
    const std::lock_guard<std::mutex> lock(mu_);
    if (impls_[type].count(funcName) != 0) {
        HCCL_ERROR("%d:%s already registered.", type, funcName.c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    impls_[type].emplace(funcName, collAlgCreator);
    return HcclResult::HCCL_SUCCESS;
}

void CollAlgRegistry::PrintAllImpls()
{
    for (auto &iter : impls_) {
        HCCL_DEBUG("-------------------------------------");
        HCCL_DEBUG("type name is %s", iter.first.Describe().c_str());
        for (auto &alg : iter.second) {
            HCCL_DEBUG("    alg name is  %s", alg.first.c_str());
            if (alg.second == nullptr) {
                HCCL_DEBUG("    alg func is nullptr");
            }
        }
    }
}

std::map<OpType, std::vector<std::string>> CollAlgRegistry::GetAvailAlgs()
{
    std::map<OpType, std::vector<std::string>> algs;
    for (auto &iter : impls_) {
        HCCL_DEBUG("-------------------------------------");
        HCCL_DEBUG("type name is %s", iter.first.Describe().c_str());
        std::vector<std::string> tmpAvailAlgs;
        for (auto &alg : iter.second) {
            HCCL_DEBUG("    alg name is  %s", alg.first.c_str());
            tmpAvailAlgs.push_back(alg.first);
            if (alg.second == nullptr) {
                HCCL_DEBUG("    alg func is nullptr");
            }
        }
        algs.insert(std::make_pair(iter.first, tmpAvailAlgs));
    }
    return algs;
}

std::shared_ptr<CollAlgBase> CollAlgRegistry::GetAlgImpl(const OpType type, const std::string &funcName)
{
    if (impls_.count(type) == 0 || impls_[type].count(funcName) == 0) {
        HCCL_ERROR("%s:%s is not registered.", type.Describe().c_str(), funcName.c_str());
        return nullptr;
    }
    return std::shared_ptr<CollAlgBase>(impls_[type][funcName]());
}
} // namespace Hccl
