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

STATIC int RaHdcNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId, unsigned long long **notifyVa)
{
    unsigned long long moudleId = HCCP;
    unsigned int notifySize = 0;
    unsigned int logicId;
    int ret, drvRet;

    CHK_PRT_RETURN(notifyType != NOTIFY, hccp_err("[init][base_addr]notify_type[%u] error", notifyType), -EINVAL);
    ret = DlDrvDeviceGetIndexByPhyId(phyId, &logicId);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]drvDeviceGetIndexByPhyId failed, ret(%d), phyId(%u)",
        ret, phyId), ret);

    ret = DlHalNotifyGetInfo(logicId, 0, RA_NOTIFY_TYPE_TOTAL_SIZE, &notifySize);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]halNotifyGetInfo failed, ret(%d), logicId(%u)",
        ret, logicId), ret);

    ret = DlHalMemAlloc((void *)notifyVa, (unsigned long long)notifySize,
        RA_MEM_TYPE_HBM | (moudleId << MEM_MODULE_ID_BIT));
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]halMemAlloc failed, ret(%d), phyId(%u)", ret, phyId), ret);

    hccp_info("notify info: size[%u]", notifySize);
    ret = RaHdcNotifyCfgSet(phyId, (uintptr_t)*notifyVa, notifySize);
    if (ret) {
        hccp_err("[init][base_addr]ra_hdc_notify_cfg_set failed, ret(%d), phyId(%u)", ret, phyId);
        goto free_mem;
    }
    return 0;

free_mem:
    drvRet = DlHalMemFree((void *)*notifyVa);
    if (drvRet) {
        hccp_err("[init][base_addr]halMemFree failed! ret(%d)", drvRet);
    }
    return ret;
}

static void RaHdcGetQpHdc(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, unsigned int qpn,
    struct ra_qp_handle *qpHdc)
{
    qpHdc->phy_id = rdmaHandle->rdev_info.phy_id;
    qpHdc->rdev_index = rdmaHandle->rdev_index;
    qpHdc->rdma_handle = rdmaHandle;
    qpHdc->rdma_ops = rdmaHandle->rdma_ops;
    qpHdc->qp_mode = qpMode;
    qpHdc->flag = flag;
    qpHdc->qpn = qpn;
}

STATIC int RaHdcCmdQpDestroy(struct ra_qp_handle *qpHdc)
{
    int ret;
    union op_qp_destroy_data qpDestroyData = {0};

    qpDestroyData.tx_data.qpn = qpHdc->qpn;
    qpDestroyData.tx_data.phy_id = qpHdc->phy_id;
    qpDestroyData.tx_data.rdev_index = qpHdc->rdev_index;
    ret = RaHdcProcessMsg(RA_RS_QP_DESTROY, qpHdc->phy_id, (char *)&qpDestroyData,
        sizeof(union op_qp_destroy_data));
    if (ret) {
        hccp_err("[destroy][ra_hdc_qp]hdc_send_recv_pkt failed ret(%d) phy_id(%u)", ret, qpHdc->phy_id);
    }

    return ret;
}

int RaHdcQpCreate(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, void **qpHandle)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    union op_qp_create_data qpCreateData = {0};
    struct ra_qp_handle *qpHdc = NULL;
    struct rdma_lite_qp_cap cap;
    int ret;

    qpHdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpHdc == NULL, hccp_err("[create][ra_hdc_qp]qp_hdc calloc failed phy_id(%u)", phyId), -ENOMEM);

    qpCreateData.tx_data.phy_id = phyId;
    qpCreateData.tx_data.rdev_index = rdmaHandle->rdev_index;
    qpCreateData.tx_data.flag = flag;
    qpCreateData.tx_data.qp_mode = qpMode;
    qpCreateData.tx_data.mem_align = rdmaHandle->support_lite;

    ret = RaHdcProcessMsg(RA_RS_QP_CREATE, phyId, (char *)&qpCreateData,
        sizeof(union op_qp_create_data));
    if (ret) {
        hccp_err("[create][ra_hdc_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(qpHdc);
        qpHdc = NULL;
        return ret;
    }

    RaHdcGetQpHdc(rdmaHandle, flag, qpMode, qpCreateData.rx_data.qpn, qpHdc);
    qpHdc->psn = qpCreateData.rx_data.psn;
    qpHdc->gid_idx = qpCreateData.rx_data.gid_idx;

    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = QP_DEFAULT_MIN_CAP_RECV_SGE;
    cap.max_send_wr = RA_QP_32K_DEPTH;
    cap.max_recv_wr = RA_QP_128_DEPTH;
    ret = RaHdcLiteQpCreate(rdmaHandle, qpHdc, &cap);
    if (ret) {
        (void)RaHdcCmdQpDestroy(qpHdc);
        hccp_err("[create][ra_hdc_qp]ra_hdc_lite_qp_create failed ret(%d) phy_id(%u)", ret, phyId);
        free(qpHdc);
        qpHdc = NULL;
        return ret;
    }

    qpHdc->sq_depth = cap.max_send_wr;
    *qpHandle = qpHdc;

    return 0;
}

int RaHdcQpCreateWithAttrs(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs, void **qpHandle)
{
    int flag = (extAttrs->qp_attr.qp_type == IBV_QPT_RC) ? 0 : 1;
    union op_qp_create_with_attrs_data opData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct ra_qp_handle *qpHdc = NULL;
    struct rdma_lite_qp_cap cap;
    int ret;

    qpHdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpHdc == NULL, hccp_err("[create][ra_hdc_qp_with_attrs]qp_hdc calloc failed phy_id(%u)", phyId),
        -ENOMEM);

    opData.tx_data.phy_id = phyId;
    opData.tx_data.rdev_index = rdmaHandle->rdev_index;
    ret = memcpy_s(&opData.tx_data.ext_attrs, sizeof(struct qp_ext_attrs), extAttrs, sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_hdc_qp_with_attrs]memcpy_s for ext_attrs failed, ret:%d", ret);
        ret = -ESAFEFUNC;
        goto out;
    }

    opData.tx_data.ext_attrs.mem_align = rdmaHandle->support_lite;
    ret = RaHdcProcessMsg(RA_RS_QP_CREATE_WITH_ATTRS, phyId, (char *)&opData,
        sizeof(union op_qp_create_with_attrs_data));
    if (ret) {
        hccp_err("[create][ra_hdc_qp_with_attrs]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        goto out;
    }

    RaHdcGetQpHdc(rdmaHandle, flag, extAttrs->qp_mode, opData.rx_data.qpn, qpHdc);
    qpHdc->psn = opData.rx_data.psn;
    qpHdc->gid_idx = opData.rx_data.gid_idx;

    cap.max_inline_data = extAttrs->qp_attr.cap.max_inline_data;
    cap.max_send_sge = extAttrs->qp_attr.cap.max_send_sge;
    cap.max_recv_sge = extAttrs->qp_attr.cap.max_recv_sge;
    cap.max_send_wr = extAttrs->qp_attr.cap.max_send_wr;
    cap.max_recv_wr = extAttrs->qp_attr.cap.max_recv_wr;
    ret = RaHdcLiteQpCreate(rdmaHandle, qpHdc, &cap);
    if (ret) {
        (void)RaHdcCmdQpDestroy(qpHdc);
        hccp_err("[create][ra_hdc_qp_with_attrs]ra_hdc_lite_qp_create failed ret(%d) phy_id(%u)", ret, phyId);
        goto out;
    }

    qpHdc->sq_sig_all = extAttrs->qp_attr.sq_sig_all;
    qpHdc->udp_sport = extAttrs->udp_sport;
    qpHdc->sq_depth = cap.max_send_wr;
    *qpHandle = qpHdc;
    return 0;

out:
    free(qpHdc);
    qpHdc = NULL;
    return ret;
}

int RaHdcAiQpCreate(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs,
    struct ai_qp_info *info, void **qpHandle)
{
#define AI_QP_DEFAULT_GID_IDX 3U
    int flag = extAttrs->qp_attr.qp_type == IBV_QPT_RC ? 0 : 1;
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    union op_ai_qp_create_data qpCreateData = {0};
    struct ra_qp_handle *qpHdc = NULL;
    int qpMode = extAttrs->qp_mode;
    int ret;

    qpHdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpHdc == NULL, hccp_err("[create][ra_hdc_ai_qp]qp_hdc calloc failed phy_id(%u)", phyId),
        -ENOMEM);

    qpCreateData.tx_data.phy_id = phyId;
    qpCreateData.tx_data.rdev_index = rdmaHandle->rdev_index;
    ret = memcpy_s(&qpCreateData.tx_data.ext_attrs, sizeof(struct qp_ext_attrs), extAttrs,
        sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]memcpy_s for ext_attrs failed, ret:%d", ret);
        free(qpHdc);
        qpHdc = NULL;
        return -ESAFEFUNC;
    }

    ret = RaHdcProcessMsg(RA_RS_AI_QP_CREATE, phyId, (char *)&qpCreateData,
        sizeof(union op_ai_qp_create_data));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(qpHdc);
        qpHdc = NULL;
        return ret;
    }

    RaHdcGetQpHdc(rdmaHandle, flag, qpMode, qpCreateData.rx_data.qpn, qpHdc);
    qpHdc->psn = qpCreateData.rx_data.psn;
    // set a default gid_idx due to compatibility issue, rs will refresh it if it is different
    qpHdc->gid_idx = AI_QP_DEFAULT_GID_IDX;
    info->ai_qp_addr = qpCreateData.rx_data.ai_qp_addr;
    info->sq_index = qpCreateData.rx_data.sq_index;
    info->db_index = qpCreateData.rx_data.db_index;
    qpHdc->udp_sport = extAttrs->udp_sport;
    *qpHandle = qpHdc;

    return 0;
}

