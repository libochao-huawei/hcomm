/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ccu representation loopgroup bundle header file
 * Create: 2026-03-22
 */

#ifndef HCOMM_CCU_REPRESENTATION_LOOPGROUP_BUNDLE_H
#define HCOMM_CCU_REPRESENTATION_LOOPGROUP_BUNDLE_H

#include <vector>
#include "ccu_types.h"
#include "ccu_datatype_v1.h"
#include "ccu_rep_base_v1.h"
#include "ccu_rep_loopblock_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepLoopGroupBundle : public CcuRepBase {
public:
    struct LoopEntry {
        CcuLoopConfig config;
        uint16_t executorId{0};
        std::shared_ptr<CcuRepLoopBlock> repLoopBlock;
        Variable loopParamVar;
        bool isVarBased{false};
    };

    CcuRepLoopGroupBundle(const CcuLoopGroupConfig &config,
                          const Variable &parallelVar, const Variable &offsetVar);
    CcuRepLoopGroupBundle(const Variable &parallelVar, const Variable &offsetVar);

    void AddLoop(const LoopEntry &entry);
    void SetRepeatLoopIdx(uint64_t idx) { repeatLoopIdx_ = idx; }
    void SetTotalLoopNum(uint64_t num) { totalLoopNum_ = num; }

    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    uint16_t    InstrCount() override;
    std::string Describe() override;

private:
    CcuLoopGroupConfig config_;
    Variable parallelVar_;
    Variable offsetVar_;
    uint64_t repeatLoopIdx_{0};
    uint64_t totalLoopNum_{0};
    std::vector<LoopEntry> loops_;
    bool isGroupVarBased_{false};
};

}; // namespace CcuRep
}; // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_LOOPGROUP_BUNDLE_H
