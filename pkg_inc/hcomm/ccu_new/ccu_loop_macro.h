/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_LOOP_MACRO_H
#define CCU_LOOP_MACRO_H

#include "ccu_types.h"

extern "C" CcuResult CcuLoopCreate(CcuLoopHandle *loop);
extern "C" CcuResult _CcuLoopBodyEnter(CcuLoopHandle loop);
extern "C" CcuResult _CcuLoopBodyExit(CcuLoopHandle loop);

class CcuLoopBodyScope {
public:
    explicit CcuLoopBodyScope(CcuLoopHandle loop) : loop_(loop) {
        _CcuLoopBodyEnter(loop_);
    }
    ~CcuLoopBodyScope() {
        _CcuLoopBodyExit(loop_);
    }
    bool Once() { return first_ ? (first_ = false, true) : false; }
private:
    CcuLoopHandle loop_;
    bool first_{true};
};

#define CCU_LOOPBODY(loopVar) \
    CCU_CHK_RET(CcuLoopCreate(&loopVar)); \
    _CCU_LB_SCOPE_H1(__COUNTER__, loopVar)

#define _CCU_LB_SCOPE_H1(c, l)  _CCU_LB_SCOPE_H2(c, l)
#define _CCU_LB_SCOPE_H2(c, l) \
    for (CcuLoopBodyScope _ccuLB##c(l); _ccuLB##c.Once(); )

#endif // CCU_LOOP_MACRO_H