int RaHdcAiQpCreateWithAttrs(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs,
    struct ai_qp_info *info, void **qpHandle)
{
    int flag = extAttrs->qp_attr.qp_type == IBV_QPT_RC ? 0 : 1;
    union op_ai_qp_create_with_attrs_data qpCreateData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct ra_qp_handle *qpHdc = NULL;
    int qpMode = extAttrs->qp_mode;
    int ret;

    qpHdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpHdc == NULL, hccp_err("[create][ra_hdc_ai_qp]qp_hdc calloc failed phy_id(%u)", phyId),
        -ENOMEM);

    qpCreateData.tx_data.phy_id = phyId;
    qpCreateData.tx_data.rdev_index = rdmaHandle->rdev_index;
    ret = memcpy_s(&qpCreateData.tx_data.ext_attrs, sizeof(struct qp_ext_attrs), extAttrs,
        sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]memcpy_s for ext_attrs failed, ret:%d", ret);
        free(qpHdc);
        qpHdc = NULL;
        return -ESAFEFUNC;
    }

    ret = RaHdcProcessMsg(RA_RS_AI_QP_CREATE_WITH_ATTRS, phyId, (char *)&qpCreateData,
        sizeof(union op_ai_qp_create_with_attrs_data));
    if (ret) {
        hccp_err("[create][ra_hdc_ai_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(qpHdc);
        qpHdc = NULL;
        return ret;
    }

    qpHdc->udp_sport = extAttrs->udp_sport;
    RaHdcGetQpHdc(rdmaHandle, flag, qpMode, qpCreateData.rx_data.qpn, qpHdc);
    qpHdc->gid_idx = qpCreateData.rx_data.gid_idx;
    qpHdc->psn = qpCreateData.rx_data.psn;
    info->ai_qp_addr = qpCreateData.rx_data.ai_qp_addr;
    info->sq_index = qpCreateData.rx_data.sq_index;
    info->db_index = qpCreateData.rx_data.db_index;
    info->ai_scq_addr = qpCreateData.rx_data.ai_scq_addr;
    info->ai_rcq_addr = qpCreateData.rx_data.ai_rcq_addr;
    (void)memcpy_s(&info->data_plane_info, sizeof(struct ai_data_plane_info), &qpCreateData.rx_data.data_plane_info,
        sizeof(struct ai_data_plane_info));
    *qpHandle = qpHdc;

    return 0;
}

int RaHdcTypicalQpCreate(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, struct typical_qp *qpInfo,
    void **qpHandle)
{
    union op_typical_qp_create_data qpCreateData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct ra_qp_handle *qpHdc = NULL;
    struct rdma_lite_qp_cap cap;
    int ret;

    qpHdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpHdc == NULL, hccp_err("[create][ra_hdc_typical_qp]qp_hdc calloc failed phy_id(%u)", phyId),
        -ENOMEM);

    qpCreateData.tx_data.phy_id = phyId;
    qpCreateData.tx_data.rdev_index = rdmaHandle->rdev_index;
    qpCreateData.tx_data.flag = flag;
    qpCreateData.tx_data.qp_mode = qpMode;
    qpCreateData.tx_data.mem_align = rdmaHandle->support_lite;

    ret = RaHdcProcessMsg(RA_RS_TYPICAL_QP_CREATE, phyId, (char *)&qpCreateData,
        sizeof(union op_typical_qp_create_data));
    if (ret) {
        hccp_err("[create][ra_hdc_typical_qp]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(qpHdc);
        qpHdc = NULL;
        return ret;
    }

    qpInfo->gid_idx = qpCreateData.rx_data.gid_idx;
    qpInfo->psn = qpCreateData.rx_data.psn;
    qpInfo->qpn = qpCreateData.rx_data.qpn;
    (void)memcpy_s(qpInfo->gid, HCCP_GID_RAW_LEN, qpCreateData.rx_data.gid.raw, HCCP_GID_RAW_LEN);

    RaHdcGetQpHdc(rdmaHandle, flag, qpMode, qpInfo->qpn, qpHdc);
    qpHdc->psn = qpCreateData.rx_data.psn;
    qpHdc->gid_idx = qpCreateData.rx_data.gid_idx;

    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = QP_DEFAULT_MIN_CAP_RECV_SGE;
    cap.max_send_wr = RA_QP_32K_DEPTH;
    cap.max_recv_wr = RA_QP_128_DEPTH;
    ret = RaHdcLiteQpCreate(rdmaHandle, qpHdc, &cap);
    if (ret) {
        (void)RaHdcCmdQpDestroy(qpHdc);
        hccp_err("[create][ra_hdc_typical_qp]ra_hdc_lite_qp_create failed ret(%d) phy_id(%u)", ret, phyId);
        free(qpHdc);
        qpHdc = NULL;
        return ret;
    }

    qpHdc->sq_depth = cap.max_send_wr;
    *qpHandle = qpHdc;

    return 0;
}

int RaHdcQpDestroy(struct ra_qp_handle *qpHdc)
{
    int ret;

    RaHdcLiteQpDestroy(qpHdc);
    ret = RaHdcCmdQpDestroy(qpHdc);
    if (ret) {
        hccp_err("[destroy][ra_hdc_qp]ra_hdc_cmd_qp_destroy failed ret(%d) phy_id(%u)", ret, qpHdc->phy_id);
    }

    free(qpHdc);
    qpHdc = NULL;
    return ret;
}

int RaHdcGetQpStatus(struct ra_qp_handle *qpHdc, int *status)
{
    union op_qp_status_data qpStatusData = {0};
    union op_qp_info_data qpInfoData = {0};
    unsigned int interfaceVersion = 0;
    int ret;

    ret = RaHdcGetInterfaceVersion(qpHdc->phy_id, RA_RS_QP_INFO, &interfaceVersion);
    if (ret != 0) {
        hccp_warn("[get][ra_hdc_qp_status]get interface version not success ret(%d) phy_id(%u)", ret, qpHdc->phy_id);
        interfaceVersion = 0;
    }

    if (interfaceVersion >= RA_RS_OPCODE_BASE_VERSION) {
        qpInfoData.tx_data.qpn = qpHdc->qpn;
        qpInfoData.tx_data.phy_id = qpHdc->phy_id;
        qpInfoData.tx_data.rdev_index = qpHdc->rdev_index;
        ret = RaHdcProcessMsg(RA_RS_QP_INFO, qpHdc->phy_id, (char *)&qpInfoData,
            sizeof(union op_qp_info_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_qp_status]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, qpHdc->phy_id), ret);
        *status = qpInfoData.rx_data.status;
        qpHdc->udp_sport = qpInfoData.rx_data.udp_sport;
    } else {
        qpStatusData.tx_data.qpn = qpHdc->qpn;
        qpStatusData.tx_data.phy_id = qpHdc->phy_id;
        qpStatusData.tx_data.rdev_index = qpHdc->rdev_index;
        ret = RaHdcProcessMsg(RA_RS_QP_STATUS, qpHdc->phy_id, (char *)&qpStatusData,
            sizeof(union op_qp_status_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_qp_status]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, qpHdc->phy_id), ret);
        *status = qpStatusData.rx_data.status;
    }

    return RaHdcLiteGetConnectedInfo(qpHdc);
}

int RaHdcTypicalQpModify(struct ra_qp_handle *qpHdc, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo)
{
    union op_typical_qp_modify_data qpModifyData = {0};
    unsigned int phyId = qpHdc->phy_id;
    int ret;

    qpModifyData.tx_data.phy_id = phyId;
    qpModifyData.tx_data.rdev_index = qpHdc->rdev_index;
    ret = memcpy_s(&(qpModifyData.tx_data.local_qp_info), sizeof(struct typical_qp), localQpInfo,
        sizeof(struct typical_qp));
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]memcpy_s local_qp_info failed, phyId[%u] ret[%d]", phyId, ret),
        -ESAFEFUNC);
    ret = memcpy_s(&(qpModifyData.tx_data.remote_qp_info), sizeof(struct typical_qp), remoteQpInfo,
        sizeof(struct typical_qp));
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]memcpy_s remote_qp_info failed, phyId[%u] ret[%d]", phyId, ret),
        -ESAFEFUNC);

    ret = RaHdcProcessMsg(RA_RS_TYPICAL_QP_MODIFY, phyId, (char *)&qpModifyData,
        sizeof(union op_typical_qp_modify_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify][modify_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    qpHdc->udp_sport = qpModifyData.rx_data.udp_sport;
    if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
        ret = RaRdmaLiteSetQpSl(qpHdc->lite_qp, localQpInfo->sl);
        CHK_PRT_RETURN(ret != 0, hccp_err("[modify][modify_qp]ra_rdma_lite_set_qp_sl sl(%u) failed ret(%d) phy_id(%u)",
            localQpInfo->sl, ret, phyId), ret);
    }
    return 0;
}

