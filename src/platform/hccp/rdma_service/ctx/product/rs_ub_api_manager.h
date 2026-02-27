#ifndef RS_UB_API_MANAGER_H
#define RS_UB_API_MANAGER_H

#include "rs_ub_jfc_950.h"
#include "rs_ub_jfc_960.h"
#include "rs_ub_jfc_default.h"
#include "rs_ub_jetty_950.h"
#include "rs_ub_jetty_960.h"
#include "rs_ub_jetty_default.h"

struct RsUbJettyOps {
    const char *version;
    int (*deleteJfcExt)(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb);
    int (*JfcCreateExt)(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc);
    void (*extJettyCreate)(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg);
    void (*extJettyDelete)(struct RsCtxJettyCb *jettyCb);
    void (*vaMunmapBatch)(struct RsCtxJettyCb **jettyCbArr, unsigned int num);
    void (*freeJettyIdBatch)(struct RsCtxJettyCb **jettyCbArr, unsigned int num);
};

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb);
int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc);
void RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg);
void RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb);
void RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num);
void RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num);

static inline struct RsUbJettyOps RsUbJettyOpsDefault(void) {
    return (struct RsUbJettyOps){
        .version = "default",
        .deleteJfcExt = RsUbDeleteJfcExtDefault,
        .JfcCreateExt = RsUbCtxJfcCreateExtDefault,
        .extJettyCreate = RsUbCtxExtJettyCreateDefault,
        .extJettyDelete = RsUbCtxExtJettyDeleteDefault,
        .vaMunmapBatch = RsUbVaMunmapBatchDefault,
        .freeJettyIdBatch = RsUbFreeJettyIdBatchDefault,
    };
}

static inline struct RsUbJettyOps RsUbJettyOps950(void) {
    return (struct RsUbJettyOps){
        .version = "950",
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
        .version = "960",
        .deleteJfcExt = RsUbDeleteJfcExt960,
        .JfcCreateExt = RsUbCtxJfcCreateExt960,
        .extJettyCreate = RsUbCtxExtJettyCreate960,
        .extJettyDelete = RsUbCtxExtJettyDelete960,
        .vaMunmapBatch = RsUbVaMunmapBatch960,
        .freeJettyIdBatch = RsUbFreeJettyIdBatch960,
    };
}
#endif // RS_UB_API_MANAGER_H