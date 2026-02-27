/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_UB_JETTY_OPS_H
#define RS_UB_JETTY_OPS_H

#include "rs_ub_jfc_950.h"
#include "rs_ub_jfc_960.h"
#include "rs_ub_jetty_950.h"
#include "rs_ub_jetty_960.h"

enum RsUbJettyOpsVersion {
    RS_UB_JETTY_OPS_VERSION_NULL = -1,
    RS_UB_JETTY_OPS_VERSION_DEFAULT = 0,
    RS_UB_JETTY_OPS_VERSION_950 = 950,
    RS_UB_JETTY_OPS_VERSION_960 = 960,
};

struct RsUbJettyOps {
    enum RsUbJettyOpsVersion version;
    int (*deleteJfcExt)(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb);
    int (*JfcCreateExt)(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc);
    void (*extJettyCreate)(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg);
    void (*extJettyDelete)(struct RsCtxJettyCb *jettyCb);
    void (*vaMunmapBatch)(struct RsCtxJettyCb **jettyCbArr, unsigned int num);
    void (*freeJettyIdBatch)(struct RsCtxJettyCb **jettyCbArr, unsigned int num);
};

int RsUbJettyApiInit(void);
void RsUbJettyApiDeinit(void);
int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb);
int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc);
int RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg);
int RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb);
int RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num);
int RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num);

static inline struct RsUbJettyOps RsUbJettyOpsNull(void) {
    return (struct RsUbJettyOps){
        .version = RS_UB_JETTY_OPS_VERSION_NULL,
        .deleteJfcExt = NULL,
        .JfcCreateExt = NULL,
        .extJettyCreate = NULL,
        .extJettyDelete = NULL,
        .vaMunmapBatch = NULL,
        .freeJettyIdBatch = NULL,
    };
}

static inline struct RsUbJettyOps RsUbJettyOps950(void) {
    return (struct RsUbJettyOps){
        .version = RS_UB_JETTY_OPS_VERSION_950,
        .deleteJfcExt = RsUbDeleteJfcExt950,
        .JfcCreateExt = RsUbCtxJfcCreateExt950,
        .extJettyCreate = RsUbCtxExtJettyCreate950,
        .extJettyDelete = RsUbCtxExtJettyDelete950,
        .vaMunmapBatch = RsUbVaMunmapBatch950,
        .freeJettyIdBatch = RsUbFreeJettyIdBatch950,
    };
}

static inline struct RsUbJettyOps RsUbJettyOps960(void) {
    return (struct RsUbJettyOps){
        .version = RS_UB_JETTY_OPS_VERSION_960,
        .deleteJfcExt = RsUbDeleteJfcExt960,
        .JfcCreateExt = RsUbCtxJfcCreateExt960,
        .extJettyCreate = RsUbCtxExtJettyCreate960,
        .extJettyDelete = RsUbCtxExtJettyDelete960,
        .vaMunmapBatch = RsUbVaMunmapBatch960,
        .freeJettyIdBatch = RsUbFreeJettyIdBatch960,
    };
}
#endif // RS_UB_JETTY_OPS_H