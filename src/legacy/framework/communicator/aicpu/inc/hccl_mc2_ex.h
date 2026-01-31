/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HCCL_MC2_EX_H
#define HCCL_MC2_EX_H

#include "mc2_data_type.h"
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

HcclResult HcclGetCommHandleByCtx(void *ctx, void **opHandle);
HcclResult HcclReleaseComm(void* opHandle);
HcclResult HcclGetTaskStatus(void* opHandle, HcclTaskStatus *status);
HcclResult HcclCheckFinishByStream(void* opHandle);
HcclResult HcclPrintTaskExceptionAllComm(void* opHandle);
HcclResult HcclLaunchCcoreWait(void* opHandle, uint64_t waitAddr, uint32_t turnNum, uint64_t turnNumAddr, bool isLast);
HcclResult HcclLaunchCcorePost(void* opHandle, uint64_t recordAddr, uint32_t turnNum, uint64_t turnNumAddr);
HcclResult HcclLaunchOp(void* opHandle, HcclOpData* data);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCCL_MC2_EX_H