int RaHdcQpConnectAsync(struct ra_qp_handle *qpHdc, const void *sockHandle)
{
    union op_qp_connect_data qpConnectData = {0};
    int ret;

    qpConnectData.tx_data.qpn = qpHdc->qpn;
    qpConnectData.tx_data.fd = (unsigned int)((const struct socket_hdc_info *)sockHandle)->fd;
    qpConnectData.tx_data.phy_id = qpHdc->phy_id;
    qpConnectData.tx_data.rdev_index = qpHdc->rdev_index;
    ret = RaHdcProcessMsg(RA_RS_QP_CONNECT, qpHdc->phy_id, (char *)&qpConnectData,
        sizeof(union op_qp_connect_data));
    CHK_PRT_RETURN(ret, hccp_err("[connect_async][ra_hdc_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    return 0;
}

static void RaHdcSendDataInit(union op_send_wr_data *sendWrData, struct ra_qp_handle *qpHdc, struct send_wr *wr)
{
    sendWrData->tx_data.phy_id = qpHdc->phy_id;
    sendWrData->tx_data.rdev_index = qpHdc->rdev_index;
    sendWrData->tx_data.qpn = qpHdc->qpn;
    sendWrData->tx_data.buf_num = wr->buf_num;
    sendWrData->tx_data.dst_addr = wr->dst_addr;
    sendWrData->tx_data.op = wr->op;
    sendWrData->tx_data.send_flags = wr->send_flag;
}

int RaHdcSendWr(struct ra_qp_handle *qpHdc, struct send_wr *wr, struct send_wr_rsp *opRsp)
{
    union op_send_wr_data sendWrData = {0};
    struct lite_send_wr liteWr = { 0 };
    int ret;

    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            liteWr.wr = *wr;
            return RaHdcLiteSendWr(qpHdc, &liteWr, opRsp, HDC_LITE_DEFAULT_WR_ID);
        }
    }

    RaHdcSendDataInit(&sendWrData, qpHdc, wr);

    ret = memcpy_s(sendWrData.tx_data.mem_list, (sizeof(struct sg_list) * MAX_SGE_NUM), wr->buf_list,
        (sizeof(struct sg_list) * wr->buf_num));
    CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_wr]memcpy_s for mem_list failed, ret(%d), phyId(%u)",
        ret, qpHdc->phy_id), -ESAFEFUNC);

    ret = RaHdcProcessMsg(RA_RS_SEND_WR, qpHdc->phy_id,
        (char *)&sendWrData, sizeof(union op_send_wr_data));
    if (ret) {
        if (ret != -ENOENT) {
            hccp_err("[send][ra_hdc_wr]ra hdc message process failed ret(%d), phyId(%u)", ret, qpHdc->phy_id);
        }
        return ret;
    }

    if (qpHdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        opRsp->wqe_tmp = sendWrData.rx_data.wr_rsp.wqe_tmp;
    } else if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
               qpHdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
               qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        opRsp->db = sendWrData.rx_data.wr_rsp.db;
    }

    return ret;
}

int RaHdcSendWrV2(struct ra_qp_handle *qpHdc, struct send_wr_v2 *wr, struct send_wr_rsp *opRsp)
{
    struct lite_send_wr liteWr = { 0 };

    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            liteWr.wr.buf_list = wr->buf_list;
            liteWr.wr.buf_num = wr->buf_num;
            liteWr.wr.dst_addr = wr->dst_addr;
            liteWr.wr.op = wr->op;
            liteWr.wr.rkey = wr->rkey;
            liteWr.wr.send_flag = wr->send_flag;
            liteWr.aux = wr->aux;
            liteWr.ext = wr->ext;
            return RaHdcLiteTypicalSendWr(qpHdc, &liteWr, opRsp, wr->wr_id);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to send_wr",
        qpHdc->qpn, qpHdc->qp_mode, qpHdc->support_lite);

    return -ENOTSUPP;
}

int RaHdcTypicalSendWr(struct ra_qp_handle *qpHdc, struct send_wr *wr, struct send_wr_rsp *opRsp)
{
    struct lite_send_wr liteWr = { 0 };

    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            liteWr.wr = *wr;
            return RaHdcLiteTypicalSendWr(qpHdc, &liteWr, opRsp, HDC_LITE_DEFAULT_WR_ID);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to send_wr",
        qpHdc->qpn, qpHdc->qp_mode, qpHdc->support_lite);

    return -ENOTSUPP;
}

