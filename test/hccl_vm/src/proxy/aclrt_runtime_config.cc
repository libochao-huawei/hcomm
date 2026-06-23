/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <atomic>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <unistd.h>
#include <vector>

#include "acl/acl_base.h"
#include "acl/acl_rt.h"
#include "sim_log.h"
#include "runtime/base.h"
#include "runtime/event.h"
#include "db_sim_runner_common.h"
#include "db_sim_runner_ops.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

aclError aclrtSetSysParamOpt(aclSysParamOpt opt, int64_t value)
{
    (void) opt;
    (void) value;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtGetSysParamOpt(aclSysParamOpt opt, int64_t *value)
{
    (void) opt;
    (void) value;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtGetDeviceResLimit(int32_t deviceId, aclrtDevResLimitType type, uint32_t* value)
{
    (void) deviceId;
    (void) type;
    (void) value;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtSetDeviceResLimit(int32_t deviceId, aclrtDevResLimitType type, uint32_t value)
{
    (void) deviceId;
    (void) type;
    (void) value;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtResetDeviceResLimit(int32_t deviceId)
{
    (void) deviceId;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtGetStreamResLimit(aclrtStream stream, aclrtDevResLimitType type, uint32_t *value)
{
    (void) stream;
    (void) type;
    (void) value;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtSetStreamResLimit(aclrtStream stream, aclrtDevResLimitType type, uint32_t value)
{
    (void) stream;
    (void) type;
    (void) value;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtResetStreamResLimit(aclrtStream stream)
{
    (void) stream;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtUseStreamResInCurrentThread(aclrtStream stream)
{
    (void) stream;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtUnuseStreamResInCurrentThread(aclrtStream stream)
{
    (void) stream;
    HCCL_VM_WARN("[{}] not support", __func__);
    return ACL_SUCCESS;
}

aclError aclrtGetResInCurrentThread(aclrtDevResLimitType type, uint32_t *value)
{
    (void)type;
    *value = 48;
    return ACL_SUCCESS;
}

#ifdef __cplusplus
}
#endif  // __cplusplus
