/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NS_RESUME_LITE_H
#define NS_RESUME_LITE_H
#include "coll_comm.h"
#include "orion_adapter_rts.h"

namespace hccl {

/**
 * @note N秒快恢device侧的处理
 */

class NsRecoveryHandlerFunc : public DaemonFunc {
public:
    static NsRecoveryHandlerFunc &GetInstance();
    ~NsRecoveryHandlerFunc() override = default;
    void Call() override;
private:
    void HandleStopLaunch(CollCommAicpu *comm) const;
    void HandleClean(CollCommAicpu *comm);
    void StreamClean(CollCommAicpu *comm);
    HcclResult DeviceQuery(const uint32_t devId, const uint32_t step, const uint64_t timeout);

    NsRecoveryHandlerFunc() = default;
    NsRecoveryHandlerFunc(const NsRecoveryHandlerFunc&) = delete;
    NsRecoveryHandlerFunc& operator=(const NsRecoveryHandlerFunc&) = delete;
};

}