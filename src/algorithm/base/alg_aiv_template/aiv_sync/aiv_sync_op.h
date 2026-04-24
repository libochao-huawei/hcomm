/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIV_SYNC_OP_H
#define AIV_SYNC_OP_H

#include "aiv_communication_base.h"

#include "aiv_sync_910b.h"
#include "aiv_sync_910b_rdma.h"

// aiv sync
__aicore__ inline void hccl_aiv_sync_inner(KERNEL_ARGS_DEF) {
    return aiv_sync_910b_inner(KERNEL_ARGS_CALL); 
}

// aiv sync rdma
__aicore__ inline void hccl_aiv_sync_rdma_inner(KERNEL_ARGS_DEF) {
    return aiv_sync_910b_rdma(KERNEL_ARGS_CALL);
}

#if defined(BUILD_SK_FUNC) && defined(SK_FUNC_ID)
    SK_BIND_FUNC_DEF_A2(hccl_aiv_sync, SK_FUNC_ID)
    SK_BIND_FUNC_DEF_A2(hccl_aiv_sync_rdma, SK_FUNC_ID)
#else
    GLOBAL_FUNC_DEF_A2(hccl_aiv_sync);
    GLOBAL_FUNC_DEF_A2(hccl_aiv_sync_rdma);
    SuperKernelBindA2(hccl_aiv_sync);
    SuperKernelBindA2(hccl_aiv_sync_rdma);
#endif

#endif  /* AIV_SYNC_OP_H */
