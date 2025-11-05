/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "user_log.h"
#include "ra_hdc.h"
#include "securec.h"
#include "ra.h"
#include "hccp.h"
#include "ra_comm.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "dl_hal_function.h"
#include "ra_rdma_lite.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"

STATIC int ra_hdc_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id, unsigned long long **notify_va)
{
    unsigned long long moudle_id = HCCP;
    unsigned int notify_size = 0;
    unsigned int logic_id;
    int ret, drv_ret;

    CHK_PRT_RETURN(notify_type != NOTIFY, hccp_err("[init][base_addr]notify_type[%u] error", notify_type), -EINVAL);
    ret = dl_drv_device_get_index_by_phy_id(phy_id, &logic_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]drvDeviceGetIndexByPhyId failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);

    ret = dl_hal_notify_get_info(logic_id, 0, RA_NOTIFY_TYPE_TOTAL_SIZE, &notify_size);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]halNotifyGetInfo failed, ret(%d), logic_id(%u)",
        ret, logic_id), ret);

    ret = dl_hal_mem_alloc((void *)notify_va, (unsigned long long)notify_size,
        RA_MEM_TYPE_HBM | (moudle_id << MEM_MODULE_ID_BIT));
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]halMemAlloc failed, ret(%d), phy_id(%u)", ret, phy_id), ret);

    hccp_info("notify info: size[%u]", notify_size);
    ret = ra_hdc_notify_cfg_set(phy_id, (uintptr_t)*notify_va, notify_size);
    if (ret) {
        hccp_err("[init][base_addr]ra_hdc_notify_cfg_set failed, ret(%d), phy_id(%u)", ret, phy_id);
        goto free_mem;
    }
    return 0;

free_mem:
    drv_ret = dl_hal_mem_free((void *)*notify_va);
    if (drv_ret) {
        hccp_err("[init][base_addr]halMemFree failed! ret(%d)", drv_ret);
    }
    return ret;
}

static void ra_hdc_get_qp_hdc(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, unsigned int qpn,
    struct ra_qp_handle *qp_hdc)
{
    qp_hdc->phy_id = rdma_handle->rdev_info.phy_id;
    qp_hdc->rdev_index = rdma_handle->rdev_index;
    qp_hdc->rdma_handle = rdma_handle;
    qp_hdc->rdma_ops = rdma_handle->rdma_ops;
    qp_hdc->qp_mode = qp_mode;
    qp_hdc->flag = flag;
    qp_hdc->qpn = qpn;
}

STATIC int ra_hdc_cmd_qp_destroy(struct ra_qp_handle *qp_hdc)
{
    int ret;
    union op_qp_destroy_data qp_destroy_data = {0};

    qp_destroy_data.tx_data.qpn = qp_hdc->qpn;
    qp_destroy_data.tx_data.phy_id = qp_hdc->phy_id;
    qp_destroy_data.tx_data.rdev_index = qp_hdc->rdev_index;
    ret = ra_hdc_process_msg(RA_RS_QP_DESTROY, qp_hdc->phy_id, (char *)&qp_destroy_data,
        sizeof(union op_qp_destroy_data));
    if (ret) {
        hccp_err("[destroy][ra_hdc_qp]hdc_send_recv_pkt failed ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
    }

    return ret;
}

int ra_hdc_qp_create(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, void **qp_handle)
{
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    union op_qp_create_data qp_create_data = {0};
    struct ra_qp_handle *qp_hdc = NULL;
    struct rdma_lite_qp_cap cap;
    int ret;

    qp_hdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_hdc == NULL, hccp_err("[create][ra_hdc_qp]qp_hdc calloc failed phy_id(%u)", phy_id), -ENOMEM);

    qp_create_data.tx_data.phy_id = phy_id;
    qp_create_data.tx_data.rdev_index = rdma_handle->rdev_index;
    qp_create_data.tx_data.flag = flag;
    qp_create_data.tx_data.qp_mode = qp_mode;
    qp_create_data.tx_data.mem_align = rdma_handle->support_lite;

    ret = ra_hdc_process_msg(RA_RS_QP_CREATE, phy_id, (char *)&qp_create_data,
        sizeof(union op_qp_create_data));
    if (ret) {
        hccp_err("[create][ra_hdc_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(qp_hdc);
        qp_hdc = NULL;
        return ret;
    }

    ra_hdc_get_qp_hdc(rdma_handle, flag, qp_mode, qp_create_data.rx_data.qpn, qp_hdc);
    qp_hdc->psn = qp_create_data.rx_data.psn;
    qp_hdc->gid_idx = qp_create_data.rx_data.gid_idx;

    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = QP_DEFAULT_MIN_CAP_RECV_SGE;
    cap.max_send_wr = RA_QP_32K_DEPTH;
    cap.max_recv_wr = RA_QP_128_DEPTH;
    ret = ra_hdc_lite_qp_create(rdma_handle, qp_hdc, &cap);
    if (ret) {
        (void)ra_hdc_cmd_qp_destroy(qp_hdc);
        hccp_err("[create][ra_hdc_qp]ra_hdc_lite_qp_create failed ret(%d) phy_id(%u)", ret, phy_id);
        free(qp_hdc);
        qp_hdc = NULL;
        return ret;
    }

    qp_hdc->sq_depth = cap.max_send_wr;
    *qp_handle = qp_hdc;

    return 0;
}

int ra_hdc_qp_create_with_attrs(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs, void **qp_handle)
{
    int flag = (ext_attrs->qp_attr.qp_type == IBV_QPT_RC) ? 0 : 1;
    union op_qp_create_with_attrs_data op_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct ra_qp_handle *qp_hdc = NULL;
    struct rdma_lite_qp_cap cap;
    int ret;

    qp_hdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_hdc == NULL, hccp_err("[create][ra_hdc_qp_with_attrs]qp_hdc calloc failed phy_id(%u)", phy_id),
        -ENOMEM);

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.rdev_index = rdma_handle->rdev_index;
    ret = memcpy_s(&op_data.tx_data.ext_attrs, sizeof(struct qp_ext_attrs), ext_attrs, sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_hdc_qp_with_attrs]memcpy_s for ext_attrs fail, ret:%d", ret);
        ret = -ESAFEFUNC;
        goto out;
    }

    op_data.tx_data.ext_attrs.mem_align = rdma_handle->support_lite;
    ret = ra_hdc_process_msg(RA_RS_QP_CREATE_WITH_ATTRS, phy_id, (char *)&op_data,
        sizeof(union op_qp_create_with_attrs_data));
    if (ret) {
        hccp_err("[create][ra_hdc_qp_with_attrs]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

    ra_hdc_get_qp_hdc(rdma_handle, flag, ext_attrs->qp_mode, op_data.rx_data.qpn, qp_hdc);
    qp_hdc->psn = op_data.rx_data.psn;
    qp_hdc->gid_idx = op_data.rx_data.gid_idx;

    cap.max_inline_data = ext_attrs->qp_attr.cap.max_inline_data;
    cap.max_send_sge = ext_attrs->qp_attr.cap.max_send_sge;
    cap.max_recv_sge = ext_attrs->qp_attr.cap.max_recv_sge;
    cap.max_send_wr = ext_attrs->qp_attr.cap.max_send_wr;
    cap.max_recv_wr = ext_attrs->qp_attr.cap.max_recv_wr;
    ret = ra_hdc_lite_qp_create(rdma_handle, qp_hdc, &cap);
    if (ret) {
        (void)ra_hdc_cmd_qp_destroy(qp_hdc);
        hccp_err("[create][ra_hdc_qp_with_attrs]ra_hdc_lite_qp_create failed ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

    qp_hdc->sq_sig_all = ext_attrs->qp_attr.sq_sig_all;
    qp_hdc->udp_sport = ext_attrs->udp_sport;
    qp_hdc->sq_depth = cap.max_send_wr;
    *qp_handle = qp_hdc;
    return 0;

out:
    free(qp_hdc);
    qp_hdc = NULL;
    return ret;
}

int ra_hdc_ai_qp_create(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
    struct ai_qp_info *info, void **qp_handle)
{
#define AI_QP_DEFAULT_GID_IDX 3U
    int flag = ext_attrs->qp_attr.qp_type == IBV_QPT_RC ? 0 : 1;
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    union op_ai_qp_create_data qp_create_data = {0};
    struct ra_qp_handle *qp_hdc = NULL;
    int qp_mode = ext_attrs->qp_mode;
    int ret;

    qp_hdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_hdc == NULL, hccp_err("[create][ra_hdc_ai_qp]qp_hdc calloc failed phy_id(%u)", phy_id),
        -ENOMEM);

    qp_create_data.tx_data.phy_id = phy_id;
    qp_create_data.tx_data.rdev_index = rdma_handle->rdev_index;
    ret = memcpy_s(&qp_create_data.tx_data.ext_attrs, sizeof(struct qp_ext_attrs), ext_attrs,
        sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]memcpy_s for ext_attrs fail, ret:%d", ret);
        free(qp_hdc);
        qp_hdc = NULL;
        return -ESAFEFUNC;
    }

    ret = ra_hdc_process_msg(RA_RS_AI_QP_CREATE, phy_id, (char *)&qp_create_data,
        sizeof(union op_ai_qp_create_data));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(qp_hdc);
        qp_hdc = NULL;
        return ret;
    }

    ra_hdc_get_qp_hdc(rdma_handle, flag, qp_mode, qp_create_data.rx_data.qpn, qp_hdc);
    qp_hdc->psn = qp_create_data.rx_data.psn;
    // set a default gid_idx due to compatibility issue, rs will refresh it if it is different
    qp_hdc->gid_idx = AI_QP_DEFAULT_GID_IDX;
    info->ai_qp_addr = qp_create_data.rx_data.ai_qp_addr;
    info->sq_index = qp_create_data.rx_data.sq_index;
    info->db_index = qp_create_data.rx_data.db_index;
    qp_hdc->udp_sport = ext_attrs->udp_sport;
    *qp_handle = qp_hdc;

    return 0;
}

