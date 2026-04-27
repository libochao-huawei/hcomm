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

    *directFlag = rsNdaGetDirectFlagByVendorId(rdevCb->deviceAttr.vendor_id);
    return ret;
}

int RsInitNdaCb(struct RsRdevCb *rdevCb)
{
    int ret = 0;

    rdevCb->ibCtxEx = RsNdaIbvOpenExtend(rdevCb->ibCtx);
    if (rdevCb->ibCtxEx == NULL) {
        return 0;
    }

    rdevCb->rsCb->ndaCb = (struct RsNdaCb *)calloc(1, sizeof(struct RsNdaCb));
    if (rdevCb->rsCb->ndaCb == NULL) {
        hccp_err("calloc for ndaCb failed");
        ret = -ENOMEM;
        RsNdaIbvCloseExtend(rdevCb->ibCtxEx);
        rdevCb->ibCtxEx = NULL;
    }

    return ret;
}

void RsFreeNdaCb(struct RsRdevCb *rdevCb)
{
    if (rdevCb->rsCb->ndaCb != NULL) {
        free(rdevCb->rsCb->ndaCb);
        rdevCb->rsCb->ndaCb = NULL;
    }
    (void)RsNdaIbvCloseExtend(rdevCb->ibCtxEx);
    rdevCb->ibCtxEx = NULL;
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

    if (attr.memType == DV_MEM_SVM_DEVICE) {
        ret = DlHalMemRegUbSegment(attr.devId, (uint64_t)(uintptr_t)ptr, size);
        if (ret != 0) {
            hccp_err("DlHalMemRegUbSegment failed, ret:%d", ret);
            goto free_ptr;
        }
    }

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
}

STATIC void RsNdaCqInitExPrepare(struct NdaCqInitAttr *attr, struct RsNdaCb *ndaCb,
    struct ibv_cq_init_attr_extend *cqInitAttrEx)
{
    if (attr->dmaMode == QBUF_DMA_MODE_DEFAULT) {
        ndaCb->ibvExOps.alloc = attr->ops->alloc;
        ndaCb->ibvExOps.free = attr->ops->free;
    } else if (attr->dmaMode == QBUF_DMA_MODE_INDEP_UB) {
        ndaCb->ndaOps.alloc = attr->ops->alloc;
        ndaCb->ndaOps.free = attr->ops->free;
        ndaCb->ibvExOps.alloc = RsNdaUbAlloc;
        ndaCb->ibvExOps.free = RsNdaUbFree;
    }
    ndaCb->ibvExOps.memset_s = attr->ops->memset_s;
    ndaCb->ibvExOps.memcpy_s = attr->ops->memcpy_s;
    ndaCb->ibvExOps.db_mmap = NULL;
    ndaCb->ibvExOps.db_unmap = NULL;

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