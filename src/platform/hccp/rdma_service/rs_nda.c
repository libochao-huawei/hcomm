/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "dl_nda_function.h"
#include "hccp_nda.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_drv_rdma.h"
#include "rs_rdma.h"
#include "rs_nda.h"

RS_ATTRI_VISI_DEF int RsNdaGetDirectFlag(unsigned int phyId, unsigned int rdevIndex, int *directFlag)
{
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    *directFlag = RsNdaGetDirectFlagByVendorId(rdevCb->deviceAttr.vendor_id);
    return ret;
}

int RsInitNdaCb(struct RsRdevCb *rdevCb)
{
    struct RsNdaCb *ndaCb = NULL;
    int ret = 0;

    rdevCb->ibCtxEx = RsNdaIbvOpenExtend(rdevCb->ibCtx);
    if (rdevCb->ibCtxEx == NULL) {
        return 0;
    }

    ndaCb = (struct RsNdaCb *)calloc(1, sizeof(struct RsNdaCb));
    if (ndaCb == NULL) {
        hccp_err("calloc for ndaCb failed");
        ret = -ENOMEM;
        goto calloc_err;
    }
    ret = pthread_mutex_init(&ndaCb->ndaMutex, NULL);
    if (ret != 0) {
        hccp_err("ndaCb mutex_init failed ret:%d", ret);
        ret = -ESYSFUNC;
        goto mutex_err;
    }

    RS_INIT_LIST_HEAD(&ndaCb->ndaDbHostList);
    RS_INIT_LIST_HEAD(&ndaCb->ndaDbGuidList);
    rdevCb->rsCb->ndaCb = ndaCb;
    return ret;

mutex_err:
    free(ndaCb);
    ndaCb = NULL;
calloc_err:
    (void)RsNdaIbvCloseExtend(rdevCb->ibCtxEx);
    rdevCb->ibCtxEx = NULL;
    return ret;
}

void RsFreeNdaCb(struct RsRdevCb *rdevCb)
{
    if (rdevCb->rsCb->ndaCb == NULL) {
        return;
    }

    (void)pthread_mutex_destroy(&rdevCb->rsCb->ndaCb->ndaMutex);
    free(rdevCb->rsCb->ndaCb);
    rdevCb->rsCb->ndaCb = NULL;
    (void)RsNdaIbvCloseExtend(rdevCb->ibCtxEx);
    rdevCb->ibCtxEx = NULL;
}

STATIC int RsGetNdaDbCb(struct RsNdaCb *ndaCb, uint64_t hva, struct NdaPcieDbCb **ndaDbCb)
{
    struct NdaPcieDbCb *ndaDbCbCurr = NULL;
    struct NdaPcieDbCb *ndaDbCbNext = NULL;

    RS_LIST_GET_HEAD_ENTRY(ndaDbCbCurr, ndaDbCbNext, &ndaCb->ndaDbHostList, list, struct NdaPcieDbCb);
    for (; (&ndaDbCbCurr->list) != &ndaCb->ndaDbHostList;
        ndaDbCbCurr = ndaDbCbNext, ndaDbCbNext = list_entry(ndaDbCbNext->list.next, struct NdaPcieDbCb, list)) {
        if (ndaDbCbCurr->hva == hva) {
            *ndaDbCb = ndaDbCbCurr;
            return 0;
        }
    }

    *ndaDbCb = NULL;
    hccp_info("ndaDbCb for hva:%u do not exist", hva);
    return -EEXIST;
}

