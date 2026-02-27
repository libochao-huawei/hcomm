#include "rs_ub_api_manager.h"

static struct RsUbJettyOps rsUbJettyOps = {
    .version = "null",
    .deleteJfcExt   = NULL,
    .JfcCreateExt   = NULL,
    .extJettyCreate = NULL,
    .extJettyDelete = NULL,
    .vaMunmapBatch  = NULL,
    .freeJettyIdBatch = NULL
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

