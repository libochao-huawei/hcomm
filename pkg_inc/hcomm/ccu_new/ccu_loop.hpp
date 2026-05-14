/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_LOOP_HPP
#define CCU_LOOP_HPP

#include <vector>

#include "ccu_types.h"
#include "ccu_variable.hpp"
#include "ccu_func.hpp"
#include "ccu_data_api_impl.h"

namespace ccu {

namespace internal {

inline void ThrowIfLoopErr(::CcuResult ret, const char *what)
{
    if (ret != ::CcuResult::CCU_SUCCESS) {
        throw what;
    }
}

} // namespace internal

class Loop {
public:
    Loop(Variable &loopCfg, Func &func)
    {
        ComposeLoopBody(func);
        isVarBased_ = true;
        loopParamVar_ = &loopCfg;
    }

    Loop(CcuLoopConfig &loopCfg, Func &func)
    {
        ComposeLoopBody(func);
        isVarBased_ = false;
        config_ = loopCfg;
    }

    CcuLoop Handle() const
    {
        return handle_;
    }

    bool IsVarBased() const
    {
        return isVarBased_;
    }

    Variable *LoopParamVar() const
    {
        return loopParamVar_;
    }

    const CcuLoopConfig *Config() const
    {
        return &config_;
    }

private:
    void ComposeLoopBody(Func &func)
    {
        if (func.NumIn() != 0) {
            throw "ccu::Loop requires a no-argument ccu::Func";
        }
        internal::ThrowIfLoopErr(::CcuLoopCreate(&handle_), "CcuLoopCreate failed");
        internal::ThrowIfLoopErr(::_CcuLoopBodyEnter(handle_), "_CcuLoopBodyEnter failed");
        try {
            func.RunBody(nullptr);
        } catch (...) {
            (void)::_CcuLoopBodyExit(handle_);
            throw;
        }
        internal::ThrowIfLoopErr(::_CcuLoopBodyExit(handle_), "_CcuLoopBodyExit failed");
    }

    CcuLoop handle_{0};
    bool isVarBased_{false};
    Variable *loopParamVar_{nullptr};
    CcuLoopConfig config_{};
};

class LoopGroup {
public:
    LoopGroup(Variable &parallelCfg, Variable &offsetCfg, const std::vector<Loop> &loops)
    {
        internal::ThrowIfLoopErr(
            ::CcuLoopGroupCreateFromVar(&handle_, parallelCfg.handle, offsetCfg.handle),
            "CcuLoopGroupCreateFromVar failed");
        AddLoops(loops);
    }

    LoopGroup(CcuLoopGroupConfig &loopGroupCfg, const std::vector<Loop> &loops)
    {
        internal::ThrowIfLoopErr(
            ::CcuLoopGroupCreate(&handle_, &loopGroupCfg),
            "CcuLoopGroupCreate failed");
        AddLoops(loops);
    }

    CcuLoopGroup Handle() const
    {
        return handle_;
    }

private:
    void AddLoops(const std::vector<Loop> &loops)
    {
        for (const auto &loop : loops) {
            if (loop.IsVarBased()) {
                auto *loopParamVar = loop.LoopParamVar();
                if (loopParamVar == nullptr) {
                    throw "ccu::Loop var-based loop has null loop parameter";
                }
                internal::ThrowIfLoopErr(
                    ::CcuLoopGroupAddLoopFromVar(handle_, loop.Handle(), loopParamVar->handle),
                    "CcuLoopGroupAddLoopFromVar failed");
            } else {
                internal::ThrowIfLoopErr(
                    ::CcuLoopGroupAddLoop(handle_, loop.Handle(), loop.Config()),
                    "CcuLoopGroupAddLoop failed");
            }
        }
    }

    CcuLoopGroup handle_{0};
};

} // namespace ccu

#endif // CCU_LOOP_HPP
