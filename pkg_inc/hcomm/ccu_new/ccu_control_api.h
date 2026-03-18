/**
<<<<<<< HEAD
<<<<<<< HEAD
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
=======
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
>>>>>>> 1e05e06b (bugfix(ccu): fix compile bugs)
=======
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
>>>>>>> 17cd52f8 (bugfix(ccu): ccu c style fix llt example)
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

<<<<<<< HEAD
<<<<<<< HEAD
#ifndef CCU_CONTROL_API_H
#define CCU_CONTROL_API_H

#include "hccl_types.h"
#include "ccu_types.h"
#include "hccl_res.h"
=======
#ifndef HCCL_CCU_CONTROL_API_H
#define HCCL_CCU_CONTROL_API_H
=======
#ifndef CCU_CONTROL_API_H
#define CCU_CONTROL_API_H
>>>>>>> 17cd52f8 (bugfix(ccu): ccu c style fix llt example)

#include "ccu_types.h"
>>>>>>> 1e05e06b (bugfix(ccu): fix compile bugs)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

<<<<<<< HEAD
extern HcclResult HcclCommQueryCcuIns(HcclComm comm,
    CcuInsHandle *insHandles, uint32_t *insNum);

=======
>>>>>>> 1e05e06b (bugfix(ccu): fix compile bugs)
extern CcuResult HcommCcuKernelRegisterStart(CcuInsHandle insHandle);

extern CcuResult HcommCcuKernelRegister(CcuInsHandle insHandle,
    char *kernelFuncName, void *kernelFunc, void *kernelArg,
    CcuKernelHandle *kernelHandle);

extern CcuResult HcommCcuKernelRegisterEnd(CcuInsHandle insHandle);

<<<<<<< HEAD
extern CcuResult HcommCcuKernelLaunch(ThreadHandle threadHandle,
    CcuKernelHandle kernelHandle, void *taskArgs, uint32_t argSize);

=======
>>>>>>> 1e05e06b (bugfix(ccu): fix compile bugs)
#ifdef __cplusplus
}
#endif // __cplusplus

<<<<<<< HEAD
<<<<<<< HEAD
#endif // CCU_CONTROL_API_H
=======
#endif
>>>>>>> 1e05e06b (bugfix(ccu): fix compile bugs)
=======
#endif // CCU_CONTROL_API_H
>>>>>>> 17cd52f8 (bugfix(ccu): ccu c style fix llt example)