int ra_hdc_ai_qp_create_with_attrs(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
    struct ai_qp_info *info, void **qp_handle)
{
    int flag = ext_attrs->qp_attr.qp_type == IBV_QPT_RC ? 0 : 1;
    union op_ai_qp_create_with_attrs_data qp_create_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct ra_qp_handle *qp_hdc = NULL;
    int qp_mode = ext_attrs->qp_mode;
    int ret;

    qp_hdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_hdc == NULL, hccp_err("[create][ra_hdc_ai_qp]qp_hdc calloc failed phy_id(%u)", phy_id),
        -ENOMEM);

    qp_create_data.tx_data.phy_id = phy_id;
    qp_create_data.tx_data.rdev_index = rdma_handle->rdev_index;
    ret = memcpy_s(&qp_create_data.tx_data.ext_attrs, sizeof(struct qp_ext_attrs), ext_attrs,
        sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]memcpy_s for ext_attrs fail, ret:%d", ret);
        free(qp_hdc);
        qp_hdc = NULL;
        return -ESAFEFUNC;
    }

    ret = ra_hdc_process_msg(RA_RS_AI_QP_CREATE_WITH_ATTRS, phy_id, (char *)&qp_create_data,
        sizeof(union op_ai_qp_create_with_attrs_data));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(qp_hdc);
        qp_hdc = NULL;
        return ret;
    }

    qp_hdc->udp_sport = ext_attrs->udp_sport;
    ra_hdc_get_qp_hdc(rdma_handle, flag, qp_mode, qp_create_data.rx_data.qpn, qp_hdc);
    qp_hdc->gid_idx = qp_create_data.rx_data.gid_idx;
    qp_hdc->psn = qp_create_data.rx_data.psn;
    info->ai_qp_addr = qp_create_data.rx_data.ai_qp_addr;
    info->sq_index = qp_create_data.rx_data.sq_index;
    info->db_index = qp_create_data.rx_data.db_index;
    info->ai_scq_addr = qp_create_data.rx_data.ai_scq_addr;
    info->ai_rcq_addr = qp_create_data.rx_data.ai_rcq_addr;
    (void)memcpy_s(&info->data_plane_info, sizeof(struct ai_data_plane_info), &qp_create_data.rx_data.data_plane_info,
        sizeof(struct ai_data_plane_info));
    *qp_handle = qp_hdc;

    return 0;
}

int ra_hdc_typical_qp_create(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, struct typical_qp *qp_info,
    void **qp_handle)
{
    union op_typical_qp_create_data qp_create_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct ra_qp_handle *qp_hdc = NULL;
    struct rdma_lite_qp_cap cap;
    int ret;

    qp_hdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_hdc == NULL, hccp_err("[create][ra_hdc_typical_qp]qp_hdc calloc failed phy_id(%u)", phy_id),
        -ENOMEM);

    qp_create_data.tx_data.phy_id = phy_id;
    qp_create_data.tx_data.rdev_index = rdma_handle->rdev_index;
    qp_create_data.tx_data.flag = flag;
    qp_create_data.tx_data.qp_mode = qp_mode;
    qp_create_data.tx_data.mem_align = rdma_handle->support_lite;

    ret = ra_hdc_process_msg(RA_RS_TYPICAL_QP_CREATE, phy_id, (char *)&qp_create_data,
        sizeof(union op_typical_qp_create_data));
    if (ret) {
        hccp_err("[create][ra_hdc_typical_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(qp_hdc);
        qp_hdc = NULL;
        return ret;
    }

    qp_info->gid_idx = qp_create_data.rx_data.gid_idx;
    qp_info->psn = qp_create_data.rx_data.psn;
    qp_info->qpn = qp_create_data.rx_data.qpn;
    (void)memcpy_s(qp_info->gid, HCCP_GID_RAW_LEN, qp_create_data.rx_data.gid.raw, HCCP_GID_RAW_LEN);

    ra_hdc_get_qp_hdc(rdma_handle, flag, qp_mode, qp_info->qpn, qp_hdc);
    qp_hdc->psn = qp_create_data.rx_data.psn;
    qp_hdc->gid_idx = qp_create_data.rx_data.gid_idx;

    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = QP_DEFAULT_MIN_CAP_RECV_SGE;
    cap.max_send_wr = RA_QP_32K_DEPTH;
    cap.max_recv_wr = RA_QP_128_DEPTH;
    ret = ra_hdc_lite_qp_create(rdma_handle, qp_hdc, &cap);
    if (ret) {
        (void)ra_hdc_cmd_qp_destroy(qp_hdc);
        hccp_err("[create][ra_hdc_typical_qp]ra_hdc_lite_qp_create failed ret(%d) phy_id(%u)", ret, phy_id);
        free(qp_hdc);
        qp_hdc = NULL;
        return ret;
    }

    qp_hdc->sq_depth = cap.max_send_wr;
    *qp_handle = qp_hdc;

    return 0;
}

int ra_hdc_qp_destroy(struct ra_qp_handle *qp_hdc)
{
    int ret;

    ra_hdc_lite_qp_destroy(qp_hdc);
    ret = ra_hdc_cmd_qp_destroy(qp_hdc);
    if (ret) {
        hccp_err("[destroy][ra_hdc_qp]ra_hdc_cmd_qp_destroy failed ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
    }

    free(qp_hdc);
    qp_hdc = NULL;
    return ret;
}

int ra_hdc_get_qp_status(struct ra_qp_handle *qp_hdc, int *status)
{
    union op_qp_status_data qp_status_data = {0};
    union op_qp_info_data qp_info_data = {0};
    unsigned int interface_version = 0;
    int ret;

    ret = ra_hdc_get_interface_version(qp_hdc->phy_id, RA_RS_QP_INFO, &interface_version);
    if (ret != 0) {
        hccp_warn("[get][ra_hdc_qp_status]get interface version not success ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
        interface_version = 0;
    }

    if (interface_version >= RA_RS_OPCODE_BASE_VERSION) {
        qp_info_data.tx_data.qpn = qp_hdc->qpn;
        qp_info_data.tx_data.phy_id = qp_hdc->phy_id;
        qp_info_data.tx_data.rdev_index = qp_hdc->rdev_index;
        ret = ra_hdc_process_msg(RA_RS_QP_INFO, qp_hdc->phy_id, (char *)&qp_info_data,
            sizeof(union op_qp_info_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_qp_status]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), ret);
        *status = qp_info_data.rx_data.status;
        qp_hdc->udp_sport = qp_info_data.rx_data.udp_sport;
    } else {
        qp_status_data.tx_data.qpn = qp_hdc->qpn;
        qp_status_data.tx_data.phy_id = qp_hdc->phy_id;
        qp_status_data.tx_data.rdev_index = qp_hdc->rdev_index;
        ret = ra_hdc_process_msg(RA_RS_QP_STATUS, qp_hdc->phy_id, (char *)&qp_status_data,
            sizeof(union op_qp_status_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_qp_status]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), ret);
        *status = qp_status_data.rx_data.status;
    }

    return ra_hdc_lite_get_connected_info(qp_hdc);
}

int ra_hdc_typical_qp_modify(struct ra_qp_handle *qp_hdc, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info)
{
    union op_typical_qp_modify_data qp_modify_data = {0};
    unsigned int phy_id = qp_hdc->phy_id;
    int ret;

    qp_modify_data.tx_data.phy_id = phy_id;
    qp_modify_data.tx_data.rdev_index = qp_hdc->rdev_index;
    ret = memcpy_s(&(qp_modify_data.tx_data.local_qp_info), sizeof(struct typical_qp), local_qp_info,
        sizeof(struct typical_qp));
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]memcpy_s local_qp_info failed, phy_id[%u] ret[%d]", phy_id, ret),
        -ESAFEFUNC);
    ret = memcpy_s(&(qp_modify_data.tx_data.remote_qp_info), sizeof(struct typical_qp), remote_qp_info,
        sizeof(struct typical_qp));
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]memcpy_s remote_qp_info failed, phy_id[%u] ret[%d]", phy_id, ret),
        -ESAFEFUNC);

    ret = ra_hdc_process_msg(RA_RS_TYPICAL_QP_MODIFY, phy_id, (char *)&qp_modify_data,
        sizeof(union op_typical_qp_modify_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify][modify_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    qp_hdc->udp_sport = qp_modify_data.rx_data.udp_sport;
    if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
        ret = ra_rdma_lite_set_qp_sl(qp_hdc->lite_qp, local_qp_info->sl);
        CHK_PRT_RETURN(ret != 0, hccp_err("[modify][modify_qp]ra_rdma_lite_set_qp_sl sl(%u) failed ret(%d) phy_id(%u)",
            local_qp_info->sl, ret, phy_id), ret);
    }
    return 0;
}

