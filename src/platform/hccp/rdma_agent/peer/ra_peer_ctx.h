/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: ra peer ctx
 * Create: 2025-11-03
 */

#ifndef RA_PEER_CTX_H
#define RA_PEER_CTX_H

#include "hccp_ctx.h"
#include "ra_ctx.h"

int RaPeerGetDevEidInfoNum(struct RaInfo info, unsigned int *num);

int RaPeerGetDevEidInfoList(unsigned int phyId, struct DevEidInfo infoList[], unsigned int *num);

int RaPeerCtxInit(struct RaCtxHandle *ctxHandle, struct CtxInitAttr *attr, unsigned int *devIndex,
    struct DevBaseAttr *devBaseAttr);

int RaPeerCtxGetAsyncEvents(struct RaCtxHandle *ctxHandle, struct AsyncEvent events[], unsigned int *num);

int RaPeerCtxDeinit(struct RaCtxHandle *ctxHandle);

int RaPeerGetEidByIp(struct RaCtxHandle *ctxHandle, struct IpInfo ip[], union HccpEid eid[],
    unsigned int *num);

int RaPeerCtxTokenIdAlloc(struct RaCtxHandle *ctxHandle, struct HccpTokenId *info,
    struct RaTokenIdHandle *tokenIdHandle);

int RaPeerCtxTokenIdFree(struct RaCtxHandle *ctxHandle, struct RaTokenIdHandle *tokenIdHandle);

int RaPeerCtxLmemRegister(struct RaCtxHandle *ctxHandle, struct MrRegInfoT *lmemInfo,
    struct RaLmemHandle *lmemHandle);

int RaPeerCtxLmemUnregister(struct RaCtxHandle *ctxHandle, struct RaLmemHandle *lmemHandle);

int RaPeerCtxRmemImport(struct RaCtxHandle *ctxHandle, struct MrImportInfoT *rmemInfo);

int RaPeerCtxRmemUnimport(struct RaCtxHandle *ctxHandle, struct RaRmemHandle *rmemHandle);

int RaPeerCtxChanCreate(struct RaCtxHandle *ctxHandle, struct ChanInfoT *chanInfo,
    struct RaChanHandle *chanHandle);

int RaPeerCtxChanDestroy(struct RaCtxHandle *ctxHandle, struct RaChanHandle *chanHandle);

int RaPeerCtxCqCreate(struct RaCtxHandle *ctxHandle, struct CqInfoT *info, struct RaCqHandle *cqHandle);

int RaPeerCtxCqDestroy(struct RaCtxHandle *ctxHandle, struct RaCqHandle *cqHandle);

int RaPeerCtxQpCreate(struct RaCtxHandle *ctxHandle, struct QpCreateAttr *qpAttr,
    struct QpCreateInfo *qpInfo, struct RaCtxQpHandle *qpHandle);

int RaPeerCtxQpDestroy(struct RaCtxQpHandle *qpHandle);

int RaPeerCtxQpImport(struct RaCtxHandle *ctxHandle, struct QpImportInfoT *qpInfo,
    struct RaCtxRemQpHandle *remQpHandle);

int RaPeerCtxQpUnimport(struct RaCtxRemQpHandle *remQpHandle);

int RaPeerCtxQpBind(struct RaCtxQpHandle *qpHandle, struct RaCtxRemQpHandle *remQpHandle);

int RaPeerCtxQpUnbind(struct RaCtxQpHandle *qpHandle);

#endif  // RA_PEER_CTX_H
