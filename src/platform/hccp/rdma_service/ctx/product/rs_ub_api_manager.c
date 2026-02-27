#include "rs_ub_api_manager.h"
#include "rs.h"

static struct RsUbJettyOps rsUbJettyOps = {
    .version = RS_UB_JETTY_OPS_VERSION_NULL,
    .deleteJfcExt   = NULL,
    .JfcCreateExt   = NULL,
    .extJettyCreate = NULL,
    .extJettyDelete = NULL,
    .vaMunmapBatch  = NULL,
    .freeJettyIdBatch = NULL
};

void RsUbJettyApiInit(void)
{
    enum ProductType type;

    type = RsGetProductType(0);
    if (type == PRODUCT_TYPE_950) {
        rsUbJettyOps = RsUbJettyOps950();
    } else if (type == PRODUCT_TYPE_910_96) {
        rsUbJettyOps = RsUbJettyOps960();
    } else {
        rsUbJettyOps = RsUbJettyOpsDefault();
    }

    return;
};

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb)
{
    
};

int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc)
{

};

void RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{

};

void RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb)
{

};

void RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{

};

void RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{

};

