/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DL_NDA_FUNCTION_H
#define DL_NDA_FUNCTION_H

#include <infiniband/verbs.h>

struct RsRoceNdaOps {
    struct ibv_context_extend *(*rsNdaIbvOpenExtend)(struct ibv_context *context);
    int (*rsNdaIbvCloseExtend)(struct ibv_context_extend *context);
    struct ibv_qp_extend *(*rsNdaCreateQpExtend)(struct ibv_context_extend *context,
        struct ibv_qp_init_attr_extend *qp_init_attr);
    
};

#endif // DL_NDA_FUNCTION_H