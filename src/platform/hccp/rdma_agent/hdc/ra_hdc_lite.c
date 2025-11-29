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
#include <sys/time.h>
#include <sys/prctl.h>
#include <pthread.h>
#include "user_log.h"
#include "ra_hdc.h"
#include "securec.h"
#include "ra.h"
#include "ra_comm.h"
#include "ra_rdma_lite.h"
#include "dl_hal_function.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_hdc_lite.h"

struct ra_cqe_err_info gRaCqeErr[RA_MAX_PHY_ID_NUM];

STATIC void *RaHdcLitePthread(void *arg);

STATIC int RaHdcGetDrvLiteSupport(unsigned int phyId, bool enabled910aLite, unsigned int *support)
{
    int ret;
    size_t outLen = 0;
    unsigned int logicId;
    int64_t deviceInfo = 0;
    struct supportFeaturePara inPara = { 0 };
    struct supportFeaturePara outPara = { 0 };

    ret = DlDrvDeviceGetIndexByPhyId(phyId, &logicId);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]dl_drv_device_get_index_by_phy_id failed, ret(%d), phyId(%u)",
        ret, phyId), ret);

    // enabled_910a_lite not explicitly set to true, disabled lite if chip_type is 910A due to memory limits
    if (!enabled910aLite) {
        ret = DlHalGetDeviceInfo(logicId, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &deviceInfo);
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]dl_hal_get_device_info failed, ret(%d), phyId(%u)",
            ret, phyId), ret);
        if (DlHalPlatGetChip((uint64_t)deviceInfo) == CHIP_TYPE_910A) { // Memory Limits
            *support = 0;
            hccp_info("[init][ra_hdc_lite]device_info:0x%llx not support, phyId(%u)", deviceInfo, phyId);
            return 0;
        }
    }

    inPara.support_feature = CTRL_SUPPORT_DEV_MEM_REGISTER_MASK | CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_MASK;
    inPara.devid = logicId;
    ret = DlHalMemCtl(CTRL_TYPE_SUPPORT_FEATURE, &inPara, sizeof(struct supportFeaturePara), &outPara, &outLen);
    if ((ret != 0) || (((outPara.support_feature & CTRL_SUPPORT_DEV_MEM_REGISTER_MASK) == 0) &&
        ((outPara.support_feature & CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_MASK) == 0))) {
        *support = 0;
        return 0;
    }

    if ((outPara.support_feature & CTRL_SUPPORT_DEV_MEM_REGISTER_MASK) != 0) {
        *support = LITE_SUPPORT_DEV_MEM_REGISTER;
    }
    if ((outPara.support_feature & CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_MASK) != 0) {
        *support |= LITE_SUPPORT_PCIE_BAR_HUGE_MEM;
    }

    return 0;
}

STATIC void RaHdcGetOpcodeLiteSupport(unsigned int phyId, unsigned int supportFeature, int *support)
{
    int ret;
    unsigned int interfaceVersion = 0;

    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_GET_LITE_SUPPORT, &interfaceVersion);
    // get version failed or opcode interface_version is 0: opcode not support lite
    if (ret != 0 || interfaceVersion == 0) {
        hccp_info("[init][ra_hdc_lite]get opcode not support, ret[%d] != 0 or interfaceVersion is 0", ret);
        *support = LITE_NOT_SUPPORT;
        return;
    }

    // at least driver&host support lite 4KB page_size align
    if ((supportFeature & LITE_SUPPORT_DEV_MEM_REGISTER) != 0) {
        *support = LITE_ALIGN_4KB;
        return;
    }

    // driver&host support lite 2MB page_size align
    if ((interfaceVersion == LITE_VERSION_V2) && ((supportFeature & LITE_SUPPORT_PCIE_BAR_HUGE_MEM) != 0)) {
        *support = LITE_ALIGN_2MB;
        return;
    }

    // none of 4KB page_size align & 2MB page_size align lite support
    hccp_info("[init][ra_hdc_lite]get opcode not support, interfaceVersion[%u] supportFeature[0x%x]",
        interfaceVersion, supportFeature);
    *support = LITE_NOT_SUPPORT;
    return;
}

STATIC int RaHdcGetRdmaLiteSupport(struct ra_rdma_handle *rdmaHandle, unsigned int supportFeature, int *support)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    unsigned int rdevIndex = rdmaHandle->rdev_index;
    union op_lite_support_data liteSupportData;
    int supportLite = 0;
    int ret;

    // get opcode support lite with support_feature
    RaHdcGetOpcodeLiteSupport(phyId, supportFeature, &supportLite);
    if (supportLite == LITE_NOT_SUPPORT) {
        *support = LITE_NOT_SUPPORT;
        return 0;
    }

    // no need to support lite if enabled_2mb_lite not enabled
    if (supportLite == LITE_ALIGN_2MB && !rdmaHandle->enabled_2mb_lite) {
        hccp_run_info("[init][ra_hdc_lite]rdma_handle->enabled_2mb_lite=%d, no need to support LITE_ALIGN_2MB",
            rdmaHandle->enabled_2mb_lite);
        *support = LITE_NOT_SUPPORT;
        return 0;
    }

    // RA_RS_GET_LITE_SUPPORT will set rdev_cb->support_lite = 1
    (void)memset_s(&liteSupportData, sizeof(liteSupportData), 0, sizeof(liteSupportData));
    liteSupportData.tx_data.phy_id = phyId;
    liteSupportData.tx_data.rdev_index = rdevIndex;
    ret = RaHdcProcessMsg(RA_RS_GET_LITE_SUPPORT, phyId, (char *)&liteSupportData,
        sizeof(union op_lite_support_data));
    if (ret != 0) {
        if (ret == -EPROTONOSUPPORT) {
            *support = LITE_NOT_SUPPORT;
            ret = 0;
        } else {
            hccp_err("[init][ra_hdc_lite]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        }
        return ret;
    }

    *support = supportLite;
    return 0;
}

STATIC int RaHdcGetLiteSupport(struct ra_rdma_handle *rdmaHandle, unsigned int phyId)
{
    int ret;
    unsigned int supportFeature = 0;

#ifndef HNS_ROCE_LLT
    ret = RaHdcGetDrvLiteSupport(phyId, rdmaHandle->enabled_910a_lite, &supportFeature);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]ra_hdc_get_drv_lite_support failed, ret(%d), phyId(%u)",
        ret, phyId), ret);
#else
    support_feature = 1;
#endif
    if (supportFeature != 0) {
        ret = RaHdcGetRdmaLiteSupport(rdmaHandle, supportFeature, &rdmaHandle->support_lite);
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]ra_hdc_get_rdma_lite_support failed, ret(%d), phyId(%u)",
            ret, phyId), ret);

        if (rdmaHandle->support_lite) {
            hccp_run_info("[init][ra_hdc_lite]support_feature:0x%x, supportLite:%u", supportFeature,
                rdmaHandle->support_lite);
            ret = RaHdcRdmaLiteApiInit();
            CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]ra_hdc_rdma_lite_api_init failed, ret(%d), phyId(%u)",
                ret, phyId), ret);
        }
    }

    return 0;
}

