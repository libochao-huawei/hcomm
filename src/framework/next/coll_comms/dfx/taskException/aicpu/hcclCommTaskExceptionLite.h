/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCL_COMM_TASKEXCEPTION_H
#define HCCL_COMM_TASKEXCEPTION_H

#include "mirror_task_manager.h"
#include "aicpu_communicator.h"

namespace hccl {

class HcclCommTaskExceptionLite : public DaemonFunc {
public:
    static TaskExceptionHandlerLite &GetInstance();
    HcclResult Init(u32 deviceId);
    static void Process(HcclCommAicpu *aicpuComm, rtLogicCqReport_t* exceptionInfo);
    void Call() override;

private:
    HcclCommTaskExceptionLite();
    ~HcclCommTaskExceptionLite();

    bool initFlag_{false};
    u32 deviceId_{INVALID_UINT};
    Hccl::MirrorTaskManager* mirrorTaskManager_{nullptr};  // 使用原始指针，不管理生命周期
};

}

#endif
