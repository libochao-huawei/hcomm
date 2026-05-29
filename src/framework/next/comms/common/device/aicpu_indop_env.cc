/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aicpu_indop_env.h"
#include "aicpu_init_param.h"

namespace hcomm {
<<<<<<< HEAD
static DevAicpuCommConfig g_configEnv;
=======
static AicpuEnvConfig g_configEnv;
>>>>>>> beta2

void SetTaskExceptionEnable(bool taskExceptionEnable)
{
    g_configEnv.taskExceptionEnable = taskExceptionEnable;
    HCCL_INFO("[%s] taskExceptionEnable[%d]", __func__, taskExceptionEnable);
}

void SetNotifyWaitTimeout(u32 notifyWaitTimeout)
{
    g_configEnv.notifyWaitTimeout = notifyWaitTimeout;
    HCCL_INFO("[%s] taskExceptionEnable[%d]", __func__, notifyWaitTimeout);
}

const bool& GetTaskExceptionEnable()
{
    return g_configEnv.taskExceptionEnable;
}

const u32& GetNotifyWaitTimeout()
{
    return g_configEnv.notifyWaitTimeout;
}

}  // namespace hcomm
