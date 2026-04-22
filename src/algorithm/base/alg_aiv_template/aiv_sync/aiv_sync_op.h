/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIV_COMMUNICATION_H
#define AIV_COMMUNICATION_H

#include "aiv_communication_base.h"

#include "aiv_sync_910b.h"
#include "aiv_sync_910b_rdma.h"
#include "aiv_sync_91093.h"

using namespace AscendC;

// aiv sync
extern "C" __global__ __aicore__ void hccl_aiv_sync(KERNEL_ARGS_DEF) {
    return aiv_sync_910b_inner(KERNEL_ARGS_CALL); 
}
EXPORT_AIV_META_INFO(hccl_aiv_sync);

// aiv sync rdma
extern "C" __global__ __aicore__ void hccl_aiv_sync_rdma(KERNEL_ARGS_DEF) {
    return aiv_sync_910b_rdma(KERNEL_ARGS_CALL);
}
EXPORT_AIV_META_INFO(hccl_aiv_sync_rdma);

// aiv sync A3
extern "C" __global__ __aicore__ void hccl_aiv_sync_cn(KERNEL_ARGS_DEF_A3) {
    return aiv_sync_91093_inner(KERNEL_ARGS_CALL_A3);
}
EXPORT_AIV_META_INFO(hccl_aiv_sync_cn);

#endif  /* AIV_COMMUNICATION_H */
