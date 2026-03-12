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
#include "hccp_nda.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "dl_nda_function.h"
#include "rs_drv_rdma.h"
#include "rs_rdma.h"
#include "rs_nda.h"

RS_ATTRI_VISI_DEF int RsNdaGetDirectFlag(unsigned int phyId, unsigned int rdevIndex, int *directFlag)
{
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb phyId:%u rdev_index:%u ret:%d", phyId, rdevIndex, ret), ret);

    *directFlag = RsNdaGetDirectFlagByVendorId(rdevCb->deviceAttr.vendor_id);
    return ret;
}

int RsGetNdaDbCb(struct rs_cb *rsCb, uint64_t hva, struct NdaDbCb **ndaDbCb)
{
    struct NdaDbCb *ndaDbCbCurr = NULL;
    struct NdaDbCb *ndaDbCbNext = NULL;

    RS_LIST_GET_HEAD_ENTRY(ndaDbCbCurr, ndaDbCbNext, &rsCb->ndaDbHostList, list, struct NdaDbCb);
    for (; (&ndaDbCbCurr->list) != &rsCb->ndaDbHostList;
        ndaDbCbCurr = ndaDbCbNext, ndaDbCbNext = list_entry(ndaDbCbNext->list.next, struct NdaDbCb, list)) {
        if (ndaDbCbCurr->hva == hva) {
            *ndaDbCb = ndaDbCbCurr;
            return 0;
        }
    }

    *ndaDbCb = NULL;
    return -ENODEV;
}

int RsGetNdaGuidCb(struct rs_cb *rsCb, uint64_t guidL, uint64_t guidH, struct NdaGuidCb **ndaGuidCb)
{
    struct NdaGuidCb *ndaGuidCbCurr = NULL;
    struct NdaGuidCb *ndaGuidCbNext = NULL;

    RS_LIST_GET_HEAD_ENTRY(ndaGuidCbCurr, ndaGuidCbNext, &rsCb->ndaDbGuidList, list, struct NdaGuidCb);
    for (; (&ndaGuidCbCurr->list) != &rsCb->ndaDbGuidList;
        ndaGuidCbCurr = ndaGuidCbNext, ndaGuidCbNext = list_entry(ndaGuidCbNext->list.next, struct NdaGuidCb, list)) {
        if (ndaGuidCbCurr->guidL == guidL && ndaGuidCbCurr->guidH == guidH) {
            *ndaGuidCb = ndaGuidCbCurr;
            return 0;
        }
    }

    *ndaGuidCb = NULL;
    return -ENODEV;
}

int RsQueryNdaDbCb(unsigned int phyId, uint64_t hva, struct NdaDbCb **ndaDbCb)
{
    struct rs_cb *rsCb = NULL;
    unsigned int chipId = 0;
    int ret = 0;

    // todo 确认一下需不需要转换成chipid，peer模式下，rscb里的chid就是phyid
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryNdaDbCb phyId:%u invalid, ret:%d", phyId, ret), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryNdaDbCb get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = RsGetNdaDbCb(rsCb, hva, ndaDbCb);
    CHK_PRT_RETURN(ret != 0, hccp_info("ndaDbCb for hva:%u do not exist, ret:%d", hva, ret), -EEXIST);

    return ret;
}

int RsQueryNdaGuidCb(unsigned int phyId, uint64_t guidL, uint64_t guidH, struct NdaGuidCb **ndaGuidCb)
{
    struct rs_cb *rsCb = NULL;
    unsigned int chipId = 0;
    int ret = 0;

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryNdaDbCb phyId:%u invalid, ret:%d", phyId, ret), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryNdaDbCb get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = RsGetNdaGuidCb(rsCb, guidL, guidH, ndaGuidCb);
    CHK_PRT_RETURN(ret != 0, hccp_info("ndaGuidCb for guidL:0x%llx guidH:0x%llx do not exist, ret:%d",
        guidL, guidH, ret), -EEXIST);

    return ret;
}

