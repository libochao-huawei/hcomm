/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TYPICAL_QP_MANAGER_H
#define TYPICAL_QP_MANAGER_H
#include <mutex>
#include <memory>
#include "adapter_hccp.h"
#include "hccl_common.h"

namespace hccl {

class TypicalQpManager {
public:
    static TypicalQpManager& GetInstance();
    HcclResult CreateQp(struct TypicalQp& qpInfo);
    HcclResult CreateQp(struct TypicalQp& qpInfo, const QpConfigInfo& qpConfig);
    HcclResult ModifyQp(struct TypicalQp& localQpInfo, struct TypicalQp& remoteQpInfo);
    HcclResult DestroyQp(struct TypicalQp& qpInfo);
    HcclResult GetQpHandleByQpn(u32 qpn, QpHandle& qpHandle);
private:
    TypicalQpManager();
    ~TypicalQpManager();
    TypicalQpManager(TypicalQpManager const&) = delete;             // Copy construct
    TypicalQpManager(TypicalQpManager&&) = delete;                  // Move construct
    TypicalQpManager& operator=(TypicalQpManager const&) = delete;  // Copy assign
    TypicalQpManager& operator=(TypicalQpManager &&) = delete;      // Move assign
    HcclResult SetQpRdmaRetryCfg(struct TypicalQp& qpInfo);

private:
    RdmaHandle rdmaHandle_ = nullptr;
    const s32 QP_FLAG_RC = 0;      // flag: 0 = RC, 1= UD，其它预留
    const s32 OPBASE_QP_MODE = 2;  // 单算子模式的QP
    std::map<u32, std::pair<struct TypicalQp, QpHandle>> qpMap_{};
    std::mutex qpMutex_;
};
} // namespace hccl
#endif
