/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_IF_LABEL_STACK_HPP
#define CCU_IF_LABEL_STACK_HPP

struct CcuIfLabelStack {
    static constexpr int MAX_DEPTH = 16;
    static const char **Labels() {
        static thread_local const char *labels[MAX_DEPTH]{};
        return labels;
    }
    static int &Top() {
        static thread_local int top = 0;
        return top;
    }
    static void Push(const char *label) { Labels()[Top()++] = label; }
    static const char *Pop() { return Labels()[--Top()]; }
};

#endif // CCU_IF_LABEL_STACK_HPP
