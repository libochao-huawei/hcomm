/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: RDMA Agent HDC async ctx header
 * Create: 2025-02-17
 */

#ifndef RA_HDC_ASYNC_CTX_H
#define RA_HDC_ASYNC_CTX_H

#include "hccp_async_ctx.h"
#include "ra_ctx.h"
#include "ra_async.h"

union OpGetTpInfoListData {
    struct {
        unsigned int phyId;
        unsigned int devIndex;
        struct GetTpCfg cfg;
        unsigned int num;
        uint32_t resv[4U];
    } txData;

    struct {
        struct TpInfo infoList[HCCP_MAX_TPID_INFO_NUM];
        unsigned int num;
        uint32_t resv[4U];
    } rxData;
};

struct RaResponseTpInfoList {
    struct TpInfo *infoList;
    unsigned int *num;
};

union OpGetTpAttrData {
    struct {
        unsigned int phyId;
        unsigned int devIndex;
        uint32_t attrBitmap;
        uint64_t tpHandle;
        uint32_t resv[4U];
    } txData;

    struct {
        struct TpAttr attr;
        uint32_t attrBitmap;
        uint32_t resv[4U];
    } rxData;
};

struct RaResponseGetTpAttr {
    struct TpAttr *attr;
    uint32_t *attrBitmap;
};

union OpSetTpAttrData {
    struct {
        unsigned int phyId;
        unsigned int devIndex;
        uint32_t attrBitmap;
        uint64_t tpHandle;
        struct TpAttr attr;
        uint32_t resv[4U];
    } txData;

    struct {
        uint32_t resv[4U];
    } rxData;
};

union OpCtxQpDestroyBatchData {
    struct {
        unsigned int phyId;
        unsigned int devIndex;
        unsigned int ids[HCCP_MAX_QP_DESTROY_BATCH_NUM]; 
        unsigned int num;
        unsigned int rsvd[4U];
    } txData;

    struct {
        unsigned int num;
        unsigned int rsvd[4U];
    } rxData;
};

struct RaResponseEidList {
    union HccpEid *eidList;
    unsigned int *num;
};

int RaHdcGetEidByIpAsync(struct RaCtxHandle *ctxHandle, struct IpInfo ip[], union HccpEid eid[],
    unsigned int *num, void **reqHandle);
void RaHdcAsyncHandleGetEidByIp(struct RaRequestHandle *reqHandle);
int RaHdcCtxLmemRegisterAsync(struct RaCtxHandle *ctxHandle, struct MrRegInfoT *lmemInfo,
    struct RaLmemHandle *lmemHandle, void **reqHandle);
void RaHdcAsyncHandleLmemRegister(struct RaRequestHandle *reqHandle);
int RaHdcCtxLmemUnregisterAsync(struct RaCtxHandle *ctxHandle, struct RaLmemHandle *lmemHandle,
    void **reqHandle);
int RaHdcCtxQpCreateAsync(struct RaCtxHandle *ctxHandle, struct QpCreateAttr *attr,
	struct QpCreateInfo *info, struct RaCtxQpHandle *qpHandle, void **reqHandle);
void RaHdcAsyncHandleQpCreate(struct RaRequestHandle *reqHandle);
int RaHdcCtxQpDestroyAsync(struct RaCtxQpHandle *qpHandle, void **reqHandle);
int RaHdcCtxQpImportAsync(struct RaCtxHandle *ctxHandle, struct QpImportInfoT *info,
    struct RaCtxRemQpHandle *remQpHandle, void **reqHandle);
void RaHdcAsyncHandleQpImport(struct RaRequestHandle *reqHandle);
int RaHdcCtxQpUnimportAsync(struct RaCtxRemQpHandle *remQpHandle, void **reqHandle);
int RaHdcGetTpInfoListAsync(struct RaCtxHandle *ctxHandle, struct GetTpCfg *cfg, struct TpInfo infoList[],
    unsigned int *num, void **reqHandle);
void RaHdcAsyncHandleTpInfoList(struct RaRequestHandle *reqHandle);
int RaHdcGetTpAttrAsync(struct RaCtxHandle *ctxHandle, uint64_t tpHandle, uint32_t *attrBitmap,
    struct TpAttr *attr, void **reqHandle);
void RaHdcAsyncHandleGetTpAttr(struct RaRequestHandle *reqHandle);
int RaHdcSetTpAttrAsync(struct RaCtxHandle *ctxHandle, uint64_t tpHandle, uint32_t attrBitmap,
    struct TpAttr *attr, void **reqHandle);
int RaHdcCtxQpDestroyBatchAsync(struct RaCtxHandle *ctxHandle, void *qpHandle[],
    unsigned int *num, void **reqHandle);
void RaHdcAsyncHandleQpDestroyBatch(struct RaRequestHandle *reqHandle);
#endif // RA_HDC_ASYNC_CTX_H