int RaHdcMrDereg(struct ra_qp_handle *qpHdc, struct mr_info *info)
{
    union op_mr_dereg_data mrDeregData = {0};
    mrDeregData.tx_data.rdev_index = qpHdc->rdev_index;
    mrDeregData.tx_data.phy_id = qpHdc->phy_id;
    mrDeregData.tx_data.qpn = qpHdc->qpn;
    mrDeregData.tx_data.addr = info->addr;
    int ret;

    ret = RaHdcProcessMsg(RA_RS_MR_DEREG, qpHdc->phy_id, (char *)&mrDeregData,
        sizeof(union op_mr_dereg_data));
    CHK_PRT_RETURN(ret, hccp_err("[dereg][ra_hdc_mr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    return 0;
}

int RaHdcMrReg(struct ra_qp_handle *qpHdc, struct mr_info *info)
{
    union op_mr_reg_data mrRegData = {0};
    int ret;

    mrRegData.tx_data.phy_id = qpHdc->phy_id;
    mrRegData.tx_data.rdev_index = qpHdc->rdev_index;
    mrRegData.tx_data.qpn = qpHdc->qpn;
    mrRegData.tx_data.mr_reg_attr.addr = info->addr;
    mrRegData.tx_data.mr_reg_attr.len = info->size;
    mrRegData.tx_data.mr_reg_attr.access = info->access;

    ret = RaHdcProcessMsg(RA_RS_MR_REG, qpHdc->phy_id,
        (char *)&mrRegData, sizeof(union op_mr_reg_data));
    CHK_PRT_RETURN(ret, hccp_err("[reg][ra_hdc_mr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    info->lkey = mrRegData.rx_data.lkey;
    info->rkey = mrRegData.rx_data.rkey;

    return 0;
}

int RaHdcTypicalMrReg(struct ra_rdma_handle *rdmaHandle, struct mr_info *info, void **mrHandle)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    union op_typical_mr_reg_data mrRegData = {0};
    unsigned int opcode = RA_RS_TYPICAL_MR_REG_V1;
    struct ra_mr_handle *mrHdc = NULL;
    unsigned int interfaceVersion = 0;
    int ret;

    mrHdc = (struct ra_mr_handle *)calloc(1, sizeof(struct ra_mr_handle));
    CHK_PRT_RETURN(mrHdc == NULL, hccp_err("[reg][ra_hdc_typical_mr]mr_hdc calloc failed phy_id(%u)",
        phyId), -ENOMEM);

    mrRegData.tx_data.phy_id = phyId;
    mrRegData.tx_data.rdev_index = rdmaHandle->rdev_index;
    mrRegData.tx_data.mr_reg_attr.addr = info->addr;
    mrRegData.tx_data.mr_reg_attr.len = info->size;
    mrRegData.tx_data.mr_reg_attr.access = info->access;

    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_TYPICAL_MR_REG, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_OPCODE_BASE_VERSION) {
        opcode = RA_RS_TYPICAL_MR_REG;
    }
    ret = RaHdcProcessMsg(opcode, phyId, (char *)&mrRegData, sizeof(union op_typical_mr_reg_data));
    if (ret) {
        hccp_err("[reg][ra_hdc_typical_mr]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(mrHdc);
        return ret;
    }

    info->lkey = mrRegData.rx_data.lkey;
    info->rkey = mrRegData.rx_data.rkey;
    if (opcode == RA_RS_TYPICAL_MR_REG_V1) {
        mrHdc->addr = (unsigned long long)(uintptr_t)info->addr;
    } else {
        mrHdc->addr = mrRegData.rx_data.addr;
    }
    *mrHandle = mrHdc;
    return 0;
}

int RaHdcRemapMr(struct ra_rdma_handle *rdmaHandle, struct mem_remap_info info[], unsigned int num)
{
    union op_remap_mr_data opData = {0};
    int ret;

    ret = memcpy_s(opData.tx_data.mem_list, REMAP_MR_MAX_NUM * sizeof(struct mem_remap_info),
        info, num * sizeof(struct mem_remap_info));
    CHK_PRT_RETURN(ret != 0, hccp_err("[remap][ra_hdc_mr]memcpy_s mem_list failed, ret:%d", ret), -ESAFEFUNC);
    opData.tx_data.mem_num = num;
    opData.tx_data.rdev_index = rdmaHandle->rdev_index;
    opData.tx_data.phy_id = rdmaHandle->rdev_info.phy_id;

    ret = RaHdcProcessMsg(RA_RS_REMAP_MR, opData.tx_data.phy_id, (char *)&opData, sizeof(union op_remap_mr_data));
    CHK_PRT_RETURN(ret, hccp_err("[remap][ra_hdc_mr]ra hdc message process failed ret(%d) phy_id(%u)", ret,
        rdmaHandle->rdev_info.phy_id), ret);

    return 0;
}

int RaHdcTypicalMrDereg(struct ra_rdma_handle *rdmaHandle, void *mrHandle)
{
    union op_typical_mr_dereg_data mrDeregData = {0};
    int ret;

    mrDeregData.tx_data.phy_id = rdmaHandle->rdev_info.phy_id;
    mrDeregData.tx_data.rdev_index = rdmaHandle->rdev_index;
    mrDeregData.tx_data.addr = ((struct ra_mr_handle*)mrHandle)->addr;

    ret = RaHdcProcessMsg(RA_RS_TYPICAL_MR_DEREG, rdmaHandle->rdev_info.phy_id, (char *)&mrDeregData,
        sizeof(union op_typical_mr_dereg_data));
    CHK_PRT_RETURN(ret, hccp_err("[dereg][ra_hdc_typical_mr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, rdmaHandle->rdev_info.phy_id), ret);

    free(mrHandle);
    mrHandle = NULL;
    return 0;
}

STATIC void RaHdcSendWrlistInit(union op_send_wrlist_data *sendWrlist, struct ra_qp_handle *qpHdc,
    unsigned int completeCnt, struct wrlist_send_complete_num wrlistNum)
{
    sendWrlist->tx_data.phy_id = qpHdc->phy_id;
    sendWrlist->tx_data.rdev_index = qpHdc->rdev_index;
    sendWrlist->tx_data.qpn = qpHdc->qpn;
    sendWrlist->tx_data.send_num = (wrlistNum.send_num - completeCnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlistNum.send_num - completeCnt;
}

STATIC void RaHdcSendWrlistExtInit(union op_send_wrlist_data_ext *sendWrlist, struct ra_qp_handle *qpHdc,
    unsigned int completeCnt, struct wrlist_send_complete_num wrlistNum)
{
    sendWrlist->tx_data.phy_id = qpHdc->phy_id;
    sendWrlist->tx_data.rdev_index = qpHdc->rdev_index;
    sendWrlist->tx_data.qpn = qpHdc->qpn;
    sendWrlist->tx_data.send_num = (wrlistNum.send_num - completeCnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlistNum.send_num - completeCnt;
}

STATIC int RaHdcSendWrlistV1(struct ra_qp_handle *qpHdc, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int completeCnt = 0;
    unsigned int currentSendNum = 0;
    union op_send_wrlist_data *sendWrlist = NULL;

    sendWrlist = calloc(1, sizeof(union op_send_wrlist_data));
    CHK_PRT_RETURN(sendWrlist == NULL, hccp_err("[send][ra_hdc_wrlist]send_wrlist calloc failed"), -ENOMEM);
    while (completeCnt < wrlistNum.send_num) {
        RaHdcSendWrlistInit(sendWrlist, qpHdc, completeCnt, wrlistNum);
        ret = memcpy_s(sendWrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data) * MAX_WR_NUM_V1),
            &wr[completeCnt], (sizeof(struct send_wrlist_data) * sendWrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        currentSendNum = sendWrlist->tx_data.send_num;
        ret = RaHdcProcessMsg(RA_RS_SEND_WRLIST, qpHdc->phy_id, (char *)sendWrlist,
            sizeof(union op_send_wrlist_data));

        if (sendWrlist->rx_data.complete_num > currentSendNum) {
            hccp_err("[send][ra_hdc_wrlist]complete_num[%u] is larger than send_num[%u], ret(%d).",
                sendWrlist->rx_data.complete_num, currentSendNum, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < sendWrlist->rx_data.complete_num; i++) {
            j = i + completeCnt;
            if (qpHdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                opRsp[j].wqe_tmp = sendWrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
                       qpHdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                opRsp[j].db = sendWrlist->rx_data.wr_rsp[i].db;
            }
        }
        completeCnt = completeCnt + sendWrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist]ra hdc message process failed ret(%d), phyId(%u)", ret, qpHdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(sendWrlist);
    sendWrlist = NULL;
    *(wrlistNum.complete_num) = completeCnt;
    return ret;
}

STATIC int RaHdcSendWrlistExtV1(struct ra_qp_handle *qpHdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp opRsp[], struct wrlist_send_complete_num wrlistNum)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int completeCnt = 0;
    unsigned int currentSendNum = 0;
    union op_send_wrlist_data_ext *sendWrlist = NULL;

    sendWrlist = calloc(1, sizeof(union op_send_wrlist_data_ext));
    CHK_PRT_RETURN(sendWrlist == NULL, hccp_err("[send][ra_hdc_wrlist_ext]send_wrlist calloc failed"), -ENOMEM);
    while (completeCnt < wrlistNum.send_num) {
        RaHdcSendWrlistExtInit(sendWrlist, qpHdc, completeCnt, wrlistNum);
        ret = memcpy_s(sendWrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data_ext) * MAX_WR_NUM_V1),
            &wr[completeCnt], (sizeof(struct send_wrlist_data_ext) * sendWrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist_ext]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        currentSendNum = sendWrlist->tx_data.send_num;
        ret = RaHdcProcessMsg(RA_RS_SEND_WRLIST_EXT, qpHdc->phy_id, (char *)sendWrlist,
            sizeof(union op_send_wrlist_data_ext));

        if (sendWrlist->rx_data.complete_num > currentSendNum) {
            hccp_err("[send][ra_hdc_wrlist_ext]complete_num[%u] is larger than send_num[%u], ret(%d).",
                sendWrlist->rx_data.complete_num, currentSendNum, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < sendWrlist->rx_data.complete_num; i++) {
            j = i + completeCnt;
            if (qpHdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                opRsp[j].wqe_tmp = sendWrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                opRsp[j].db = sendWrlist->rx_data.wr_rsp[i].db;
            }
        }
        completeCnt = completeCnt + sendWrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist_ext]ra hdc message process failed ret(%d), phyId(%u)",
                    ret, qpHdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(sendWrlist);
    sendWrlist = NULL;
    *(wrlistNum.complete_num) = completeCnt;
    return ret;
}

STATIC void RaHdcSendWrlistInitV2(union op_send_wrlist_data_v2 *sendWrlist, struct ra_qp_handle *qpHdc,
    unsigned int completeCnt, struct wrlist_send_complete_num wrlistNum)
{
    sendWrlist->tx_data.phy_id = qpHdc->phy_id;
    sendWrlist->tx_data.rdev_index = qpHdc->rdev_index;
    sendWrlist->tx_data.qpn = qpHdc->qpn;
    sendWrlist->tx_data.send_num = (wrlistNum.send_num - completeCnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlistNum.send_num - completeCnt;
}

STATIC void RaHdcSendWrlistExtInitV2(union op_send_wrlist_data_ext_v2 *sendWrlist, struct ra_qp_handle *qpHdc,
    unsigned int completeCnt, struct wrlist_send_complete_num wrlistNum)
{
    sendWrlist->tx_data.phy_id = qpHdc->phy_id;
    sendWrlist->tx_data.rdev_index = qpHdc->rdev_index;
    sendWrlist->tx_data.qpn = qpHdc->qpn;
    sendWrlist->tx_data.send_num = (wrlistNum.send_num - completeCnt) >= MAX_WR_NUM ? MAX_WR_NUM :
        wrlistNum.send_num - completeCnt;
}

STATIC int RaHdcSendWrlistV2(struct ra_qp_handle *qpHdc, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int completeCnt = 0;
    unsigned int currentSendNum = 0;
    union op_send_wrlist_data_v2 *sendWrlist = NULL;

    sendWrlist = calloc(1, sizeof(union op_send_wrlist_data_v2));
    CHK_PRT_RETURN(sendWrlist == NULL, hccp_err("[send][ra_hdc_wrlist_v2]send_wrlist calloc failed"), -ENOMEM);
    while (completeCnt < wrlistNum.send_num) {
        RaHdcSendWrlistInitV2(sendWrlist, qpHdc, completeCnt, wrlistNum);
        ret = memcpy_s(sendWrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data) * MAX_WR_NUM),
            &wr[completeCnt], (sizeof(struct send_wrlist_data) * sendWrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist_v2]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        currentSendNum = sendWrlist->tx_data.send_num;
        ret = RaHdcProcessMsg(RA_RS_SEND_WRLIST_V2, qpHdc->phy_id, (char *)sendWrlist,
            sizeof(union op_send_wrlist_data_v2));

        if (sendWrlist->rx_data.complete_num > currentSendNum) {
            hccp_err("[send][ra_hdc_wrlist_v2]complete_num[%u] is larger than send_num[%u], ret(%d).",
                sendWrlist->rx_data.complete_num, currentSendNum, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < sendWrlist->rx_data.complete_num; i++) {
            j = i + completeCnt;
            if (qpHdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                opRsp[j].wqe_tmp = sendWrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                opRsp[j].db = sendWrlist->rx_data.wr_rsp[i].db;
            }
        }
        completeCnt = completeCnt + sendWrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist_v2]ra hdc message process failed ret(%d), phyId(%u)",
                    ret, qpHdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(sendWrlist);
    sendWrlist = NULL;
    *(wrlistNum.complete_num) = completeCnt;
    return ret;
}

STATIC int RaHdcSendWrlistExtV2(struct ra_qp_handle *qpHdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp opRsp[], struct wrlist_send_complete_num wrlistNum)
{
    int ret = 0;
    unsigned int i, j;
    unsigned int completeCnt = 0;
    unsigned int currentSendNum = 0;
    union op_send_wrlist_data_ext_v2 *sendWrlist = NULL;

    sendWrlist = calloc(1, sizeof(union op_send_wrlist_data_ext_v2));
    CHK_PRT_RETURN(sendWrlist == NULL, hccp_err("[send][ra_hdc_wrlist_ext_v2]send_wrlist calloc failed"), -ENOMEM);
    while (completeCnt < wrlistNum.send_num) {
        RaHdcSendWrlistExtInitV2(sendWrlist, qpHdc, completeCnt, wrlistNum);
        ret = memcpy_s(sendWrlist->tx_data.wrlist, (sizeof(struct send_wrlist_data_ext) * MAX_WR_NUM),
            &wr[completeCnt], (sizeof(struct send_wrlist_data_ext) * sendWrlist->tx_data.send_num));
        if (ret) {
            hccp_err("[send][ra_hdc_wrlist_ext_v2]memcpy_s for wrlist failed, ret(%d).", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        currentSendNum = sendWrlist->tx_data.send_num;
        ret = RaHdcProcessMsg(RA_RS_SEND_WRLIST_EXT_V2, qpHdc->phy_id, (char *)sendWrlist,
            sizeof(union op_send_wrlist_data_ext_v2));

        if (sendWrlist->rx_data.complete_num > currentSendNum) {
            hccp_err("[send][ra_hdc_wrlist_ext_v2]complete_num[%u] is larger than send_num[%u], ret(%d).",
                sendWrlist->rx_data.complete_num, currentSendNum, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }

        for (i = 0; i < sendWrlist->rx_data.complete_num; i++) {
            j = i + completeCnt;
            if (qpHdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                opRsp[j].wqe_tmp = sendWrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                       qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                opRsp[j].db = sendWrlist->rx_data.wr_rsp[i].db;
            }
        }
        completeCnt = completeCnt + sendWrlist->rx_data.complete_num;
        if (ret) {
            if (ret != -ENOENT) {
                hccp_err("[send][ra_hdc_wrlist_ext_v2]ra hdc message process failed ret(%d), phyId(%u)",
                    ret, qpHdc->phy_id);
            }
            goto err_send_wrlist;
        }
    }

err_send_wrlist:
    free(sendWrlist);
    sendWrlist = NULL;
    *(wrlistNum.complete_num) = completeCnt;
    return ret;
}

int RaHdcSendWrlist(struct ra_qp_handle *qpHdc, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    int ret;
    unsigned int interfaceVersion = 0;

    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            return RaHdcLiteSendWrlist(qpHdc, wr, opRsp, wrlistNum);
        }
    }

    ret = RaHdcGetInterfaceVersion(qpHdc->phy_id, RA_RS_SEND_WRLIST_V2, &interfaceVersion);
    if (ret != 0 || interfaceVersion != RA_RS_SEND_WRLIST_V2_VERSION) {
        return RaHdcSendWrlistV1(qpHdc, wr, opRsp, wrlistNum);
    }

    return RaHdcSendWrlistV2(qpHdc, wr, opRsp, wrlistNum);
}

int RaHdcSendWrlistExt(struct ra_qp_handle *qpHdc, struct send_wrlist_data_ext wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    int ret;
    unsigned int interfaceVersion = 0;

    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            return RaHdcLiteSendWrlistExt(qpHdc, wr, opRsp, wrlistNum);
        }
    }

    ret = RaHdcGetInterfaceVersion(qpHdc->phy_id, RA_RS_SEND_WRLIST_EXT_V2, &interfaceVersion);
    if (ret != 0 || interfaceVersion != RA_RS_SEND_WRLIST_EXT_V2_VERSION) {
        return RaHdcSendWrlistExtV1(qpHdc, wr, opRsp, wrlistNum);
    }

    return RaHdcSendWrlistExtV2(qpHdc, wr, opRsp, wrlistNum);
}

STATIC void RaHdcSendWrlistNormalInit(union op_send_normal_wrlist_data *sendWrlist, struct ra_qp_handle *qpHdc,
    unsigned int completeCnt, struct wrlist_send_complete_num wrlistNum)
{
    (void)memset_s(sendWrlist, sizeof(union op_send_normal_wrlist_data), 0, sizeof(union op_send_normal_wrlist_data));
    sendWrlist->tx_data.phy_id = qpHdc->phy_id;
    sendWrlist->tx_data.rdev_index = qpHdc->rdev_index;
    sendWrlist->tx_data.qpn = qpHdc->qpn;
    sendWrlist->tx_data.send_num = ((wrlistNum.send_num - completeCnt) >= MAX_WR_NUM) ? MAX_WR_NUM :
        wrlistNum.send_num - completeCnt;
}
 
STATIC int RaHdcSendWrlistNormal(struct ra_qp_handle *qpHdc, struct wr_info wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    union op_send_normal_wrlist_data *sendWrlist = NULL;
    unsigned int currentSendNum = 0;
    unsigned int completeCnt = 0;
    unsigned int i;
    int ret = 0;
 
    sendWrlist = calloc(1, sizeof(union op_send_normal_wrlist_data));
    CHK_PRT_RETURN(sendWrlist == NULL, hccp_err("[send][send_wrlist]send_wrlist calloc failed"), -ENOMEM);
 
    while (completeCnt < wrlistNum.send_num) {
        RaHdcSendWrlistNormalInit(sendWrlist, qpHdc, completeCnt, wrlistNum);
        ret = memcpy_s(sendWrlist->tx_data.wrlist, (sizeof(struct wr_info) * MAX_WR_NUM),
            &wr[completeCnt], (sizeof(struct wr_info) * sendWrlist->tx_data.send_num));
        if (ret != 0) {
            hccp_err("[send][send_wrlist]memcpy_s for wrlist failed, ret(%d)", ret);
            ret = -ESAFEFUNC;
            goto err_send_wrlist;
        }
        currentSendNum = sendWrlist->tx_data.send_num;
        ret = RaHdcProcessMsg(RA_RS_SEND_NORMAL_WRLIST, qpHdc->phy_id, (char *)sendWrlist,
            sizeof(union op_send_normal_wrlist_data));
 
        if (sendWrlist->rx_data.complete_num > currentSendNum) {
            hccp_err("[send][send_wrlist]complete_num[%u] is larger than send_num[%u], ret(%d)",
                sendWrlist->rx_data.complete_num, currentSendNum, ret);
            ret = -EINVAL;
            goto err_send_wrlist;
        }
 
        for (i = 0; i < sendWrlist->rx_data.complete_num; i++) {
            if (qpHdc->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
                opRsp[completeCnt + i].wqe_tmp = sendWrlist->rx_data.wr_rsp[i].wqe_tmp;
            } else if (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_GDR_ASYN_QP_MODE ||
                qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
                opRsp[completeCnt + i].db = sendWrlist->rx_data.wr_rsp[i].db;
            }
        }
        completeCnt += sendWrlist->rx_data.complete_num;
        // send wrlist success, continue to send
        if (ret == 0) {
            continue;
        }
        if (ret != -ENOENT && ret != -ENOMEM) {
            hccp_err("[send][send_wrlist]ra hdc message process failed ret(%d), phyId(%u)", ret, qpHdc->phy_id);
        }
        goto err_send_wrlist;
    }
 
err_send_wrlist:
    free(sendWrlist);
    sendWrlist = NULL;
    *(wrlistNum.complete_num) = completeCnt;
    return ret;
}

int RaHdcSendNormalWrlist(struct ra_qp_handle *qpHdc, struct wr_info wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            return RaHdcLiteSendNormalWrlist(qpHdc, wr, opRsp, wrlistNum);
        }
    }

    return RaHdcSendWrlistNormal(qpHdc, wr, opRsp, wrlistNum);
}

int RaHdcGetNotifyBaseAddr(struct ra_rdma_handle *rdmaHandle, unsigned long long *va, unsigned long long *size)
{
    int ret;
    union op_get_notify_ba_data getNotifyBaData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;

    getNotifyBaData.tx_data.phy_id = phyId;
    getNotifyBaData.tx_data.rdev_index = rdmaHandle->rdev_index;

    ret = RaHdcProcessMsg(RA_RS_GET_NOTIFY_BA, phyId, (char *)&getNotifyBaData,
        sizeof(union op_get_notify_ba_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_notify_base_addr]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    *va = getNotifyBaData.rx_data.va;
    *size = getNotifyBaData.rx_data.size;
    return 0;
}

int RaHdcGetNotifyMrInfo(struct ra_rdma_handle *rdmaHandle, struct mr_info *info)
{
    union op_get_notify_ba_data getNotifyBaData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    unsigned int interfaceVersion = 0;
    int ret;

    // check opcode version, reuse RA_RS_GET_NOTIFY_BA
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_GET_NOTIFY_BA, &interfaceVersion);
    if (ret != 0 || interfaceVersion == RA_RS_GET_NOTIFY_BA_VERSION) {
        hccp_err("[get][ra_hdc_notify_mr_info]interface_version(%u) not support, ret(%d)", interfaceVersion, ret);
        return -ENOTSUPP;
    }

    getNotifyBaData.tx_data.phy_id = phyId;
    getNotifyBaData.tx_data.rdev_index = rdmaHandle->rdev_index;

    ret = RaHdcProcessMsg(RA_RS_GET_NOTIFY_BA, phyId, (char *)&getNotifyBaData,
        sizeof(union op_get_notify_ba_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_notify_mr_info]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    info->addr = (void *)(uintptr_t)getNotifyBaData.rx_data.va;
    info->size = getNotifyBaData.rx_data.size;
    info->access = getNotifyBaData.rx_data.access;
    info->lkey = getNotifyBaData.rx_data.lkey;
    return 0;
}

int RaHdcRecvWrlist(struct ra_qp_handle *qpHdc, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum)
{
    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            return RaHdcLiteRecvWrlist(qpHdc, wr, recvNum, completeNum);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to recv_wrlist",
        qpHdc->qpn, qpHdc->qp_mode, qpHdc->support_lite);

    return -ENOTSUPP;
}

int RaHdcPollCq(struct ra_qp_handle *qpHdc, bool isSendCq, unsigned int numEntries, void *wc)
{
    struct rdma_lite_wc_v2 *liteWc = (struct rdma_lite_wc_v2 *)wc;

    if (qpHdc->qp_mode == RA_RS_OP_QP_MODE ||
        qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT) {
        if (qpHdc->support_lite != LITE_NOT_SUPPORT) {
            return RaHdcLitePollCq(qpHdc, isSendCq, numEntries, liteWc);
        }
    }

    hccp_warn("qpn:%u qp_mode:%d support_lite:%d not support to poll_cq",
        qpHdc->qpn, qpHdc->qp_mode, qpHdc->support_lite);

    return -ENOTSUPP;
}

int RaHdcNotifyCfgSet(unsigned int phyId, unsigned long long va, unsigned long long size)
{
    union op_notify_cfg_set_data setNotifyBaData = {0};
    int ret;

    setNotifyBaData.tx_data.phy_id = phyId;
    setNotifyBaData.tx_data.va = va;
    setNotifyBaData.tx_data.size = size;

    ret = RaHdcProcessMsg(RA_RS_NOTIFY_CFG_SET, phyId, (char *)&setNotifyBaData,
        sizeof(union op_notify_cfg_set_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_notify_cfg]ra hdc message process failed ret(%d), phyId(%u)",
        ret, phyId), ret);

    return 0;
}

int RaHdcNotifyCfgGet(unsigned int phyId, unsigned long long *va,
    unsigned long long *size)
{
    union op_notify_cfg_get_data getNotifyBaData = {0};
    int ret;

    getNotifyBaData.tx_data.phy_id = phyId;

    ret = RaHdcProcessMsg(RA_RS_NOTIFY_CFG_GET, phyId, (char *)&getNotifyBaData,
        sizeof(union op_notify_cfg_get_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_notify_cfg]ra hdc message process failed ret(%d), phyId(%u)",
        ret, phyId), ret);
    *va = getNotifyBaData.rx_data.va;
    *size = getNotifyBaData.rx_data.size;
    return 0;
}

STATIC int RaHdcRdevInitWithBackup(struct ra_rdma_handle *rdmaHandle, unsigned int *rdevIndex)
{
    union op_rdev_init_with_backup_data rdevInitData = { 0 };
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    unsigned int interfaceVersion = 0;
    int ret;

    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_RDEV_INIT_WITH_BACKUP, &interfaceVersion);
    // check opcode version, not support to init rdev with backup info
    if (ret != 0 || interfaceVersion < RA_RS_OPCODE_BASE_VERSION) {
        hccp_warn("[init][ra_hdc_rdev]get opcode[%d] not support, ret[%d] != 0 or interfaceVersion[%u] is 0",
            RA_RS_RDEV_INIT_WITH_BACKUP, ret, interfaceVersion);
        return -ENOTSUPP;
    }

    (void)memcpy_s(&(rdevInitData.tx_data.rdev_info), sizeof(struct rdev),
        &rdmaHandle->rdev_info, sizeof(struct rdev));
    (void)memcpy_s(&(rdevInitData.tx_data.backup_rdev_info), sizeof(struct rdev),
        &rdmaHandle->backup_info.rdev_info, sizeof(struct rdev));
    ret = RaHdcProcessMsg(RA_RS_RDEV_INIT_WITH_BACKUP, phyId, (char *)&rdevInitData,
        sizeof(union op_rdev_init_with_backup_data));
    if (ret) {
        hccp_err("[init][ra_hdc_rdev]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        return ret;
    }

    *rdevIndex = rdevInitData.rx_data.rdev_index;
    return 0;
}

int RaHdcRdevInit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType, struct rdev rdevInfo,
    unsigned int *rdevIndex)
{
    union op_rdev_init_data rdevInitData = { 0 };
    unsigned long long *notifyVa = NULL;
    int ret, drvRet;

    ret = RaHdcNotifyBaseAddrInit(notifyType, rdevInfo.phy_id, &notifyVa);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_rdev]ra_hdc_notify_base_addr_init failed, ret(%d), phyId(%u)",
        ret, rdevInfo.phy_id), ret);

    // need to init backup rdev: reg notify & normal mr, prepare for aicpu unfold
    if (rdmaHandle->backup_info.backup_flag) {
        ret = RaHdcRdevInitWithBackup(rdmaHandle, rdevIndex);
        if (ret) {
            goto free_mem;
        }
    } else {
        (void)memcpy_s(&(rdevInitData.tx_data.rdev_info), sizeof(struct rdev), &rdevInfo, sizeof(struct rdev));
        ret = RaHdcProcessMsg(RA_RS_RDEV_INIT, rdevInfo.phy_id, (char *)&rdevInitData,
            sizeof(union op_rdev_init_data));
        if (ret) {
            hccp_err("[init][ra_hdc_rdev]ra hdc message process failed ret(%d) phy_id(%u)", ret, rdevInfo.phy_id);
            goto free_mem;
        }
        *rdevIndex = rdevInitData.rx_data.rdev_index;
    }

    ret = RaHdcLiteInit(rdmaHandle, rdevInfo.phy_id, *rdevIndex);
    if (ret) {
        hccp_err("[init][ra_hdc_rdev]ra_hdc_lite_init failed ret(%d) phy_id(%u)", ret, rdevInfo.phy_id);
        goto free_mem;
    }

    return 0;

