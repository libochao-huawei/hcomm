/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "inc/hccl_mc2_ex.h"
#include "inc/aicpu_mc2_handler.h"
#include "log.h"

using namespace Hccl;
HcclResult HcclGetCommHandleByCtx(void *ctx, void **opHandle)
{
    if (ctx == nullptr) {
        HCCL_ERROR("[HcclGetCommHandleByCtx]Args ctx is nullptr.");
        return HCCL_E_PTR;
    }
    if (opHandle == nullptr) {
        HCCL_ERROR("[HcclGetCommHandleByCtx]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclGetCommHandleByCtx(ctx, opHandle)));
    return HCCL_SUCCESS;
}

HcclResult HcclReleaseComm(void *opHandle)
{
    if (opHandle == nullptr) {
        HCCL_WARNING("[HcclReleaseComm] opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclReleaseComm(opHandle)));
    return HCCL_SUCCESS;
}

HcclResult HcclGetTaskStatus(void *opHandle, HcclTaskStatus *status)
{
    if (opHandle == nullptr) {
        HCCL_ERROR("[HcclGetTaskStatus]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    if (status == nullptr) {
        HCCL_ERROR("[HcclGetTaskStatus]Args status is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclGetTaskStatus(opHandle, status)));
    return HCCL_SUCCESS;
}

HcclResult HcclCheckFinishByStream(void *opHandle)
{    
    if (opHandle == nullptr) {
        HCCL_ERROR("[HcclCheckFinishByStream]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclCheckFinishByStream(opHandle)));
    return HCCL_SUCCESS;
}

HcclResult HcclPrintTaskExceptionAllComm(void *opHandle)
{
    if (opHandle == nullptr) {
        HCCL_WARNING("[HcclPrintTaskExceptionAllComm]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclPrintTaskExceptionAllComm(opHandle)));
    return HCCL_SUCCESS;
}

HcclResult HcclLaunchCcoreWait(void *opHandle, uint64_t waitAddr, uint32_t turnNum, uint64_t turnNumAddr, bool isLast)
{
    if (opHandle == nullptr) {
        HCCL_ERROR("[HcclLaunchCcoreWait]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclLaunchCcoreWait(opHandle, waitAddr, turnNum, turnNumAddr, isLast)));
    return HCCL_SUCCESS;
}

HcclResult HcclLaunchCcorePost(void *opHandle, uint64_t recordAddr, uint32_t turnNum, uint64_t turnNumAddr)
{
    if (opHandle == nullptr) {
        HCCL_ERROR("[HcclLaunchCcorePost]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclLaunchCcorePost(opHandle, recordAddr, turnNum, turnNumAddr)));
    return HCCL_SUCCESS;
}

HcclResult HcclLaunchOp(void *opHandle, HcclOpData *data)
{
    if (opHandle == nullptr) {
        HCCL_ERROR("[HcclLaunchOp]Args opHandle is nullptr.");
        return HCCL_E_PTR;
    }
    if (data == nullptr) {
        HCCL_ERROR("[HcclLaunchOp]Args data is nullptr.");
        return HCCL_E_PTR;
    }
    TRY_CATCH_RETURN(CHK_RET(AicpuMc2Handler::GetInstance().HcclLaunchOp(opHandle, data)));
    return HCCL_SUCCESS;
}