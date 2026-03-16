/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_nda.h"
#include "rs_inner.h"
#include "rs_rdma.h"
#include "rs.h"
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

STATIC void RsNdaCqInitExPrepare(struct RsRdevCb *rdevCb, struct NdaCqInitAttr *attr,
    struct ibv_cq_init_attr_extend *cqInitAttrEx)
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
    gRsCb->ibvExOps.db_mmap = NULL;
    gRsCb->ibvExOps.db_unmap = NULL;

    cqInitAttrEx->pd = rdevCb->ibPd;
    (void)memcpy_s(&cqInitAttrEx->attr, sizeof(struct ibv_cq_init_attr_ex), &attr->attr, sizeof(struct ibv_cq_init_attr_ex));
    cqInitAttrEx->qp_cap_flag = attr->cqCapFlag;
    cqInitAttrEx->type = attr->dmaMode;
    cqInitAttrEx->ops = &gRsCb->ibvExOps;
}

STATIC int RsNdaCqCreateEx(struct RsRdevCb *rdevCb, struct ibv_cq_init_attr_extend *cqInitAttrEx, struct NdaCqInfo *info)
{
    struct ibv_cq_extend *cqExt = NULL;
    int ret = 0;
    hccp_dbg("RsNdaCqCreateEx begin..");

    cqExt = RsNdaCreateCqExtend(rdevCb->ibCtxEx, cqInitAttrEx);
    CHK_PRT_RETURN(cqExt == NULL, hccp_err("RsNdaCreateCqExtend failed, errno:%d", errno), -ENOMEM);

    info->cq = cqExt->cq;
    (void)memcpy_s(&info->cqInfo, sizeof(struct queue_info), &cqExt->cq_info, sizeof(struct queue_info));

    hccp_info("chip_id:%u, rdevIndex:%u cq create succ", rdevCb->rsCb->chipId, rdevCb->rdevIndex);
    return ret;
}

RS_ATTRI_VISI_DEF int RsNdaCqCreate(unsigned int phyId, unsigned int rdevIndex, struct NdaCqInitAttr *attr, 
    struct NdaCqInfo *info)
{
    struct ibv_cq_init_attr_extend cqInitAttrEx = {0};
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    CHK_PRT_RETURN(attr == NULL || info == NULL, hccp_err("attr or info is NULL, phyId:%u", phyId), -EINVAL);

    CHK_PRT_RETURN(attr->dmaMode >= QBUF_DMA_MODE_MAX, hccp_err("param err, dmaMode:%u >= %u, phyId:%u",
        attr->dmaMode, QBUF_DMA_MODE_MAX, phyId), -EINVAL);

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    RsNdaCqInitExPrepare(rdevCb, attr, &cqInitAttrEx);

    ret = RsNdaCqCreateEx(rdevCb, &cqInitAttrEx, info);
    if (ret != 0) {
        hccp_err("create nda qp create extend failed, ret:%d", ret);
        return ret;
    }

    hccp_info("create cq successfully");
    return ret;
}

RS_ATTRI_VISI_DEF int RsNdaCqDestroy(unsigned int phyId, unsigned int rdevIndex, struct NdaCqInfo *info)
{
    struct ibv_cq_extend *ibvCqExt = NULL;
    struct RsRdevCb *rdevCb = NULL;
    int ret = 0;

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsQueryRdevCb phyId:%u rdevIndex:%u ret:%d", phyId, rdevIndex, ret), ret);

    ibvCqExt = (struct ibv_cq_extend *)calloc(1, sizeof(struct ibv_cq_extend));
    CHK_PRT_RETURN(ibvCqExt == NULL, hccp_err("[RsNdaCqDestroyp]ibvCqExt calloc failed"), -ENOMEM);
    ibvCqExt->cq = info->cq;
    (void)memcpy_s(&ibvCqExt->cq_info, sizeof(struct queue_info), &info->cqInfo, sizeof(struct queue_info));

    ret = RsNdaIbvDestroyCqExtend(rdevCb->ibCtxEx, ibvCqExt);
    if (ret != 0) {
        hccp_err("cq destroy extend failed, ret:%d", ret);
        return ret;
    }

    hccp_info("cq destroy successfully");
    return ret;
}