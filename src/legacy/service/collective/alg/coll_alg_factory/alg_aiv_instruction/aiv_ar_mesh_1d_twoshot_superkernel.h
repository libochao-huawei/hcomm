/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: AivAllGatherMesh1D类头文件
 * Author: linyefeng
 * Create: 2025-12-15
 */

#ifndef AIV_AR_TWOSHOT_SUPERKERNEL_H
#define AIV_AR_TWOSHOT_SUPERKERNEL_H

#include "aiv_communication_base_v2.h"
#include "aiv_all_reduce_mesh_1D_twoshot.h"

extern "C"
__aicore__ void sk_allreduce_mesh_1d_twoshot(SUPERKERNEL_LITE_ARGS_DEF) {
    SUPERKERNEL_LITE_ARGS_EXTRACT;
    return sk_ar_mesh_1d_twoshot(SUPERKERNEL_ARGS_CALL);
}

#endif  /* AIV_AR_TWOSHOT_SUPERKERNEL_H */