STATIC int RsGetNdaGuidCb(struct RsNdaCb *ndaCb, uint64_t guidL, uint64_t guidH, struct NdaUbDbCb **ndaGuidCb)
{
    struct NdaUbDbCb *ndaGuidCbCurr = NULL;
    struct NdaUbDbCb *ndaGuidCbNext = NULL;

    RS_LIST_GET_HEAD_ENTRY(ndaGuidCbCurr, ndaGuidCbNext, &ndaCb->ndaDbGuidList, list, struct NdaUbDbCb);
    for (; (&ndaGuidCbCurr->list) != &ndaCb->ndaDbGuidList;
        ndaGuidCbCurr = ndaGuidCbNext, ndaGuidCbNext = list_entry(ndaGuidCbNext->list.next, struct NdaUbDbCb, list)) {
        if (ndaGuidCbCurr->guidL == guidL && ndaGuidCbCurr->guidH == guidH) {
            *ndaGuidCb = ndaGuidCbCurr;
            return 0;
        }
    }

    *ndaGuidCb = NULL;
    hccp_info("ndaGuidCb for guidL:0x%llx guidH:0x%llx do not exist", guidL, guidH);
    return -EEXIST;
}

STATIC void *RsNdaUbAlloc(size_t size)
{
    struct RsNdaCb *ndaCb = gRsCb->ndaCb;
    struct DVattribute attr = {0};
    void *ptr = NULL;
    int ret = 0;

    CHK_PRT_RETURN(ndaCb == NULL, hccp_err("gRsCb->ndaCb is NULL"), NULL);
    CHK_PRT_RETURN(ndaCb->ndaOps.alloc == NULL, hccp_err("ndaOps.alloc is NULL"), NULL);
    CHK_PRT_RETURN(ndaCb->ndaOps.free == NULL, hccp_err("ndaOps.free is NULL"), NULL);

    ptr = ndaCb->ndaOps.alloc(size);
    CHK_PRT_RETURN(ptr == NULL, hccp_err("ptr alloc failed"), NULL);

    ret = DlDrvMemGetAttribute((uint64_t)(uintptr_t)ptr, &attr);
    if (ret != 0) {
        hccp_err("DlDrvMemGetAttribute failed, ret:%d", ret);
        goto free_ptr;
    }

    hccp_run_info("attr.memtype:%u, attr.devId:%u", attr.memType, attr.devId);

    // if (attr.memType == DV_MEM_SVM_DEVICE) {
    ret = DlHalMemRegUbSegment(attr.devId, (uint64_t)(uintptr_t)ptr, size);
    if (ret != 0) {
        hccp_err("DlHalMemRegUbSegment failed, ret:%d", ret);
        goto free_ptr;
    }
    // }

    return ptr;

free_ptr:
    ndaCb->ndaOps.free(ptr);
    ptr = NULL;
    return NULL;
}

STATIC void RsNdaUbFree(void *ptr)
{
    struct RsNdaCb *ndaCb = gRsCb->ndaCb;
    struct DVattribute attr = {0};
    int ret = 0;

    if (ndaCb == NULL || ndaCb->ndaOps.free == NULL) {
        hccp_err("gRsCb->ndaCb or ndaOps.free is NULL");
        return;
    }

    ret = DlDrvMemGetAttribute((uint64_t)(uintptr_t)ptr, &attr);
    if (ret != 0) {
        hccp_err("DlDrvMemGetAttribute failed, ret:%d", ret);
        return;
    }

    if (attr.memType == DV_MEM_SVM_DEVICE) {
        (void)DlHalMemUnRegUbSegment(attr.devId, (uint64_t)(uintptr_t)ptr);
    }

    ndaCb->ndaOps.free(ptr);
    ptr = NULL;
}

