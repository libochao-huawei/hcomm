/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

    // 等价于老 CcuRepLoopGroup::GetStartLoopInstrId：第一条 LoopInstr 的 instrId
    // 仅在 Translate 之后、AddLoop 全部完成后调用有意义
    uint16_t GetStartLoopInstrId() const;
    const Variable &GetOffsetParam() const { return offsetVar_; }

private:
    // LoopGroupInstr 在 Bundle 内相对 Bundle 起始的偏移
    // = (config-based loop 1 条 LoadImd) + (var-based loop 2 条 LoadImd+LoadXX)
    //   + (group config-based 时 parallel/offset 2 条)
    uint16_t LoopGroupInstrOffsetInBundle() const;

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