int ra_hdc_qp_connect_async(struct ra_qp_handle *qp_hdc, const void *sock_handle)
{
    union op_qp_connect_data qp_connect_data = {0};
    int ret;

    qp_connect_data.tx_data.qpn = qp_hdc->qpn;
    qp_connect_data.tx_data.fd = (unsigned int)((const struct socket_hdc_info *)sock_handle)->fd;
    qp_connect_data.tx_data.phy_id = qp_hdc->phy_id;
    qp_connect_data.tx_data.rdev_index = qp_hdc->rdev_index;
    ret = ra_hdc_process_msg(RA_RS_QP_CONNECT, qp_hdc->phy_id, (char *)&qp_connect_data,
        sizeof(union op_qp_connect_data));
    CHK_PRT_RETURN(ret, hccp_err("[connect_async][ra_hdc_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    return 0;
}

static void ra_hdc_send_data_init(union op_send_wr_data *send_wr_data, struct ra_qp_handle *qp_hdc, struct send_wr *wr)
{
    send_wr_data->tx_data.phy_id = qp_hdc->phy_id;
    send_wr_data->tx_data.rdev_index = qp_hdc->rdev_index;
    send_wr_data->tx_data.qpn = qp_hdc->qpn;
    send_wr_data->tx_data.buf_num = wr->buf_num;
    send_wr_data->tx_data.dst_addr = wr->dst_addr;
    send_wr_data->tx_data.op = wr->op;
    send_wr_data->tx_data.send_flags = wr->send_flag;
}

int ra_hdc_send_wr(struct ra_qp_handle *qp_hdc, struct send_wr *wr, struct send_wr_rsp *op_rsp)
{
    union op_send_wr_data send_wr_data = {0};
    struct lite_send_wr lite_wr = { 0 };
    int ret;

    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            lite_wr.wr = *wr;
            return ra_hdc_lite_send_wr(qp_hdc, &lite_wr, op_rsp, HDC_LITE_DEFAULT_WR_ID);
        }
    }

    ra_hdc_send_data_init(&send_wr_data, qp_hdc, wr);

    ret = memcpy_s(send_wr_data.tx_data.mem_list, (sizeof(struct sg_list) * MAX_SGE_NUM), wr->buf_list,
        (sizeof(struct sg_list) * wr->buf_num));
    CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_wr]memcpy_s for mem_list failed, ret(%d), phy_id(%u)",
        ret, qp_hdc->phy_id), -ESAFEFUNC);

    ret = ra_hdc_process_msg(RA_RS_SEND_WR, qp_hdc->phy_id,
        (char *)&send_wr_data, sizeof(union op_send_wr_data));
    if (ret) {
        if (ret != -ENOENT) {
            hccp_err("[send][ra_hdc_wr]ra hdc message process failed ret(%d), phy_id(%u)", ret, qp_hdc->phy_id);
        }
        return ret;
    }

    if (qp_hdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        op_rsp->wqe_tmp = send_wr_data.rx_data.wr_rsp.wqe_tmp;
    } else if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
               qp_hdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
               qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        op_rsp->db = send_wr_data.rx_data.wr_rsp.db;
    }

    return ret;
}

int ra_hdc_send_wr_v2(struct ra_qp_handle *qp_hdc, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp)
{
    struct lite_send_wr lite_wr = { 0 };

    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            lite_wr.wr.buf_list = wr->buf_list;
            lite_wr.wr.buf_num = wr->buf_num;
            lite_wr.wr.dst_addr = wr->dst_addr;
            lite_wr.wr.op = wr->op;
            lite_wr.wr.rkey = wr->rkey;
            lite_wr.wr.send_flag = wr->send_flag;
            lite_wr.aux = wr->aux;
            lite_wr.ext = wr->ext;
            return ra_hdc_lite_typical_send_wr(qp_hdc, &lite_wr, op_rsp, wr->wr_id);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to send_wr",
        qp_hdc->qpn, qp_hdc->qp_mode, qp_hdc->support_lite);

    return -ENOTSUPP;
}

int ra_hdc_typical_send_wr(struct ra_qp_handle *qp_hdc, struct send_wr *wr, struct send_wr_rsp *op_rsp)
{
    struct lite_send_wr lite_wr = { 0 };

    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            lite_wr.wr = *wr;
            return ra_hdc_lite_typical_send_wr(qp_hdc, &lite_wr, op_rsp, HDC_LITE_DEFAULT_WR_ID);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to send_wr",
        qp_hdc->qpn, qp_hdc->qp_mode, qp_hdc->support_lite);

    return -ENOTSUPP;
}