STATIC void *RsNdaDbMmapHostVa(struct RsNdaCb *ndaCb, struct doorbell_map_desc *desc)
{
    uint64_t alignHva = AlignDown(desc->hva, RA_RS_4K_PAGE_SIZE);
    uint64_t alignSize = AlignUp(desc->size, RA_RS_4K_PAGE_SIZE);
    struct NdaPcieDbCb *ndaDbCb = NULL;
    unsigned int devId = 0;
    void *dbDva = NULL;
    int ret = 0;

    ret = RsGetNdaDbCb(ndaCb, desc->hva, &ndaDbCb);
    if (ret == -EEXIST) {
        ret = DlDrvDeviceGetIndexByPhyId(gRsCb->chipId, &devId);
        CHK_PRT_RETURN(ret != 0, hccp_err("get devId failed, chipId:%u, ret:%d", gRsCb->chipId, ret), NULL);

        ndaDbCb = (struct NdaPcieDbCb *)calloc(1, sizeof(struct NdaPcieDbCb));
        CHK_PRT_RETURN(ndaDbCb == NULL, hccp_err("ndaDbCb calloc failed"), NULL);
        ret = DlHalHostRegister((void *)(uintptr_t)alignHva, alignSize, HOST_IO_MAP_DEV, devId,
            &dbDva);
        if (ret != 0) {
            hccp_err("register host failed, chipId:%u, ret:%d", gRsCb->chipId, ret);
            free(ndaDbCb);
            ndaDbCb = NULL;
            return NULL;
        }
        ndaDbCb->hva = desc->hva;
        ndaDbCb->dva = (uint64_t )(uintptr_t)dbDva;
        RS_PTHREAD_MUTEX_LOCK(&ndaCb->ndaMutex);
        ndaDbCb->refCnt++;
        RsListAddTail(&ndaDbCb->list, &ndaCb->ndaDbHostList);
        RS_PTHREAD_MUTEX_ULOCK(&ndaCb->ndaMutex);
        return (void *)(uintptr_t)ndaDbCb->dva;
    } else if (ret == 0) {
        RS_PTHREAD_MUTEX_LOCK(&ndaCb->ndaMutex);
        ndaDbCb->refCnt++;
        RS_PTHREAD_MUTEX_ULOCK(&ndaCb->ndaMutex);
        return (void *)(uintptr_t)ndaDbCb->dva;
    } else {
        hccp_err("RsGetNdaDbCb failed, chipId:%u, hva:0x%llx", gRsCb->chipId, desc->hva);
        return NULL;
    }
}

STATIC void RsNdaMapPrivPrepare(struct doorbell_map_desc *desc, struct NdaResMapPrivInfo *resMapIn)
{
    resMapIn->guid_l = desc->ub_res.guid_l;
    resMapIn->guid_h = desc->ub_res.guid_h;
    resMapIn->db_idx = desc->ub_res.bits.offset / RA_RS_4K_PAGE_SIZE;
    resMapIn->db_num = 1;
    return;
}