STATIC int RaSensorNodeRegister(unsigned int phyId, struct ra_rdma_handle *rdmaHandle)
{
    struct halSensorNodeCfg cfg = { 0 };
    unsigned int interfaceVersion = 0;
    int ret;

    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_RDEV_INIT, &interfaceVersion);
    if ((ret != 0) || (interfaceVersion <= RA_RS_OPCODE_BASE_VERSION)) {
        /* unknown or old version, not support sensor */
        rdmaHandle->sensor_handle = 0;
        hccp_warn("[init][ra_hdc_lite]not support sensor, ret:%d, phyId:%u, interfaceVersion:%u",
            ret, phyId, interfaceVersion);
        return 0;
    }

    rdmaHandle->sensor_update_cnt = 0;
    ret = sprintf_s(cfg.name, sizeof(cfg.name), "roce_ra_%d", getpid());
    CHK_PRT_RETURN(ret <= 0, hccp_err("[init][ra_hdc_lite]sprintf_s name err, ret:%d, phyId:%u",
        ret, phyId), -ESAFEFUNC);

    cfg.NodeType = HAL_DMS_DEV_TYPE_HCCP;
    cfg.SensorType = RDMA_CQE_ERR_SENSOR_TYPE;
    cfg.AssertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_MASK;
    cfg.DeassertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE_MASK;
    ret = DlHalSensorNodeRegister(rdmaHandle->logic_devid, &cfg, &rdmaHandle->sensor_handle);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]dl_hal_sensor_node_register failed, ret(%d)", ret), ret);

    return 0;
}

STATIC int RaHdcLiteGetRdevCap(struct ra_rdma_handle *rdmaHandle, unsigned int phyId, unsigned int rdevIndex,
    union op_lite_rdev_cap_data *liteRdevCapData)
{
#define PAGE_ALIGN_2MB (2 * 1024 * 1024)
    int ret;

    liteRdevCapData->tx_data.phy_id = phyId;
    liteRdevCapData->tx_data.rdev_index = rdevIndex;
    ret = RaHdcProcessMsg(RA_RS_GET_LITE_RDEV_CAP, phyId, (char *)liteRdevCapData,
        sizeof(union op_lite_rdev_cap_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lite_ctx]hdc get lite rdev cap failed, ret(%d), phyId(%u)",
        ret, phyId), ret);

    // should change page_size to 2MB
    if (rdmaHandle->support_lite == LITE_ALIGN_2MB) {
        liteRdevCapData->rx_data.resp.cap.page_size = PAGE_ALIGN_2MB;
    }
    return 0;
}

STATIC int RaHdcLiteMutexInit(struct ra_rdma_handle *rdmaHandle, unsigned int phyId)
{
    int ret;

    ret = pthread_mutex_init(&rdmaHandle->rdev_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_lite_ctx]pthread_mutex_init rdev_mutex failed ret(%d) phy_id(%u)", ret, phyId);
        return -ESYSFUNC;
    }

    ret = pthread_mutex_init(&rdmaHandle->cqe_err_cnt_mutex, NULL);
    if (ret != 0) {
        (void)pthread_mutex_destroy(&rdmaHandle->rdev_mutex);
        hccp_err("[init][ra_hdc_lite_ctx]pthread_mutex_init cqe_err_cnt_mutex failed ret(%d) phy_id(%u)", ret, phyId);
        return -ESYSFUNC;
    }

    return 0;
}

STATIC void RaHdcLiteMutexDeinit(struct ra_rdma_handle *rdmaHandle)
{
    (void)pthread_mutex_destroy(&rdmaHandle->cqe_err_cnt_mutex);
    (void)pthread_mutex_destroy(&rdmaHandle->rdev_mutex);
}

STATIC int RaHdcLiteCtxInit(struct ra_rdma_handle *rdmaHandle, unsigned int phyId, unsigned int rdevIndex)
{
    union op_lite_rdev_cap_data liteRdevCapData = { 0 };
    int ret = 0;

    if (rdmaHandle->support_lite == 0) {
        return 0;
    }

    // register sensor node
    ret = DlDrvDeviceGetIndexByPhyId(phyId, &rdmaHandle->logic_devid);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite_ctx]dl_drv_device_get_index_by_phy_id failed, ret(%d) phyId(%u)",
        ret, phyId), ret);
    ret = RaSensorNodeRegister(phyId, rdmaHandle);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lite_ctx]ra_sensor_node_register failed, ret(%d) phyId(%u)",
        ret, phyId), ret);

    // alloc ctx
    ret = RaHdcLiteGetRdevCap(rdmaHandle, phyId, rdevIndex, &liteRdevCapData);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lite_ctx]ra_hdc_lite_get_rdev_cap failed, ret(%d) phyId(%u)",
        ret, phyId), ret);
    rdmaHandle->lite_ctx = RaRdmaLiteAllocCtx(phyId, &liteRdevCapData.rx_data.resp.cap);
    if (rdmaHandle->lite_ctx == NULL) {
        hccp_err("[init][ra_hdc_lite_ctx]ra_rdma_lite_alloc_ctx errno(%d) phy_id(%u)", errno, phyId);
        ret = -EFAULT;
        goto unreg_sensor;
    }

    RA_INIT_LIST_HEAD(&rdmaHandle->qp_list);

    ret = RaHdcLiteMutexInit(rdmaHandle, phyId);
    if (ret != 0) {
        goto free_ctx;
    }

    if (rdmaHandle->disabled_lite_thread) {
        hccp_run_info("lite thread disabled");
        return 0;
    }

    rdmaHandle->thread_status = LITE_THREAD_STATUS_RUNNING;
    ret = pthread_create(&rdmaHandle->tid, NULL, RaHdcLitePthread, (void *)rdmaHandle);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_lite_ctx]pthread_create failed, ret:%d, phyId:%u errno:%d", ret, phyId, errno);
        rdmaHandle->thread_status = LITE_THREAD_STATUS_DESTROY;
        ret = -ESYSFUNC;
        goto mutex_deinit;
    }

    return 0;

mutex_deinit:
    RaHdcLiteMutexDeinit(rdmaHandle);
free_ctx:
    RaRdmaLiteFreeCtx(rdmaHandle->lite_ctx);
unreg_sensor:
    (void)DlHalSensorNodeUnregister(rdmaHandle->logic_devid, rdmaHandle->sensor_handle);
    return ret;
}

int RaHdcLiteInit(struct ra_rdma_handle *rdmaHandle, unsigned int phyId, unsigned int rdevIndex)
{
    int ret;

    ret = RaHdcGetLiteSupport(rdmaHandle, phyId);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_rdev]ra_hdc_get_lite_support failed ret(%d) phy_id(%u)", ret, phyId);
        return ret;
    }

    ret = RaHdcLiteCtxInit(rdmaHandle, phyId, rdevIndex);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_rdev]ra_hdc_lite_ctx_init failed ret(%d) phy_id(%u)", ret, phyId);
        goto free_lite_api;
    }

    return 0;

free_lite_api:
    RaHdcRdmaLiteApiDeinit();
    return ret;
}

