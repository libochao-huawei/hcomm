/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu rep reference manager header file
 * Create: 2025-02-20
 */

#ifndef CCU_REP_REFERENCE_MANAGER_H
#define CCU_REP_REFERENCE_MANAGER_H

#include <unordered_map>
#include <vector>
#include <memory>

#include "ccu_res_repo.h"
#include "ccu_rep_block_v1.h"
#include "ccu_context_resource_v1.h"

namespace hcomm {
namespace CcuRep {

constexpr uint16_t FUNC_ARG_MAX            = 32;
constexpr uint16_t FUNC_NEST_MAX           = 8;
constexpr uint16_t FUNC_CALL_LAYER_INVALID = 0xFFFF;

class CcuRepReferenceManager {
public:
    explicit CcuRepReferenceManager(uint8_t deiId);
    static CcuResReq             GetResReq(uint8_t reqDieId);
    void                         GetRes(CcuRepResource &res);
    std::shared_ptr<CcuRepBlock> GetRefBlock(const std::string &label);
    void                         SetRefBlock(const std::string &label, std::shared_ptr<CcuRepBlock> refBlock);
    uint16_t                     GetFuncAddr(const std::string &label);
    const Variable              &GetFuncCall();
    const Variable              &GetFuncRet(uint16_t callLayer);
    const std::vector<Variable> &GetFuncIn();
    const std::vector<Variable> &GetFuncOut();
    void                         Dump() const;
    void                         ClearRepReference();

private:
    bool CheckValid(const std::string &label);
    bool CheckUnique(const std::string &label);

private:
    uint8_t                                                       dieId{0};
    std::unordered_map<std::string, std::shared_ptr<CcuRepBlock>> referenceMap;
    std::vector<Variable>                                         funcCallVar;
    std::vector<Variable>                                         funcInVar;
    std::vector<Variable>                                         funcOutVar;
};

}; // namespace CcuRep
}; // namespace hcomm

#endif // _CCU_REP_REFERENCE_MANAGER_H
