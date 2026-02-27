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

void RsUbJettyApiInit(void)
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
        hccp_run_info("Rs Ub Jetty Ops init successfully, version:null");
    }

    return;
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

void RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return;
    }
    rsUbJettyOps.extJettyCreate(jettyCb, jettyCfg);
};

void RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return;
    }
    rsUbJettyOps.extJettyDelete(jettyCb);
};

void RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return;
    }
    rsUbJettyOps.vaMunmapBatch(jettyCbArr, num);
};

void RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    if (rsUbJettyOps.version == RS_UB_JETTY_OPS_VERSION_NULL) {
        hccp_err("rs ub jetty api have not been inited");
        return;
    }
    rsUbJettyOps.freeJettyIdBatch(jettyCbArr, num);
};