STATIC void *RsNdaDbMmapUbRes(struct RsNdaCb *ndaCb, struct doorbell_map_desc *desc)
{
    struct res_map_info_out resInfoOut = {0};
    struct NdaResMapPrivInfo resMapIn = {0};
    struct res_map_info_in resInfoIn = {0};
    struct NdaUbDbCb *ndaGuidCb = NULL;
    unsigned int devId = 0;
    int ret = 0;

    ret = RsGetNdaGuidCb(ndaCb, desc->ub_res.guid_l, desc->ub_res.guid_h, &ndaGuidCb);
    if (ret == -EEXIST) {
        ret = DlDrvDeviceGetIndexByPhyId(gRsCb->chipId, &devId);
        CHK_PRT_RETURN(ret != 0, hccp_err("get devId failed, chipId:%u, ret:%d", gRsCb->chipId, ret), NULL);

        ndaGuidCb = (struct NdaUbDbCb *)calloc(1, sizeof(struct NdaUbDbCb));
        CHK_PRT_RETURN(ndaGuidCb == NULL, hccp_err("ndaGuidCb calloc failed"), NULL);

        RsNdaMapPrivPrepare(desc, &resMapIn);
        resInfoIn.target_proc_type = PROCESS_CP1;
        resInfoIn.res_type = RES_ADDR_TYPE_NDA_URMA_DB;
        resInfoIn.res_id = RsNdaGenerateResId(resMapIn.db_idx, ndaCb->ndaDbGuidCnt);
        resInfoIn.priv_len = sizeof(struct NdaResMapPrivInfo);
        resInfoIn.priv = (void *)&resMapIn;
        ret = DlHalResAddrMapV2(devId, &resInfoIn, &resInfoOut);
        if (ret != 0) {
            hccp_err("map resAddr failed, chipId:%u, ret:%d", gRsCb->chipId, ret);
            free(ndaGuidCb);
            ndaGuidCb = NULL;
            return NULL;
        }
        ndaGuidCb->dva = resInfoOut.va + (desc->ub_res.bits.offset % RA_RS_4K_PAGE_SIZE);
        ndaGuidCb->guidL = desc->ub_res.guid_l;
        ndaGuidCb->guidH = desc->ub_res.guid_h;
        ndaGuidCb->guidIdx = ndaCb->ndaDbGuidCnt;
        RS_PTHREAD_MUTEX_LOCK(&ndaCb->ndaMutex);
        ndaGuidCb->refCnt++;
        RsListAddTail(&ndaGuidCb->list, &ndaCb->ndaDbGuidList);
        ndaCb->ndaDbGuidCnt++;
        RS_PTHREAD_MUTEX_ULOCK(&ndaCb->ndaMutex);
        return (void *)(uintptr_t)ndaGuidCb->dva;
    } else if (ret == 0) {
        RS_PTHREAD_MUTEX_LOCK(&ndaCb->ndaMutex);
        ndaGuidCb->refCnt++;
        RS_PTHREAD_MUTEX_ULOCK(&ndaCb->ndaMutex);
        return (void *)(uintptr_t)ndaGuidCb->dva;
    } else {
        hccp_err("RsGetNdaGuidCb failed, chipId:%u guidL:0x%llx guidH:0x%llx",
            gRsCb->chipId, desc->ub_res.guid_l, desc->ub_res.guid_h);
        return NULL;
    }
}

STATIC void *RsNdaDbMmap(struct doorbell_map_desc *desc)
{
    struct RsNdaCb *ndaCb = NULL;

    CHK_PRT_RETURN(gRsCb == NULL, hccp_err("gRsCb is null"), NULL);
    ndaCb = gRsCb->ndaCb;
    CHK_PRT_RETURN(ndaCb == NULL, hccp_err("ndaCb is null, chipId:%u", gRsCb->chipId), NULL);

    if (desc->type == DB_MAP_MODE_HOST_VA) {
        return RsNdaDbMmapHostVa(ndaCb, desc);
    } else if (desc->type == DB_MAP_MODE_UB_RES) {
        return RsNdaDbMmapUbRes(ndaCb, desc);
    } else {
        hccp_err("invalid desc->type:%u, chipId:%u", desc->type, gRsCb->chipId);
        return NULL;
    }
}

STATIC int RsNdaDbUnmapHostVa(struct RsNdaCb *ndaCb, void *ptr, struct doorbell_map_desc *desc)
{
    uint64_t alignHva = AlignDown(desc->hva, RA_RS_4K_PAGE_SIZE);
    struct NdaPcieDbCb *ndaDbCb = NULL;
    unsigned int devId = 0;
    int ret = 0;

    ret = RsGetNdaDbCb(ndaCb, desc->hva, &ndaDbCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsGetNdaDbCb failed, hva:0x%llx chipId:%u ret:%d",
        desc->hva, gRsCb->chipId, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&ndaCb->ndaMutex);
    ndaDbCb->refCnt--;
    if (ndaDbCb->refCnt == 0) {
        ret = DlDrvDeviceGetIndexByPhyId(gRsCb->chipId, &devId);
        if (ret != 0) {
            hccp_err("get devId failed, chipId:%u, ret:%d", gRsCb->chipId, ret);
            goto free_db_cb;
        }
        ret = DlHalHostUnRegisterEx((void *)(uintptr_t)alignHva, devId, HOST_IO_MAP_DEV);
        if (ret != 0) {
            hccp_err("DlHalHostUnRegisterEx failed, chipId:%u, ret:%d", gRsCb->chipId, ret);
        }
free_db_cb:
        RsListDel(&ndaDbCb->list);
        free(ndaDbCb);
        ndaDbCb = NULL;
    }

    RS_PTHREAD_MUTEX_ULOCK(&ndaCb->ndaMutex);
    return ret;
}

