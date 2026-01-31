/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HCCLV2_AICPU_MC2_HANDLER_H
#define HCCLV2_AICPU_MC2_HANDLER_H

#include <memory>
#include <shared_mutex>
#include "kernel_param_lite.h"
#include "mc2_data_type.h"
#include "stream_lite.h"
#include "ascend_hal_define.h"
#include "data_type.h"
#include "communicator_impl_lite.h"
#include "aicpu_utils.h"

namespace Hccl {

class AicpuMc2Handler {
public:
    ~AicpuMc2Handler() = default;
    static AicpuMc2Handler& GetInstance();

    HcclResult HcclGetCommHandleByCtx(void *ctx, void **opHandle) const;
    HcclResult HcclReleaseComm(void *opHandle) const;
    HcclResult HcclGetTaskStatus(void *opHandle, HcclTaskStatus *status) const;
    HcclResult HcclCheckFinishByStream(void *opHandle) const;
    HcclResult HcclPrintTaskExceptionAllComm(void *opHandle) const;
    HcclResult HcclLaunchCcoreWait(void *opHandle, uint64_t waitAddr, uint32_t turnNum, uint64_t turnNumAddr,
                                   bool isLast) const;
    HcclResult HcclLaunchCcorePost(void *opHandle, uint64_t recordAddr, uint32_t turnNum, uint64_t turnNumAddr) const;
    HcclResult HcclLaunchOp(void *opHandle, HcclOpData *data) const;

private:
    AicpuMc2Handler();
};

} // namespace Hccl

#endif // HCCLV2_AICPU_MC2_HANDLER_H