STATIC void *RsNdaUbAlloc(size_t size)
{
    struct DVattribute attr = {0};
    void *ptr = NULL;
    int ret = 0;

    CHK_PRT_RETURN(gRsCb->ndaOps.alloc == NULL, hccp_err("ndaOps.alloc is NULL"), NULL);

    ptr = gRsCb->ndaOps.alloc(size);

    ret = DlDrvMemGetAttribute((uint64_t)(uintptr_t)ptr, &attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("DlDrvMemGetAttribute failed, ret:%d", ret), NULL);

    if (attr.memType == DV_MEM_SVM_DEVICE) {
        ret = DlHalMemRegUbSegment(attr.devId, (uint64_t)(uintptr_t)ptr, size);
        CHK_PRT_RETURN(ret != 0, hccp_err("DlHalMemRegUbSegment failed, ret:%d", ret), NULL);
    }

    return ptr;
}

STATIC void RsNdaUbFree(void *ptr)
{
    struct DVattribute attr = {0};
    int ret = 0;

    if (gRsCb->ndaOps.free == NULL) {
        hccp_err("ndaOps.free is NULL");
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
    gRsCb->ndaOps.free(ptr);
}

STATIC void *RsNdaDbMmapHostVa(struct doorbell_map_desc *desc)
{
    struct NdaDbCb *ndaDbCb = NULL;
    unsigned int devId = 0;
    int ret = 0;

    ret = RsQueryNdaDbCb(gRsCb->chipId, desc->hva, &ndaDbCb);
    if (ret == -EEXIST) {
        // todo 确认需不需要通过phyid转成chipid
        ret = rsGetLocalDevIDByHostDevID(gRsCb->chipId, &devId);
        CHK_PRT_RETURN(ret != 0, hccp_err("get devId failed, phyId:%u, ret:%d", gRsCb->chipId, ret), NULL);

        ndaDbCb = (struct NdaDbCb *)calloc(1, sizeof(struct NdaDbCb));
        CHK_PRT_RETURN(ndaDbCb == NULL, hccp_err("ndaDbCb calloc failed"), NULL);

        // todo 确认一下flag填啥
        ret = DlHalHostRegister((void *)(uintptr_t)desc->hva, desc->size, HOST_SVM_MAP_DEV, devId,
            (void**)&ndaDbCb->dva);
        if (ret != 0) {
            hccp_err("register host failed, phyId:%u, ret:%d", gRsCb->chipId, ret);
            free(ndaDbCb);
            ndaDbCb = NULL;
            return NULL;
        }
        ndaDbCb->hva = desc->hva;
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->ndaMutex);
        ndaDbCb->refCnt++;
        RsListAddTail(&ndaDbCb->list, &gRsCb->ndaDbHostList);
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->ndaMutex);
        return (void *)(uintptr_t)ndaDbCb->dva;
    } else if (ret == 0) {
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->ndaMutex);
        ndaDbCb->refCnt++;
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->ndaMutex);
        return (void *)(uintptr_t)ndaDbCb->dva;
    } else {
        hccp_err("RsQueryNdaDbCb failed, phyId:%u hva:0x%llx", gRsCb->chipId, desc->hva);
        return NULL;
    }
}

STATIC void RsNdaMapPrivPrepare(struct doorbell_map_desc *desc, struct ascend_nda_res_map_in *resMapIn)
{
    resMapIn->guid_l = desc->ub_res.guid_l;
    resMapIn->guid_h = desc->ub_res.guid_h;
    resMapIn->db_idx = desc->ub_res.bits.offset / RS_NDA_4K;
    resMapIn->db_num = 1;
    return;
}

