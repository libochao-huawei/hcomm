/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: AivAllGatherMesh1D类头文件
 * Author: linyefeng
 * Create: 2025-12-15
 */

#ifndef AIV_RS_SUPERKERNEL_H
#define AIV_RS_SUPERKERNEL_H

#include "aiv_communication_base_v2.h"
#include "aiv_reduce_scatter_mesh_1d.h"

extern "C"
__aicore__ void sk_reducescatter_mesh_1d(SUPERKERNEL_LITE_ARGS_DEF) {
    SUPERKERNEL_LITE_ARGS_EXTRACT;
    return sk_rs_mesh_1d(SUPERKERNEL_ARGS_CALL);
}

#endif  /* AIV_RS_SUPERKERNEL_H */