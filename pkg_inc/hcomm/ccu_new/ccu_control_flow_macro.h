/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_CONTROL_FLOW_MACRO_H
#define CCU_CONTROL_FLOW_MACRO_H

#include "ccu_variable.hpp"
#include "ccu_if_label_stack.hpp"
#include "ccu_data_api_impl.h"

/* ---------------------------------------------------------------------------
 * Macro wrappers for structured control flow
 * ---------------------------------------------------------------------------
 *
 * Condition expressions use C++ operator overloads on CcuVariable:
 *
 *   counter != limit   →  CcuCondExpr{&counter, limit, CCU_CONDITION_NE}
 *   var == expected     →  CcuCondExpr{&var, expected, CCU_CONDITION_EQ}
 *
 * CCU_WHILE — single brace block, label auto-generated:
 *
 *   CCU_WHILE(counter != limit) {
 *       accumulator = accumulator + step;
 *       counter = counter + one;
 *   }
 *
 * CCU_DO_WHILE — body executes at least once, then condition is checked:
 *
 *   CCU_DO_WHILE(counter != limit) {
 *       accumulator = accumulator + step;
 *       counter = counter + one;
 *   }
 *
 * CCU_IF / CCU_ELSE — label auto-generated, passed via TLS stack:
 *
 *   CCU_IF(var == expected) {
 *       // then-block
 *   } CCU_ELSE {
 *       // else-block
 *   }
 *
 * CCU_IF_ONLY — if-without-else, self-closing (no CCU_ELSE needed):
 *
 *   CCU_IF_ONLY(var == expected) {
 *       // then-block
 *   }
 * --------------------------------------------------------------------------- */

#define CCU_CONCAT_INNER(a, b) a##b
#define CCU_CONCAT(a, b)       CCU_CONCAT_INNER(a, b)
#define CCU_STRINGIFY_INNER(x) #x
#define CCU_STRINGIFY(x)       CCU_STRINGIFY_INNER(x)

/**
 * CCU_WHILE — wraps CcuWhileBegin / CcuWhileEnd around a brace block.
 * Accepts a CcuCondExpr produced by operator== / operator!= on CcuVariable.
 * Label is auto-generated via __COUNTER__ so the user never sees it.
 */
#define CCU_WHILE(expr)                                                     \
    CCU_WHILE_EXPAND(expr, CCU_CONCAT(__ccu_wh_, __COUNTER__))

#define CCU_WHILE_EXPAND(expr, uid)                                         \
    CCU_WHILE_IMPL(expr, uid)

#define CCU_WHILE_IMPL(expr, uid)                                           \
    for (CcuCondExpr uid##_ce = (expr),                                     \
             *uid##_p = &uid##_ce;                                          \
         uid##_p != nullptr;                                                \
         uid##_p = nullptr)                                                 \
    for (int uid##_rc = (int)CcuWhileBegin(uid##_ce.var->handle,            \
                 uid##_ce.imm, uid##_ce.cond, CCU_STRINGIFY(uid)),          \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             uid##_rc = (int)CcuWhileEnd(CCU_STRINGIFY(uid)))

/**
 * CCU_IF / CCU_ELSE — label is auto-generated and passed between the two
 * macros via CcuIfLabelStack (thread-local). CCU_IF pushes the label;
 * CCU_ELSE pops it.
 *
 *   CCU_IF(var == expected) { ... } CCU_ELSE { ... }
 */
#define CCU_IF(expr)                                                        \
    CCU_IF_EXPAND(expr, CCU_CONCAT(__ccu_if_, __COUNTER__))

#define CCU_IF_EXPAND(expr, uid)                                            \
    CCU_IF_IMPL(expr, uid)

#define CCU_IF_IMPL(expr, uid)                                              \
    for (CcuCondExpr uid##_ce = (expr),                                     \
             *uid##_p = &uid##_ce;                                          \
         uid##_p != nullptr;                                                \
         uid##_p = nullptr)                                                 \
    for (int uid##_push = (CcuIfLabelStack::Push(CCU_STRINGIFY(uid)), 0),   \
             uid##_rc = (int)CcuIfBegin(uid##_ce.var->handle,               \
                 uid##_ce.imm, uid##_ce.cond, CCU_STRINGIFY(uid));          \
         uid##_rc == (int)CCU_SUCCESS && uid##_push == 0;                   \
         uid##_push = 1)

#define CCU_ELSE                                                            \
    CCU_ELSE_EXPAND(CCU_CONCAT(__ccu_el_, __COUNTER__))

#define CCU_ELSE_EXPAND(uid)                                                \
    CCU_ELSE_IMPL(uid)

#define CCU_ELSE_IMPL(uid)                                                  \
    for (const char *uid##_lbl = CcuIfLabelStack::Pop(),                    \
             *uid##_end = uid##_lbl;                                        \
         uid##_end != nullptr;                                              \
         uid##_end = nullptr)                                               \
    for (int uid##_rc = (int)CcuIfElse(uid##_lbl),                          \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             uid##_rc = (int)CcuIfEnd(uid##_lbl))

/**
 * CCU_IF_ONLY — if-without-else, self-closing. Wraps CcuIfBegin, the user
 * body, CcuIfElse, and CcuIfEnd in a single macro expansion. No CCU_ELSE
 * is needed.
 *
 *   CCU_IF_ONLY(var == expected) { ... }
 */
#define CCU_IF_ONLY(expr)                                                   \
    CCU_IF_ONLY_EXPAND(expr, CCU_CONCAT(__ccu_io_, __COUNTER__))

#define CCU_IF_ONLY_EXPAND(expr, uid)                                       \
    CCU_IF_ONLY_IMPL(expr, uid)

#define CCU_IF_ONLY_IMPL(expr, uid)                                         \
    for (CcuCondExpr uid##_ce = (expr),                                     \
             *uid##_p = &uid##_ce;                                          \
         uid##_p != nullptr;                                                \
         uid##_p = nullptr)                                                 \
    for (int uid##_rc = (int)CcuIfBegin(uid##_ce.var->handle,               \
                 uid##_ce.imm, uid##_ce.cond, CCU_STRINGIFY(uid)),          \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             uid##_rc = (int)CcuIfEnd(CCU_STRINGIFY(uid)))

/**
 * CCU_DO_WHILE — wraps CcuDoWhileBegin / CcuDoWhileEnd around a brace block.
 * Accepts a CcuCondExpr. The body is always executed once, then the condition
 * is checked to decide whether to loop back.
 * Label is auto-generated via __COUNTER__.
 */
#define CCU_DO_WHILE(expr)                                                  \
    CCU_DO_WHILE_EXPAND(expr, CCU_CONCAT(__ccu_dw_, __COUNTER__))

#define CCU_DO_WHILE_EXPAND(expr, uid)                                      \
    CCU_DO_WHILE_IMPL(expr, uid)

#define CCU_DO_WHILE_IMPL(expr, uid)                                        \
    for (CcuCondExpr uid##_ce = (expr),                                     \
             *uid##_p = &uid##_ce;                                          \
         uid##_p != nullptr;                                                \
         uid##_p = nullptr)                                                 \
    for (int uid##_rc = (int)CcuDoWhileBegin(CCU_STRINGIFY(uid)),           \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             uid##_rc = (int)CcuDoWhileEnd(uid##_ce.var->handle,            \
                 uid##_ce.imm, uid##_ce.cond, CCU_STRINGIFY(uid)))

#endif // CCU_CONTROL_FLOW_MACRO_H