STATIC int RsNdaDbUnmapUbRes(struct RsNdaCb *ndaCb, void *ptr, struct doorbell_map_desc *desc)
{
    struct NdaResMapPrivInfo resMapIn = {0};
    struct res_map_info_in resInfoIn = {0};
    struct NdaUbDbCb *ndaGuidCb = NULL;
    unsigned int devId = 0;
    int ret = 0;

    ret = RsGetNdaGuidCb(ndaCb, desc->ub_res.guid_l, desc->ub_res.guid_h, &ndaGuidCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsGetNdaGuidCb failed, chipId:%u guidL:0x%llx guidH:0x%llx",
        gRsCb->chipId, desc->ub_res.guid_l, desc->ub_res.guid_h), ret);

    RS_PTHREAD_MUTEX_LOCK(&ndaCb->ndaMutex);
    ndaGuidCb->refCnt--;
    if (ndaGuidCb->refCnt == 0) {
        ret = DlDrvDeviceGetIndexByPhyId(gRsCb->chipId, &devId);
        if (ret != 0) {
            hccp_err("get devId failed, chipId:%u, ret:%d", gRsCb->chipId, ret);
            goto free_guid_cb;
        }
        RsNdaMapPrivPrepare(desc, &resMapIn);
        resInfoIn.target_proc_type = PROCESS_CP1;
        resInfoIn.res_type = RES_ADDR_TYPE_HCCP_URMA_DB;
        resInfoIn.res_id = RsNdaGenerateResId(resMapIn.db_idx, ndaGuidCb->guidIdx);
        resInfoIn.priv_len = sizeof(struct NdaResMapPrivInfo);
        resInfoIn.priv = (void *)&resMapIn;
        ret = DlHalResAddrUnmapV2(devId, &resInfoIn);
        if (ret != 0) {
            hccp_err("DlHalResAddrUnmapV2 failed, chipId:%u, ret:%d", gRsCb->chipId, ret);
        }
free_guid_cb:
        RsListDel(&ndaGuidCb->list);
        free(ndaGuidCb);
        ndaGuidCb = NULL;
    }

    RS_PTHREAD_MUTEX_ULOCK(&ndaCb->ndaMutex);
    return ret;
}

STATIC int RsNdaDbUnmap(void *ptr, struct doorbell_map_desc *desc)
{
    struct RsNdaCb *ndaCb = NULL;

    CHK_PRT_RETURN(gRsCb == NULL, hccp_err("gRsCb is null"), -ENODEV);
    ndaCb = gRsCb->ndaCb;
    CHK_PRT_RETURN(ndaCb == NULL, hccp_err("ndaCb is null, chipId:%u", gRsCb->chipId), -ENODEV);

    if (desc->type == DB_MAP_MODE_HOST_VA) {
        return RsNdaDbUnmapHostVa(ndaCb, ptr, desc);
    } else if (desc->type == DB_MAP_MODE_UB_RES) {
        return RsNdaDbUnmapUbRes(ndaCb, ptr, desc);
    } else {
        hccp_err("invalid desc->type:%u, chipId:%u", desc->type, gRsCb->chipId);
        return -EINVAL;
    }
}

