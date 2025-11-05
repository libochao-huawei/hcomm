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

STATIC int rs_get_nslb_cb(uint32_t phy_id, struct rs_nslb_cb **nslb_cb)
{
    struct rs_cb *rs_cb = NULL;
    int ret;

    ret = rs_get_rs_cb(phy_id, &rs_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phy_id(%u) invalid, ret(%d)", phy_id, ret), ret);

    *nslb_cb = &rs_cb->nslb_cb;
    return 0;
}

STATIC int rs_nslb_init(unsigned int phy_id, unsigned int *buffer_size)
{
    struct rs_nslb_cb *nslb_cb = NULL;
    struct rs_cb *rs_cb = NULL;
    int ret = 0;

    ret = rs_get_rs_cb(phy_id, &rs_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phy_id(%u) invalid, ret(%d)", phy_id, ret), ret);
    nslb_cb = &rs_cb->nslb_cb;
    CHK_PRT_RETURN(nslb_cb->netco_init_flag, hccp_err("rs_nslb init repeat, phy_id(%u)", phy_id), -EINVAL);

    nslb_cb->rscb = rs_cb;
    nslb_cb->phy_id = phy_id;
    ret = pthread_mutex_init(&nslb_cb->mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_nslb mutex_init failed ret(%d)", ret), -ESYSFUNC);

    nslb_cb->buf_info.buf = (char *)calloc(RS_NSLB_BUFFER_SIZE, sizeof(char));
    if (nslb_cb->buf_info.buf == NULL) {
        hccp_err("rs_nslb calloc buf failed errno(%d)", errno);
        (void)pthread_mutex_destroy(&nslb_cb->mutex);
        return -ENOMEM;
    }

    ret = rs_nslb_netco_init(nslb_cb);
    if (ret == -ENOTSUPP) {
        hccp_warn("rs_nslb_init unsuccessful, ret(%d) phy_id(%u)", ret, phy_id);
        goto nslb_init_error;
    } else if (ret != 0) {
        hccp_err("rs_nslb_init failed, ret(%d) phy_id(%u)", ret, phy_id);
        goto nslb_init_error;
    }
    nslb_cb->buf_info.buffer_size = RS_NSLB_BUFFER_SIZE;
    *buffer_size = RS_NSLB_BUFFER_SIZE;
    return 0;

nslb_init_error:
    free(nslb_cb->buf_info.buf);
    nslb_cb->buf_info.buf = NULL;
    pthread_mutex_destroy(&nslb_cb->mutex);
    return ret;
}

STATIC int rs_nslb_deinit(unsigned int phy_id)
{
    struct rs_nslb_cb *nslb_cb = NULL;
    int ret = 0;

    ret = rs_get_nslb_cb(phy_id, &nslb_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_nslb_cb failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    CHK_PRT_RETURN(!nslb_cb->netco_init_flag,
        hccp_warn("rs_nslb not init or already deinit, phy_id(%u)", phy_id), 0);

    RS_PTHREAD_MUTEX_LOCK(&nslb_cb->mutex);
    rs_nslb_netco_deinit(nslb_cb);
    free(nslb_cb->buf_info.buf);
    nslb_cb->buf_info.buf = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&nslb_cb->mutex);
    pthread_mutex_destroy(&nslb_cb->mutex);
    return 0;
}

STATIC int rs_tlv_assemble_send_data(struct tlv_buf_info *buf_info, struct tlv_request_msg_head *head, char *data,
    bool *is_send_finish)
{
    int ret = 0;

    CHK_PRT_RETURN(head->offset >= buf_info->buffer_size,
        hccp_err("[recv][rs_tlv]param error, offset >= buffer_size(%u), phy_id(%u)",
        head->offset, buf_info->buffer_size, head->phy_id), -EINVAL);
    CHK_PRT_RETURN((head->offset + head->send_bytes) > head->total_bytes,
        hccp_err("[recv][rs_tlv]data overflow, offset(%u) + send_bytes(%u) > total_bytes(%u), phy_id(%u)",
        head->offset, head->send_bytes, head->total_bytes, head->phy_id), -EINVAL);

    if (head->offset == 0) {
        (void)memset_s(buf_info->buf, buf_info->buffer_size, 0, buf_info->buffer_size);
    }

    ret = memcpy_s(buf_info->buf + head->offset, buf_info->buffer_size - head->offset, data, head->send_bytes);
    CHK_PRT_RETURN(ret != 0, hccp_err("[recv][rs_tlv]memcpy_s data failed, ret(%d) phy_id(%u)",
        ret, head->phy_id), -ESAFEFUNC);

    if (head->offset + head->send_bytes == head->total_bytes) {
        *is_send_finish = true;
    }

    return 0;
}

STATIC int rs_nslb_request(struct tlv_request_msg_head *head, char *data)
{
    struct rs_nslb_cb *nslb_cb = NULL;
    bool is_send_finish = false;
    unsigned int recv_len = 0;
    int ret = 0;

    ret = rs_get_nslb_cb(head->phy_id, &nslb_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_nslb_cb failed, ret(%d) phy_id(%u)", ret, head->phy_id), ret);
    CHK_PRT_RETURN(nslb_cb->buf_info.buf == NULL,
        hccp_err("rs_nslb buf not initialized, phy_id(%u)", head->phy_id), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&nslb_cb->mutex);
    ret = rs_tlv_assemble_send_data(&nslb_cb->buf_info, head, data, &is_send_finish);
    if (ret != 0) {
        hccp_err("rs_tlv_assemble_send_data failed, ret(%d) phy_id(%u)", ret, nslb_cb->phy_id);
        goto nslb_request_release_lock;
    }

    if (!is_send_finish) {
        goto nslb_request_release_lock;
    }

    recv_len = head->total_bytes;
    ret = rs_nslb_netco_request(nslb_cb, head->type, nslb_cb->buf_info.buf, recv_len);
nslb_request_release_lock:
    RS_PTHREAD_MUTEX_ULOCK(&nslb_cb->mutex);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_tlv_init(unsigned int module_type, unsigned int phy_id, unsigned int *buffer_size)
{
    int ret = 0;

    CHK_PRT_RETURN(buffer_size == NULL, hccp_err("param error, buffer_size is NULL"), -EINVAL);

    switch(module_type) {
        case TLV_MODULE_TYPE_NSLB:
            ret = rs_nslb_init(phy_id, buffer_size);
            break;
        default:
            hccp_err("[init][rs_tlv]module type error, module_type(%u) phy_id(%u)", module_type, phy_id);
            ret = -EINVAL;
            break;
    }

    return ret;
}

RS_ATTRI_VISI_DEF int rs_tlv_deinit(unsigned int module_type, unsigned int phy_id)
{
    int ret = 0;

    switch(module_type) {
        case TLV_MODULE_TYPE_NSLB:
            ret = rs_nslb_deinit(phy_id);
            break;
        default:
            hccp_err("[deinit][rs_tlv]module type error, module_type(%u) phy_id(%u)", module_type, phy_id);
            ret = -EINVAL;
            break;
    }

    return ret;
}

RS_ATTRI_VISI_DEF int rs_tlv_request(struct tlv_request_msg_head *head, char *data)
{
    int ret = 0;

    CHK_PRT_RETURN(head == NULL || data == NULL, hccp_err("param error, head or data is NULL"), -EINVAL);

    switch(head->module_type) {
        case TLV_MODULE_TYPE_NSLB:
            ret = rs_nslb_request(head, data);
            break;
        default:
            hccp_err("[request][rs_tlv]module type error, module_type(%u) phy_id(%u)", head->module_type, head->phy_id);
            ret = -EINVAL;
            break;
    }

    return ret;
}