void RaHdcLiteDeinit(struct ra_rdma_handle *rdmaHandle)
{
#define FINISH_RUNNING 2
#define THREAD_STATUS_CHANGE_TIMEOUT 100
    int i;

    if (rdmaHandle->support_lite) {
        if (rdmaHandle->disabled_lite_thread) {
            goto disabled_thread_out;
        }

        rdmaHandle->thread_status = LITE_THREAD_STATUS_DESTROY;
        // wait thread change to finish running status(2), wait 100 times(total cost: 1s) until timeout
        for (i = 0; i < THREAD_STATUS_CHANGE_TIMEOUT && rdmaHandle->thread_status != FINISH_RUNNING; i++) {
            usleep(RA_LITE_POLL_CQE_PERIOD_TIME);
        }
        // thread not in finish running status(2), report timeout
        if (rdmaHandle->thread_status != FINISH_RUNNING) {
            hccp_run_info("hdc wait thread tid:%lu finish running timeout, thread status:%d",
                rdmaHandle->tid, rdmaHandle->thread_status);
        }

disabled_thread_out:
        RaHdcLiteMutexDeinit(rdmaHandle);
        RaRdmaLiteFreeCtx(rdmaHandle->lite_ctx);
        RaHdcRdmaLiteApiDeinit();
        (void)DlHalSensorNodeUnregister(rdmaHandle->logic_devid, rdmaHandle->sensor_handle);
    }
}

STATIC void RaHdcLiteQpAttrInit(struct ra_qp_handle *qpHdc, struct rdma_lite_qp_attr *liteQpAttr,
    struct rdma_lite_qp_cap *cap)
{
    liteQpAttr->send_cq = qpHdc->send_lite_cq;
    liteQpAttr->recv_cq = qpHdc->recv_lite_cq;
    liteQpAttr->qp_mode = qpHdc->qp_mode;
    liteQpAttr->qp_type = RDMA_LITE_QPT_RC;
    liteQpAttr->cap.max_inline_data = cap->max_inline_data;
    liteQpAttr->cap.max_send_sge = cap->max_send_sge;
    liteQpAttr->cap.max_recv_sge = cap->max_recv_sge;
    liteQpAttr->cap.max_send_wr = cap->max_send_wr;
    liteQpAttr->cap.max_recv_wr = cap->max_recv_wr;
}

STATIC int RaHdcLiteInitMemPool(struct ra_rdma_handle *rdmaHandle, struct ra_qp_handle *qpHdc,
    struct rdma_lite_cq_attr *liteSendCqAttr, struct rdma_lite_cq_attr *liteRecvCqAttr,
    struct rdma_lite_qp_attr *liteQpAttr)
{
    union op_lite_mem_attr_data liteMemAttrData = { 0 };
    unsigned int phyId = qpHdc->phy_id;
    int ret;

    if (rdmaHandle->support_lite != LITE_ALIGN_2MB) {
        return 0;
    }