free_mem:
    drvRet = DlHalMemFree((void *)notifyVa);
    if (drvRet) {
        hccp_err("[init][ra_hdc_rdev]halMemFree failed! drv_ret(%d)", drvRet);
    }
    return ret;
}

int RaHdcRdevGetPortStatus(struct ra_rdma_handle *rdmaHandle, enum port_status *status)
{
    union op_rdev_get_port_status_data statusData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    int ret;

    statusData.tx_data.rdev_index = rdmaHandle->rdev_index;
    statusData.tx_data.phy_id = phyId;

    ret = RaHdcProcessMsg(RA_RS_RDEV_GET_PORT_STATUS, phyId, (char *)&statusData,
        sizeof(union op_rdev_get_port_status_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_hdc_port_status]ra hdc message process failed, ret(%d) phyId(%u)",
        ret, phyId), ret);

    *status = statusData.rx_data.status;
    return 0;
}

int RaHdcRdevRestoreDeinit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType)
{
    // lite thread is an inner thread, make sure it will exit
    RaHdcLiteDeinit(rdmaHandle);

    CHK_PRT_RETURN(notifyType != NOTIFY, hccp_err("[deinit][ra_hdc_rdev]notify_type[%u] error",
        notifyType), -EINVAL);

    return 0;
}