STATIC void RsNdaInitExOps(struct RsNdaCb *ndaCb, uint32_t dmaMode, struct NdaOps *ops)
{
    if (dmaMode == QBUF_DMA_MODE_DEFAULT) {
        ndaCb->ibvExOps.alloc = ops->alloc;
        ndaCb->ibvExOps.free = ops->free;
    } else if (dmaMode == QBUF_DMA_MODE_INDEP_UB) {
        ndaCb->ndaOps.alloc = ops->alloc;
        ndaCb->ndaOps.free = ops->free;
        ndaCb->ibvExOps.alloc = RsNdaUbAlloc;
        ndaCb->ibvExOps.free = RsNdaUbFree;
    }
    ndaCb->ibvExOps.db_mmap = RsNdaDbMmap;
    ndaCb->ibvExOps.db_unmap = RsNdaDbUnmap;
    ndaCb->ibvExOps.memset_s = ops->memset_s;
    ndaCb->ibvExOps.memcpy_s = ops->memcpy_s;
}

STATIC void RsNdaCqInitExPrepare(struct NdaCqInitAttr *attr, struct RsNdaCb *ndaCb,
    struct ibv_cq_init_attr_extend *cqInitAttrEx)
{
    RsNdaInitExOps(ndaCb, attr->dmaMode, attr->ops);

    (void)memcpy_s(&cqInitAttrEx->attr, sizeof(struct ibv_cq_init_attr_ex), &attr->attr, sizeof(struct ibv_cq_init_attr_ex));
    cqInitAttrEx->cq_cap_flag = attr->cqCapFlag;
    cqInitAttrEx->type = attr->dmaMode;
    cqInitAttrEx->ops = &ndaCb->ibvExOps;
}

STATIC int RsNdaCqCreateEx(struct RsRdevCb *rdevCb, struct ibv_cq_init_attr_extend *cqInitAttrEx, struct NdaCqInfo *info,
    void **ibvCqExt)
{
    struct ibv_cq_extend *cqExt = NULL;

    cqExt = RsNdaIbvCreateCqExtend(rdevCb->ibCtxEx, cqInitAttrEx);
    CHK_PRT_RETURN(cqExt == NULL, hccp_err("RsNdaCreateCqExtend failed, errno:%d", errno), -ENOMEM);

    info->cq = cqExt->cq;
    (void)memcpy_s(&info->cqInfo, sizeof(struct queue_info), &cqExt->cq_info, sizeof(struct queue_info));
    *ibvCqExt = cqExt;

    return 0;
}

RS_ATTRI_VISI_DEF int RsNdaCqCreate(unsigned int phyId, unsigned int rdevIndex, struct NdaCqInitAttr *attr, 
    struct NdaCqInfo *info, void **ibvCqExt)
{
    struct ibv_cq_init_attr_extend cqInitAttrEx = {0};
    struct RsNdaCb *ndaCb = gRsCb->ndaCb;
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    CHK_PRT_RETURN(attr == NULL || info == NULL, hccp_err("attr or info is NULL, phyId:%u", phyId), -EINVAL);
    CHK_PRT_RETURN(attr->dmaMode >= QBUF_DMA_MODE_MAX, hccp_err("param err, dmaMode:%u >= %u, phyId:%u",
        attr->dmaMode, QBUF_DMA_MODE_MAX, phyId), -EINVAL);
    CHK_PRT_RETURN(ndaCb == NULL, hccp_err("ndaCb is NULL, do not support nda, phyId:%u", phyId), -ENOTSUPP);

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb failed, phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    RsNdaCqInitExPrepare(attr, ndaCb, &cqInitAttrEx);