    liteMemAttrData.tx_data.phy_id = phyId;
    liteMemAttrData.tx_data.rdev_index = rdmaHandle->rdev_index;
    liteMemAttrData.tx_data.qpn = qpHdc->qpn;
    ret = RaHdcProcessMsg(RA_RS_GET_LITE_MEM_ATTR, phyId, (char *)&liteMemAttrData,
        sizeof(union op_lite_mem_attr_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    ret = RaRdmaLiteInitMemPool(rdmaHandle->lite_ctx,
        (struct rdma_lite_mem_attr *)&liteMemAttrData.rx_data.resp.mem_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra_rdma_lite_init_mem_pool failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    qpHdc->mem_idx = liteMemAttrData.rx_data.resp.mem_data.mem_idx;
    liteSendCqAttr->mem_idx = qpHdc->mem_idx;
    liteRecvCqAttr->mem_idx = qpHdc->mem_idx;
    liteQpAttr->mem_idx = qpHdc->mem_idx;
    return 0;
}

STATIC void RaHdcLiteDeinitMemPool(struct ra_rdma_handle *rdmaHandle, struct ra_qp_handle *qpHdc)
{
    unsigned int phyId = qpHdc->phy_id;
    int ret;

    if (rdmaHandle->support_lite != LITE_ALIGN_2MB) {
        return;
    }

    ret = RaRdmaLiteDeinitMemPool(rdmaHandle->lite_ctx, qpHdc->mem_idx);
    if (ret != 0) {
        hccp_err("[create][ra_hdc_lite_qp]ra_rdma_lite_deinit_mem_pool failed ret(%d) phy_id(%u)", ret, phyId);
    }
    return;
}

STATIC int RaHdcLiteGetCqQpAttr(struct ra_qp_handle *qpHdc, struct rdma_lite_cq_attr *liteSendCqAttr,
    struct rdma_lite_cq_attr *liteRecvCqAttr, struct rdma_lite_qp_attr *liteQpAttr)
{
    union op_lite_qp_cq_attr_data liteQpCqAttrData = { 0 };
    unsigned int phyId = qpHdc->phy_id;
    int ret;

    liteQpCqAttrData.tx_data.phy_id = phyId;
    liteQpCqAttrData.tx_data.rdev_index = qpHdc->rdev_index;
    liteQpCqAttrData.tx_data.qpn = qpHdc->qpn;
    ret = RaHdcProcessMsg(RA_RS_GET_LITE_QP_CQ_ATTR, phyId, (char *)&liteQpCqAttrData,
        sizeof(union op_lite_qp_cq_attr_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    qpHdc->db_index = liteQpCqAttrData.rx_data.resp.qp_data.qp_info;
    liteSendCqAttr->device_cq_attr = liteQpCqAttrData.rx_data.resp.send_cq_data;
    liteRecvCqAttr->device_cq_attr = liteQpCqAttrData.rx_data.resp.recv_cq_data;
    ret = memcpy_s((void *)&(liteQpAttr->device_qp_attr), sizeof(liteQpAttr->device_qp_attr),
        (void *)&liteQpCqAttrData.rx_data.resp.qp_data, sizeof(liteQpCqAttrData.rx_data.resp.qp_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]memcpy_s failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    return 0;
}

int RaHdcLiteQpCreate(struct ra_rdma_handle *rdmaHandle, struct ra_qp_handle *qpHdc,
    struct rdma_lite_qp_cap *cap)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct rdma_lite_cq_attr liteSendCqAttr = { 0 };
    struct rdma_lite_cq_attr liteRecvCqAttr = { 0 };
    struct rdma_lite_qp_attr liteQpAttr = { 0 };
    int ret;

    // not support rdma lite or not op mode qp
    if (rdmaHandle->support_lite == 0 || (qpHdc->qp_mode != RA_RS_OP_QP_MODE && qpHdc->qp_mode != RA_RS_OP_QP_MODE_EXT)) {
        return 0;
    }

    ret = RaHdcLiteGetCqQpAttr(qpHdc, &liteSendCqAttr, &liteRecvCqAttr, &liteQpAttr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra_hdc_lite_get_cq_qp_attr failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    ret = RaHdcLiteInitMemPool(rdmaHandle, qpHdc, &liteSendCqAttr, &liteRecvCqAttr, &liteQpAttr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra_hdc_lite_init_mem_pool failed ret(%d) phy_id(%u)",
        ret, phyId), -EFAULT);

    qpHdc->send_lite_cq = RaRdmaLiteCreateCq(rdmaHandle->lite_ctx, &liteSendCqAttr);
    if (qpHdc->send_lite_cq == NULL) {
        hccp_err("[create][ra_hdc_lite_qp]create send_lite_cq failed, errno(%d) phyId(%u)", errno, phyId);
        ret = -EFAULT;
        goto free_mem_pool;
    }

    qpHdc->recv_lite_cq = RaRdmaLiteCreateCq(rdmaHandle->lite_ctx, &liteRecvCqAttr);
    if (qpHdc->recv_lite_cq == NULL) {
        hccp_err("[create][ra_hdc_lite_qp]create recv_lite_cq failed, errno(%d) phyId(%u)", errno, phyId);
        ret = -EFAULT;
        goto free_send_lite_cq;
    }

    RaHdcLiteQpAttrInit(qpHdc, &liteQpAttr, cap);
    qpHdc->lite_qp = RaRdmaLiteCreateQp(rdmaHandle->lite_ctx, &liteQpAttr);
    if (qpHdc->lite_qp == NULL) {
        hccp_err("[create][ra_hdc_lite_qp]ra_rdma_lite_create_qp failed, errno(%d) phyId(%u)", errno, phyId);
        ret = -EFAULT;
        goto free_recv_lite_cq;
    }

    ret = pthread_mutex_init(&qpHdc->qp_mutex, NULL);
    if (ret != 0) {
        hccp_err("[create][ra_hdc_lite_qp]pthread_mutex_init failed ret(%d) phy_id(%u)", ret, phyId);
        goto free_lite_qp;
    }

    ret = pthread_mutex_init(&qpHdc->cqe_err_info.mutex, NULL);
    if (ret != 0) {
        hccp_err("[create][ra_hdc_lite_qp]pthread_mutex_init failed ret(%d) phy_id(%u)", ret, phyId);
        (void)pthread_mutex_destroy(&qpHdc->qp_mutex);
        goto free_lite_qp;
    }

    qpHdc->lite_wc = calloc(MAX_POLL_CQE_NUM, sizeof(struct rdma_lite_wc));
    if (qpHdc->lite_wc == NULL) {
        ret = -ENOMEM;
        (void)pthread_mutex_destroy(&qpHdc->qp_mutex);
        (void)pthread_mutex_destroy(&qpHdc->cqe_err_info.mutex);
        hccp_err("[create][ra_hdc_lite_qp]lite_wc calloc failed phy_id(%u)", phyId);
        goto free_lite_qp;
    }

    RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->rdev_mutex);
    RaListAddTail(&qpHdc->list, &rdmaHandle->qp_list);
    RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);

    qpHdc->support_lite = rdmaHandle->support_lite;

    return 0;

free_lite_qp:
    (void)RaRdmaLiteDestroyQp(qpHdc->lite_qp);
free_recv_lite_cq:
    (void)RaRdmaLiteDestroyCq(qpHdc->recv_lite_cq);
free_send_lite_cq:
    (void)RaRdmaLiteDestroyCq(qpHdc->send_lite_cq);
free_mem_pool:
    RaHdcLiteDeinitMemPool(rdmaHandle, qpHdc);
    return ret;
}

void RaHdcLiteQpDestroy(struct ra_qp_handle *qpHdc)
{
    if ((qpHdc->support_lite != LITE_NOT_SUPPORT) &&
        (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT)) {
        RA_PTHREAD_MUTEX_LOCK(&qpHdc->rdma_handle->rdev_mutex);
        RaListDel(&qpHdc->list);
        RA_PTHREAD_MUTEX_UNLOCK(&qpHdc->rdma_handle->rdev_mutex);

        free(qpHdc->lite_wc);
        qpHdc->lite_wc = NULL;
        (void)pthread_mutex_destroy(&qpHdc->qp_mutex);
        (void)pthread_mutex_destroy(&qpHdc->cqe_err_info.mutex);
        (void)RaRdmaLiteDestroyQp(qpHdc->lite_qp);
        qpHdc->lite_qp = NULL;
        (void)RaRdmaLiteDestroyCq(qpHdc->send_lite_cq);
        qpHdc->send_lite_cq = NULL;
        (void)RaRdmaLiteDestroyCq(qpHdc->recv_lite_cq);
        qpHdc->recv_lite_cq = NULL;
        if (qpHdc->support_lite == LITE_ALIGN_2MB) {
            (void)RaRdmaLiteDeinitMemPool(qpHdc->rdma_handle->lite_ctx, qpHdc->mem_idx);
        }
    }
}

int RaHdcLiteGetConnectedInfo(struct ra_qp_handle *qpHdc)
{
    int ret;
    union op_lite_connected_info_data liteConnectedInfoData = { {0} };

    if ((qpHdc->support_lite != LITE_NOT_SUPPORT) &&
        (qpHdc->qp_mode == RA_RS_OP_QP_MODE || qpHdc->qp_mode == RA_RS_OP_QP_MODE_EXT)) {
        liteConnectedInfoData.tx_data.phy_id = qpHdc->phy_id;
        liteConnectedInfoData.tx_data.rdev_index = qpHdc->rdev_index;
        liteConnectedInfoData.tx_data.qpn = qpHdc->qpn;
        ret = RaHdcProcessMsg(RA_RS_GET_LITE_CONNECTED_INFO, qpHdc->phy_id, (char *)&liteConnectedInfoData,
            sizeof(union op_lite_connected_info_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_lite_connect]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, qpHdc->phy_id), ret);

        ret = memcpy_s((void *)&qpHdc->local_mr[0],
            sizeof(qpHdc->local_mr),
            (void *)&liteConnectedInfoData.rx_data.resp.local_mr[0],
            sizeof(liteConnectedInfoData.rx_data.resp.local_mr));
        CHK_PRT_RETURN(ret, hccp_err("[recv][ra_hdc_lite_connect]memcpy_s local_mr failed, ret(%d) phyId(%u)",
            ret, qpHdc->phy_id), -ESAFEFUNC);

        ret = memcpy_s((void *)&qpHdc->rem_mr[0],
            sizeof(qpHdc->rem_mr),
            (void *)&liteConnectedInfoData.rx_data.resp.rem_mr[0],
            sizeof(liteConnectedInfoData.rx_data.resp.rem_mr));
        CHK_PRT_RETURN(ret, hccp_err("[recv][ra_hdc_lite_connect]memcpy_s rem_mr failed, ret(%d) phyId(%u)",
            ret, qpHdc->phy_id), -ESAFEFUNC);

        ret = RaRdmaLiteSetQpSl(qpHdc->lite_qp, liteConnectedInfoData.rx_data.resp.qos_attr.sl);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_lite_connect]ra_rdma_lite_set_qp_sl failed ret(%d) phy_id(%u)",
            ret, qpHdc->phy_id), ret);
    }

    return 0;
}

void RaHdcLiteGetCqeErrInfo(unsigned int phyId, struct cqe_err_info *info)
{
    struct ra_cqe_err_info *errInfo = &gRaCqeErr[phyId];
    struct cqe_err_info *tempInfo = &errInfo->info;

    RA_PTHREAD_MUTEX_LOCK(&errInfo->mutex);
    info->qpn = tempInfo->qpn;
    info->status = tempInfo->status;
    info->time = tempInfo->time;
    (void)memset_s(&errInfo->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));
    RA_PTHREAD_MUTEX_UNLOCK(&errInfo->mutex);
}