STATIC void *RsNdaDbMmapUbRes(struct doorbell_map_desc *desc)
{
    struct ascend_nda_res_map_in resMapIn = {0};
    struct res_map_info_out resInfoOut = {0};
    struct res_map_info_in resInfoIn = {0};
    struct NdaGuidCb *ndaGuidCb = NULL;
    unsigned int resId = 0;
    unsigned int devId = 0;
    int ret = 0;

    ret = RsQueryNdaGuidCb(gRsCb->chipId, desc->ub_res.guid_l, desc->ub_res.guid_h, &ndaGuidCb);
    if (ret == -EEXIST) {
        // todo 确认需不需要通过phyid转成chipid
        ret = rsGetLocalDevIDByHostDevID(gRsCb->chipId, &devId);
        CHK_PRT_RETURN(ret != 0, hccp_err("get devId failed, phyId:%u, ret:%d", gRsCb->chipId, ret), NULL);

        ndaGuidCb = (struct NdaGuidCb *)calloc(1, sizeof(struct NdaGuidCb));
        CHK_PRT_RETURN(ndaGuidCb == NULL, hccp_err("ndaGuidCb calloc failed"), NULL);

        RsNdaMapPrivPrepare(desc, &resMapIn);
        resId = RsNdaGenerateResId(resMapIn.db_idx, gRsCb->ndaDbGuidCnt);
        resInfoIn.target_proc_type = PROCESS_CP1;
        resInfoIn.res_type = RES_ADDR_TYPE_HCCP_URMA_DB;
        resInfoIn.res_id = resId;
        resInfoIn.priv_len = sizeof(struct ascend_nda_res_map_in);
        resInfoIn.priv = (void *)&resMapIn;
        ret = DlHalResAddrMapV2(devId, &resInfoIn, &resInfoOut);
        if (ret != 0) {
            hccp_err("map resAddr failed, phyId:%u, ret:%d", gRsCb->chipId, ret);
            free(ndaGuidCb);
            ndaGuidCb = NULL;
            return NULL;
        }
        ndaGuidCb->dva = resInfoOut.va + (desc->ub_res.bits.offset % RS_NDA_4K);
        ndaGuidCb->guidL = desc->ub_res.guid_l;
        ndaGuidCb->guidH = desc->ub_res.guid_h;
        ndaGuidCb->guidIdx = gRsCb->ndaDbGuidCnt;
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->ndaMutex);
        ndaGuidCb->refCnt++;
        RsListAddTail(&ndaGuidCb->list, &gRsCb->ndaDbGuidList);
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->ndaMutex);
        gRsCb->ndaDbGuidCnt++;
        return (void *)(uintptr_t)ndaGuidCb->dva;
    } else if (ret == 0) {
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->ndaMutex);
        ndaGuidCb->refCnt++;
        RsListAddTail(&ndaGuidCb->list, &gRsCb->ndaDbGuidList);
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->ndaMutex);
        return (void *)(uintptr_t)ndaGuidCb->dva;
    } else {
        hccp_err("RsGetNdaGuidCb failed, phyId:%u guidL:0x%llx guidH:0x%llx",
            gRsCb->chipId, desc->ub_res.guid_l, desc->ub_res.guid_h);
        return NULL;
    }
}

STATIC void *RsNdaDbMmap(struct doorbell_map_desc *desc)
{
    if (desc->type == DB_MAP_MODE_HOST_VA) {
        return RsNdaDbMmapHostVa(desc);
    } else if (desc->type == DB_MAP_MODE_UB_RES) {
        return RsNdaDbMmapUbRes(desc);
    } else {
        hccp_err("invalid desc->type:%u, phyId:%u", desc->type, gRsCb->chipId);
        return NULL;
    }
}