    ret = RsNdaCqCreateEx(rdevCb, &cqInitAttrEx, info, ibvCqExt);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsNdaCqCreateEx failed, phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int RsNdaCqDestroy(unsigned int phyId, unsigned int rdevIndex, void *ibvCqExt)
{
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb failed, phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    ret = RsNdaIbvDestroyCqExtend(rdevCb->ibCtxEx, ibvCqExt);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsNdaIbvDestroyCqExtend failed, phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    return ret;
}

STATIC int RsBuildUpNdaQpcb(struct RsRdevCb *rdevCb, struct ibv_qp_init_attr *qpInitAttr, struct RsQpCb **qpCb)
{
    int ret = 0;

    *qpCb = (struct RsQpCb *)calloc(1, sizeof(struct RsQpCb));
    CHK_PRT_RETURN(*qpCb == NULL, hccp_err("RsQpCb calloc failed"), -ENOMEM);

    ret = pthread_mutex_init(&(*qpCb)->qpMutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init failed, ret:%d", ret);
        goto qp_mutex_init_err;
    }

    ret = pthread_mutex_init(&(*qpCb)->cqeErrInfo.mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init failed, ret:%d", ret);
        goto cqe_err_mutex_init_err;
    }

    (*qpCb)->rdevCb = rdevCb;
    RS_INIT_LIST_HEAD(&(*qpCb)->mrList);
    RS_INIT_LIST_HEAD(&(*qpCb)->remMrList);
    (*qpCb)->state = RS_QP_STATUS_DISCONNECT;
    (*qpCb)->ibPd = rdevCb->ibPd;
    (*qpCb)->txDepth = qpInitAttr->cap.max_send_wr;
    (*qpCb)->rxDepth = qpInitAttr->cap.max_recv_wr;

    (*qpCb)->numRecvCqEvents = 0;
    (*qpCb)->numSendCqEvents = 0;
    (*qpCb)->qosAttr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    (*qpCb)->qosAttr.sl = RS_ROCE_4_SL;
    (*qpCb)->timeout = RS_QP_ATTR_TIMEOUT;
    (*qpCb)->retryCnt = RS_QP_ATTR_RETRY_CNT;

    return ret;

cqe_err_mutex_init_err:
    pthread_mutex_destroy(&(*qpCb)->qpMutex);
qp_mutex_init_err:
    free(*qpCb);
    *qpCb = NULL;
    return ret;
}

STATIC void RsNdaQpInitExPrepare(struct RsRdevCb *rdevCb, struct NdaQpInitAttr *attr, struct RsNdaCb *ndaCb,
    struct ibv_qp_init_attr_extend *qpInitAttrEx)
{
    RsNdaInitExOps(ndaCb, attr->dmaMode, attr->ops);

    qpInitAttrEx->pd = rdevCb->ibPd;
    (void)memcpy_s(&qpInitAttrEx->attr, sizeof(struct ibv_qp_init_attr), &attr->attr, sizeof(struct ibv_qp_init_attr));
    qpInitAttrEx->qp_cap_flag = attr->qpCapFlag;
    qpInitAttrEx->type = attr->dmaMode;
    qpInitAttrEx->ops = &ndaCb->ibvExOps;
}

STATIC int RsNdaQpCreateEx(struct RsQpCb *qpCb, struct ibv_qp_init_attr_extend *qpInitAttrEx, struct NdaQpInfo *info)
{
    struct ibv_qp_attr qpAttr = {0};
    struct ibv_port_attr attr = {0};
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    rdevCb = qpCb->rdevCb;

    qpCb->ibQpEx = RsNdaIbvCreateQpExtend(rdevCb->ibCtxEx, qpInitAttrEx);
    CHK_PRT_RETURN(qpCb->ibQpEx == NULL, hccp_err("RsNdaCreateQpExtend failed, errno:%d", errno), -ENOMEM);

    qpCb->ibQp = qpCb->ibQpEx->qp;
    ret = RsIbvQueryQp(qpCb->ibQp, &qpAttr, IBV_QP_CAP, &qpInitAttrEx->attr);
    if (ret != 0) {
        hccp_err("query qp attr failed ret:%d", ret);
        ret = -EOPENSRC;
        goto nda_init_qp_err;
    }

    ret = RsDrvQpInfoRelated(qpCb, rdevCb, &attr, &qpAttr);
    if (ret != 0) {
        hccp_err("qp info related failed ret:%d", ret);
        goto nda_init_qp_err;
    }

    info->qp = qpCb->ibQp;
    (void)memcpy_s(&info->sqInfo, sizeof(struct queue_info), &qpCb->ibQpEx->sq_info, sizeof(struct queue_info));
    (void)memcpy_s(&info->rqInfo, sizeof(struct queue_info), &qpCb->ibQpEx->rq_info, sizeof(struct queue_info));
    hccp_info("chip_id:%u, rdevIndex:%u, qp:%d create succ", qpCb->rdevCb->rsCb->chipId,
        qpCb->rdevCb->rdevIndex, qpCb->qpInfoLo.qpn);
    return ret;

nda_init_qp_err:
    (void)RsNdaIbvDestroyQpExtend(rdevCb->ibCtxEx, qpCb->ibQpEx);
    return ret;
}

RS_ATTRI_VISI_DEF int RsNdaQpCreate(unsigned int phyId, unsigned int rdevIndex, struct NdaQpInitAttr *attr,
    struct NdaQpInfo *info, struct RsQpResp *qpResp)
{
    struct ibv_qp_init_attr_extend qpInitAttrEx = {0};
    struct RsRdevCb *rdevCb = NULL;
    struct RsNdaCb *ndaCb = NULL;
    struct RsQpCb *qpCb = NULL;
    int ret = 0;