int RaHdcLiteGetCqeErrInfoList(struct ra_rdma_handle *rdmaHandle, struct cqe_err_info *infoList,
    unsigned int *num)
{
    struct ra_qp_handle *qpHdcTmp1 = NULL;
    struct ra_qp_handle *qpHdcTmp = NULL;
    unsigned int cqeErrIdx = 0;
    unsigned int numTmp = 0;

    // not support lite
    if (rdmaHandle->support_lite == 0) {
        *num = 0;
        return 0;
    }

    // no cqe err or no qp
    RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->cqe_err_cnt_mutex);
    if (rdmaHandle->cqe_err_cnt == 0) {
        *num = 0;
        RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->cqe_err_cnt_mutex);
        return 0;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->cqe_err_cnt_mutex);

    RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->rdev_mutex);
    if (RaListEmpty(&rdmaHandle->qp_list)) {
        *num = 0;
        RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);
        return 0;
    }

    // get & clear cqe err info from qp
    numTmp = *num;
    RA_LIST_GET_HEAD_ENTRY(qpHdcTmp, qpHdcTmp1, &rdmaHandle->qp_list, list, struct ra_qp_handle);
    for (; (&qpHdcTmp->list) != &rdmaHandle->qp_list;
        qpHdcTmp = qpHdcTmp1, qpHdcTmp1 = list_entry(qpHdcTmp1->list.next, struct ra_qp_handle, list)) {
        RA_PTHREAD_MUTEX_LOCK(&qpHdcTmp->cqe_err_info.mutex);
        if (qpHdcTmp->cqe_err_info.info.status == 0) {
            RA_PTHREAD_MUTEX_UNLOCK(&qpHdcTmp->cqe_err_info.mutex);
            continue;
        }
        infoList[cqeErrIdx].status = qpHdcTmp->cqe_err_info.info.status;
        infoList[cqeErrIdx].qpn = qpHdcTmp->cqe_err_info.info.qpn;
        infoList[cqeErrIdx].time = qpHdcTmp->cqe_err_info.info.time;
        qpHdcTmp->cqe_err_info.info.status = 0;
        RA_PTHREAD_MUTEX_UNLOCK(&qpHdcTmp->cqe_err_info.mutex);

        RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->cqe_err_cnt_mutex);
        rdmaHandle->cqe_err_cnt--;
        RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->cqe_err_cnt_mutex);
        cqeErrIdx++;
        if (cqeErrIdx >= numTmp) {
            break;
        }
    }
    RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);

    *num = cqeErrIdx;
    return 0;
}

STATIC void RaHdcLiteSaveCqeErrInfo(struct ra_qp_handle *qpHdc, unsigned int status)
{
    unsigned int phyId = qpHdc->phy_id;
    struct ra_cqe_err_info *errInfo = &gRaCqeErr[phyId];
    struct cqe_err_info *tempInfo = &errInfo->info;

    RA_PTHREAD_MUTEX_LOCK(&errInfo->mutex);
    if (tempInfo->status != 0) {
        hccp_run_info("over status=[0x%x], drop qpn[0x%x] err cqe status[0x%x]",
            tempInfo->status, qpHdc->qpn, status);
        RA_PTHREAD_MUTEX_UNLOCK(&errInfo->mutex);
        return;
    }
    tempInfo->status = status;
    tempInfo->qpn = qpHdc->qpn;
    (void)gettimeofday(&tempInfo->time, NULL);
    RA_PTHREAD_MUTEX_UNLOCK(&errInfo->mutex);
}

STATIC void RaHdcLiteSaveQpCqeErrInfo(struct ra_qp_handle *qpHdc, unsigned int status)
{
    RA_PTHREAD_MUTEX_LOCK(&qpHdc->cqe_err_info.mutex);
    if (qpHdc->cqe_err_info.info.status != 0) {
        RA_PTHREAD_MUTEX_UNLOCK(&qpHdc->cqe_err_info.mutex);
        return;
    }
    qpHdc->cqe_err_info.info.status = status;
    qpHdc->cqe_err_info.info.qpn = (uint32_t)qpHdc->qpn;
    (void)gettimeofday(&qpHdc->cqe_err_info.info.time, NULL);
    RA_PTHREAD_MUTEX_UNLOCK(&qpHdc->cqe_err_info.mutex);

    RA_PTHREAD_MUTEX_LOCK(&qpHdc->rdma_handle->cqe_err_cnt_mutex);
    qpHdc->rdma_handle->cqe_err_cnt++;
    RA_PTHREAD_MUTEX_UNLOCK(&qpHdc->rdma_handle->cqe_err_cnt_mutex);
    return;
}