STATIC int RsNdaDbUnmapHostVa(void *ptr, struct doorbell_map_desc *desc)
{
    uint64_t dva = (uint64_t)(uintptr_t)ptr;
    struct NdaDbCb *ndaDbCb = NULL;
    unsigned int devId = 0;
    int ret = 0;

    ret = RsQueryNdaDbCb(gRsCb->chipId, desc->hva, &ndaDbCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryNdaDbCb failed, hva:0x%llu phyId:%u ret:%d",
        desc->hva, gRsCb->chipId, ret), ret);

    CHK_PRT_RETURN(dva != ndaDbCb->dva, hccp_err("dva:0x%llx is not equal to ndaDbCb->dva:0x%llx, phyId:%u",
        dva, ndaDbCb->dva, gRsCb->chipId), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&gRsCb->ndaMutex);
    ndaDbCb->refCnt--;
    if (ndaDbCb->refCnt == 0) {
        ret = rsGetLocalDevIDByHostDevID(gRsCb->chipId, &devId);
        if (ret != 0) {
            hccp_err("get devId failed, phyId:%u, ret:%d", gRsCb->chipId, ret);
            goto out;
        }
        // todo 确认传入dva还是hva
        ret = DlHalHostUnRegister((void *)(uintptr_t)ndaDbCb->dva, devId);
        if (ret != 0) {
            hccp_err("DlHalHostUnRegister failed, phyId:%u, ret:%d", gRsCb->chipId, ret);
        }
        RsListDel(&ndaDbCb->list);
    }

out:
    RS_PTHREAD_MUTEX_ULOCK(&gRsCb->ndaMutex);
    return ret;
}

STATIC int RsNdaDbUnmapUbRes(void *ptr, struct doorbell_map_desc *desc)
{
    struct ascend_nda_res_map_in resMapIn = {0};
    uint64_t dva = (uint64_t)(uintptr_t)ptr;
    struct res_map_info_in resInfoIn = {0};
    struct NdaGuidCb *ndaGuidCb = NULL;
    unsigned int devId = 0;
    unsigned int resId = 0;
    int ret = 0;

    ret = RsQueryNdaGuidCb(gRsCb->chipId, desc->ub_res.guid_l, desc->ub_res.guid_h, &ndaGuidCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsGetNdaGuidCb failed, phyId:%u guidL:0x%llx guidH:0x%llx",
        gRsCb->chipId, desc->ub_res.guid_l, desc->ub_res.guid_h), ret);

    // todo 确认一下要不要做dva校验
    CHK_PRT_RETURN(dva != ndaGuidCb->dva, hccp_err("dva:0x%llx is not equal to ndaGuidCb->dva:0x%llx, phyId:%u",
        dva, ndaGuidCb->dva, gRsCb->chipId), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&gRsCb->ndaMutex);
    ndaGuidCb->refCnt--;
    if (ndaGuidCb->refCnt == 0) {
        ret = rsGetLocalDevIDByHostDevID(gRsCb->chipId, &devId);
        if (ret != 0) {
            hccp_err("get devId failed, phyId:%u, ret:%d", gRsCb->chipId, ret);
            goto out;
        }
        RsNdaMapPrivPrepare(desc, &resMapIn);
        resId = RsNdaGenerateResId(resMapIn.db_idx, ndaGuidCb->guidIdx);
        resInfoIn.target_proc_type = PROCESS_CP1;
        resInfoIn.res_type = RES_ADDR_TYPE_HCCP_URMA_DB;
        resInfoIn.res_id = resId;
        resInfoIn.priv_len = sizeof(struct ascend_nda_res_map_in);
        resInfoIn.priv = (void *)&resMapIn;
        ret = DlHalResAddrUnmapV2(devId, &resInfoIn);
        if (ret != 0) {
            hccp_err("DlHalResAddrUnmapV2 failed, phyId:%u, ret:%d", gRsCb->chipId, ret);
        }
        RsListDel(&ndaGuidCb->list);
    }

out:
    RS_PTHREAD_MUTEX_ULOCK(&gRsCb->ndaMutex);
    return ret;
}

STATIC int RsNdaDbUnmap(void *ptr, struct doorbell_map_desc *desc)
{
    if (desc->type == DB_MAP_MODE_HOST_VA) {
        return RsNdaDbUnmapHostVa(ptr, desc);
    } else if (desc->type == DB_MAP_MODE_UB_RES) {
        return RsNdaDbUnmapUbRes(ptr, desc);
    } else {
        hccp_err("invalid desc->type:%u, phyId:%u", desc->type, gRsCb->chipId);
        return -EINVAL;
    }
}