int ra_hdc_mr_dereg(struct ra_qp_handle *qp_hdc, struct mr_info *info)
{
    union op_mr_dereg_data mr_dereg_data = {0};
    mr_dereg_data.tx_data.rdev_index = qp_hdc->rdev_index;
    mr_dereg_data.tx_data.phy_id = qp_hdc->phy_id;
    mr_dereg_data.tx_data.qpn = qp_hdc->qpn;
    mr_dereg_data.tx_data.addr = info->addr;
    int ret;

    ret = ra_hdc_process_msg(RA_RS_MR_DEREG, qp_hdc->phy_id, (char *)&mr_dereg_data,
        sizeof(union op_mr_dereg_data));
    CHK_PRT_RETURN(ret, hccp_err("[dereg][ra_hdc_mr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    return 0;
}

int ra_hdc_mr_reg(struct ra_qp_handle *qp_hdc, struct mr_info *info)
{
    union op_mr_reg_data mr_reg_data = {0};
    int ret;

    mr_reg_data.tx_data.phy_id = qp_hdc->phy_id;
    mr_reg_data.tx_data.rdev_index = qp_hdc->rdev_index;
    mr_reg_data.tx_data.qpn = qp_hdc->qpn;
    mr_reg_data.tx_data.mr_reg_attr.addr = info->addr;
    mr_reg_data.tx_data.mr_reg_attr.len = info->size;
    mr_reg_data.tx_data.mr_reg_attr.access = info->access;

    ret = ra_hdc_process_msg(RA_RS_MR_REG, qp_hdc->phy_id,
        (char *)&mr_reg_data, sizeof(union op_mr_reg_data));
    CHK_PRT_RETURN(ret, hccp_err("[reg][ra_hdc_mr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    info->lkey = mr_reg_data.rx_data.lkey;
    info->rkey = mr_reg_data.rx_data.rkey;

    return 0;
}

int ra_hdc_typical_mr_reg(struct ra_rdma_handle *rdma_handle, struct mr_info *info, void **mr_handle)
{
    union op_typical_mr_reg_data mr_reg_data = {0};
    struct ra_mr_handle *mr_hdc = NULL;
    int ret;

    mr_hdc = (struct ra_mr_handle *)calloc(1, sizeof(struct ra_mr_handle));
    CHK_PRT_RETURN(mr_hdc == NULL, hccp_err("[reg][ra_hdc_typical_mr]mr_hdc calloc failed phy_id(%u)",
        rdma_handle->rdev_info.phy_id), -ENOMEM);

    mr_reg_data.tx_data.phy_id = rdma_handle->rdev_info.phy_id;
    mr_reg_data.tx_data.rdev_index = rdma_handle->rdev_index;
    mr_reg_data.tx_data.mr_reg_attr.addr = info->addr;
    mr_reg_data.tx_data.mr_reg_attr.len = info->size;
    mr_reg_data.tx_data.mr_reg_attr.access = info->access;

    ret = ra_hdc_process_msg(RA_RS_TYPICAL_MR_REG, rdma_handle->rdev_info.phy_id,
        (char *)&mr_reg_data, sizeof(union op_typical_mr_reg_data));
    if (ret) {
        hccp_err("[reg][ra_hdc_typical_mr]ra hdc message process failed ret(%d) phy_id(%u)", ret,
            rdma_handle->rdev_info.phy_id);
        free(mr_hdc);
        return ret;
    }

    info->lkey = mr_reg_data.rx_data.lkey;
    info->rkey = mr_reg_data.rx_data.rkey;
    mr_hdc->addr = (unsigned long long)(uintptr_t)info->addr;
    *mr_handle = mr_hdc;

    return 0;
}

int ra_hdc_remap_mr(struct ra_rdma_handle *rdma_handle, struct mem_remap_info info[], unsigned int num)
{
    union op_remap_mr_data op_data = {0};
    int ret;

    ret = memcpy_s(op_data.tx_data.mem_list, REMAP_MR_MAX_NUM * sizeof(struct mem_remap_info),
        info, num * sizeof(struct mem_remap_info));
    CHK_PRT_RETURN(ret != 0, hccp_err("[remap][ra_hdc_mr]memcpy_s mem_list failed, ret:%d", ret), -ESAFEFUNC);
    op_data.tx_data.mem_num = num;
    op_data.tx_data.rdev_index = rdma_handle->rdev_index;
    op_data.tx_data.phy_id = rdma_handle->rdev_info.phy_id;

    ret = ra_hdc_process_msg(RA_RS_REMAP_MR, op_data.tx_data.phy_id, (char *)&op_data, sizeof(union op_remap_mr_data));
    CHK_PRT_RETURN(ret, hccp_err("[remap][ra_hdc_mr]ra hdc message process failed ret(%d) phy_id(%u)", ret,
        rdma_handle->rdev_info.phy_id), ret);

    return 0;
}

int ra_hdc_typical_mr_dereg(struct ra_rdma_handle *rdma_handle, void *mr_handle)
{
    union op_typical_mr_dereg_data mr_dereg_data = {0};
    int ret;

    mr_dereg_data.tx_data.phy_id = rdma_handle->rdev_info.phy_id;
    mr_dereg_data.tx_data.rdev_index = rdma_handle->rdev_index;
    mr_dereg_data.tx_data.addr = ((struct ra_mr_handle*)mr_handle)->addr;

    ret = ra_hdc_process_msg(RA_RS_TYPICAL_MR_DEREG, rdma_handle->rdev_info.phy_id, (char *)&mr_dereg_data,
        sizeof(union op_typical_mr_dereg_data));
    CHK_PRT_RETURN(ret, hccp_err("[dereg][ra_hdc_typical_mr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, rdma_handle->rdev_info.phy_id), ret);

    free(mr_handle);
    mr_handle = NULL;
    return 0;
}

STATIC void ra_hdc_send_wrlist_init(union op_send_wrlist_data *send_wrlist, struct ra_qp_handle *qp_hdc,
    unsigned int complete_cnt, struct wrlist_send_complete_num wrlist_num)
{
    send_wrlist->tx_data.phy_id = qp_hdc->phy_id;
    send_wrlist->tx_data.rdev_index = qp_hdc->rdev_index;
    send_wrlist->tx_data.qpn = qp_hdc->qpn;
    send_wrlist->tx_data.send_num = (wrlist_num.send_num - complete_cnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlist_num.send_num - complete_cnt;
}

STATIC void ra_hdc_send_wrlist_ext_init(union op_send_wrlist_data_ext *send_wrlist, struct ra_qp_handle *qp_hdc,
    unsigned int complete_cnt, struct wrlist_send_complete_num wrlist_num)
{
    send_wrlist->tx_data.phy_id = qp_hdc->phy_id;
    send_wrlist->tx_data.rdev_index = qp_hdc->rdev_index;
    send_wrlist->tx_data.qpn = qp_hdc->qpn;
    send_wrlist->tx_data.send_num = (wrlist_num.send_num - complete_cnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlist_num.send_num - complete_cnt;
}

STATIC int ra_hdc_send_wrlist_v1(struct ra_qp_handle *qp_hdc, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int complete_cnt = 0;
    unsigned int current_send_num = 0;
    union op_send_wrlist_data *send_wrlist = NULL;

    send_wrlist = calloc(1, sizeof(union op_send_wrlist_data));
    CHK_PRT_RETURN(send_wrlist == NULL, hccp_err("[send][ra_hdc_wrlist]send_wrlist calloc failed"), -ENOMEM);
    while (complete_cnt < wrlist_num.send_num) {
        ra_hdc_send_wrlist_init(send_wrlist, qp_hdc, complete_cnt, wrlist_num);
        ret = memcpy_s(send_wrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data) * MAX_WR_NUM_V1),
            &wr[complete_cnt], (sizeof(struct send_wrlist_data) * send_wrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        current_send_num = send_wrlist->tx_data.send_num;
        ret = ra_hdc_process_msg(RA_RS_SEND_WRLIST, qp_hdc->phy_id, (char *)send_wrlist,
            sizeof(union op_send_wrlist_data));

        if (send_wrlist->rx_data.complete_num > current_send_num) {
            hccp_err("[send][ra_hdc_wrlist]complete_num[%u] is larger than send_num[%u], ret(%d).",
                send_wrlist->rx_data.complete_num, current_send_num, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < send_wrlist->rx_data.complete_num; i++) {
            j = i + complete_cnt;
            if (qp_hdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                op_rsp[j].wqe_tmp = send_wrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
                       qp_hdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                op_rsp[j].db = send_wrlist->rx_data.wr_rsp[i].db;
            }
        }
        complete_cnt = complete_cnt + send_wrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist]ra hdc message process failed ret(%d), phy_id(%u)", ret, qp_hdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(send_wrlist);
    send_wrlist = NULL;
    *(wrlist_num.complete_num) = complete_cnt;
    return ret;
}

STATIC int ra_hdc_send_wrlist_ext_v1(struct ra_qp_handle *qp_hdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp op_rsp[], struct wrlist_send_complete_num wrlist_num)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int complete_cnt = 0;
    unsigned int current_send_num = 0;
    union op_send_wrlist_data_ext *send_wrlist = NULL;

    send_wrlist = calloc(1, sizeof(union op_send_wrlist_data_ext));
    CHK_PRT_RETURN(send_wrlist == NULL, hccp_err("[send][ra_hdc_wrlist_ext]send_wrlist calloc failed"), -ENOMEM);
    while (complete_cnt < wrlist_num.send_num) {
        ra_hdc_send_wrlist_ext_init(send_wrlist, qp_hdc, complete_cnt, wrlist_num);
        ret = memcpy_s(send_wrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data_ext) * MAX_WR_NUM_V1),
            &wr[complete_cnt], (sizeof(struct send_wrlist_data_ext) * send_wrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist_ext]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        current_send_num = send_wrlist->tx_data.send_num;
        ret = ra_hdc_process_msg(RA_RS_SEND_WRLIST_EXT, qp_hdc->phy_id, (char *)send_wrlist,
            sizeof(union op_send_wrlist_data_ext));

        if (send_wrlist->rx_data.complete_num > current_send_num) {
            hccp_err("[send][ra_hdc_wrlist_ext]complete_num[%u] is larger than send_num[%u], ret(%d).",
                send_wrlist->rx_data.complete_num, current_send_num, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < send_wrlist->rx_data.complete_num; i++) {
            j = i + complete_cnt;
            if (qp_hdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                op_rsp[j].wqe_tmp = send_wrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                op_rsp[j].db = send_wrlist->rx_data.wr_rsp[i].db;
            }
        }
        complete_cnt = complete_cnt + send_wrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist_ext]ra hdc message process failed ret(%d), phy_id(%u)",
                    ret, qp_hdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(send_wrlist);
    send_wrlist = NULL;
    *(wrlist_num.complete_num) = complete_cnt;
    return ret;
}

STATIC void ra_hdc_send_wrlist_init_v2(union op_send_wrlist_data_v2 *send_wrlist, struct ra_qp_handle *qp_hdc,
    unsigned int complete_cnt, struct wrlist_send_complete_num wrlist_num)
{
    send_wrlist->tx_data.phy_id = qp_hdc->phy_id;
    send_wrlist->tx_data.rdev_index = qp_hdc->rdev_index;
    send_wrlist->tx_data.qpn = qp_hdc->qpn;
    send_wrlist->tx_data.send_num = (wrlist_num.send_num - complete_cnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlist_num.send_num - complete_cnt;
}

STATIC void ra_hdc_send_wrlist_ext_init_v2(union op_send_wrlist_data_ext_v2 *send_wrlist, struct ra_qp_handle *qp_hdc,
    unsigned int complete_cnt, struct wrlist_send_complete_num wrlist_num)
{
    send_wrlist->tx_data.phy_id = qp_hdc->phy_id;
    send_wrlist->tx_data.rdev_index = qp_hdc->rdev_index;
    send_wrlist->tx_data.qpn = qp_hdc->qpn;
    send_wrlist->tx_data.send_num = (wrlist_num.send_num - complete_cnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlist_num.send_num - complete_cnt;
}

STATIC int ra_hdc_send_wrlist_v2(struct ra_qp_handle *qp_hdc, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int complete_cnt = 0;
    unsigned int current_send_num = 0;
    union op_send_wrlist_data_v2 *send_wrlist = NULL;

    send_wrlist = calloc(1, sizeof(union op_send_wrlist_data_v2));
    CHK_PRT_RETURN(send_wrlist == NULL, hccp_err("[send][ra_hdc_wrlist_v2]send_wrlist calloc failed"), -ENOMEM);
    while (complete_cnt < wrlist_num.send_num) {
        ra_hdc_send_wrlist_init_v2(send_wrlist, qp_hdc, complete_cnt, wrlist_num);
        ret = memcpy_s(send_wrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data) * MAX_WR_NUM),
            &wr[complete_cnt], (sizeof(struct send_wrlist_data) * send_wrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist_v2]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        current_send_num = send_wrlist->tx_data.send_num;
        ret = ra_hdc_process_msg(RA_RS_SEND_WRLIST_V2, qp_hdc->phy_id, (char *)send_wrlist,
            sizeof(union op_send_wrlist_data_v2));

        if (send_wrlist->rx_data.complete_num > current_send_num) {
            hccp_err("[send][ra_hdc_wrlist_v2]complete_num[%u] is larger than send_num[%u], ret(%d).",
                send_wrlist->rx_data.complete_num, current_send_num, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < send_wrlist->rx_data.complete_num; i++) {
            j = i + complete_cnt;
            if (qp_hdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                op_rsp[j].wqe_tmp = send_wrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                op_rsp[j].db = send_wrlist->rx_data.wr_rsp[i].db;
            }
        }
        complete_cnt = complete_cnt + send_wrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist_v2]ra hdc message process failed ret(%d), phy_id(%u)",
                    ret, qp_hdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(send_wrlist);
    send_wrlist = NULL;
    *(wrlist_num.complete_num) = complete_cnt;
    return ret;
}

STATIC int ra_hdc_send_wrlist_ext_v2(struct ra_qp_handle *qp_hdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp op_rsp[], struct wrlist_send_complete_num wrlist_num)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int complete_cnt = 0;
    unsigned int current_send_num = 0;
    union op_send_wrlist_data_ext_v2 *send_wrlist = NULL;

    send_wrlist = calloc(1, sizeof(union op_send_wrlist_data_ext_v2));
    CHK_PRT_RETURN(send_wrlist == NULL, hccp_err("[send][ra_hdc_wrlist_ext_v2]send_wrlist calloc failed"), -ENOMEM);
    while (complete_cnt < wrlist_num.send_num) {
        ra_hdc_send_wrlist_ext_init_v2(send_wrlist, qp_hdc, complete_cnt, wrlist_num);
        ret = memcpy_s(send_wrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data_ext) * MAX_WR_NUM),
            &wr[complete_cnt], (sizeof(struct send_wrlist_data_ext) * send_wrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist_ext_v2]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        current_send_num = send_wrlist->tx_data.send_num;
        ret = ra_hdc_process_msg(RA_RS_SEND_WRLIST_EXT_V2, qp_hdc->phy_id, (char *)send_wrlist,
            sizeof(union op_send_wrlist_data_ext_v2));

        if (send_wrlist->rx_data.complete_num > current_send_num) {
            hccp_err("[send][ra_hdc_wrlist_ext_v2]complete_num[%u] is larger than send_num[%u], ret(%d).",
                send_wrlist->rx_data.complete_num, current_send_num, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < send_wrlist->rx_data.complete_num; i++) {
            j = i + complete_cnt;
            if (qp_hdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                op_rsp[j].wqe_tmp = send_wrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                op_rsp[j].db = send_wrlist->rx_data.wr_rsp[i].db;
            }
        }
        complete_cnt = complete_cnt + send_wrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist_ext_v2]ra hdc message process failed ret(%d), phy_id(%u)",
                    ret, qp_hdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(send_wrlist);
    send_wrlist = NULL;
    *(wrlist_num.complete_num) = complete_cnt;
    return ret;
}

int ra_hdc_send_wrlist(struct ra_qp_handle *qp_hdc, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    int ret;
    unsigned int interface_version = 0;

    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            return ra_hdc_lite_send_wrlist(qp_hdc, wr, op_rsp, wrlist_num);
        }
    }

    ret = ra_hdc_get_interface_version(qp_hdc->phy_id, RA_RS_SEND_WRLIST_V2, &interface_version);
    if (ret != 0 || interface_version != RA_RS_SEND_WRLIST_V2_VERSION) {
        return ra_hdc_send_wrlist_v1(qp_hdc, wr, op_rsp, wrlist_num);
    }

    return ra_hdc_send_wrlist_v2(qp_hdc, wr, op_rsp, wrlist_num);
}

int ra_hdc_send_wrlist_ext(struct ra_qp_handle *qp_hdc, struct send_wrlist_data_ext wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    int ret;
    unsigned int interface_version = 0;

    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            return ra_hdc_lite_send_wrlist_ext(qp_hdc, wr, op_rsp, wrlist_num);
        }
    }

    ret = ra_hdc_get_interface_version(qp_hdc->phy_id, RA_RS_SEND_WRLIST_EXT_V2, &interface_version);
    if (ret != 0 || interface_version != RA_RS_SEND_WRLIST_EXT_V2_VERSION) {
        return ra_hdc_send_wrlist_ext_v1(qp_hdc, wr, op_rsp, wrlist_num);
    }

    return ra_hdc_send_wrlist_ext_v2(qp_hdc, wr, op_rsp, wrlist_num);
}

STATIC void ra_hdc_send_wrlist_normal_init(union op_send_normal_wrlist_data *send_wrlist, struct ra_qp_handle *qp_hdc,
    unsigned int complete_cnt, struct wrlist_send_complete_num wrlist_num)
{
    (void)memset_s(send_wrlist, sizeof(union op_send_normal_wrlist_data), 0, sizeof(union op_send_normal_wrlist_data));
    send_wrlist->tx_data.phy_id = qp_hdc->phy_id;
    send_wrlist->tx_data.rdev_index = qp_hdc->rdev_index;
    send_wrlist->tx_data.qpn = qp_hdc->qpn;
    send_wrlist->tx_data.send_num = ((wrlist_num.send_num - complete_cnt) >= MAX_WR_NUM) ? MAX_WR_NUM :
        wrlist_num.send_num - complete_cnt;
}
 
STATIC int ra_hdc_send_wrlist_normal(struct ra_qp_handle *qp_hdc, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    union op_send_normal_wrlist_data *send_wrlist = NULL;
    unsigned int current_send_num = 0;
    unsigned int complete_cnt = 0;
    unsigned int i;
    int ret = 0;
 
    send_wrlist = calloc(1, sizeof(union op_send_normal_wrlist_data));
    CHK_PRT_RETURN(send_wrlist == NULL, hccp_err("[send][send_wrlist]send_wrlist calloc failed"), -ENOMEM);
 
    while (complete_cnt < wrlist_num.send_num) {
        ra_hdc_send_wrlist_normal_init(send_wrlist, qp_hdc, complete_cnt, wrlist_num);
        ret = memcpy_s(send_wrlist->tx_data.wrlist, (sizeof(struct wr_info) * MAX_WR_NUM),
            &wr[complete_cnt], (sizeof(struct wr_info) * send_wrlist->tx_data.send_num));
        if (ret != 0) {
            hccp_err("[send][send_wrlist]memcpy_s for wrlist failed, ret(%d)", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        current_send_num = send_wrlist->tx_data.send_num;
        ret = ra_hdc_process_msg(RA_RS_SEND_NORMAL_WRLIST, qp_hdc->phy_id, (char *)send_wrlist,
            sizeof(union op_send_normal_wrlist_data));
 
        if (send_wrlist->rx_data.complete_num > current_send_num) {
            hccp_err("[send][send_wrlist]complete_num[%u] is larger than send_num[%u], ret(%d)",
                send_wrlist->rx_data.complete_num, current_send_num, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }
 
        for (i = 0; i < send_wrlist->rx_data.complete_num; i++) {
            if (qp_hdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                op_rsp[complete_cnt + i].wqe_tmp = send_wrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                op_rsp[complete_cnt + i].db = send_wrlist->rx_data.wr_rsp[i].db;
            }
        }
        complete_cnt += send_wrlist->rx_data.complete_num;
        // send wrlist success, continue to send
        if (ret == 0) {
            continue;
        }
        if (ret != -ENOENT && ret != -ENOMEM) {
            hccp_err("[send][send_wrlist]ra hdc message process failed ret(%d), phy_id(%u)", ret, qp_hdc->phy_id);
        }
        goto err_send_wrlist;
    }
 
err_send_wrlist:
    free(send_wrlist);
    send_wrlist = NULL;
    *(wrlist_num.complete_num) = complete_cnt;
    return ret;
}

int ra_hdc_send_normal_wrlist(struct ra_qp_handle *qp_hdc, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            return ra_hdc_lite_send_normal_wrlist(qp_hdc, wr, op_rsp, wrlist_num);
        }
    }

    return ra_hdc_send_wrlist_normal(qp_hdc, wr, op_rsp, wrlist_num);
}

int ra_hdc_get_notify_base_addr(struct ra_rdma_handle *rdma_handle, unsigned long long *va, unsigned long long *size)
{
    int ret;
    union op_get_notify_ba_data get_notify_ba_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;

    get_notify_ba_data.tx_data.phy_id = phy_id;
    get_notify_ba_data.tx_data.rdev_index = rdma_handle->rdev_index;

    ret = ra_hdc_process_msg(RA_RS_GET_NOTIFY_BA, phy_id, (char *)&get_notify_ba_data,
        sizeof(union op_get_notify_ba_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_notify_base_addr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    *va = get_notify_ba_data.rx_data.va;
    *size = get_notify_ba_data.rx_data.size;
    return 0;
}

int ra_hdc_get_notify_mr_info(struct ra_rdma_handle *rdma_handle, struct mr_info *info)
{
    union op_get_notify_ba_data get_notify_ba_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    unsigned int interface_version = 0;
    int ret;

    // check opcode version, reuse RA_RS_GET_NOTIFY_BA
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_GET_NOTIFY_BA, &interface_version);
    if (ret != 0 || interface_version == RA_RS_GET_NOTIFY_BA_VERSION) {
        hccp_err("[get][ra_hdc_notify_mr_info]interface_version(%u) not support, ret(%d)", interface_version, ret);
        return -ENOTSUPP;
    }

    get_notify_ba_data.tx_data.phy_id = phy_id;
    get_notify_ba_data.tx_data.rdev_index = rdma_handle->rdev_index;

    ret = ra_hdc_process_msg(RA_RS_GET_NOTIFY_BA, phy_id, (char *)&get_notify_ba_data,
        sizeof(union op_get_notify_ba_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_notify_mr_info]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    info->addr = (void *)(uintptr_t)get_notify_ba_data.rx_data.va;
    info->size = get_notify_ba_data.rx_data.size;
    info->access = get_notify_ba_data.rx_data.access;
    info->lkey = get_notify_ba_data.rx_data.lkey;
    return 0;
}

int ra_hdc_recv_wrlist(struct ra_qp_handle *qp_hdc, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num)
{
    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            return ra_hdc_lite_recv_wrlist(qp_hdc, wr, recv_num, complete_num);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to recv_wrlist",
        qp_hdc->qpn, qp_hdc->qp_mode, qp_hdc->support_lite);

    return -ENOTSUPP;
}

int ra_hdc_poll_cq(struct ra_qp_handle *qp_hdc, bool is_send_cq, unsigned int num_entries, void *wc)
{
    struct rdma_lite_wc_v2 *lite_wc = (struct rdma_lite_wc_v2 *)wc;

    if (qp_hdc->qp_mode == RA_RS_OP_QP_MODE ||
        qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qp_hdc->support_lite != LITE_NOT_SUPPORT) {
            return ra_hdc_lite_poll_cq(qp_hdc, is_send_cq, num_entries, lite_wc);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to poll_cq",
        qp_hdc->qpn, qp_hdc->qp_mode, qp_hdc->support_lite);

    return -ENOTSUPP;
}

int ra_hdc_notify_cfg_set(unsigned int phy_id, unsigned long long va, unsigned long long size)
{
    union op_notify_cfg_set_data set_notify_ba_data = {0};
    int ret;

    set_notify_ba_data.tx_data.phy_id = phy_id;
    set_notify_ba_data.tx_data.va = va;
    set_notify_ba_data.tx_data.size = size;

    ret = ra_hdc_process_msg(RA_RS_NOTIFY_CFG_SET, phy_id, (char *)&set_notify_ba_data,
        sizeof(union op_notify_cfg_set_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_notify_cfg]ra hdc message process failed ret(%d), phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}

int ra_hdc_notify_cfg_get(unsigned int phy_id, unsigned long long *va,
    unsigned long long *size)
{
    union op_notify_cfg_get_data get_notify_ba_data = {0};
    int ret;

    get_notify_ba_data.tx_data.phy_id = phy_id;

    ret = ra_hdc_process_msg(RA_RS_NOTIFY_CFG_GET, phy_id, (char *)&get_notify_ba_data,
        sizeof(union op_notify_cfg_get_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_notify_cfg]ra hdc message process failed ret(%d), phy_id(%u)",
        ret, phy_id), ret);
    *va = get_notify_ba_data.rx_data.va;
    *size = get_notify_ba_data.rx_data.size;
    return 0;
}

STATIC int ra_hdc_rdev_init_with_backup(struct ra_rdma_handle *rdma_handle, unsigned int *rdev_index)
{
    union op_rdev_init_with_backup_data rdev_init_data = { 0 };
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    unsigned int interface_version = 0;
    int ret;

    ret = ra_hdc_get_interface_version(phy_id, RA_RS_RDEV_INIT_WITH_BACKUP, &interface_version);
    // check opcode version, not support to init rdev with backup info
    if (ret != 0 || interface_version < RA_RS_OPCODE_BASE_VERSION) {
        hccp_warn("[init][ra_hdc_rdev]get opcode[%d] not support, ret[%d] != 0 or interface_version[%u] is 0",
            RA_RS_RDEV_INIT_WITH_BACKUP, ret, interface_version);
        return -ENOTSUPP;
    }

    (void)memcpy_s(&(rdev_init_data.tx_data.rdev_info), sizeof(struct rdev),
        &rdma_handle->rdev_info, sizeof(struct rdev));
    (void)memcpy_s(&(rdev_init_data.tx_data.backup_rdev_info), sizeof(struct rdev),
        &rdma_handle->backup_info.rdev_info, sizeof(struct rdev));
    ret = ra_hdc_process_msg(RA_RS_RDEV_INIT_WITH_BACKUP, phy_id, (char *)&rdev_init_data,
        sizeof(union op_rdev_init_with_backup_data));
    if (ret) {
        hccp_err("[init][ra_hdc_rdev]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        return ret;
    }

    *rdev_index = rdev_init_data.rx_data.rdev_index;
    return 0;
}

int ra_hdc_rdev_init(struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info,
    unsigned int *rdev_index)
{
    union op_rdev_init_data rdev_init_data = { 0 };
    unsigned long long *notify_va = NULL;
    int ret, drv_ret;

    ret = ra_hdc_notify_base_addr_init(notify_type, rdev_info.phy_id, &notify_va);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_rdev]ra_hdc_notify_base_addr_init failed, ret(%d), phy_id(%u)",
        ret, rdev_info.phy_id), ret);

    // need to init backup rdev: reg notify & normal mr, prepare for aicpu unfold
    if (rdma_handle->backup_info.backup_flag) {
        ret = ra_hdc_rdev_init_with_backup(rdma_handle, rdev_index);
        if (ret) {
            goto free_mem;
        }
    } else {
        (void)memcpy_s(&(rdev_init_data.tx_data.rdev_info), sizeof(struct rdev), &rdev_info, sizeof(struct rdev));
        ret = ra_hdc_process_msg(RA_RS_RDEV_INIT, rdev_info.phy_id, (char *)&rdev_init_data,
            sizeof(union op_rdev_init_data));
        if (ret) {
            hccp_err("[init][ra_hdc_rdev]ra hdc message process failed ret(%d) phy_id(%u)", ret, rdev_info.phy_id);
            goto free_mem;
        }
        *rdev_index = rdev_init_data.rx_data.rdev_index;
    }

    ret = ra_hdc_lite_init(rdma_handle, rdev_info.phy_id, *rdev_index);
    if (ret) {
        hccp_err("[init][ra_hdc_rdev]ra_hdc_lite_init failed ret(%d) phy_id(%u)", ret, rdev_info.phy_id);
        goto free_mem;
    }

    return 0;

free_mem:
    drv_ret = dl_hal_mem_free((void *)notify_va);
    if (drv_ret) {
        hccp_err("[init][ra_hdc_rdev]halMemFree failed! drv_ret(%d)", drv_ret);
    }
    return ret;
}

int ra_hdc_rdev_get_port_status(struct ra_rdma_handle *rdma_handle, enum port_status *status)
{
    union op_rdev_get_port_status_data status_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    int ret;

    status_data.tx_data.rdev_index = rdma_handle->rdev_index;
    status_data.tx_data.phy_id = phy_id;

    ret = ra_hdc_process_msg(RA_RS_RDEV_GET_PORT_STATUS, phy_id, (char *)&status_data,
        sizeof(union op_rdev_get_port_status_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_hdc_port_status]ra hdc message process failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    *status = status_data.rx_data.status;
    return 0;
}

int ra_hdc_rdev_restore_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type)
{
    // lite thread is an inner thread, make sure it will exit
    ra_hdc_lite_deinit(rdma_handle);

    CHK_PRT_RETURN(notify_type != NOTIFY, hccp_err("[deinit][ra_hdc_rdev]notify_type[%u] error",
        notify_type), -EINVAL);

    return 0;
}

int ra_hdc_rdev_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type)
{
    union op_rdev_deinit_data rdev_deinit_data = {0};
    unsigned long long va, size;
    int ret;

    // lite thread is an inner thread, make sure it will exit
    ra_hdc_lite_deinit(rdma_handle);

    CHK_PRT_RETURN(notify_type != NOTIFY, hccp_err("[deinit][ra_hdc_rdev]notify_type[%u] error",
        notify_type), -EINVAL);

    rdev_deinit_data.tx_data.rdev_index = rdma_handle->rdev_index;
    rdev_deinit_data.tx_data.phy_id = rdma_handle->rdev_info.phy_id;

    ret = ra_hdc_process_msg(RA_RS_RDEV_DEINIT, rdma_handle->rdev_info.phy_id, (char *)&rdev_deinit_data,
        sizeof(union op_rdev_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rdev]ra_hdc_notify_cfg_get failed, ret(%d)", ret), ret);

    ret = ra_hdc_notify_cfg_get(rdma_handle->rdev_info.phy_id, &va, &size);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rdev]ra_hdc_notify_cfg_get failed, ret(%d)", ret), ret);

    ret = dl_hal_mem_free((void *)(uintptr_t)va);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rdev]halMemFree failed, ret(%d)", ret), ret);

    return 0;
}

int ra_hdc_set_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int temp_depth, unsigned int *qp_num)
{
    union op_set_tsqp_depth_data set_tsqp_depth_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    int ret;

    set_tsqp_depth_data.tx_data.phy_id = rdma_handle->rdev_info.phy_id;
    set_tsqp_depth_data.tx_data.rdev_index = rdma_handle->rdev_index;
    set_tsqp_depth_data.tx_data.temp_depth = temp_depth;

    ret = ra_hdc_process_msg(RA_RS_SET_TSQP_DEPTH, phy_id, (char *)&set_tsqp_depth_data,
        sizeof(union op_set_tsqp_depth_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_tsqp_depth]ra hdc message process failed ret(%d), opcode(%d)"
        "phy_id(%u)", ret, RA_RS_SET_TSQP_DEPTH, phy_id), ret);

    *qp_num = set_tsqp_depth_data.rx_data.qp_num;
    return 0;
}

int ra_hdc_get_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int *temp_depth, unsigned int *qp_num)
{
    union op_get_tsqp_depth_data get_tsqp_depth_data = {0};
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    int ret;

    get_tsqp_depth_data.tx_data.phy_id = rdma_handle->rdev_info.phy_id;
    get_tsqp_depth_data.tx_data.rdev_index = rdma_handle->rdev_index;

    ret = ra_hdc_process_msg(RA_RS_GET_TSQP_DEPTH, phy_id, (char *)&get_tsqp_depth_data,
        sizeof(union op_get_tsqp_depth_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_tsqp_depth]ra hdc message process failed ret(%d), opcode(%d)"
        "phy_id(%u)", ret, RA_RS_GET_TSQP_DEPTH, phy_id), ret);

    *temp_depth = get_tsqp_depth_data.rx_data.temp_depth;
    *qp_num = get_tsqp_depth_data.rx_data.qp_num;

    return 0;
}