int RaHdcLiteInitCqeErrInfo(unsigned int phyId)
{
    int ret;
    struct ra_cqe_err_info *errInfo = &gRaCqeErr[phyId];

    ret = pthread_mutex_init(&errInfo->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("cqe err mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);

    (void)memset_s(&errInfo->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));

    return 0;
}

void RaHdcLiteDeinitCqeErrInfo(unsigned int phyId)
{
    struct ra_cqe_err_info *errInfo = &gRaCqeErr[phyId];

    (void)pthread_mutex_destroy(&errInfo->mutex);
}

STATIC void RaRetryTimeoutExceptionCheck(struct ra_rdma_handle *rdmaHandle, struct rdma_lite_wc *wc)
{
    int ret = 0;

    if (rdmaHandle->sensor_handle == 0) {
        return;
    }

    if (wc->status != RDMA_LITE_WC_RETRY_EXC_ERR) {
        return;
    }

    /* The notification alarm framework does not filter alarms. In this example, only one notification
       alarm is reported by a single process, which does not need to be accurate. Therefore, no lock is used. */
    if (rdmaHandle->sensor_update_cnt == 0) {
        ret = DlHalSensorNodeUpdateState(rdmaHandle->logic_devid, rdmaHandle->sensor_handle,
            RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE, GENERAL_EVENT_TYPE_ONE_TIME);
        if (ret == 0) {
            rdmaHandle->sensor_update_cnt++;
        }
    }

    hccp_warn("update sensor state logic_devid(%u), qpn(%u), sensor_update_cnt(%d), ret(%d)\n",
        rdmaHandle->logic_devid, wc->qp_num, rdmaHandle->sensor_update_cnt, ret);
}

STATIC void RaHdcLitePeriodPollCqe(struct ra_rdma_handle *rdmaHandle)
{
    int i;
    int ret = 0;
    struct rdma_lite_wc *liteWc;
    unsigned int sentWr, pollCqe;
    struct ra_qp_handle *qpHdcTmp = NULL;
    struct ra_qp_handle *qpHdcTmp1 = NULL;

    RA_LIST_GET_HEAD_ENTRY(qpHdcTmp, qpHdcTmp1, &rdmaHandle->qp_list, list, struct ra_qp_handle);
    for (; (&qpHdcTmp->list) != &rdmaHandle->qp_list;
        qpHdcTmp = qpHdcTmp1, qpHdcTmp1 = list_entry(qpHdcTmp1->list.next, struct ra_qp_handle, list)) {
        sentWr = qpHdcTmp->send_wr_num;
        pollCqe = sentWr - qpHdcTmp->poll_cqe_num;

        if (pollCqe == 0) {
            continue;
        }

        liteWc = calloc(pollCqe, sizeof(struct rdma_lite_wc));
        if (liteWc == NULL) {
            hccp_err("[create][ra_hdc_period_poll]lite_wc calloc failed phy_id(%u)", qpHdcTmp->phy_id);
            break;
        }

        ret = RaRdmaLitePollCq(qpHdcTmp->send_lite_cq, pollCqe, liteWc);
        if (ret < 0) {
            hccp_err("ra_rdma_lite_poll_cq failed ret %d", ret);
            goto poll_cq_err;
        }

        for (i = 0; i < ret; i++) {
            if (liteWc[i].status != RDMA_LITE_WC_SUCCESS && liteWc[i].status != RDMA_LITE_WC_WR_FLUSH_ERR) {
                hccp_err(
                    "[create][ra_hdc_period_poll]failed CQE status[%u], wr[%llu]", liteWc[i].status, liteWc[i].wr_id);
                RaHdcLiteSaveCqeErrInfo(qpHdcTmp, liteWc[i].status);
                RaHdcLiteSaveQpCqeErrInfo(qpHdcTmp, liteWc[i].status);
                RaRetryTimeoutExceptionCheck(rdmaHandle, &liteWc[i]);
            }
        }

        qpHdcTmp->poll_cqe_num += (unsigned int)ret;

poll_cq_err:
        free(liteWc);
        liteWc = NULL;
    }
}

STATIC void *RaHdcLitePthread(void *arg)
{
    struct ra_rdma_handle *rdmaHandle = (struct ra_rdma_handle *)arg;
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;

    hccp_run_info("lite thread begin! thread_id:%lu, pid:%d, ppid:%d, phyId:%u",
        pthread_self(), getpid(), getppid(), phyId);
    CHK_PRT_RETURN(pthread_detach(pthread_self()), hccp_err("pthread_detach failed! thread_id:%lu, errno:%d, phyId:%u",
        pthread_self(), errno, phyId), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_hdc_lite");

    while (1) {
        if (rdmaHandle->thread_status == LITE_THREAD_STATUS_DESTROY) {
            break;
        }
        RA_PTHREAD_MUTEX_LOCK(&rdmaHandle->rdev_mutex);
        if (rdmaHandle->thread_status == LITE_THREAD_STATUS_SUSPEND) {
            RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);
            usleep(THREAD_SLEEP_TIME);
            continue;
        }
        RaHdcLitePeriodPollCqe(rdmaHandle);
        RA_PTHREAD_MUTEX_UNLOCK(&rdmaHandle->rdev_mutex);
        usleep(RA_LITE_POLL_CQE_PERIOD_TIME);
    }

    // thread quit, change status to finish running status(2)
    rdmaHandle->thread_status = LITE_THREAD_STATUS_FINISH_RUNNING;
    hccp_run_info("lite QUIT thread_id:%lu, pid:%d, phyId:%u", pthread_self(), getpid(), phyId);

    return NULL;
}

int RaHdcLitePollCq(struct ra_qp_handle *qpHdc, bool isSendCq, unsigned int numEntries,
    struct rdma_lite_wc_v2 *liteWc)
{
    int ret = 0;
    struct rdma_lite_cq *cq = isSendCq ? qpHdc->lite_qp->send_cq : qpHdc->lite_qp->recv_cq;
    unsigned int *pollCqeNum = isSendCq ? &qpHdc->poll_cqe_num : &qpHdc->poll_recv_cqe_num;
    unsigned int wrNum = isSendCq ? qpHdc->send_wr_num : qpHdc->recv_wr_num;
    int i;

    // no need to poll
    if ((wrNum - *pollCqeNum) == 0) {
        return 0;
    }

    ret = RaRdmaLitePollCqV2(cq, (int)numEntries, liteWc);
    CHK_PRT_RETURN(ret < 0, hccp_err("ra_rdma_lite_poll_cq_v2 failed, ret %d", ret), ret);
    CHK_PRT_RETURN(ret > (int)numEntries,
        hccp_err("ra_rdma_lite_poll_cq_v2 failed, expect maximum numEntries:%u but got %d", numEntries, ret), -EIO);

    for (i = 0; i < ret; i++) {
        RaRetryTimeoutExceptionCheck(qpHdc->rdma_handle, &liteWc[i].wc);
    }

    *pollCqeNum += (unsigned int)ret;
    return ret;
}

STATIC int RaHdcLitePostSend(struct ra_qp_handle *qpHdc, struct lite_mr_info *localMr,
    struct lite_mr_info *remMr, struct lite_send_wr *wr, struct send_wr_rsp *wrRsp, u64 wrId)
{
    int i;
    int ret;
    struct rdma_lite_sge list[RA_SGLIST_MAX];
    struct rdma_lite_send_wr liteWr = {
        .sg_list    = list,
        .opcode     = wr->wr.op,
        .send_flags = wr->wr.send_flag,
    };
    struct rdma_lite_send_wr *badWr = NULL;
    struct rdma_lite_post_send_resp resp = { 0 };
    struct rdma_lite_post_send_attr attr = { 0 };

    for (i = 0; i < wr->wr.buf_num && i < RA_SGLIST_MAX; i++) {
        list[i].addr = (uintptr_t)wr->wr.buf_list[i].addr;
        list[i].length = wr->wr.buf_list[i].len;
        list[i].lkey = localMr->key;
    }

    if (liteWr.opcode == RDMA_LITE_WR_WRITE_WITH_NOTIFY ||
        liteWr.opcode == RDMA_LITE_WR_REDUCE_WRITE ||
        liteWr.opcode == RDMA_LITE_WR_REDUCE_WRITE_NOTIFY) {
        liteWr.imm_data = htobe32((wr->aux.notify_offset & WRITE_NOTIFY_OFFSET_MASK) |
            WRITE_NOTIFY_VALUE_RECORD);
        attr.reduce_op = wr->aux.reduce_type;
        attr.reduce_type = wr->aux.data_type;
    }

    if (liteWr.opcode == RDMA_LITE_WR_RDMA_WRITE_WITH_IMM ||
        liteWr.opcode == RDMA_LITE_WR_SEND_WITH_IMM ||
        liteWr.opcode == RDMA_LITE_WR_ATOMIC_WRITE) {
        liteWr.imm_data = htobe32(wr->ext.imm_data);
    }

    liteWr.num_sge = i;
    liteWr.wr_id = wrId;
    // send op has no rem_mr, no need to assign
    if (wr->wr.op != RA_WR_SEND && wr->wr.op != RA_WR_SEND_WITH_IMM) {
        liteWr.rkey = remMr->key;
        liteWr.remote_addr = wr->wr.dst_addr;
    }

    ret = RaRdmaLitePostSend(qpHdc->lite_qp, &liteWr, &badWr, &attr, &resp);
    if (ret) {
        return ret;
    }

    wrRsp->db.db_index = (unsigned int)qpHdc->db_index;
    wrRsp->db.db_info = resp.db.lite_db_info;

    return 0;
}

static int RaHdcLiteGetMr(struct ra_qp_handle *qpHdc, unsigned long long addr, struct lite_mr_info **mr,
    struct lite_mr_info *srcMr, unsigned int mrNum)
{
    unsigned int i;

    RA_PTHREAD_MUTEX_LOCK(&qpHdc->qp_mutex);

    for (i = 0; i < mrNum; i++) {
        if ((srcMr[i].addr <= addr) && (addr < srcMr[i].addr + srcMr[i].len)) {
            *mr = &srcMr[i];
            RA_PTHREAD_MUTEX_UNLOCK(&qpHdc->qp_mutex);
            return 0;
        }
    }

    RA_PTHREAD_MUTEX_UNLOCK(&qpHdc->qp_mutex);

    return -EINVAL;
}

STATIC int RaHdcLiteHandleBp(struct ra_qp_handle *qpHdc)
{
    u32 sendWr;

    if (qpHdc->send_wr_num >= qpHdc->poll_cqe_num) {
        sendWr = qpHdc->send_wr_num - qpHdc->poll_cqe_num;
    } else {
        sendWr = qpHdc->send_wr_num + (0xFFFFFFFF - qpHdc->poll_cqe_num);
    }

    /*
     * Due to driver limitations, the software pointer updates before the hardware pointer.
     * The software must reserve sq_depth(2^x - 2) to prevent the backpressure mechanism from failing.
     */
    if (sendWr < (qpHdc->sq_depth - 2U)) {
        if (qpHdc->bp_cnt != 0) {
            hccp_run_info("qpn:%u send_wr_num:%u poll_cqe_num:%u send_wr:%u sq_depth:%u "
                "bp_cnt:%u, back pressure relieved",
                qpHdc->qpn, qpHdc->send_wr_num, qpHdc->poll_cqe_num, sendWr, qpHdc->sq_depth,
                qpHdc->bp_cnt);
            qpHdc->bp_cnt = 0;
        }
        return 0;
    }

    // first time back pressure occurred
    if (qpHdc->bp_cnt == 0) {
        hccp_run_warn("qpn:%u send_wr_num:%u poll_cqe_num:%u send_wr:%u sq_depth:%u, back pressure occurred",
            qpHdc->qpn, qpHdc->send_wr_num, qpHdc->poll_cqe_num, sendWr, qpHdc->sq_depth);
    } else {
        hccp_warn("qpn:%u send_wr_num:%u poll_cqe_num:%u send_wr:%u sq_depth:%u, back pressure continues bp_cnt:%u",
            qpHdc->qpn, qpHdc->send_wr_num, qpHdc->poll_cqe_num, sendWr, qpHdc->sq_depth, qpHdc->bp_cnt);
    }

    qpHdc->bp_cnt++;
    return -ENOMEM;
}

int RaHdcLiteTypicalSendWr(struct ra_qp_handle *qpHdc, struct lite_send_wr *wr, struct send_wr_rsp *opRsp,
    unsigned long long wrId)
{
    struct rdma_lite_post_send_resp resp = { 0 };
    struct rdma_lite_post_send_attr attr = { 0 };
    struct rdma_lite_sge list[RA_SGLIST_MAX];
    struct rdma_lite_send_wr liteWr = {
        .sg_list    = list,
        .opcode     = wr->wr.op,
        .send_flags = wr->wr.send_flag,
    };
    struct rdma_lite_send_wr *badWr = NULL;
    int ret;
    int i;

    ret = RaHdcLiteHandleBp(qpHdc);
    if (ret != 0) {
        return ret;
    }

    for (i = 0; i < wr->wr.buf_num && i < RA_SGLIST_MAX; i++) {
        list[i].addr = (uintptr_t)wr->wr.buf_list[i].addr;
        list[i].length = wr->wr.buf_list[i].len;
        list[i].lkey = wr->wr.buf_list[i].lkey;
    }

    liteWr.num_sge = i;
    liteWr.wr_id = wrId;
    liteWr.rkey = wr->wr.rkey;
    liteWr.remote_addr = wr->wr.dst_addr;
    if (liteWr.opcode == RDMA_LITE_WR_WRITE_WITH_NOTIFY ||
        liteWr.opcode == RDMA_LITE_WR_REDUCE_WRITE ||
        liteWr.opcode == RDMA_LITE_WR_REDUCE_WRITE_NOTIFY) {
        liteWr.imm_data = htobe32((wr->aux.notify_offset & WRITE_NOTIFY_OFFSET_MASK) |
            WRITE_NOTIFY_VALUE_RECORD);
        attr.reduce_op = wr->aux.reduce_type;
        attr.reduce_type = wr->aux.data_type;
    }
    liteWr.imm_data = htobe32(wr->ext.imm_data);

    ret = RaRdmaLitePostSend(qpHdc->lite_qp, &liteWr, &badWr, &attr, &resp);
    if (ret) {
        if (ret == -ENOMEM) {
            hccp_warn("[send][ra_hdc_wr]ra hdc post send unsuccessful, ret(%d) phyId(%u)", ret, qpHdc->phy_id);
        } else {
            hccp_err("[send][ra_hdc_wr]ra hdc post send failed ret(%d) phy_id(%u)", ret, qpHdc->phy_id);
        }

        return ret;
    }

    opRsp->db.db_index = (unsigned int)qpHdc->db_index;
    opRsp->db.db_info = resp.db.lite_db_info;

    // user specify wr send_signal flag or user specify qp sq_sig_all flag
    if ((((uint32_t)wr->wr.send_flag & RA_SEND_SIGNALED) != 0) || (qpHdc->sq_sig_all != 0)) {
        qpHdc->send_wr_num++;
    }

    return 0;
}

int RaHdcLiteSendWr(struct ra_qp_handle *qpHdc, struct lite_send_wr *wr, struct send_wr_rsp *opRsp,
    unsigned long long wrId)
{
    struct lite_mr_info *localMr = NULL;
    struct lite_mr_info *remMr = NULL;
    int ret;

    ret = RaHdcLiteGetMr(qpHdc, wr->wr.buf_list[0].addr, &localMr, qpHdc->local_mr, RA_MR_MAX_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_wr]ra hdc get local_mr failed ret(%d) phy_id(%u)",
        ret, qpHdc->phy_id), ret);

    // send op no need to check & get remote mr
    if (wr->wr.op != RA_WR_SEND && wr->wr.op != RA_WR_SEND_WITH_IMM) {
        ret = RaHdcLiteGetMr(qpHdc, wr->wr.dst_addr, &remMr, qpHdc->rem_mr, RA_MR_MAX_NUM);
        CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_wr]ra hdc get rem_mr failed ret(%d) phy_id(%u)",
            ret, qpHdc->phy_id), ret);
    }

    ret = RaHdcLiteHandleBp(qpHdc);
    if (ret != 0) {
        return ret;
    }

    ret = RaHdcLitePostSend(qpHdc, localMr, remMr, wr, opRsp, wrId);
    if (ret) {
        if (ret == -ENOMEM) {
            hccp_warn("[send][ra_hdc_wr]ra hdc post send unsuccessful, ret(%d) phyId(%u)", ret, qpHdc->phy_id);
        } else {
            hccp_err("[send][ra_hdc_wr]ra hdc post send failed, ret(%d) phyId(%u)", ret, qpHdc->phy_id);
        }

        return ret;
    }

    // user specify wr send_signal flag or user specify qp sq_sig_all flag
    if ((((uint32_t)wr->wr.send_flag & RA_SEND_SIGNALED) != 0) || (qpHdc->sq_sig_all != 0)) {
        qpHdc->send_wr_num++;
    }

    return 0;
}

