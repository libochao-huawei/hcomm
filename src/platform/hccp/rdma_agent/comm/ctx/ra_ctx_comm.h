/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: ra ctx comm
 * Create: 2025-12-08
 */

#ifndef RA_CTX_COMM_H
#define RA_CTX_COMM_H

#include "ra.h"
#include "hccp_common.h"
#include "hccp_ctx.h"
#include "ra_ctx.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"

int RaCtxPrepareLmemRegister(struct MrRegInfoT *lmemInfo, struct MemRegAttrT *memAttr);

void RaCtxGetLmemInfo(struct MemRegInfoT *memInfo, struct MrRegInfoT *lmemInfo,
    struct RaLmemHandle *lmemHandle);

void RaCtxPrepareRmemImport(struct MrImportInfoT *rmemInfo, struct MemImportAttrT *memAttr);

void RaCtxPrepareCqCreate(struct CqInfoT *info, struct CtxCqAttr *cqAttr);

int RaCtxPrepareQpCreate(struct QpCreateAttr *qpAttr, struct CtxQpAttr *ctxQpAttr);

void RaCtxGetQpCreateInfo(struct RaCtxHandle *ctxHandle, struct QpCreateAttr *qpAttr,
    struct QpCreateInfo *qpInfo, struct RaCtxQpHandle *qpHandle);

void RaCtxPrepareQpImport(struct QpImportInfoT *qpInfo, struct RaRsJettyImportAttr *importAttr);

void RaCtxGetQpImportInfo(struct RaCtxHandle *ctxHandle, struct QpImportInfoT *qpInfo,
    struct RaRsJettyImportInfo *importInfo, struct RaCtxRemQpHandle *qpHandle);

#endif // RA_CTX_COMM_H
