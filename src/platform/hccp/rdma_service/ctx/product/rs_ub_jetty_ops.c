#include <errno.h>
#include "rs.h"
#include "user_log.h"
#include "rs_ub_jetty_ops.h"

static struct RsUbJettyOps rsUbJettyOps = {
    .version = RS_UB_JETTY_OPS_VERSION_DEFAULT,
    .deleteJfcExt = RsUbDeleteJfcExtDefault,
    .JfcCreateExt = RsUbCtxJfcCreateExtDefault,
    .extJettyCreate = RsUbCtxExtJettyCreateDefault,
    .extJettyDelete = RsUbCtxExtJettyDeleteDefault,
    .vaMunmapBatch = RsUbVaMunmapBatchDefault,
    .freeJettyIdBatch = RsUbFreeJettyIdBatchDefault,
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
        rsUbJettyOps = RsUbJettyOpsDefault();
        hccp_run_info("Rs Ub Jetty Ops init successfully, version:default");
    }

    return;
};

void RsUbJettyApiDeinit(void)
{
    hccp_run_info("Rs Ub Jetty Ops deinit successfully");
    return;
};

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb)
{
    return rsUbJettyOps.deleteJfcExt(devCb, jfcCb);
};

int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc)
{
    return rsUbJettyOps.JfcCreateExt(ctxJfcCb, jfcCfg, jfc);
};

void RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    return rsUbJettyOps.extJettyCreate(jettyCb, jettyCfg);
};

void RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb)
{
    return rsUbJettyOps.extJettyDelete(jettyCb);
};

void RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    return rsUbJettyOps.vaMunmapBatch(jettyCbArr, num);
};

void RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    return rsUbJettyOps.freeJettyIdBatch(jettyCbArr, num);
};

