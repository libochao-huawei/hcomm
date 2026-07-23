/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DL_IBV_EXTEND_FUNCTION_H
#define DL_IBV_EXTEND_FUNCTION_H

#include <infiniband/verbs.h>
#include "ibv_extend.h"

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

struct RsIbvExtendOps {
    const char *(*rsIbvExtendGetVersion)(uint32_t *major, uint32_t *minor, uint32_t *patch);
    int (*rsIbvExtendCheckVersion)(uint32_t driverMajor, uint32_t driverMinor, uint32_t driverPatch);
    struct ibv_context_extend *(*rsIbvOpenExtend)(struct ibv_context *context);
    int (*rsIbvCloseExtend)(struct ibv_context_extend *context);
    int (*rsIbvQueryDeviceExtend)(struct ibv_context_extend *context, struct ibv_device_attr_extend *extDevAttr);
    struct ibv_qp_extend *(*rsIbvCreateQpExtend)(struct ibv_context_extend *context,
        struct ibv_qp_init_attr_extend *qpInitAttr);
    struct ibv_cq_extend *(*rsIbvCreateCqExtend)(struct ibv_context_extend *context,
        struct ibv_cq_init_attr_extend *cqInitAttr);
    int (*rsIbvDestroyQpExtend)(struct ibv_context_extend *context, struct ibv_qp_extend *qpExtend);
    int (*rsIbvDestroyCqExtend)(struct ibv_context_extend *context, struct ibv_cq_extend *cqExtend);
};

int RsIbvExtendApiInit(void);
void RsIbvExtendApiDeinit(void);
const char *RsIbvExtendGetVersion(uint32_t *major, uint32_t *minor, uint32_t *patch);
int RsIbvExtendCheckVersion(uint32_t driverMajor, uint32_t driverMinor, uint32_t driverPatch);
struct ibv_context_extend *RsIbvOpenExtend(struct ibv_context *context);
int RsIbvCloseExtend(struct ibv_context_extend *context);
int RsIbvQueryDeviceExtend(struct ibv_context_extend *context, struct ibv_device_attr_extend *extDevAttr);
struct ibv_cq_extend *RsIbvCreateCqExtend(struct ibv_context_extend *context,
    struct ibv_cq_init_attr_extend *cqInitAttr);
int RsIbvDestroyCqExtend(struct ibv_context_extend *context, void *ibvCqExt);
struct ibv_qp_extend *RsIbvCreateQpExtend(struct ibv_context_extend *context,
    struct ibv_qp_init_attr_extend *qpInitAttr);
int RsIbvDestroyQpExtend(struct ibv_context_extend *context, struct ibv_qp_extend *qpExtend);

#endif // DL_IBV_EXTEND_FUNCTION_H