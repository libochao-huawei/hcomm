/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

<<<<<<<< HEAD:pkg_inc/hcomm/ccu_new/ccu_control_api.h
#ifndef CCU_CONTROL_API_H
#define CCU_CONTROL_API_H

#include "hccl_types.h"
#include "ccu_types.h"
#include "hccl_res.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern HcclResult HcclCommQueryCcuIns(HcclComm comm,
    CcuInsHandle *insHandles, uint32_t *insNum);

extern CcuResult HcommCcuKernelRegisterStart(CcuInsHandle insHandle);

extern CcuResult HcommCcuKernelRegister(CcuInsHandle insHandle,
    char *kernelFuncName, void *kernelFunc, void *kernelArg,
    CcuKernelHandle *kernelHandle);

extern CcuResult HcommCcuKernelRegisterEnd(CcuInsHandle insHandle);

extern CcuResult HcommCcuKernelLaunch(ThreadHandle threadHandle,
    CcuKernelHandle kernelHandle, void *taskArgs, uint32_t argSize);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CCU_CONTROL_API_H
========
#include "gtest/gtest.h"
#include "comm.h"
#include "env_config.h"

GTEST_API_ int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
    return RUN_ALL_TESTS();
}
>>>>>>>> origin/ccu_c:test/ut/platform/task/main.cc
