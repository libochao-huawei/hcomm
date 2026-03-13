/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include "ibv_extend.h"

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

struct RsNdaOps {
    const char *(*rsNdaIbvExtendGetVersion)(uint32_t *major, uint32_t *minor, uint32_t *patch);
    int (*rsNdaIbvExtendCheckVersion)(uint32_t driverMajor, uint32_t driverMinor, uint32_t driverPatch);
    struct ibv_context_extend *(*rsNdaIbvOpenExtend)(struct ibv_context *context);
    int (*rsNdaIbvCloseExtend)(struct ibv_context_extend *context);
    struct ibv_qp_extend *(*rsNdaCreateQpExtend)(struct ibv_context_extend *context,
        struct ibv_qp_init_attr_extend *qpInitAttr);
    struct ibv_cq_extend *(*rsNdaCreateCqExtend)(struct ibv_context_extend *context,
        struct ibv_cq_init_attr_extend *cqInitAttr);
    int (*rsNdaIbvDestroyQpExtend)(struct ibv_context_extend *context, struct ibv_qp_extend *qpExtend);
    int (*rsNdaIbvDestroyCqExtend)(struct ibv_context_extend *context, struct ibv_cq_extend *cqExtend);
};

int RsNdaApiInit(void);
void RsNdaApiDeinit(void);
const char *RsNdaIbvExtendGetVersion(uint32_t *major, uint32_t *minor, uint32_t *patch);
int RsNdaIbvExtendCheckVersion(uint32_t driverMajor, uint32_t driverMinor, uint32_t driverPatch);
struct ibv_context_extend *RsNdaIbvOpenExtend(struct ibv_context *context);
int RsNdaIbvCloseExtend(struct ibv_context_extend *context);

#endif // DL_NDA_FUNCTION_H