int RaHdcRdevDeinit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType)
{
    union op_rdev_deinit_data rdevDeinitData = {0};
    unsigned long long va, size;
    int ret;

    // lite thread is an inner thread, make sure it will exit
    RaHdcLiteDeinit(rdmaHandle);

    CHK_PRT_RETURN(notifyType != NOTIFY, hccp_err("[deinit][ra_hdc_rdev]notify_type[%u] error",
        notifyType), -EINVAL);

    rdevDeinitData.tx_data.rdev_index = rdmaHandle->rdev_index;
    rdevDeinitData.tx_data.phy_id = rdmaHandle->rdev_info.phy_id;

    ret = RaHdcProcessMsg(RA_RS_RDEV_DEINIT, rdmaHandle->rdev_info.phy_id, (char *)&rdevDeinitData,
        sizeof(union op_rdev_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rdev]ra_hdc_notify_cfg_get failed, ret(%d)", ret), ret);

    ret = RaHdcNotifyCfgGet(rdmaHandle->rdev_info.phy_id, &va, &size);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rdev]ra_hdc_notify_cfg_get failed, ret(%d)", ret), ret);

    ret = DlHalMemFree((void *)(uintptr_t)va);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rdev]halMemFree failed, ret(%d)", ret), ret);

    return 0;
}

int RaHdcSetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int tempDepth, unsigned int *qpNum)
{
    union op_set_tsqp_depth_data setTsqpDepthData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    int ret;

    setTsqpDepthData.tx_data.phy_id = rdmaHandle->rdev_info.phy_id;
    setTsqpDepthData.tx_data.rdev_index = rdmaHandle->rdev_index;
    setTsqpDepthData.tx_data.temp_depth = tempDepth;

    ret = RaHdcProcessMsg(RA_RS_SET_TSQP_DEPTH, phyId, (char *)&setTsqpDepthData,
        sizeof(union op_set_tsqp_depth_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_tsqp_depth]ra hdc message process failed ret(%d), opcode(%d)"
        "phyId(%u)", ret, RA_RS_SET_TSQP_DEPTH, phyId), ret);

    *qpNum = setTsqpDepthData.rx_data.qp_num;
    return 0;
}

int RaHdcGetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int *tempDepth, unsigned int *qpNum)
{
    union op_get_tsqp_depth_data getTsqpDepthData = {0};
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    int ret;

    getTsqpDepthData.tx_data.phy_id = rdmaHandle->rdev_info.phy_id;
    getTsqpDepthData.tx_data.rdev_index = rdmaHandle->rdev_index;

    ret = RaHdcProcessMsg(RA_RS_GET_TSQP_DEPTH, phyId, (char *)&getTsqpDepthData,
        sizeof(union op_get_tsqp_depth_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_tsqp_depth]ra hdc message process failed ret(%d), opcode(%d)"
        "phyId(%u)", ret, RA_RS_GET_TSQP_DEPTH, phyId), ret);

    *tempDepth = getTsqpDepthData.rx_data.temp_depth;
    *qpNum = getTsqpDepthData.rx_data.qp_num;

    return 0;
}

