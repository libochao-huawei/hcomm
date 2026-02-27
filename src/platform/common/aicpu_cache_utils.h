/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __AICPU_CACHE_UTILS_H__
#define __AICPU_CACHE_UTILS_H__

#include "aicpu_hccl_sqcqv2.h"
#include "hccl_types.h"
#include "rt_external_stars_define.h"

// 确认ptr应该为空
#define CHK_PTR_NOTNULL(ptr) \
    do { \
        if (UNLIKELY((ptr) != nullptr)) { \
            HCCL_ERROR("[%s] errNo[0x%016llx] ptr[%s] is 0x%016llx (should be null), return HCCL_E_INTERNAL", \
                __func__, HCCL_ERROR_CODE(HCCL_E_INTERNAL), #ptr, (ptr)); \
            return HCCL_E_INTERNAL; \
        } \
    } while (0)

// 确认ptrPtr不应该为空, 但*ptrPtr应该为空
#define CHK_PTRPTR_NULL(ptrPtr) \
    do { \
        CHK_PTR_NULL(ptrPtr); \
        CHK_PTR_NOTNULL(*(ptrPtr)); \
    } while (0)

namespace hccl {

enum AsyncUnfoldStage {
    NORMAL_UNFOLD = 0,
    ASYNC_UNFOLD_GENERATE = 1, // 异步展开生成阶段 (当前算子提前生成下一算子的SQE)
    ASYNC_UNFOLD_APPLY = 2, // 异步展开应用阶段 (下一算子加载异步展开结果, 下发执行)
};

class AicpuCacheUtils {
public:
    // 只会在DEBUG_LEVEL下打印SQE内容 (通过比较打印算子正常展开的SQE与缓存的SQE, 判断刷新后的SQE是否正确)
    static HcclResult DumpSqeContent(const uint8_t *sqePtr, const uint8_t sqeType);
private:
    // 只会在DEBUG_LEVEL下打印SQE header的内容
    static HcclResult DumpSqeHeader(const rtStarsSqeHeader_t& sqeHeader);
    static HcclResult DumpSqeHeader(const rtStarsSqeHeaderV2_t& sqeHeader);
};

} // namespace hccl

#endif // __AICPU_CACHE_UTILS_H__