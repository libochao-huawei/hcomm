/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COLL_COMM_GROUP_UTILS_H
#define COLL_COMM_GROUP_UTILS_H

#include <deque>
#include <vector>
#include <hccl/hccl_types.h>
#include "acl/acl_base_rt.h"
#include "hcomm_res_defs.h"

constexpr int32_t MAX_P2P_TASK_NUM = 2048;

namespace hccl {

struct HcclP2pPair {
    uint32_t sendRank;
    uint32_t recvRank;
};

struct HcclP2pTask {
    HcclOpP2pDesc desc;
    aclrtStream stream;
    HcclKernelFuncInfo funcInfo;
    void* args;
    uint32_t argSize;
};

struct HcclP2pSendRecvQueue {
    std::deque<HcclP2pTask> sendQue;
    std::deque<HcclP2pTask> recvQue;
};

struct hcclKernelPlannerV2 {
    int32_t nTasksP2p = -1;
    uint32_t rankSize = 0;

    std::vector<HcclP2pPair> p2pSchedule;
    std::vector<HcclP2pSendRecvQueue> peers;
};

} // namespace hccl
#endif