int ra_hdc_set_qp_attr_qos(struct ra_qp_handle *qp_hdc, struct qos_attr *attr)
{
    union op_set_qp_attr_qos_data qp_attr_qos_data = {0};
    int ret;

    qp_attr_qos_data.tx_data.phy_id = qp_hdc->phy_id;
    qp_attr_qos_data.tx_data.rdev_index = qp_hdc->rdev_index;
    qp_attr_qos_data.tx_data.qpn = qp_hdc->qpn;
    qp_attr_qos_data.tx_data.qos_attr.tc = attr->tc;
    qp_attr_qos_data.tx_data.qos_attr.sl = attr->sl;

    ret = ra_hdc_process_msg(RA_RS_SET_QP_ATTR_QOS, qp_hdc->phy_id,
        (char *)&qp_attr_qos_data, sizeof(union op_set_qp_attr_qos_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_qp_attr_qos]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    return 0;
}

int ra_hdc_set_qp_attr_timeout(struct ra_qp_handle *qp_hdc, unsigned int *timeout)
{
    union op_set_qp_attr_timeout_data qp_attr_timeout_data = {0};
    int ret;

    qp_attr_timeout_data.tx_data.phy_id = qp_hdc->phy_id;
    qp_attr_timeout_data.tx_data.rdev_index = qp_hdc->rdev_index;
    qp_attr_timeout_data.tx_data.qpn = qp_hdc->qpn;
    qp_attr_timeout_data.tx_data.timeout = *timeout;

    ret = ra_hdc_process_msg(RA_RS_SET_QP_ATTR_TIMEOUT, qp_hdc->phy_id,
        (char *)&qp_attr_timeout_data, sizeof(union op_set_qp_attr_timeout_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_qp_attr_timeout]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    return 0;
}

int ra_hdc_set_qp_attr_retry_cnt(struct ra_qp_handle *qp_hdc, unsigned int *retry_cnt)
{
    union op_set_qp_attr_retry_cnt_data qp_attr_retry_cnt_data = {0};
    int ret;

    qp_attr_retry_cnt_data.tx_data.phy_id = qp_hdc->phy_id;
    qp_attr_retry_cnt_data.tx_data.rdev_index = qp_hdc->rdev_index;
    qp_attr_retry_cnt_data.tx_data.qpn = qp_hdc->qpn;
    qp_attr_retry_cnt_data.tx_data.retry_cnt = *retry_cnt;

    ret = ra_hdc_process_msg(RA_RS_SET_QP_ATTR_RETRY_CNT, qp_hdc->phy_id,
        (char *)&qp_attr_retry_cnt_data, sizeof(union op_set_qp_attr_retry_cnt_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_qp_attr_retry_cnt]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    return 0;
}

STATIC int ra_hdc_get_cqe_err_info_num(struct ra_rdma_handle *rdma_handle, unsigned int *num)
{
    union op_get_cqe_err_info_num_data cqe_err_info_num_data = { 0 };
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    unsigned int interface_version = 0;
    int ret;

    *num = 0;
    // check opcode version, not support to get cqe err info
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_GET_CQE_ERR_INFO_NUM, &interface_version);
    if (ret != 0 || interface_version == 0) {
        hccp_warn("[get][cqe_err_info_list]get opcode[%d] not support, ret[%d] != 0 or interface_version[%u] is 0",
            RA_RS_GET_CQE_ERR_INFO_NUM, ret, interface_version);
        return 0;
    }

    cqe_err_info_num_data.tx_data.phy_id = phy_id;
    cqe_err_info_num_data.tx_data.rdev_index = rdma_handle->rdev_index;
    ret = ra_hdc_process_msg(RA_RS_GET_CQE_ERR_INFO_NUM, phy_id,
        (char *)&cqe_err_info_num_data, sizeof(union op_get_cqe_err_info_num_data));
    CHK_PRT_RETURN(ret, hccp_err("ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id), ret);

    *num = cqe_err_info_num_data.rx_data.num;
    return 0;
}

int ra_hdc_get_cqe_err_info_list(struct ra_rdma_handle *rdma_handle, struct cqe_err_info *info_list, unsigned int *num)
{
    union op_get_cqe_err_info_list_data cqe_err_info_list_data = { 0 };
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    unsigned int lite_cqe_err_num = *num;
    unsigned int cqe_err_info_num = 0;
    unsigned int hdc_cqe_err_num = 0;
    int ret = 0;

    ret = ra_hdc_lite_get_cqe_err_info_list(rdma_handle, info_list, &lite_cqe_err_num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][cqe_err_info_list]get lite err info list failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    hdc_cqe_err_num = *num - lite_cqe_err_num;
    *num = lite_cqe_err_num;
    // lite cqe err info full up info_list
    if (hdc_cqe_err_num == 0) {
        return 0;
    }

    // get cqe err info num failed or not support to get cqe err info, skip get cqe err info list
    ret = ra_hdc_get_cqe_err_info_num(rdma_handle, &cqe_err_info_num);
    if (ret != 0 || cqe_err_info_num == 0) {
        return ret;
    }

    cqe_err_info_list_data.tx_data.phy_id = phy_id;
    cqe_err_info_list_data.tx_data.rdev_index = rdma_handle->rdev_index;
    cqe_err_info_list_data.tx_data.num = hdc_cqe_err_num;
    ret = ra_hdc_process_msg(RA_RS_GET_CQE_ERR_INFO_LIST, phy_id,
        (char *)&cqe_err_info_list_data, sizeof(union op_get_cqe_err_info_list_data));
    CHK_PRT_RETURN(ret, hccp_err("ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id), ret);

    if (cqe_err_info_list_data.rx_data.num > hdc_cqe_err_num) {
        hccp_err("[get][cqe_err_info_list]rx_data.num(%u) is invalid, num(%u), phy_id(%u)",
            cqe_err_info_list_data.rx_data.num, hdc_cqe_err_num, phy_id);
        return -EINVAL;
    }
    ret = memcpy_s(&info_list[lite_cqe_err_num], sizeof(struct cqe_err_info) * hdc_cqe_err_num,
        &cqe_err_info_list_data.rx_data.info_list, sizeof(struct cqe_err_info) * cqe_err_info_list_data.rx_data.num);
    if (ret) {
        hccp_err("[get][cqe_err_info_list]memcpy_s info_list failed, ret(%d) phy_id(%u) num(%u) rx_data.num(%u)",
            ret, phy_id, hdc_cqe_err_num, cqe_err_info_list_data.rx_data.num);
        return ret;
    }

    *num = lite_cqe_err_num + cqe_err_info_list_data.rx_data.num;
    return 0;
}

STATIC int ra_hdc_lite_clean_cq(struct ra_qp_handle *qp_handle, bool is_send_cq, unsigned int num_entries)
{
    void *wc = NULL;
    int ret;

    if (num_entries == 0) {
        return 0;
    }

    wc = calloc(num_entries, sizeof(struct rdma_lite_wc_v2));
    if (wc == NULL) {
        hccp_err("calloc failed, phy_id:%u, qpn:%u", qp_handle->phy_id, qp_handle->qpn);
        return -ENOMEM;
    }

    ret = ra_hdc_lite_poll_cq(qp_handle, is_send_cq, num_entries, wc);
    free(wc);
    if (ret < 0) {
        hccp_err("ra_hdc_lite_poll_cq failed, ret:%d, phy_id:%u, qpn:%u", ret, qp_handle->phy_id, qp_handle->qpn);
        return ret;
    }

    return 0;
}

STATIC int ra_hdc_lite_clean_qp(struct ra_qp_handle *qp_handle)
{
    unsigned int interface_version = 0;
    int ret;

    // check opcode versioin, not support to clean qp
    ret = ra_hdc_get_interface_version(qp_handle->phy_id, RA_RS_QP_BATCH_MODIFY, &interface_version);
    if (ret != 0 || interface_version <= RA_RS_OPCODE_BASE_VERSION) {
        hccp_warn("RA_RS_QP_BATCH_MODIFY interface_version:%u <= %u, not support to clean qp, phy_id:%u, qpn:%u",
            interface_version, RA_RS_OPCODE_BASE_VERSION, qp_handle->phy_id, qp_handle->qpn);
        return 0;
    }

    // lite qp clean
    ret = ra_rdma_lite_clean_qp(qp_handle->lite_qp);
    if (ret != 0) {
        hccp_err("ra_rdma_lite_clean_qp failed, ret:%d, phy_id:%u, qpn:%u", ret, qp_handle->phy_id, qp_handle->qpn);
        return ret;
    }

    return 0;
}

STATIC int ra_hdc_lite_clean_queue(struct ra_qp_handle *qp_handle, int expect_status)
{
    int ret;

    // not pause status, no need to clean
    if (expect_status != RA_QP_STATUS_PAUSE) {
        return 0;
    }

    // not lite or not op mode, no need to clean
    if ((qp_handle->support_lite == 0) ||
        (qp_handle->qp_mode != RA_RS_OP_QP_MODE && qp_handle->qp_mode != RA_RS_OP_QP_MODE_EXT)) {
        return 0;
    }

    // poll cq to clean lite send cq
    if (qp_handle->send_wr_num > qp_handle->poll_cqe_num) {
        ret = ra_hdc_lite_clean_cq(qp_handle, true, qp_handle->send_wr_num - qp_handle->poll_cqe_num);
        if (ret != 0) {
            hccp_err("ra_hdc_lite_clean_cq send_cq failed, ret:%d, phy_id:%u, qpn:%u",
                ret, qp_handle->phy_id, qp_handle->qpn);
            return ret;
        }
    }

    // poll cq to clean lite recv cq
    if (qp_handle->recv_wr_num > qp_handle->poll_recv_cqe_num) {
        ret = ra_hdc_lite_clean_cq(qp_handle, false, qp_handle->recv_wr_num - qp_handle->poll_recv_cqe_num);
        if (ret < 0) {
            hccp_err("ra_hdc_lite_clean_cq recv_cq failed, ret:%d, phy_id:%u, qpn:%u",
                ret, qp_handle->phy_id, qp_handle->qpn);
            return ret;
        }
    }

    // lite qp clean
    ret = ra_hdc_lite_clean_qp(qp_handle);
    if (ret != 0) {
        hccp_err("ra_hdc_lite_clean_qp failed, ret:%d, phy_id:%u, qpn:%u", ret, qp_handle->phy_id, qp_handle->qpn);
        return ret;
    }

    return 0;
}

int ra_hdc_qp_batch_modify(struct ra_rdma_handle *rdma_handle, void *qp_hdc[], unsigned int num, int expect_status)
{
    union op_qp_batch_modify_data *qp_batch_modify_data = NULL;
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct ra_qp_handle *qp_handle = NULL;
    unsigned int current_send_cnt = 0;
    unsigned int complete_cnt = 0;
    int op_qpn_cnt = 0;
    unsigned int i;
    int ret = 0;

    qp_batch_modify_data = calloc(1, sizeof(union op_qp_batch_modify_data));
    CHK_PRT_RETURN(qp_batch_modify_data == NULL,
        hccp_err("[send][ra_hdc_qp_batch_modify]qp_batch_modify calloc failed"), -ENOMEM);
    while (complete_cnt < num) {
        qp_batch_modify_data->tx_data.phy_id = phy_id;
        qp_batch_modify_data->tx_data.rdev_index = rdma_handle->rdev_index;
        qp_batch_modify_data->tx_data.status = expect_status;

        current_send_cnt = (num - complete_cnt) < RA_MAX_BATCH_QP_MODIFY_NUM ?
            (num - complete_cnt) : RA_MAX_BATCH_QP_MODIFY_NUM;
        op_qpn_cnt = 0;
        for (i = complete_cnt; i < complete_cnt + current_send_cnt; i++) {
            qp_handle = (struct ra_qp_handle *)qp_hdc[i];
            qp_batch_modify_data->tx_data.qpn[op_qpn_cnt] = (int)qp_handle->qpn;
            op_qpn_cnt++;

            // avoid poll invalid cqe after modify to RESET state, make sure lite cq ci & qp pointer are valid
            ret = ra_hdc_lite_clean_queue(qp_handle, expect_status);
            if (ret != 0) {
                hccp_err("[modify][qp_batch_modify]ra_hdc_lite_clean_queue failed ret(%d) phy_id(%u) qpn(%u)",
                    ret, phy_id, qp_handle->qpn);
                goto err_qp_batch_modify;
            }
        }
        qp_batch_modify_data->tx_data.qpn_num = op_qpn_cnt;

        ret = ra_hdc_process_msg(RA_RS_QP_BATCH_MODIFY, phy_id, (char *)qp_batch_modify_data,
            sizeof(union op_qp_batch_modify_data));
        if (ret) {
            hccp_err("[modify][qp_batch_modify]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
            goto err_qp_batch_modify;
        }
        complete_cnt += (unsigned int)op_qpn_cnt;
    }

err_qp_batch_modify:
    free(qp_batch_modify_data);
    qp_batch_modify_data = NULL;

    return ret;
}

int ra_hdc_rdma_set_ops(struct ra_rdma_handle *rdma_handle, struct ra_rdma_ops *rdma_ops)
{
    CHK_PRT_RETURN(rdma_handle == NULL, hccp_err("ra_hdc_rdma_set_ops rdma_handle is NULL"), -EINVAL);

    rdma_handle->rdma_ops = rdma_ops;
    return 0;
}

int ra_hdc_rdma_save_snapshot(struct ra_rdma_handle *rdma_handle, enum save_snapshot_action action)
{
    int ret = 0;

    if (rdma_handle == NULL || rdma_handle->support_lite == LITE_NOT_SUPPORT || rdma_handle->disabled_lite_thread) {
        return 0;
    }

    RA_PTHREAD_MUTEX_LOCK(&rdma_handle->rdev_mutex);
    if (action == SAVE_SNAPSHOT_ACTION_PRE_PROCESSING && rdma_handle->thread_status == LITE_THREAD_STATUS_RUNNING) {
        rdma_handle->thread_status = LITE_THREAD_STATUS_SUSPEND;
    } else if (action == SAVE_SNAPSHOT_ACTION_POST_PROCESSING && rdma_handle->thread_status == LITE_THREAD_STATUS_SUSPEND) {
        rdma_handle->thread_status = LITE_THREAD_STATUS_RUNNING;
    } else {
        hccp_err("duplicate or incorrect order calls are not allowed, thread_status[%d] action[%d]",
            rdma_handle->thread_status, action);
        ret = -EPERM;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);

    return ret;
}

int ra_hdc_rdma_restore_snapshot(struct ra_rdma_handle *rdma_handle, struct ra_rdma_ops *rdma_ops)
{
    int ret = 0;

    if (rdma_handle == NULL) {
        return 0;
    }
    ret = ra_hdc_rdma_set_ops(rdma_handle, rdma_ops);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_rdma_set_ops failed, ret[%d]", ret), ret);

    if (rdma_handle->support_lite == LITE_NOT_SUPPORT || rdma_handle->disabled_lite_thread) {
        return 0;
    }

    RA_PTHREAD_MUTEX_LOCK(&rdma_handle->rdev_mutex);
    if (rdma_handle->thread_status != LITE_THREAD_STATUS_SUSPEND) {
        hccp_err("incorrect order calls are not allowed, thread_status[%d]", rdma_handle->thread_status);
        ret = -EPERM;
        goto unlock_mutex;
    }
    ret = ra_rdma_lite_restore_snapshot(rdma_handle->lite_ctx);
    if (ret != 0) {
        hccp_err("ra_rdma_lite_restore_snapshot failed, ret[%d]", ret);
    }

unlock_mutex:
    RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);
    return ret;
}
