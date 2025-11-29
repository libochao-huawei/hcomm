/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include "securec.h"
#include "hccp_tlv.h"
#include "ra_rs_err.h"
#include "rs_adp_nslb.h"
#include "rs_inner.h"
#include "rs_tlv.h"

STATIC int RsGetNslbCb(uint32_t phyId, struct rs_nslb_cb **nslbCb)
{
    struct rs_cb *rsCb = NULL;
    int ret;

    ret = RsGetRsCb(phyId, &rsCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phyId(%u) invalid, ret(%d)", phyId, ret), ret);

    *nslbCb = &rsCb->nslb_cb;
    return 0;
}

STATIC int RsNslbInit(unsigned int phyId, unsigned int *bufferSize)
{
    struct rs_nslb_cb *nslbCb = NULL;
    struct rs_cb *rsCb = NULL;
    int ret = 0;

    ret = RsGetRsCb(phyId, &rsCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phyId(%u) invalid, ret(%d)", phyId, ret), ret);
    nslbCb = &rsCb->nslb_cb;
    CHK_PRT_RETURN(nslbCb->netco_init_flag, hccp_err("rs_nslb init repeat, phyId(%u)", phyId), -EINVAL);

    nslbCb->rscb = rsCb;
    nslbCb->phy_id = phyId;
    ret = pthread_mutex_init(&nslbCb->mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_nslb mutex_init failed ret(%d)", ret), -ESYSFUNC);

    nslbCb->buf_info.buf = (char *)calloc(RS_NSLB_BUFFER_SIZE, sizeof(char));
    if (nslbCb->buf_info.buf == NULL) {
        hccp_err("rs_nslb calloc buf failed errno(%d)", errno);
        (void)pthread_mutex_destroy(&nslbCb->mutex);
        return -ENOMEM;
    }

    ret = RsNslbNetcoInit(nslbCb);
    if (ret == -ENOTSUPP) {
        hccp_warn("rs_nslb_init unsuccessful, ret(%d) phyId(%u)", ret, phyId);
        goto nslb_init_error;
    } else if (ret != 0) {
        hccp_err("rs_nslb_init failed, ret(%d) phyId(%u)", ret, phyId);
        goto nslb_init_error;
    }
    nslbCb->buf_info.buffer_size = RS_NSLB_BUFFER_SIZE;
    *bufferSize = RS_NSLB_BUFFER_SIZE;
    return 0;

nslb_init_error:
    free(nslbCb->buf_info.buf);
    nslbCb->buf_info.buf = NULL;
    pthread_mutex_destroy(&nslbCb->mutex);
    return ret;
}

STATIC int RsNslbDeinit(unsigned int phyId)
{
    struct rs_nslb_cb *nslbCb = NULL;
    int ret = 0;

    ret = RsGetNslbCb(phyId, &nslbCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_nslb_cb failed, ret(%d) phyId(%u)", ret, phyId), ret);

    CHK_PRT_RETURN(!nslbCb->netco_init_flag,
        hccp_warn("rs_nslb not init or already deinit, phyId(%u)", phyId), 0);

    RS_PTHREAD_MUTEX_LOCK(&nslbCb->mutex);
    RsNslbNetcoDeinit(nslbCb);
    free(nslbCb->buf_info.buf);
    nslbCb->buf_info.buf = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&nslbCb->mutex);
    pthread_mutex_destroy(&nslbCb->mutex);
    return 0;
}

STATIC int RsTlvAssembleSendData(struct tlv_buf_info *bufInfo, struct tlv_request_msg_head *head, char *data,
    bool *isSendFinish)
{
    int ret = 0;

    CHK_PRT_RETURN(head->offset >= bufInfo->buffer_size,
        hccp_err("[recv][rs_tlv]param error, offset >= bufferSize(%u), phyId(%u)",
        head->offset, bufInfo->buffer_size, head->phy_id), -EINVAL);
    CHK_PRT_RETURN((head->offset + head->send_bytes) > head->total_bytes,
        hccp_err("[recv][rs_tlv]data overflow, offset(%u) + send_bytes(%u) > total_bytes(%u), phyId(%u)",
        head->offset, head->send_bytes, head->total_bytes, head->phy_id), -EINVAL);

    if (head->offset == 0) {
        (void)memset_s(bufInfo->buf, bufInfo->buffer_size, 0, bufInfo->buffer_size);
    }

    ret = memcpy_s(bufInfo->buf + head->offset, bufInfo->buffer_size - head->offset, data, head->send_bytes);
    CHK_PRT_RETURN(ret != 0, hccp_err("[recv][rs_tlv]memcpy_s data failed, ret(%d) phyId(%u)",
        ret, head->phy_id), -ESAFEFUNC);

    if (head->offset + head->send_bytes == head->total_bytes) {
        *isSendFinish = true;
    }

    return 0;
}

STATIC int RsNslbRequest(struct tlv_request_msg_head *head, char *data)
{
    struct rs_nslb_cb *nslbCb = NULL;
    bool isSendFinish = false;
    unsigned int recvLen = 0;
    int ret = 0;

    ret = RsGetNslbCb(head->phy_id, &nslbCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_nslb_cb failed, ret(%d) phyId(%u)", ret, head->phy_id), ret);
    CHK_PRT_RETURN(nslbCb->buf_info.buf == NULL,
        hccp_err("rs_nslb buf not initialized, phyId(%u)", head->phy_id), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&nslbCb->mutex);
    ret = RsTlvAssembleSendData(&nslbCb->buf_info, head, data, &isSendFinish);
    if (ret != 0) {
        hccp_err("rs_tlv_assemble_send_data failed, ret(%d) phyId(%u)", ret, nslbCb->phy_id);
        goto nslb_request_release_lock;
    }

    if (!isSendFinish) {
        goto nslb_request_release_lock;
    }

    recvLen = head->total_bytes;
    ret = RsNslbNetcoRequest(nslbCb, head->type, nslbCb->buf_info.buf, recvLen);
nslb_request_release_lock:
    RS_PTHREAD_MUTEX_ULOCK(&nslbCb->mutex);
    return ret;
}

RS_ATTRI_VISI_DEF int RsTlvInit(unsigned int moduleType, unsigned int phyId, unsigned int *bufferSize)
{
    int ret = 0;

    CHK_PRT_RETURN(bufferSize == NULL, hccp_err("param error, bufferSize is NULL"), -EINVAL);

    switch(moduleType) {
        case TLV_MODULE_TYPE_NSLB:
            ret = RsNslbInit(phyId, bufferSize);
            break;
        default:
            hccp_err("[init][rs_tlv]module type error, moduleType(%u) phyId(%u)", moduleType, phyId);
            ret = -EINVAL;
            break;
    }

    return ret;
}

RS_ATTRI_VISI_DEF int RsTlvDeinit(unsigned int moduleType, unsigned int phyId)
{
    int ret = 0;

    switch(moduleType) {
        case TLV_MODULE_TYPE_NSLB:
            ret = RsNslbDeinit(phyId);
            break;
        default:
            hccp_err("[deinit][rs_tlv]module type error, moduleType(%u) phyId(%u)", moduleType, phyId);
            ret = -EINVAL;
            break;
    }

    return ret;
}

RS_ATTRI_VISI_DEF int RsTlvRequest(struct tlv_request_msg_head *head, char *data)
{
    int ret = 0;

    CHK_PRT_RETURN(head == NULL || data == NULL, hccp_err("param error, head or data is NULL"), -EINVAL);

    switch(head->module_type) {
        case TLV_MODULE_TYPE_NSLB:
            ret = RsNslbRequest(head, data);
            break;
        default:
            hccp_err("[request][rs_tlv]module type error, moduleType(%u) phyId(%u)", head->module_type, head->phy_id);
            ret = -EINVAL;
            break;
    }

    return ret;
}
