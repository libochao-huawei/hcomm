/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ROCE_CQE_ERR_HANDLER_LITE_H
#define ROCE_CQE_ERR_HANDLER_LITE_H

#include "daemon_func.h"
#include "coll_comm_aicpu.h"

namespace hcomm {

class RoceCqeErrHandlerLite : public Hccl::DaemonFunc {
public:
    static RoceCqeErrHandlerLite &GetInstance();
    void Init(u32 devId);
    void Call() override;

private:
    RoceCqeErrHandlerLite();
    ~RoceCqeErrHandlerLite();

    HcclResult HandleRoceCqeErr();
    HcclResult ProcessRoceCqeErr(CollCommAicpu *aicpuComm, const RoceCqeErrInfo &errInfo);
    HcclResult ReportErrorToHost(CollCommAicpu *aicpuComm, const RoceCqeErrInfo &errInfo);

    bool initFlag_{false};
    bool stopCall_{false};
    u32 devId_{INVALID_UINT};
};

} // namespace hcomm

#endif
