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
 * CCU_WHILE — while loop (condition-first):
 *
 *   CCU_WHILE(counter != limit) {
 *       accumulator = accumulator + step;
 *       counter = counter + one;
 *   }
 *
 * CCU_DO / CCU_WHILE — do-while loop (body-first, condition-last):
 *
 *   CCU_DO {
 *       accumulator = accumulator + step;
 *       counter = counter + one;
 *   } CCU_WHILE(counter != limit);
 *
 * CCU_WHILE is polymorphic: when preceded by CCU_DO it acts as the
 * closing condition of a do-while; otherwise it is a standard while.
 * The dispatch is done at runtime via the do-while TLS label stack
 * (_CcuDoWhileStackPopForWhile).
 *
 * CCU_DO_WHILE — retained as compatibility alias for CCU_DO + CCU_WHILE.
 *
 * CCU_IF / CCU_ELSE — unified interface. CCU_ELSE is optional:
 *
 *   // With else:
 *   CCU_IF(var == expected) {
 *       // then-block
 *   } CCU_ELSE {
 *       // else-block
 *   }
 *
 *   // Without else (previously required CCU_IF_ONLY):
 *   CCU_IF(var == expected) {
 *       // then-block
 *   }
 *
 * CCU_IF_ONLY — retained as compatibility alias for CCU_IF.
 * --------------------------------------------------------------------------- */

#define CCU_CONCAT_INNER(a, b) a##b
#define CCU_CONCAT(a, b)       CCU_CONCAT_INNER(a, b)
#define CCU_STRINGIFY_INNER(x) #x
#define CCU_STRINGIFY(x)       CCU_STRINGIFY_INNER(x)

/**
 * CCU_WHILE — polymorphic while macro.
 *
 * Normal while mode (no preceding CCU_DO):
 *   Wraps CcuWhileBegin / CcuWhileEnd around the brace block that follows.
 *
 * Do-while mode (preceded by CCU_DO):
 *   The body has already been executed inside CCU_DO { ... }.
 *   CCU_WHILE pops the pending do-while label from TLS and emits
 *   CcuDoWhileEnd. The for-loop body is empty (terminated by ';').
 *
 * Dispatch is done at runtime: a non-null result from
 * _CcuDoWhileStackPopForWhile() means do-while mode.
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
    for (const char *uid##_dwLbl = _CcuDoWhileStackPopForWhile(),           \
             *uid##_sen = (const char *)1;                                  \
         uid##_sen != nullptr;                                              \
         uid##_sen = nullptr)                                               \
    for (int uid##_rc = (uid##_dwLbl != nullptr)                            \
                 ? (int)CCU_SUCCESS                                         \
                 : (int)CcuWhileBegin(uid##_ce.var->handle,                 \
                       uid##_ce.imm, uid##_ce.cond, CCU_STRINGIFY(uid)),    \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             uid##_rc = (uid##_dwLbl != nullptr)                            \
                 ? (int)CcuDoWhileEnd(uid##_ce.var->handle,                 \
                       uid##_ce.imm, uid##_ce.cond, uid##_dwLbl)            \
                 : (int)CcuWhileEnd(CCU_STRINGIFY(uid)))

/**
 * CCU_IF — unified if macro. Uses delayed-close strategy:
 *   - On entry: emits CcuIfBegin first (which may flush prior BodyDone
 *     entries via Append), then pushes label to the if TLS label stack
 *     (state = InBody) via _CcuIfStackPush.
 *   - On body completion (inner for-loop increment): marks stack top
 *     as BodyDone. The actual CcuIfEnd is NOT emitted here.
 *   - Closure happens in one of three ways:
 *     (a) A subsequent CCU_ELSE pops the entry and takes over;
 *     (b) Any subsequent CcuKernel::Append auto-flushes BodyDone entries;
 *     (c) Kernel finalize flushes any remaining entries.
 *
 * CCU_ELSE is optional. If omitted, the if is closed automatically.
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
    for (int uid##_rc =                                                     \
             (int)CcuIfBegin(uid##_ce.var->handle, uid##_ce.imm,            \
                 uid##_ce.cond, CCU_STRINGIFY(uid)),                        \
             uid##_done = (_CcuIfStackPush(CCU_STRINGIFY(uid)), 0);         \
         uid##_rc == (int)CCU_SUCCESS && uid##_done == 0;                   \
         uid##_done = 1,                                                    \
             ((void)CcuFlushPendingIfs(),                                   \
              _CcuIfStackMarkBodyDone(), (void)0))

#define CCU_ELSE                                                            \
    CCU_ELSE_EXPAND(CCU_CONCAT(__ccu_el_, __COUNTER__))

#define CCU_ELSE_EXPAND(uid)                                                \
    CCU_ELSE_IMPL(uid)

#define CCU_ELSE_IMPL(uid)                                                  \
    for (const char *uid##_lbl = _CcuIfStackPopForElse(),                   \
             *uid##_sen = uid##_lbl;                                        \
         uid##_sen != nullptr;                                              \
         uid##_sen = nullptr)                                               \
    for (int uid##_rc = (int)CcuIfElse(uid##_lbl),                          \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             uid##_rc = (int)CcuIfEnd(uid##_lbl))


/**
 * CCU_DO — opens a do-while block. Must be closed by CCU_WHILE(expr);
 *
 *   CCU_DO {
 *       // body (always executes at least once)
 *   } CCU_WHILE(counter != limit);
 *
 * Internally: calls CcuDoWhileBegin on entry. The label is pushed onto
 * the do-while TLS label stack (_CcuDoWhileStackPush) only AFTER the
 * body finishes (in the for-loop step expression). This deferred push
 * prevents any CCU_WHILE inside the body from incorrectly consuming the
 * pending label and being misidentified as the closing condition. The
 * subsequent CCU_WHILE pops the label and calls CcuDoWhileEnd instead
 * of CcuWhileBegin/CcuWhileEnd.
 */
#define CCU_DO                                                              \
    CCU_DO_EXPAND(CCU_CONCAT(__ccu_dw_, __COUNTER__))

#define CCU_DO_EXPAND(uid)                                                  \
    CCU_DO_IMPL(uid)

#define CCU_DO_IMPL(uid)                                                    \
    for (int uid##_rc = (int)CcuDoWhileBegin(CCU_STRINGIFY(uid)),           \
             uid##_done = 0;                                                \
         uid##_rc == (int)CCU_SUCCESS && !uid##_done;                       \
         uid##_done = 1,                                                    \
             _CcuDoWhileStackPush(CCU_STRINGIFY(uid)))


#endif // CCU_CONTROL_FLOW_MACRO_H
