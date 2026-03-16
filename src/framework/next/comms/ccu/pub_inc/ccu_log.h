/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_LOG_H
#define CCU_LOG_H

#include "log.h"

// todo: 需要适配资源不足

#define HCCL_TO_CCU_RET(ret) static_cast<CcuResult>(ret)
#define CCU_TO_HCCL_RET(ret) \
    (static_cast<int>(ret) <= static_cast<int>(HCCL_E_RESERVED) ? static_cast<HcclResult>(ret) : HCCL_E_INTERNAL)

/* 检查函数返回值, 并返回指定错误码 */
#define CCU_CHK_RET(call)                                 \
    do {                                              \
        CcuResult ccuRet = HCCL_TO_CCU_RET(call);                        \
        if (UNLIKELY(ccuRet != CCU_SUCCESS)) {                    \
            if (ccuRet == CCU_E_AGAIN) {                \
                HCCL_WARNING("[%s]call trace: ccuRet -> %d", __func__, ccuRet); \
            } else {                                  \
                HCCL_ERROR("[%s]call trace: ccuRet -> %d", __func__, ccuRet); \
            }                                         \
            return ccuRet;                               \
        }                                             \
    } while (0)

#define CCU_CHK_PTR_NULL(ptr)                                                                               \
    do {                                                                                                           \
        if (UNLIKELY((ptr) == nullptr)) {                  \
            HCCL_ERROR("[%s]errNo[0x%016llx]ptr [%s] is nullptr, return HCCL_E_PTR", \
            __func__, HCCL_ERROR_CODE(HCCL_E_PTR), #ptr); \
            return CCU_E_PTR;                                                                                     \
        }                                                                                                          \
    } while (0)


#endif // CCU_LOG_H