STATIC void RsNdaQpInitExPrepare(struct RsRdevCb *rdevCb, struct NdaQpInitAttr *attr,
    struct ibv_qp_init_attr_extend *qpInitAttrEx)
{
    gRsCb->ndaOps.alloc = attr->ops->alloc;
    gRsCb->ndaOps.free = attr->ops->free;
    if (attr->dmaMode == QBUF_DMA_MODE_DEFAULT) {
        gRsCb->ibvExOps.alloc = attr->ops->alloc;
        gRsCb->ibvExOps.free = attr->ops->free;
    } else if (attr->dmaMode == QBUF_DMA_MODE_INDEP_UB) {
        gRsCb->ibvExOps.alloc = RsNdaUbAlloc;
        gRsCb->ibvExOps.free = RsNdaUbFree;
    }
    gRsCb->ibvExOps.memset_s = attr->ops->memset_s;
    gRsCb->ibvExOps.memcpy_s = attr->ops->memcpy_s;
    gRsCb->ibvExOps.db_mmap = RsNdaDbMmap;
    gRsCb->ibvExOps.db_unmap = RsNdaDbUnmap;

    qpInitAttrEx->pd = rdevCb->ibPd;
    (void)memcpy_s(&qpInitAttrEx->attr, sizeof(struct ibv_qp_init_attr), &attr->attr, sizeof(struct ibv_qp_init_attr));
    qpInitAttrEx->qp_cap_flag = attr->qpCapFlag;
    qpInitAttrEx->type = attr->dmaMode;
    qpInitAttrEx->ops = &gRsCb->ibvExOps;
}

STATIC int RsNdaQpCreateEx(struct RsQpCb *qpCb, struct ibv_qp_init_attr_extend *qpInitAttrEx, struct NdaQpInfo *info)
{
    struct ibv_qp_attr qpAttr = {0};
    struct ibv_port_attr attr = {0};
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    hccp_dbg("RsNdaQpCreateEx begin..");
    rdevCb = qpCb->rdevCb;

    qpCb->ibQpEx = RsNdaCreateQpExtend(rdevCb->ibCtxEx, qpInitAttrEx);
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
    hccp_info("chip_id:%u, rdevIndex:%u, qp:%d create succ.", qpCb->rdevCb->rsCb->chipId,
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
    struct RsCqContext *cqContext = NULL;
    struct RsRdevCb *rdevCb = NULL;
    struct RsQpCb *qpCb = NULL;
    int ret = 0;

    CHK_PRT_RETURN(attr == NULL || info == NULL, hccp_err("attr or info is NULL, phyId:%u", phyId), -EINVAL);

    CHK_PRT_RETURN(attr->dmaMode >= QBUF_DMA_MODE_MAX, hccp_err("param err, dmaMode:%u >= %u, phyId:%u",
        attr->dmaMode, QBUF_DMA_MODE_MAX, phyId), -EINVAL);

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    cqContext = attr->attr.qp_context;
    CHK_PRT_RETURN(cqContext == NULL, hccp_err("cqContext is NULL!"), -EINVAL);
    CHK_PRT_RETURN(rdevCb != cqContext->rdevCb, hccp_err("RsQueryRdevCb phyId:%u rdevIndex:%u,"
        "rdevCb is invalid.", phyId, rdevIndex), -EINVAL);

    ret = RsBuildUpQpcb(cqContext, &attr->attr, &qpCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsBuildUpQpcb failed, ret:%d", ret), ret);

    RsNdaQpInitExPrepare(rdevCb, attr, &qpInitAttrEx);

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

    hccp_info("qp %d create qp.", qpResp->qpn);
    return 0;

create_qp_err:
    pthread_mutex_destroy(&qpCb->qpMutex);
    free(qpCb);
    qpCb = NULL;
    return ret;
}