int RaHdcSetQpAttrQos(struct ra_qp_handle *qpHdc, struct qos_attr *attr)
{
    union op_set_qp_attr_qos_data qpAttrQosData = {0};
    int ret;

    qpAttrQosData.tx_data.phy_id = qpHdc->phy_id;
    qpAttrQosData.tx_data.rdev_index = qpHdc->rdev_index;
    qpAttrQosData.tx_data.qpn = qpHdc->qpn;
    qpAttrQosData.tx_data.qos_attr.tc = attr->tc;
    qpAttrQosData.tx_data.qos_attr.sl = attr->sl;

    ret = RaHdcProcessMsg(RA_RS_SET_QP_ATTR_QOS, qpHdc->phy_id,
        (char *)&qpAttrQosData, sizeof(union op_set_qp_attr_qos_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_qp_attr_qos]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    return 0;
}

int RaHdcSetQpAttrTimeout(struct ra_qp_handle *qpHdc, unsigned int *timeout)
{
    union op_set_qp_attr_timeout_data qpAttrTimeoutData = {0};
    int ret;

    qpAttrTimeoutData.tx_data.phy_id = qpHdc->phy_id;
    qpAttrTimeoutData.tx_data.rdev_index = qpHdc->rdev_index;
    qpAttrTimeoutData.tx_data.qpn = qpHdc->qpn;
    qpAttrTimeoutData.tx_data.timeout = *timeout;

    ret = RaHdcProcessMsg(RA_RS_SET_QP_ATTR_TIMEOUT, qpHdc->phy_id,
        (char *)&qpAttrTimeoutData, sizeof(union op_set_qp_attr_timeout_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_qp_attr_timeout]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    return 0;
}

int RaHdcSetQpAttrRetryCnt(struct ra_qp_handle *qpHdc, unsigned int *retryCnt)
{
    union op_set_qp_attr_retry_cnt_data qpAttrRetryCntData = {0};
    int ret;

    qpAttrRetryCntData.tx_data.phy_id = qpHdc->phy_id;
    qpAttrRetryCntData.tx_data.rdev_index = qpHdc->rdev_index;
    qpAttrRetryCntData.tx_data.qpn = qpHdc->qpn;
    qpAttrRetryCntData.tx_data.retry_cnt = *retryCnt;

    ret = RaHdcProcessMsg(RA_RS_SET_QP_ATTR_RETRY_CNT, qpHdc->phy_id,
        (char *)&qpAttrRetryCntData, sizeof(union op_set_qp_attr_retry_cnt_data));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_hdc_qp_attr_retry_cnt]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    return 0;
}

STATIC int RaHdcGetCqeErrInfoNum(struct ra_rdma_handle *rdmaHandle, unsigned int *num)
{
    union op_get_cqe_err_info_num_data cqeErrInfoNumData = { 0 };
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    unsigned int interfaceVersion = 0;
    int ret;

    *num = 0;
    // check opcode version, not support to get cqe err info
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_GET_CQE_ERR_INFO_NUM, &interfaceVersion);
    if (ret != 0 || interfaceVersion == 0) {
        hccp_warn("[get][cqe_err_info_list]get opcode[%d] not support, ret[%d] != 0 or interfaceVersion[%u] is 0",
            RA_RS_GET_CQE_ERR_INFO_NUM, ret, interfaceVersion);
        return 0;
    }

    cqeErrInfoNumData.tx_data.phy_id = phyId;
    cqeErrInfoNumData.tx_data.rdev_index = rdmaHandle->rdev_index;
    ret = RaHdcProcessMsg(RA_RS_GET_CQE_ERR_INFO_NUM, phyId,
        (char *)&cqeErrInfoNumData, sizeof(union op_get_cqe_err_info_num_data));
    CHK_PRT_RETURN(ret, hccp_err("ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId), ret);

    *num = cqeErrInfoNumData.rx_data.num;
    return 0;
}

int RaHdcGetCqeErrInfoList(struct ra_rdma_handle *rdmaHandle, struct cqe_err_info *infoList, unsigned int *num)
{
    union op_get_cqe_err_info_list_data cqeErrInfoListData = { 0 };
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    unsigned int liteCqeErrNum = *num;
    unsigned int cqeErrInfoNum = 0;
    unsigned int hdcCqeErrNum = 0;
    int ret = 0;

    ret = RaHdcLiteGetCqeErrInfoList(rdmaHandle, infoList, &liteCqeErrNum);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][cqe_err_info_list]get lite err info list failed, ret(%d) phyId(%u)",
        ret, phyId), ret);

    hdcCqeErrNum = *num - liteCqeErrNum;
    *num = liteCqeErrNum;
    // lite cqe err info full up info_list
    if (hdcCqeErrNum == 0) {
        return 0;
    }

    // get cqe err info num failed or not support to get cqe err info, skip get cqe err info list
    ret = RaHdcGetCqeErrInfoNum(rdmaHandle, &cqeErrInfoNum);
    if (ret != 0 || cqeErrInfoNum == 0) {
        return ret;
    }

    cqeErrInfoListData.tx_data.phy_id = phyId;
    cqeErrInfoListData.tx_data.rdev_index = rdmaHandle->rdev_index;
    cqeErrInfoListData.tx_data.num = hdcCqeErrNum;
    ret = RaHdcProcessMsg(RA_RS_GET_CQE_ERR_INFO_LIST, phyId,
        (char *)&cqeErrInfoListData, sizeof(union op_get_cqe_err_info_list_data));
    CHK_PRT_RETURN(ret, hccp_err("ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId), ret);

    if (cqeErrInfoListData.rx_data.num > hdcCqeErrNum) {
        hccp_err("[get][cqe_err_info_list]rx_data.num(%u) is invalid, num(%u), phyId(%u)",
            cqeErrInfoListData.rx_data.num, hdcCqeErrNum, phyId);
        return -EINVAL;
    }
    ret = memcpy_s(&infoList[liteCqeErrNum], sizeof(struct cqe_err_info) * hdcCqeErrNum,
        &cqeErrInfoListData.rx_data.info_list, sizeof(struct cqe_err_info) * cqeErrInfoListData.rx_data.num);
    if (ret) {
        hccp_err("[get][cqe_err_info_list]memcpy_s info_list failed, ret(%d) phyId(%u) num(%u) rx_data.num(%u)",
            ret, phyId, hdcCqeErrNum, cqeErrInfoListData.rx_data.num);
        return ret;
    }

    *num = liteCqeErrNum + cqeErrInfoListData.rx_data.num;
    return 0;
}

STATIC int RaHdcLiteCleanCq(struct ra_qp_handle *qpHandle, bool isSendCq, unsigned int numEntries)
{
    void *wc = NULL;
    int ret;

    if (numEntries == 0) {
        return 0;
    }

    wc = calloc(numEntries, sizeof(struct rdma_lite_wc_v2));
    if (wc == NULL) {
        hccp_err("calloc failed, phyId:%u, qpn:%u", qpHandle->phy_id, qpHandle->qpn);
        return -ENOMEM;
    }

    ret = RaHdcLitePollCq(qpHandle, isSendCq, numEntries, wc);
    free(wc);
    if (ret < 0) {
        hccp_err("ra_hdc_lite_poll_cq failed, ret:%d, phyId:%u, qpn:%u", ret, qpHandle->phy_id, qpHandle->qpn);
        return ret;
    }

    return 0;
}

STATIC int RaHdcLiteCleanQp(struct ra_qp_handle *qpHandle)
{
    unsigned int interfaceVersion = 0;
    int ret;

    // check opcode versioin, not support to clean qp
    ret = RaHdcGetInterfaceVersion(qpHandle->phy_id, RA_RS_QP_BATCH_MODIFY, &interfaceVersion);
    if (ret != 0 || interfaceVersion <= RA_RS_OPCODE_BASE_VERSION) {
        hccp_warn("RA_RS_QP_BATCH_MODIFY interface_version:%u <= %u, not support to clean qp, phyId:%u, qpn:%u",
            interfaceVersion, RA_RS_OPCODE_BASE_VERSION, qpHandle->phy_id, qpHandle->qpn);
        return 0;
    }

    // lite qp clean
    ret = RaRdmaLiteCleanQp(qpHandle->lite_qp);
    if (ret != 0) {
        hccp_err("ra_rdma_lite_clean_qp failed, ret:%d, phyId:%u, qpn:%u", ret, qpHandle->phy_id, qpHandle->qpn);
        return ret;
    }

    return 0;
}

STATIC int RaHdcLiteCleanQueue(struct ra_qp_handle *qpHandle, int expectStatus)
{
    int ret;

    // not pause status, no need to clean
    if (expectStatus != RA_QP_STATUS_PAUSE) {
        return 0;
    }

    // not lite or not op mode, no need to clean
    if ((qpHandle->support_lite == 0) ||
        (qpHandle->qp_mode != RA_RS_OP_QP_MODE && qpHandle->qp_mode != RA_RS_OP_QP_MODE_EXT)) {
        return 0;
    }

    // poll cq to clean lite send cq
    if (qpHandle->send_wr_num > qpHandle->poll_cqe_num) {
        ret = RaHdcLiteCleanCq(qpHandle, true, qpHandle->send_wr_num - qpHandle->poll_cqe_num);
        if (ret != 0) {
            hccp_err("ra_hdc_lite_clean_cq send_cq failed, ret:%d, phyId:%u, qpn:%u",
                ret, qpHandle->phy_id, qpHandle->qpn);
            return ret;
        }
    }

    // poll cq to clean lite recv cq
    if (qpHandle->recv_wr_num > qpHandle->poll_recv_cqe_num) {
        ret = RaHdcLiteCleanCq(qpHandle, false, qpHandle->recv_wr_num - qpHandle->poll_recv_cqe_num);
        if (ret < 0) {
            hccp_err("ra_hdc_lite_clean_cq recv_cq failed, ret:%d, phyId:%u, qpn:%u",
                ret, qpHandle->phy_id, qpHandle->qpn);
            return ret;
        }
    }

    // lite qp clean
    ret = RaHdcLiteCleanQp(qpHandle);
    if (ret != 0) {
        hccp_err("ra_hdc_lite_clean_qp failed, ret:%d, phyId:%u, qpn:%u", ret, qpHandle->phy_id, qpHandle->qpn);
        return ret;
    }

    return 0;
}

int RaHdcQpBatchModify(struct ra_rdma_handle *rdmaHandle, void *qpHdc[], unsigned int num, int expectStatus)
{
    union op_qp_batch_modify_data *qpBatchModifyData = NULL;
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct ra_qp_handle *qpHandle = NULL;
    unsigned int currentSendCnt = 0;
    unsigned int completeCnt = 0;
    int opQpnCnt = 0;
    unsigned int i;
    int ret = 0;

    qpBatchModifyData = calloc(1, sizeof(union op_qp_batch_modify_data));
    CHK_PRT_RETURN(qpBatchModifyData == NULL,
        hccp_err("[send][ra_hdc_qp_batch_modify]qp_batch_modify calloc failed"), -ENOMEM);
    while (completeCnt < num) {
        qpBatchModifyData->tx_data.phy_id = phyId;
        qpBatchModifyData->tx_data.rdev_index = rdmaHandle->rdev_index;
        qpBatchModifyData->tx_data.status = expectStatus;

        currentSendCnt = (num - completeCnt) < RA_MAX_BATCH_QP_MODIFY_NUM ?
            (num - completeCnt) : RA_MAX_BATCH_QP_MODIFY_NUM;
        opQpnCnt = 0;
        for (i = completeCnt; i < completeCnt + currentSendCnt; i++) {
            qpHandle = (struct ra_qp_handle *)qpHdc[i];
            qpBatchModifyData->tx_data.qpn[opQpnCnt] = (int)qpHandle->qpn;
            opQpnCnt++;

            // avoid poll invalid cqe after modify to RESET state, make sure lite cq ci & qp pointer are valid
            ret = RaHdcLiteCleanQueue(qpHandle, expectStatus);
            if (ret != 0) {
                hccp_err("[modify][qp_batch_modify]ra_hdc_lite_clean_queue failed ret(%d) phy_id(%u) qpn(%u)",
                    ret, phyId, qpHandle->qpn);
                goto err_qp_batch_modify;
            }
        }
        qpBatchModifyData->tx_data.qpn_num = opQpnCnt;

        ret = RaHdcProcessMsg(RA_RS_QP_BATCH_MODIFY, phyId, (char *)qpBatchModifyData,
            sizeof(union op_qp_batch_modify_data));
        if (ret) {
            hccp_err("[modify][qp_batch_modify]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
            goto err_qp_batch_modify;
        }
        completeCnt += (unsigned int)opQpnCnt;
    }

err_qp_batch_modify:
    free(qpBatchModifyData);
    qpBatchModifyData = NULL;

    return ret;
}

int RaHdcRdmaSetOps(struct ra_rdma_handle *rdmaHandle, struct ra_rdma_ops *rdmaOps)
{
    CHK_PRT_RETURN(rdmaHandle == NULL, hccp_err("ra_hdc_rdma_set_ops rdma_handle is NULL"), -EINVAL);

    rdmaHandle->rdma_ops = rdmaOps;
    return 0;
}

int RaHdcRdmaSaveSnapshot(struct ra_rdma_handle *rdmaHandle, enum save_snapshot_action action)
{
    int ret = 0;

    if (rdmaHandle == NULL || rdmaHandle->support_lite == LITE_NOT_SUPPORT || rdmaHandle->disabled_lite_thread) {
        return 0;
    }

    RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->rdev_mutex);
    if (action == SAVE_SNAPSHOT_ACTION_PRE_PROCESSING && rdmaHandle->thread_status == LITE_THREAD_STATUS_RUNNING) {
        rdmaHandle->thread_status = LITE_THREAD_STATUS_SUSPEND;
    } else if (action == SAVE_SNAPSHOT_ACTION_POST_PROCESSING && rdmaHandle->thread_status == LITE_THREAD_STATUS_SUSPEND) {
        rdmaHandle->thread_status = LITE_THREAD_STATUS_RUNNING;
    } else {
        hccp_err("duplicate or incorrect order calls are not allowed, thread_status[%d] action[%d]",
            rdmaHandle->thread_status, action);
        ret = -EPERM;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);

    return ret;
}

int RaHdcRdmaRestoreSnapshot(struct ra_rdma_handle *rdmaHandle, struct ra_rdma_ops *rdmaOps)
{
    int ret = 0;

    if (rdmaHandle == NULL) {
        return 0;
    }
    ret = RaHdcRdmaSetOps(rdmaHandle, rdmaOps);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_rdma_set_ops failed, ret[%d]", ret), ret);

    if (rdmaHandle->support_lite == LITE_NOT_SUPPORT || rdmaHandle->disabled_lite_thread) {
        return 0;
    }

    RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->rdev_mutex);
    if (rdmaHandle->thread_status != LITE_THREAD_STATUS_SUSPEND) {
        hccp_err("incorrect order calls are not allowed, thread_status[%d]", rdmaHandle->thread_status);
        ret = -EPERM;
        goto unlock_mutex;
    }
    ret = RaRdmaLiteRestoreSnapshot(rdmaHandle->lite_ctx);
    if (ret != 0) {
        hccp_err("ra_rdma_lite_restore_snapshot failed, ret[%d]", ret);
    }

unlock_mutex:
    RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);
    return ret;
}