    CHK_PRT_RETURN(attr == NULL || info == NULL || qpResp == NULL, hccp_err("attr or info or qpResp is NULL, phyId:%u",
        phyId), -EINVAL);

    CHK_PRT_RETURN(attr->dmaMode >= QBUF_DMA_MODE_MAX, hccp_err("param err, dmaMode:%u >= %u, phyId:%u",
        attr->dmaMode, QBUF_DMA_MODE_MAX, phyId), -EINVAL);

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    ndaCb = gRsCb->ndaCb;
    CHK_PRT_RETURN(ndaCb == NULL, hccp_err("ndaCb is NULL, phyId:%u rdevIndex:%u", phyId, rdevIndex), -ENODEV);

    ret = RsBuildUpNdaQpcb(rdevCb, &attr->attr, &qpCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsBuildUpNdaQpcb failed, ret:%d", ret), ret);

    RsNdaQpInitExPrepare(rdevCb, attr, ndaCb, &qpInitAttrEx);

    ret = RsNdaQpCreateEx(qpCb, &qpInitAttrEx, info);
    if (ret != 0) {
        hccp_err("create nda qp create extend failed, ret:%d", ret);
        goto create_qp_err;
    }

    RS_PTHREAD_MUTEX_LOCK(&rdevCb->rdevMutex);
    RsListAddTail(&qpCb->list, &rdevCb->qpList);
    rdevCb->qpCnt++;
    RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rdevMutex);
    qpResp->qpn = (unsigned int)qpCb->qpInfoLo.qpn;
    qpResp->gidIdx = (unsigned int)qpCb->qpInfoLo.gidIdx;
    qpResp->psn = (unsigned int)qpCb->qpInfoLo.psn;
    qpResp->gid = qpCb->qpInfoLo.gid;

    return 0;

create_qp_err:
    pthread_mutex_destroy(&qpCb->cqeErrInfo.mutex);
    pthread_mutex_destroy(&qpCb->qpMutex);
    free(qpCb);
    qpCb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsNdaQpDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn)
{
    struct RsQpCb *qpCb = NULL;
    int ret = 0;

    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed, qpn:%u, ret:%d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->rdevCb->rdevMutex);
    RsListDel(&qpCb->list);
    qpCb->rdevCb->qpCnt--;
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->rdevCb->rdevMutex);

    ret = RsNdaIbvDestroyQpExtend(qpCb->rdevCb->ibCtxEx, qpCb->ibQpEx);
    if (ret != 0) {
        hccp_err("qp:%d destroy extend failed, ret:%d", qpn, ret);
    }

    pthread_mutex_destroy(&qpCb->cqeErrInfo.mutex);
    pthread_mutex_destroy(&qpCb->qpMutex);
    hccp_info("qp:%d destroy qp, send wr:%u", qpn, qpCb->sendWrNum);

    free(qpCb);
    qpCb = NULL;
    return ret;
}