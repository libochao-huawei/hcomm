/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

 #include <errno.h>
#include "rs.h"
#include "user_log.h"
#include "rs_ub_jetty_ops.h"

static struct RsUbJettyOps rsUbJettyOps = {
    .version = RS_UB_JETTY_OPS_VERSION_NULL,
    .deleteJfcExt = NULL,
    .JfcCreateExt = NULL,
    .extJettyCreate = NULL,
    .extJettyDelete = NULL,
    .vaMunmapBatch = NULL,
    .freeJettyIdBatch = NULL,
};

int RsUbJettyApiInit(void)
{
    enum ProductType type;

    type = RsGetProductType(0);

    if (type == PRODUCT_TYPE_950) {
        rsUbJettyOps = RsUbJettyOps950();
        hccp_run_info("Rs Ub Jetty Ops init successfully, version:950");
    } else if (type == PRODUCT_TYPE_910_96) {
        rsUbJettyOps = RsUbJettyOps960();
        hccp_run_info("Rs Ub Jetty Ops init successfully, version:960");
    } else {
        rsUbJettyOps = RsUbJettyOpsNull();
        hccp_err("Rs Ub Jetty Ops init failed, version:null");
        return -EINVAL;
    }

    return 0;
};

void RsUbJettyApiDeinit(void)
{   
    rsUbJettyOps = RsUbJettyOpsNull();
    hccp_run_info("Rs Ub Jetty Ops deinit successfully");
    return;
};

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, 
        hccp_err("rs ub jetty api have not been inited"), -EINVAL);
    return rsUbJettyOps.deleteJfcExt(devCb, jfcCb);
};

int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, 
        hccp_err("rs ub jetty api have not been inited"), -EINVAL);
    return rsUbJettyOps.JfcCreateExt(ctxJfcCb, jfcCfg, jfc);
};

int RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return -EINVAL;
    }

    rsUbJettyOps.extJettyCreate(jettyCb, jettyCfg);
    return 0;
};

int RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return -EINVAL;
    }

    rsUbJettyOps.extJettyDelete(jettyCb);
    return 0;
};

int RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return -EINVAL;
    }

    rsUbJettyOps.vaMunmapBatch(jettyCbArr, num);
    return 0;
};

int RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return -EINVAL;
    }

    rsUbJettyOps.freeJettyIdBatch(jettyCbArr, num);
    return 0;
};

