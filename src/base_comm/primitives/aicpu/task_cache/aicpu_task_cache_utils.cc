/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_task_cache_utils.h"

#include "log.h"

namespace hcomm {

HcclResult AicpuTaskCacheUtils::DumpSqeContent(const uint8_t* sqePtr)
{
    // TODO: AR5 打印task
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheUtils::DumpWqeContent(const uint8_t* wqePtr)
{
    // TODO: AR5 打印task
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheUtils::DumpSqeHeader_(const Rt91095StarsSqeHeader& sqeHeader)
{
    // TODO: AR5 打印task
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheUtils::DumpWqeHeader_(const UdmaSqeCommon& wqeHeader)
{
    // TODO: AR5 打印task
    return HCCL_SUCCESS;
}

} // namespace hcomm