int RaHdcLiteSendWrlist(struct ra_qp_handle *qpHdc, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    int ret;
    unsigned int i = 0;
    struct lite_send_wr normalWr = { 0 };

    while (i < wrlistNum.send_num) {
        normalWr.wr.buf_list = &(wr[i].mem_list);
        normalWr.wr.buf_num = 1;
        normalWr.wr.dst_addr = wr[i].dst_addr;
        normalWr.wr.op = wr[i].op;
        normalWr.wr.send_flag = wr[i].send_flags;
        ret = RaHdcLiteSendWr(qpHdc, &normalWr, &opRsp[i], HDC_LITE_DEFAULT_WR_ID);
        if (ret) {
            if (ret == -ENOMEM) {
                hccp_warn("[send][ra_hdc_lite_wrlist]ra_hdc_lite_send_wr unsuccessful, ret(%d) phyId(%u) "
                    "send_index(%u)", ret, qpHdc->phy_id, i);
            } else {
                hccp_err("[send][ra_hdc_lite_wrlist]ra_hdc_lite_send_wr failed, ret(%d) phyId(%u) send_index(%u)",
                    ret, qpHdc->phy_id, i);
            }

            *(wrlistNum.complete_num) = i;
            return ret;
        }

        i++;
    }

    *(wrlistNum.complete_num) = i;

    return 0;
}

