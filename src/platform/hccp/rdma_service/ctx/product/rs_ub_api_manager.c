#include "user_log.h"
#include "rs.h"
#include "rs_ub_api_manager.h"

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

void RsUbJettyApiDeinit(void)
{
    return;
};

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, hccp_err("Rs Ub Api has not been inited"), -EINVAL);
    return rsUbJettyOps.deleteJfcExt(devCb, jfcCb);
};

int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, hccp_err("Rs Ub Api has not been inited"), -EINVAL);
    return rsUbJettyOps.JfcCreateExt(ctxJfcCb, jfcCfg, jfc);
};

void RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, hccp_err("Rs Ub Api has not been inited"), -EINVAL);
    return rsUbJettyOps.extJettyCreate(jettyCb, jettyCfg);
};

void RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, hccp_err("Rs Ub Api has not been inited"), -EINVAL);
    return rsUbJettyOps.extJettyDelete(jettyCb);
};

void RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, hccp_err("Rs Ub Api has not been inited"), -EINVAL);
    return rsUbJettyOps.vaMunmapBatch(jettyCbArr, num);
};

void RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    CHK_PRT_RETURN(rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL, hccp_err("Rs Ub Api has not been inited"), -EINVAL);
    return rsUbJettyOps.freeJettyIdBatch(jettyCbArr, num);
};

