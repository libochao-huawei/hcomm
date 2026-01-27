/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * Description: rdma_server ctx ub internal interface declaration
 * Create: 2023-11-17
 */

#ifndef RS_UB_H
#define RS_UB_H

#include <udma_u_ctl.h>
#include <urma_types.h>
#include "dl_urma_function.h"
#include "hccp_async_ctx.h"
#include "hccp_async.h"
#include "hccp_common.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"
#include "rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs.h"

struct RsJettyKeyInfo {
    urma_jetty_id_t jettyId;
    urma_transport_mode_t transMode;
};

enum RsJettyState {
    RS_JETTY_STATE_INIT = 0,
    RS_JETTY_STATE_CREATED = 1,
    RS_JETTY_STATE_IMPORTED = 2,
    RS_JETTY_STATE_BIND = 3,
    RS_JETTY_STATE_MAX
};

union CreateJfcCfg {
    struct udma_u_lock_jfc_cfg lockJfcCfg;
    struct udma_u_jfc_cfg_ex jfcCfgEx;
};

struct JettyDestroyBatchInfo {
    struct RsCtxJettyCb **jettyCbArr;
    urma_jetty_t **jettyArr;
    urma_jfr_t **jfrArr;
};

int RsUbGetDevEidInfoNum(unsigned int phyId, unsigned int *num);
int RsUbGetUeInfo(urma_context_t *urmaCtx, struct DevBaseAttr *devAttr);
int RsUbGetDevEidInfoList(unsigned int phyId, struct DevEidInfo infoList[], unsigned int startIndex,
    unsigned int count);
int RsUbCtxInit(struct rs_cb *rsCb, struct CtxInitAttr *attr, unsigned int *devIndex,
    struct DevBaseAttr *devAttr);
int RsUbGetDevCb(struct rs_cb *rscb, unsigned int devIndex, struct RsUbDevCb **devCb);
int RsUbCtxDeinit(struct RsUbDevCb *devCb);
int RsUbGetEidByIp(struct RsUbDevCb *devCb, struct IpInfo ip[], union HccpEid eid[], unsigned int *num);
int RsUbGetTpInfoList(struct RsUbDevCb *devCb, struct GetTpCfg *cfg, struct TpInfo infoList[],
    unsigned int *num);
int RsUbGetTpAttr(struct RsUbDevCb *devCb, unsigned int *attrBitmap, const uint64_t tpHandle,
    struct TpAttr *attr);
int RsUbSetTpAttr(struct RsUbDevCb *devCb, const unsigned int attrBitmap, const uint64_t tpHandle,
    struct TpAttr *attr);
int RsUbCtxTokenIdAlloc(struct RsUbDevCb *devCb, unsigned long long *addr, unsigned int *tokenId);
int RsUbCtxTokenIdFree(struct RsUbDevCb *devCb, unsigned long long addr);
int RsUbCtxLmemReg(struct RsUbDevCb *devCb, struct MemRegAttrT *memAttr, struct MemRegInfoT *memInfo);
int RsUbCtxLmemUnreg(struct RsUbDevCb *devCb, unsigned long long addr);
int RsUbCtxRmemImport(struct RsUbDevCb *devCb, struct MemImportAttrT *memAttr,
    struct MemImportInfoT *memInfo);
int RsUbCtxRmemUnimport(struct RsUbDevCb *devCb, unsigned long long addr);
int RsUbCtxChanCreate(struct RsUbDevCb *devCb, union DataPlaneCstmFlag dataPlaneFlag,
    unsigned long long *addr, int *fd);
int RsUbCtxChanDestroy(struct RsUbDevCb *devCb, unsigned long long addr);
int RsUbCtxJfcCreate(struct RsUbDevCb *devCb, struct CtxCqAttr *attr, struct CtxCqInfo *info);
int RsUbCtxJfcDestroy(struct RsUbDevCb *devCb, unsigned long long addr);
int RsUbCtxJettyCreate(struct RsUbDevCb *devCb, struct CtxQpAttr *attr, struct QpCreateInfo *info);
int RsUbCtxQueryJettyBatch(struct RsUbDevCb *devCb, unsigned int jettyIds[], struct JettyAttr attr[],
    unsigned int *num);
int RsUbCtxJettyDestroy(struct RsUbDevCb *devCb, unsigned int jettyId);
int RsUbCtxJettyDestroyBatch(struct RsUbDevCb *devCb, unsigned int jettyIds[], unsigned int *num);
int RsUbCtxJettyImport(struct RsUbDevCb *devCb, struct RsJettyImportAttr *importAttr,
    struct RsJettyImportInfo *importInfo);
int RsUbCtxJettyUnimport(struct RsUbDevCb *devCb, unsigned int remJettyId);
int RsUbCtxJettyBind(struct RsUbDevCb *devCb, struct RsCtxQpInfo *jettyInfo,
    struct RsCtxQpInfo *rjettyInfo);
int RsUbCtxJettyUnbind(struct RsUbDevCb *devCb, unsigned int jettyId);
int RsUbCtxJettyFree(struct rs_cb *rscb, unsigned int ueInfo, unsigned int jettyId);
void RsUbFreeJettyCbList(struct RsUbDevCb *devCb, struct RsListHead *jettyList,
    struct RsListHead *rjettyList);
int RsUbCtxBatchSendWr(struct rs_cb *rsCb, struct WrlistBaseInfo *baseInfo,
    struct BatchSendWrData *wrData, struct SendWrResp *wrResp, struct WrlistSendCompleteNum *wrlistNum);
int RsUbCtxJettyUpdateCi(struct RsUbDevCb *devCb, unsigned int jettyId, uint16_t ci);
int RsUbCtxGetAuxInfo(struct RsUbDevCb *devCb, struct AuxInfoIn *infoIn, struct AuxInfoOut *infoOut);
int RsEpollEventJfcInHandle(struct rs_cb *rsCb, int fd);
#endif // RS_UB_H
