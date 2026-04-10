/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pthread.h>
#include "errno.h"
#include "hccp_dl.h"
#include "ra_rs_err.h"
#include "dl_nda_function.h"

void *gRoceNdaApiHandle = NULL;
#ifndef CA_CONFIG_LLT
STATIC struct RsNdaOps gNdaOps = {0};
#else
struct RsNdaOps gNdaOps = {
    .rsNdaIbvExtendGetVersion = ibv_extend_get_version,
    .rsNdaIbvExtendCheckVersion = ibv_extend_check_version,
    .rsNdaIbvOpenExtend = ibv_open_extend,
    .rsNdaIbvCloseExtend = ibv_close_extend,
    .rsNdaCreateQpExtend = ibv_create_qp_extend,
    .rsNdaCreateCqExtend = ibv_create_cq_extend,
    .rsNdaIbvDestroyQpExtend = ibv_destroy_qp_extend,
    .rsNdaIbvDestroyCqExtend = ibv_destroy_cq_extend,
};
#endif

STATIC int RsNdaIbvApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gNdaOps.rsNdaIbvExtendGetVersion = (const char* (*)(uint32_t *, uint32_t *, uint32_t *))
        HccpDlsym(gRoceNdaApiHandle, "ibv_extend_get_version");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaIbvExtendGetVersion, "ibv_extend_get_version");
    gNdaOps.rsNdaIbvExtendCheckVersion = (int (*)(uint32_t, uint32_t, uint32_t))
        HccpDlsym(gRoceNdaApiHandle, "ibv_extend_check_version");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaIbvExtendCheckVersion, "ibv_extend_check_version");
    gNdaOps.rsNdaIbvOpenExtend = (struct ibv_context_extend* (*)(struct ibv_context *))
        HccpDlsym(gRoceNdaApiHandle, "ibv_open_extend");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaIbvOpenExtend, "ibv_open_extend");
    gNdaOps.rsNdaIbvCloseExtend = (int (*)(struct ibv_context_extend *))
        HccpDlsym(gRoceNdaApiHandle, "ibv_close_extend");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaIbvCloseExtend, "ibv_close_extend");
    gNdaOps.rsNdaCreateQpExtend = (struct ibv_qp_extend* (*)(struct ibv_context_extend *,
        struct ibv_qp_init_attr_extend *)) HccpDlsym(gRoceNdaApiHandle, "ibv_create_qp_extend");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaCreateQpExtend, "ibv_create_qp_extend");
    gNdaOps.rsNdaCreateCqExtend = (struct ibv_cq_extend* (*)(struct ibv_context_extend *,
        struct ibv_cq_init_attr_extend *)) HccpDlsym(gRoceNdaApiHandle, "ibv_create_cq_extend");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaCreateCqExtend, "ibv_create_cq_extend");
    gNdaOps.rsNdaIbvDestroyQpExtend = (int (*)(struct ibv_context_extend *,
        struct ibv_qp_extend *)) HccpDlsym(gRoceNdaApiHandle, "ibv_destroy_qp_extend");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaIbvDestroyQpExtend, "ibv_destroy_qp_extend");
    gNdaOps.rsNdaIbvDestroyCqExtend = (int (*)(struct ibv_context_extend *,
        struct ibv_cq_extend *)) HccpDlsym(gRoceNdaApiHandle, "ibv_destroy_cq_extend");
    DL_API_RET_IS_NULL_CHECK(gNdaOps.rsNdaIbvDestroyCqExtend, "ibv_destroy_cq_extend");
#endif
    return 0;
}

STATIC int RsOpenNdaSo(void)
{
#ifndef CA_CONFIG_LLT
    if (gRoceNdaApiHandle == NULL) {
        gRoceNdaApiHandle = HccpDlopen("libibv_extend.so", RTLD_NOW);
        if (gRoceNdaApiHandle != NULL) {
            return 0;
        }
        return -EINVAL;
    } else {
        hccp_run_info("NdaApi dlopen again!");
    }
#endif
    return 0;
}

STATIC void RsCloseNdaSo(void)
{
#ifndef CA_CONFIG_LLT
    if (gRoceNdaApiHandle != NULL) {
        (void)HccpDlclose(gRoceNdaApiHandle);
        gRoceNdaApiHandle = NULL;
    }
#endif
    return;
}

int RsNdaApiInit(void)
{
#ifndef CA_CONFIG_LLT
    int ret = 0;

    ret = RsOpenNdaSo();
    if (ret != 0) {
        hccp_warn("HccpDlopen[libibv_extend.so] doesn't exist!");
        return 0;
    }

    ret = RsNdaIbvApiInit();
    if (ret != 0) {
        hccp_err("RsNdaIbvApiInit failed! ret:%d", ret);
        RsCloseNdaSo();
        return ret;
    }
#endif
    return 0;
}

void RsNdaApiDeinit(void)
{
    RsCloseNdaSo();
    return;
}

const char *RsNdaIbvExtendGetVersion(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (gNdaOps.rsNdaIbvExtendGetVersion == NULL) {
#ifndef CA_CONFIG_LLT
        return NULL;
#endif
    }
    return gNdaOps.rsNdaIbvExtendGetVersion(major, minor, patch);
}

int RsNdaIbvExtendCheckVersion(uint32_t driverMajor, uint32_t driverMinor, uint32_t driverPatch)
{
    if (gNdaOps.rsNdaIbvExtendCheckVersion == NULL) {
#ifndef CA_CONFIG_LLT
        return -EINVAL;
#endif
    }
    return gNdaOps.rsNdaIbvExtendCheckVersion(driverMajor, driverMinor, driverPatch);
}

struct ibv_context_extend *RsNdaIbvOpenExtend(struct ibv_context *context)
{
    if (gNdaOps.rsNdaIbvOpenExtend == NULL) {
#ifndef CA_CONFIG_LLT
        return NULL;
#endif
    }
    return gNdaOps.rsNdaIbvOpenExtend(context);
}

int RsNdaIbvCloseExtend(struct ibv_context_extend *context)
{
    if (context == NULL) {
        return 0;
    }

    if (gNdaOps.rsNdaIbvCloseExtend == NULL) {
#ifndef CA_CONFIG_LLT
        return 0;
#endif
    }
    return gNdaOps.rsNdaIbvCloseExtend(context);
}
