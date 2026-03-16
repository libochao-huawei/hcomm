/**
<<<<<<< HEAD
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
=======
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
>>>>>>> 9168438f (bugfix(ccu): fix compile bugs)
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

<<<<<<< HEAD
#ifndef CCU_CONTROL_API_H
#define CCU_CONTROL_API_H

#include "ccu_types.h"
#include "hccl_res.h"
=======
#ifndef HCCL_CCU_CONTROL_API_H
#define HCCL_CCU_CONTROL_API_H

#include "ccu_types.h"
>>>>>>> 9168438f (bugfix(ccu): fix compile bugs)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern CcuResult HcommCcuKernelRegisterStart(CcuInsHandle insHandle);

extern CcuResult HcommCcuKernelRegister(CcuInsHandle insHandle,
    char *kernelFuncName, void *kernelFunc, void *kernelArg,
    CcuKernelHandle *kernelHandle);

extern CcuResult HcommCcuKernelRegisterEnd(CcuInsHandle insHandle);

<<<<<<< HEAD
extern CcuResult HcommCcuKernelLaunch(ThreadHandle threadHandle,
    CcuKernelHandle kernelHandle, void *taskArgs, uint32_t argSize);

=======
>>>>>>> 9168438f (bugfix(ccu): fix compile bugs)
#ifdef __cplusplus
}
#endif // __cplusplus

<<<<<<< HEAD
#endif // CCU_CONTROL_API_H
=======
#endif
>>>>>>> 9168438f (bugfix(ccu): fix compile bugs)