int RaHdcLiteSendWrlistExt(struct ra_qp_handle *qpHdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp opRsp[], struct wrlist_send_complete_num wrlistNum)
{
    int ret;
    unsigned int i = 0;
    struct lite_send_wr normalWr = { 0 };

    while (i < wrlistNum.send_num) {
        normalWr.wr.buf_list = &(wr[i].mem_list);
        normalWr.wr.buf_num = 1;
        normalWr.wr.dst_addr = wr[i].dst_addr;
        normalWr.wr.op = wr[i].op;
        normalWr.wr.send_flag = wr[i].send_flags;
        normalWr.aux = wr[i].aux;
        normalWr.ext = wr[i].ext;
        ret = RaHdcLiteSendWr(qpHdc, &normalWr, &opRsp[i], HDC_LITE_DEFAULT_WR_ID);
        if (ret) {
            if (ret == -ENOMEM) {
                hccp_warn("[send][ra_hdc_lite_send_wrlist_ext]ra_hdc_lite_send_wr unsuccessful, ret(%d) phyId(%u) "
                    "send_index(%u)", ret, qpHdc->phy_id, i);
            } else {
                hccp_err("[send][ra_hdc_lite_send_wrlist_ext]ra_hdc_lite_send_wr failed, ret(%d) phyId(%u) "
                    "send_index(%u)", ret, qpHdc->phy_id, i);
            }

            *(wrlistNum.complete_num) = i;
            return ret;
        }

        i++;
    }

    *(wrlistNum.complete_num) = i;

    return 0;
}

int RaHdcLiteSendNormalWrlist(struct ra_qp_handle *qpHdc, struct wr_info wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    struct lite_send_wr normalWr = { 0 };
    unsigned int i = 0;
    int ret = 0;

    while (i < wrlistNum.send_num) {
        normalWr.wr.send_flag = wr[i].send_flags;
        normalWr.wr.rkey = wr[i].rkey;
        normalWr.wr.op = wr[i].op;
        normalWr.wr.dst_addr = wr[i].dst_addr;
        normalWr.wr.buf_list = &(wr[i].mem_list);
        normalWr.wr.buf_num = 1;
        normalWr.aux = wr[i].aux;
        if (wr[i].op == RDMA_LITE_WR_RDMA_WRITE_WITH_IMM || wr[i].op == RDMA_LITE_WR_SEND_WITH_IMM ||
            wr[i].op == RDMA_LITE_WR_ATOMIC_WRITE) {
            normalWr.ext.imm_data = wr[i].imm_data;
        }
        ret = RaHdcLiteTypicalSendWr(qpHdc, &normalWr, &opRsp[i], wr[i].wr_id);
        if (ret != 0) {
            if (ret == -ENOMEM) {
                hccp_warn("[send][send_wrlist]ra_hdc_lite_send_wr unsuccessful, ret(%d) phyId(%u) send_index(%u)",
                    ret, qpHdc->phy_id, i);
            } else {
                hccp_err("[send][send_wrlist]ra_hdc_lite_send_wr failed, ret(%d) phyId(%u) send_index(%u)",
                    ret, qpHdc->phy_id, i);
            }

            break;
        }

        i++;
    }

    *(wrlistNum.complete_num) = i;

    return ret;
}

STATIC void RaHdcLiteBuildRecvWr(struct recv_wrlist_data *wr, struct rdma_lite_sge *list,
    struct rdma_lite_recv_wr *liteWr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey = wr->mem_list.lkey;

    liteWr->sg_list = list;
    liteWr->wr_id = wr->wr_id;
    liteWr->num_sge = 1; /* only support one sge */
}

int RaHdcLiteRecvWrlist(struct ra_qp_handle *qpHdc, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum)
{
    struct rdma_lite_recv_wr *liteWr = NULL;
    struct rdma_lite_recv_wr *badWr = NULL;
    struct rdma_lite_sge *list = NULL;
    unsigned int index;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(recvNum == 0, hccp_err("lite recv_num[%u] is invalid!", recvNum), -EINVAL);

    liteWr = (struct rdma_lite_recv_wr *)calloc(recvNum, sizeof(struct rdma_lite_recv_wr));
    CHK_PRT_RETURN(liteWr == NULL, hccp_err("lite calloc lite_wr failed!"), -ENOSPC);

    list = (struct rdma_lite_sge *)calloc(recvNum, sizeof(struct rdma_lite_sge));
    if (list == NULL) {
        hccp_err("lite calloc list failed!");
        ret = -ENOSPC;
        goto alloc_sge_fail;
    }

    // build up recv lite wr
    for (i = 0; i < recvNum; i++) {
        RaHdcLiteBuildRecvWr(&wr[i], &list[i], &liteWr[i]);
        index = i + 1;
        liteWr[i].next = (i < recvNum - 1) ? &(liteWr[index]) : NULL;
    }

    ret = RaRdmaLitePostRecv(qpHdc->lite_qp, liteWr, &badWr);
    if (ret == 0) {
        *completeNum = recvNum;
    } else if (ret == -ENOMEM) {
        *completeNum = (unsigned int)((void *)badWr - (void *)liteWr) / sizeof(struct rdma_lite_recv_wr);
        hccp_dbg("ra_rdma_lite_post_recv wqe overflow, completeNum[%d]", *completeNum);
    } else {
        *completeNum = 0;
        hccp_err("ra_rdma_lite_post_recv failed, ret[%d]", ret);
    }

    qpHdc->recv_wr_num += *completeNum;

    free(list);
    list = NULL;

alloc_sge_fail:
    free(liteWr);
    liteWr = NULL;
    return (ret == -ENOMEM) ? 0 : ret;
}
