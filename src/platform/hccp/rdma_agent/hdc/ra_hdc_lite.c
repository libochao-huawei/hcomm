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

struct ra_cqe_err_info g_ra_cqe_err[RA_MAX_PHY_ID_NUM];

STATIC void *ra_hdc_lite_pthread(void *arg);

STATIC int ra_hdc_get_drv_lite_support(unsigned int phy_id, bool enabled_910a_lite, unsigned int *support)
{
    int ret;
    size_t out_len = 0;
    unsigned int logic_id;
    int64_t device_info = 0;
    struct supportFeaturePara in_para = { 0 };
    struct supportFeaturePara out_para = { 0 };

    ret = dl_drv_device_get_index_by_phy_id(phy_id, &logic_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]dl_drv_device_get_index_by_phy_id failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);

    // enabled_910a_lite not explicitly set to true, disabled lite if chip_type is 910A due to memory limits
    if (!enabled_910a_lite) {
        ret = dl_hal_get_device_info(logic_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &device_info);
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]dl_hal_get_device_info failed, ret(%d), phy_id(%u)",
            ret, phy_id), ret);
        if (dl_hal_plat_get_chip((uint64_t)device_info) == CHIP_TYPE_910A) { // Memory Limits
            *support = 0;
            hccp_info("[init][ra_hdc_lite]device_info:0x%llx not support, phy_id(%u)", device_info, phy_id);
            return 0;
        }
    }

    in_para.support_feature = CTRL_SUPPORT_DEV_MEM_REGISTER_MASK | CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_MASK;
    in_para.devid = logic_id;
    ret = dl_hal_mem_ctl(CTRL_TYPE_SUPPORT_FEATURE, &in_para, sizeof(struct supportFeaturePara), &out_para, &out_len);
    if ((ret != 0) || (((out_para.support_feature & CTRL_SUPPORT_DEV_MEM_REGISTER_MASK) == 0) &&
        ((out_para.support_feature & CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_MASK) == 0))) {
        *support = 0;
        return 0;
    }

    if ((out_para.support_feature & CTRL_SUPPORT_DEV_MEM_REGISTER_MASK) != 0) {
        *support = LITE_SUPPORT_DEV_MEM_REGISTER;
    }
    if ((out_para.support_feature & CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_MASK) != 0) {
        *support |= LITE_SUPPORT_PCIE_BAR_HUGE_MEM;
    }

    return 0;
}

STATIC void ra_hdc_get_opcode_lite_support(unsigned int phy_id, unsigned int support_feature, int *support)
{
    int ret;
    unsigned int interface_version = 0;

    ret = ra_hdc_get_interface_version(phy_id, RA_RS_GET_LITE_SUPPORT, &interface_version);
    // get version failed or opcode interface_version is 0: opcode not support lite
    if (ret != 0 || interface_version == 0) {
        hccp_info("[init][ra_hdc_lite]get opcode not support, ret[%d] != 0 or interface_version is 0", ret);
        *support = LITE_NOT_SUPPORT;
        return;
    }

    // at least driver&host support lite 4KB page_size align
    if ((support_feature & LITE_SUPPORT_DEV_MEM_REGISTER) != 0) {
        *support = LITE_ALIGN_4KB;
        return;
    }

    // driver&host support lite 2MB page_size align
    if ((interface_version == LITE_VERSION_V2) && ((support_feature & LITE_SUPPORT_PCIE_BAR_HUGE_MEM) != 0)) {
        *support = LITE_ALIGN_2MB;
        return;
    }

    // none of 4KB page_size align & 2MB page_size align lite support
    hccp_info("[init][ra_hdc_lite]get opcode not support, interface_version[%u] support_feature[0x%x]",
        interface_version, support_feature);
    *support = LITE_NOT_SUPPORT;
    return;
}

STATIC int ra_hdc_get_rdma_lite_support(struct ra_rdma_handle *rdma_handle, unsigned int support_feature, int *support)
{
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    unsigned int rdev_index = rdma_handle->rdev_index;
    union op_lite_support_data lite_support_data;
    int support_lite = 0;
    int ret;

    // get opcode support lite with support_feature
    ra_hdc_get_opcode_lite_support(phy_id, support_feature, &support_lite);
    if (support_lite == LITE_NOT_SUPPORT) {
        *support = LITE_NOT_SUPPORT;
        return 0;
    }

    // no need to support lite if enabled_2mb_lite not enabled
    if (support_lite == LITE_ALIGN_2MB && !rdma_handle->enabled_2mb_lite) {
        hccp_run_info("[init][ra_hdc_lite]rdma_handle->enabled_2mb_lite=%d, no need to support LITE_ALIGN_2MB",
            rdma_handle->enabled_2mb_lite);
        *support = LITE_NOT_SUPPORT;
        return 0;
    }

    // RA_RS_GET_LITE_SUPPORT will set rdev_cb->support_lite = 1
    (void)memset_s(&lite_support_data, sizeof(lite_support_data), 0, sizeof(lite_support_data));
    lite_support_data.tx_data.phy_id = phy_id;
    lite_support_data.tx_data.rdev_index = rdev_index;
    ret = ra_hdc_process_msg(RA_RS_GET_LITE_SUPPORT, phy_id, (char *)&lite_support_data,
        sizeof(union op_lite_support_data));
    if (ret != 0) {
        if (ret == -EPROTONOSUPPORT) {
            *support = LITE_NOT_SUPPORT;
            ret = 0;
        } else {
            hccp_err("[init][ra_hdc_lite]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        }
        return ret;
    }

    *support = support_lite;
    return 0;
}

STATIC int ra_hdc_get_lite_support(struct ra_rdma_handle *rdma_handle, unsigned int phy_id)
{
    int ret;
    unsigned int support_feature = 0;

#ifndef HNS_ROCE_LLT
    ret = ra_hdc_get_drv_lite_support(phy_id, rdma_handle->enabled_910a_lite, &support_feature);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]ra_hdc_get_drv_lite_support failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);
#else
    support_feature = 1;
#endif
    if (support_feature != 0) {
        ret = ra_hdc_get_rdma_lite_support(rdma_handle, support_feature, &rdma_handle->support_lite);
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]ra_hdc_get_rdma_lite_support failed, ret(%d), phy_id(%u)",
            ret, phy_id), ret);

        if (rdma_handle->support_lite) {
            hccp_run_info("[init][ra_hdc_lite]support_feature:0x%x, support_lite:%u", support_feature,
                rdma_handle->support_lite);
            ret = ra_hdc_rdma_lite_api_init();
            CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]ra_hdc_rdma_lite_api_init failed, ret(%d), phy_id(%u)",
                ret, phy_id), ret);
        }
    }

    return 0;
}

STATIC int ra_sensor_node_register(unsigned int phy_id, struct ra_rdma_handle *rdma_handle)
{
    struct halSensorNodeCfg cfg = { 0 };
    unsigned int interface_version = 0;
    int ret;

    ret = ra_hdc_get_interface_version(phy_id, RA_RS_RDEV_INIT, &interface_version);
    if ((ret != 0) || (interface_version <= RA_RS_OPCODE_BASE_VERSION)) {
        /* unknown or old version, not support sensor */
        rdma_handle->sensor_handle = 0;
        hccp_warn("[init][ra_hdc_lite]not support sensor, ret:%d, phy_id:%u, interface_version:%u",
            ret, phy_id, interface_version);
        return 0;
    }

    rdma_handle->sensor_update_cnt = 0;
    ret = sprintf_s(cfg.name, sizeof(cfg.name), "roce_ra_%d", getpid());
    CHK_PRT_RETURN(ret <= 0, hccp_err("[init][ra_hdc_lite]sprintf_s name err, ret:%d, phy_id:%u",
        ret, phy_id), -ESAFEFUNC);

    cfg.NodeType = HAL_DMS_DEV_TYPE_HCCP;
    cfg.SensorType = RDMA_CQE_ERR_SENSOR_TYPE;
    cfg.AssertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_MASK;
    cfg.DeassertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE_MASK;
    ret = dl_hal_sensor_node_register(rdma_handle->logic_devid, &cfg, &rdma_handle->sensor_handle);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite]dl_hal_sensor_node_register failed, ret(%d)", ret), ret);

    return 0;
}

STATIC int ra_hdc_lite_get_rdev_cap(struct ra_rdma_handle *rdma_handle, unsigned int phy_id, unsigned int rdev_index,
    union op_lite_rdev_cap_data *lite_rdev_cap_data)
{
#define PAGE_ALIGN_2MB (2 * 1024 * 1024)
    int ret;

    lite_rdev_cap_data->tx_data.phy_id = phy_id;
    lite_rdev_cap_data->tx_data.rdev_index = rdev_index;
    ret = ra_hdc_process_msg(RA_RS_GET_LITE_RDEV_CAP, phy_id, (char *)lite_rdev_cap_data,
        sizeof(union op_lite_rdev_cap_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lite_ctx]hdc get lite rdev cap failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);

    // should change page_size to 2MB
    if (rdma_handle->support_lite == LITE_ALIGN_2MB) {
        lite_rdev_cap_data->rx_data.resp.cap.page_size = PAGE_ALIGN_2MB;
    }
    return 0;
}

STATIC int ra_hdc_lite_mutex_init(struct ra_rdma_handle *rdma_handle, unsigned int phy_id)
{
    int ret;

    ret = pthread_mutex_init(&rdma_handle->rdev_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_lite_ctx]pthread_mutex_init rdev_mutex failed ret(%d) phy_id(%u)", ret, phy_id);
        return -ESYSFUNC;
    }

    ret = pthread_mutex_init(&rdma_handle->cqe_err_cnt_mutex, NULL);
    if (ret != 0) {
        (void)pthread_mutex_destroy(&rdma_handle->rdev_mutex);
        hccp_err("[init][ra_hdc_lite_ctx]pthread_mutex_init cqe_err_cnt_mutex failed ret(%d) phy_id(%u)", ret, phy_id);
        return -ESYSFUNC;
    }

    return 0;
}

STATIC void ra_hdc_lite_mutex_deinit(struct ra_rdma_handle *rdma_handle)
{
    (void)pthread_mutex_destroy(&rdma_handle->cqe_err_cnt_mutex);
    (void)pthread_mutex_destroy(&rdma_handle->rdev_mutex);
}

STATIC int ra_hdc_lite_ctx_init(struct ra_rdma_handle *rdma_handle, unsigned int phy_id, unsigned int rdev_index)
{
    union op_lite_rdev_cap_data lite_rdev_cap_data = { 0 };
    int ret = 0;

    if (rdma_handle->support_lite == 0) {
        return 0;
    }

    // register sensor node
    ret = dl_drv_device_get_index_by_phy_id(phy_id, &rdma_handle->logic_devid);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lite_ctx]dl_drv_device_get_index_by_phy_id failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);
    ret = ra_sensor_node_register(phy_id, rdma_handle);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lite_ctx]ra_sensor_node_register failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    // alloc ctx
    ret = ra_hdc_lite_get_rdev_cap(rdma_handle, phy_id, rdev_index, &lite_rdev_cap_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lite_ctx]ra_hdc_lite_get_rdev_cap failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);
    rdma_handle->lite_ctx = ra_rdma_lite_alloc_ctx(phy_id, &lite_rdev_cap_data.rx_data.resp.cap);
    if (rdma_handle->lite_ctx == NULL) {
        hccp_err("[init][ra_hdc_lite_ctx]ra_rdma_lite_alloc_ctx errno(%d) phy_id(%u)", errno, phy_id);
        ret = -EFAULT;
        goto unreg_sensor;
    }

    RA_INIT_LIST_HEAD(&rdma_handle->qp_list);

    ret = ra_hdc_lite_mutex_init(rdma_handle, phy_id);
    if (ret != 0) {
        goto free_ctx;
    }

    if (rdma_handle->disabled_lite_thread) {
        hccp_run_info("lite thread disabled");
        return 0;
    }

    rdma_handle->thread_status = LITE_THREAD_STATUS_RUNNING;
    ret = pthread_create(&rdma_handle->tid, NULL, ra_hdc_lite_pthread, (void *)rdma_handle);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_lite_ctx]pthread_create failed, ret:%d, phy_id:%u errno:%d", ret, phy_id, errno);
        rdma_handle->thread_status = LITE_THREAD_STATUS_DESTROY;
        ret = -ESYSFUNC;
        goto mutex_deinit;
    }

    return 0;

mutex_deinit:
    ra_hdc_lite_mutex_deinit(rdma_handle);
free_ctx:
    ra_rdma_lite_free_ctx(rdma_handle->lite_ctx);
unreg_sensor:
    (void)dl_hal_sensor_node_unregister(rdma_handle->logic_devid, rdma_handle->sensor_handle);
    return ret;
}

int ra_hdc_lite_init(struct ra_rdma_handle *rdma_handle, unsigned int phy_id, unsigned int rdev_index)
{
    int ret;

    ret = ra_hdc_get_lite_support(rdma_handle, phy_id);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_rdev]ra_hdc_get_lite_support failed ret(%d) phy_id(%u)", ret, phy_id);
        return ret;
    }

    ret = ra_hdc_lite_ctx_init(rdma_handle, phy_id, rdev_index);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_rdev]ra_hdc_lite_ctx_init failed ret(%d) phy_id(%u)", ret, phy_id);
        goto free_lite_api;
    }

    return 0;

free_lite_api:
    ra_hdc_rdma_lite_api_deinit();
    return ret;
}

void ra_hdc_lite_deinit(struct ra_rdma_handle *rdma_handle)
{
#define FINISH_RUNNING 2
#define THREAD_STATUS_CHANGE_TIMEOUT 100
    int i;

    if (rdma_handle->support_lite) {
        if (rdma_handle->disabled_lite_thread) {
            goto disabled_thread_out;
        }

        rdma_handle->thread_status = LITE_THREAD_STATUS_DESTROY;
        // wait thread change to finish running status(2), wait 100 times(total cost: 1s) until timeout
        for (i = 0; i < THREAD_STATUS_CHANGE_TIMEOUT && rdma_handle->thread_status != FINISH_RUNNING; i++) {
            usleep(RA_LITE_POLL_CQE_PERIOD_TIME);
        }
        // thread not in finish running status(2), report timeout
        if (rdma_handle->thread_status != FINISH_RUNNING) {
            hccp_run_info("hdc wait thread tid:%lu finish running timeout, thread status:%d",
                rdma_handle->tid, rdma_handle->thread_status);
        }

disabled_thread_out:
        ra_hdc_lite_mutex_deinit(rdma_handle);
        ra_rdma_lite_free_ctx(rdma_handle->lite_ctx);
        ra_hdc_rdma_lite_api_deinit();
        (void)dl_hal_sensor_node_unregister(rdma_handle->logic_devid, rdma_handle->sensor_handle);
    }
}

STATIC void ra_hdc_lite_qp_attr_init(struct ra_qp_handle *qp_hdc, struct rdma_lite_qp_attr *lite_qp_attr,
    struct rdma_lite_qp_cap *cap)
{
    lite_qp_attr->send_cq = qp_hdc->send_lite_cq;
    lite_qp_attr->recv_cq = qp_hdc->recv_lite_cq;
    lite_qp_attr->qp_mode = qp_hdc->qp_mode;
    lite_qp_attr->qp_type = RDMA_LITE_QPT_RC;
    lite_qp_attr->cap.max_inline_data = cap->max_inline_data;
    lite_qp_attr->cap.max_send_sge = cap->max_send_sge;
    lite_qp_attr->cap.max_recv_sge = cap->max_recv_sge;
    lite_qp_attr->cap.max_send_wr = cap->max_send_wr;
    lite_qp_attr->cap.max_recv_wr = cap->max_recv_wr;
}

STATIC int ra_hdc_lite_init_mem_pool(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle *qp_hdc,
    struct rdma_lite_cq_attr *lite_send_cq_attr, struct rdma_lite_cq_attr *lite_recv_cq_attr,
    struct rdma_lite_qp_attr *lite_qp_attr)
{
    union op_lite_mem_attr_data lite_mem_attr_data = { 0 };
    unsigned int phy_id = qp_hdc->phy_id;
    int ret;

    if (rdma_handle->support_lite != LITE_ALIGN_2MB) {
        return 0;
    }

    lite_mem_attr_data.tx_data.phy_id = phy_id;
    lite_mem_attr_data.tx_data.rdev_index = rdma_handle->rdev_index;
    lite_mem_attr_data.tx_data.qpn = qp_hdc->qpn;
    ret = ra_hdc_process_msg(RA_RS_GET_LITE_MEM_ATTR, phy_id, (char *)&lite_mem_attr_data,
        sizeof(union op_lite_mem_attr_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ret = ra_rdma_lite_init_mem_pool(rdma_handle->lite_ctx,
        (struct rdma_lite_mem_attr *)&lite_mem_attr_data.rx_data.resp.mem_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra_rdma_lite_init_mem_pool failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    qp_hdc->mem_idx = lite_mem_attr_data.rx_data.resp.mem_data.mem_idx;
    lite_send_cq_attr->mem_idx = qp_hdc->mem_idx;
    lite_recv_cq_attr->mem_idx = qp_hdc->mem_idx;
    lite_qp_attr->mem_idx = qp_hdc->mem_idx;
    return 0;
}

STATIC void ra_hdc_lite_deinit_mem_pool(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle *qp_hdc)
{
    unsigned int phy_id = qp_hdc->phy_id;
    int ret;

    if (rdma_handle->support_lite != LITE_ALIGN_2MB) {
        return;
    }

    ret = ra_rdma_lite_deinit_mem_pool(rdma_handle->lite_ctx, qp_hdc->mem_idx);
    if (ret != 0) {
        hccp_err("[create][ra_hdc_lite_qp]ra_rdma_lite_deinit_mem_pool failed ret(%d) phy_id(%u)", ret, phy_id);
    }
    return;
}

STATIC int ra_hdc_lite_get_cq_qp_attr(struct ra_qp_handle *qp_hdc, struct rdma_lite_cq_attr *lite_send_cq_attr,
    struct rdma_lite_cq_attr *lite_recv_cq_attr, struct rdma_lite_qp_attr *lite_qp_attr)
{
    union op_lite_qp_cq_attr_data lite_qp_cq_attr_data = { 0 };
    unsigned int phy_id = qp_hdc->phy_id;
    int ret;

    lite_qp_cq_attr_data.tx_data.phy_id = phy_id;
    lite_qp_cq_attr_data.tx_data.rdev_index = qp_hdc->rdev_index;
    lite_qp_cq_attr_data.tx_data.qpn = qp_hdc->qpn;
    ret = ra_hdc_process_msg(RA_RS_GET_LITE_QP_CQ_ATTR, phy_id, (char *)&lite_qp_cq_attr_data,
        sizeof(union op_lite_qp_cq_attr_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    qp_hdc->db_index = lite_qp_cq_attr_data.rx_data.resp.qp_data.qp_info;
    lite_send_cq_attr->device_cq_attr = lite_qp_cq_attr_data.rx_data.resp.send_cq_data;
    lite_recv_cq_attr->device_cq_attr = lite_qp_cq_attr_data.rx_data.resp.recv_cq_data;
    ret = memcpy_s((void *)&(lite_qp_attr->device_qp_attr), sizeof(lite_qp_attr->device_qp_attr),
        (void *)&lite_qp_cq_attr_data.rx_data.resp.qp_data, sizeof(lite_qp_cq_attr_data.rx_data.resp.qp_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]memcpy_s failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}

int ra_hdc_lite_qp_create(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle *qp_hdc,
    struct rdma_lite_qp_cap *cap)
{
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct rdma_lite_cq_attr lite_send_cq_attr = { 0 };
    struct rdma_lite_cq_attr lite_recv_cq_attr = { 0 };
    struct rdma_lite_qp_attr lite_qp_attr = { 0 };
    int ret;

    // not support rdma lite or not op mode qp
    if (rdma_handle->support_lite == 0 || (qp_hdc->qp_mode != RA_RS_OP_QP_MODE && qp_hdc->qp_mode != RA_RS_OP_QP_MODE_EXT)) {
        return 0;
    }

    ret = ra_hdc_lite_get_cq_qp_attr(qp_hdc, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra_hdc_lite_get_cq_qp_attr failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ret = ra_hdc_lite_init_mem_pool(rdma_handle, qp_hdc, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[create][ra_hdc_lite_qp]ra_hdc_lite_init_mem_pool failed ret(%d) phy_id(%u)",
        ret, phy_id), -EFAULT);

    qp_hdc->send_lite_cq = ra_rdma_lite_create_cq(rdma_handle->lite_ctx, &lite_send_cq_attr);
    if (qp_hdc->send_lite_cq == NULL) {
        hccp_err("[create][ra_hdc_lite_qp]create send_lite_cq failed, errno(%d) phy_id(%u)", errno, phy_id);
        ret = -EFAULT;
        goto free_mem_pool;
    }

    qp_hdc->recv_lite_cq = ra_rdma_lite_create_cq(rdma_handle->lite_ctx, &lite_recv_cq_attr);
    if (qp_hdc->recv_lite_cq == NULL) {
        hccp_err("[create][ra_hdc_lite_qp]create recv_lite_cq failed, errno(%d) phy_id(%u)", errno, phy_id);
        ret = -EFAULT;
        goto free_send_lite_cq;
    }

    ra_hdc_lite_qp_attr_init(qp_hdc, &lite_qp_attr, cap);
    qp_hdc->lite_qp = ra_rdma_lite_create_qp(rdma_handle->lite_ctx, &lite_qp_attr);
    if (qp_hdc->lite_qp == NULL) {
        hccp_err("[create][ra_hdc_lite_qp]ra_rdma_lite_create_qp failed, errno(%d) phy_id(%u)", errno, phy_id);
        ret = -EFAULT;
        goto free_recv_lite_cq;
    }

    ret = pthread_mutex_init(&qp_hdc->qp_mutex, NULL);
    if (ret != 0) {
        hccp_err("[create][ra_hdc_lite_qp]pthread_mutex_init failed ret(%d) phy_id(%u)", ret, phy_id);
        goto free_lite_qp;
    }

    ret = pthread_mutex_init(&qp_hdc->cqe_err_info.mutex, NULL);
    if (ret != 0) {
        hccp_err("[create][ra_hdc_lite_qp]pthread_mutex_init failed ret(%d) phy_id(%u)", ret, phy_id);
        (void)pthread_mutex_destroy(&qp_hdc->qp_mutex);
        goto free_lite_qp;
    }

    qp_hdc->lite_wc = calloc(MAX_POLL_CQE_NUM, sizeof(struct rdma_lite_wc));
    if (qp_hdc->lite_wc == NULL) {
        ret = -ENOMEM;
        (void)pthread_mutex_destroy(&qp_hdc->qp_mutex);
        (void)pthread_mutex_destroy(&qp_hdc->cqe_err_info.mutex);
        hccp_err("[create][ra_hdc_lite_qp]lite_wc calloc failed phy_id(%u)", phy_id);
        goto free_lite_qp;
    }

    RA_PTHREAD_MUTEX_LOCK(&rdma_handle->rdev_mutex);
    ra_list_add_tail(&qp_hdc->list, &rdma_handle->qp_list);
    RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);

    qp_hdc->support_lite = rdma_handle->support_lite;

    return 0;

free_lite_qp:
    (void)ra_rdma_lite_destroy_qp(qp_hdc->lite_qp);
free_recv_lite_cq:
    (void)ra_rdma_lite_destroy_cq(qp_hdc->recv_lite_cq);
free_send_lite_cq:
    (void)ra_rdma_lite_destroy_cq(qp_hdc->send_lite_cq);
free_mem_pool:
    ra_hdc_lite_deinit_mem_pool(rdma_handle, qp_hdc);
    return ret;
}

void ra_hdc_lite_qp_destroy(struct ra_qp_handle *qp_hdc)
{
    if ((qp_hdc->support_lite != LITE_NOT_SUPPORT) &&
        (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT)) {
        RA_PTHREAD_MUTEX_LOCK(&qp_hdc->rdma_handle->rdev_mutex);
        ra_list_del(&qp_hdc->list);
        RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc->rdma_handle->rdev_mutex);

        free(qp_hdc->lite_wc);
        qp_hdc->lite_wc = NULL;
        (void)pthread_mutex_destroy(&qp_hdc->qp_mutex);
        (void)pthread_mutex_destroy(&qp_hdc->cqe_err_info.mutex);
        (void)ra_rdma_lite_destroy_qp(qp_hdc->lite_qp);
        qp_hdc->lite_qp = NULL;
        (void)ra_rdma_lite_destroy_cq(qp_hdc->send_lite_cq);
        qp_hdc->send_lite_cq = NULL;
        (void)ra_rdma_lite_destroy_cq(qp_hdc->recv_lite_cq);
        qp_hdc->recv_lite_cq = NULL;
        if (qp_hdc->support_lite == LITE_ALIGN_2MB) {
            (void)ra_rdma_lite_deinit_mem_pool(qp_hdc->rdma_handle->lite_ctx, qp_hdc->mem_idx);
        }
    }
}

int ra_hdc_lite_get_connected_info(struct ra_qp_handle *qp_hdc)
{
    int ret;
    union op_lite_connected_info_data lite_connected_info_data = { {0} };

    if ((qp_hdc->support_lite != LITE_NOT_SUPPORT) &&
        (qp_hdc->qp_mode == RA_RS_OP_QP_MODE || qp_hdc->qp_mode == RA_RS_OP_QP_MODE_EXT)) {
        lite_connected_info_data.tx_data.phy_id = qp_hdc->phy_id;
        lite_connected_info_data.tx_data.rdev_index = qp_hdc->rdev_index;
        lite_connected_info_data.tx_data.qpn = qp_hdc->qpn;
        ret = ra_hdc_process_msg(RA_RS_GET_LITE_CONNECTED_INFO, qp_hdc->phy_id, (char *)&lite_connected_info_data,
            sizeof(union op_lite_connected_info_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_lite_connect]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), ret);

        ret = memcpy_s((void *)&qp_hdc->local_mr[0],
            sizeof(qp_hdc->local_mr),
            (void *)&lite_connected_info_data.rx_data.resp.local_mr[0],
            sizeof(lite_connected_info_data.rx_data.resp.local_mr));
        CHK_PRT_RETURN(ret, hccp_err("[recv][ra_hdc_lite_connect]memcpy_s local_mr failed, ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), -ESAFEFUNC);

        ret = memcpy_s((void *)&qp_hdc->rem_mr[0],
            sizeof(qp_hdc->rem_mr),
            (void *)&lite_connected_info_data.rx_data.resp.rem_mr[0],
            sizeof(lite_connected_info_data.rx_data.resp.rem_mr));
        CHK_PRT_RETURN(ret, hccp_err("[recv][ra_hdc_lite_connect]memcpy_s rem_mr failed, ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), -ESAFEFUNC);

        ret = ra_rdma_lite_set_qp_sl(qp_hdc->lite_qp, lite_connected_info_data.rx_data.resp.qos_attr.sl);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_lite_connect]ra_rdma_lite_set_qp_sl failed ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), ret);
    }

    return 0;
}

void ra_hdc_lite_get_cqe_err_info(unsigned int phy_id, struct cqe_err_info *info)
{
    struct ra_cqe_err_info *err_info = &g_ra_cqe_err[phy_id];
    struct cqe_err_info *temp_info = &err_info->info;

    RA_PTHREAD_MUTEX_LOCK(&err_info->mutex);
    info->qpn = temp_info->qpn;
    info->status = temp_info->status;
    info->time = temp_info->time;
    (void)memset_s(&err_info->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));
    RA_PTHREAD_MUTEX_UNLOCK(&err_info->mutex);
}

int ra_hdc_lite_get_cqe_err_info_list(struct ra_rdma_handle *rdma_handle, struct cqe_err_info *info_list,
    unsigned int *num)
{
    struct ra_qp_handle *qp_hdc_tmp1 = NULL;
    struct ra_qp_handle *qp_hdc_tmp = NULL;
    unsigned int cqe_err_idx = 0;
    unsigned int num_tmp = 0;

    // not support lite
    if (rdma_handle->support_lite == 0) {
        *num = 0;
        return 0;
    }

    // no cqe err or no qp
    RA_PTHREAD_MUTEX_LOCK(&rdma_handle->cqe_err_cnt_mutex);
    if (rdma_handle->cqe_err_cnt == 0) {
        *num = 0;
        RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->cqe_err_cnt_mutex);
        return 0;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->cqe_err_cnt_mutex);

    RA_PTHREAD_MUTEX_LOCK(&rdma_handle->rdev_mutex);
    if (ra_list_empty(&rdma_handle->qp_list)) {
        *num = 0;
        RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);
        return 0;
    }

    // get & clear cqe err info from qp
    num_tmp = *num;
    RA_LIST_GET_HEAD_ENTRY(qp_hdc_tmp, qp_hdc_tmp1, &rdma_handle->qp_list, list, struct ra_qp_handle);
    for (; (&qp_hdc_tmp->list) != &rdma_handle->qp_list;
        qp_hdc_tmp = qp_hdc_tmp1, qp_hdc_tmp1 = list_entry(qp_hdc_tmp1->list.next, struct ra_qp_handle, list)) {
        RA_PTHREAD_MUTEX_LOCK(&qp_hdc_tmp->cqe_err_info.mutex);
        if (qp_hdc_tmp->cqe_err_info.info.status == 0) {
            RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc_tmp->cqe_err_info.mutex);
            continue;
        }
        info_list[cqe_err_idx].status = qp_hdc_tmp->cqe_err_info.info.status;
        info_list[cqe_err_idx].qpn = qp_hdc_tmp->cqe_err_info.info.qpn;
        info_list[cqe_err_idx].time = qp_hdc_tmp->cqe_err_info.info.time;
        qp_hdc_tmp->cqe_err_info.info.status = 0;
        RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc_tmp->cqe_err_info.mutex);

        RA_PTHREAD_MUTEX_LOCK(&rdma_handle->cqe_err_cnt_mutex);
        rdma_handle->cqe_err_cnt--;
        RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->cqe_err_cnt_mutex);
        cqe_err_idx++;
        if (cqe_err_idx >= num_tmp) {
            break;
        }
    }
    RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);

    *num = cqe_err_idx;
    return 0;
}

STATIC void ra_hdc_lite_save_cqe_err_info(struct ra_qp_handle *qp_hdc, unsigned int status)
{
    unsigned int phy_id = qp_hdc->phy_id;
    struct ra_cqe_err_info *err_info = &g_ra_cqe_err[phy_id];
    struct cqe_err_info *temp_info = &err_info->info;

    RA_PTHREAD_MUTEX_LOCK(&err_info->mutex);
    if (temp_info->status != 0) {
        hccp_run_info("over status=[0x%x], drop qpn[0x%x] err cqe status[0x%x]",
            temp_info->status, qp_hdc->qpn, status);
        RA_PTHREAD_MUTEX_UNLOCK(&err_info->mutex);
        return;
    }
    temp_info->status = status;
    temp_info->qpn = qp_hdc->qpn;
    (void)gettimeofday(&temp_info->time, NULL);
    RA_PTHREAD_MUTEX_UNLOCK(&err_info->mutex);
}

STATIC void ra_hdc_lite_save_qp_cqe_err_info(struct ra_qp_handle *qp_hdc, unsigned int status)
{
    RA_PTHREAD_MUTEX_LOCK(&qp_hdc->cqe_err_info.mutex);
    if (qp_hdc->cqe_err_info.info.status != 0) {
        RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc->cqe_err_info.mutex);
        return;
    }
    qp_hdc->cqe_err_info.info.status = status;
    qp_hdc->cqe_err_info.info.qpn = (uint32_t)qp_hdc->qpn;
    (void)gettimeofday(&qp_hdc->cqe_err_info.info.time, NULL);
    RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc->cqe_err_info.mutex);

    RA_PTHREAD_MUTEX_LOCK(&qp_hdc->rdma_handle->cqe_err_cnt_mutex);
    qp_hdc->rdma_handle->cqe_err_cnt++;
    RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc->rdma_handle->cqe_err_cnt_mutex);
    return;
}

int ra_hdc_lite_init_cqe_err_info(unsigned int phy_id)
{
    int ret;
    struct ra_cqe_err_info *err_info = &g_ra_cqe_err[phy_id];

    ret = pthread_mutex_init(&err_info->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("cqe err mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);

    (void)memset_s(&err_info->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));

    return 0;
}

void ra_hdc_lite_deinit_cqe_err_info(unsigned int phy_id)
{
    struct ra_cqe_err_info *err_info = &g_ra_cqe_err[phy_id];

    (void)pthread_mutex_destroy(&err_info->mutex);
}

STATIC void ra_retry_timeout_exception_check(struct ra_rdma_handle *rdma_handle, struct rdma_lite_wc *wc)
{
    int ret = 0;

    if (rdma_handle->sensor_handle == 0) {
        return;
    }

    if (wc->status != RDMA_LITE_WC_RETRY_EXC_ERR) {
        return;
    }

    /* The notification alarm framework does not filter alarms. In this example, only one notification
       alarm is reported by a single process, which does not need to be accurate. Therefore, no lock is used. */
    if (rdma_handle->sensor_update_cnt == 0) {
        ret = dl_hal_sensor_node_update_state(rdma_handle->logic_devid, rdma_handle->sensor_handle,
            RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE, GENERAL_EVENT_TYPE_ONE_TIME);
        if (ret == 0) {
            rdma_handle->sensor_update_cnt++;
        }
    }

    hccp_warn("update sensor state logic_devid(%u), qpn(%u), sensor_update_cnt(%d), ret(%d)\n",
        rdma_handle->logic_devid, wc->qp_num, rdma_handle->sensor_update_cnt, ret);
}

STATIC void ra_hdc_lite_period_poll_cqe(struct ra_rdma_handle *rdma_handle)
{
    int i;
    int ret = 0;
    struct rdma_lite_wc *lite_wc;
    unsigned int sent_wr, poll_cqe;
    struct ra_qp_handle *qp_hdc_tmp = NULL;
    struct ra_qp_handle *qp_hdc_tmp1 = NULL;

    RA_LIST_GET_HEAD_ENTRY(qp_hdc_tmp, qp_hdc_tmp1, &rdma_handle->qp_list, list, struct ra_qp_handle);
    for (; (&qp_hdc_tmp->list) != &rdma_handle->qp_list;
        qp_hdc_tmp = qp_hdc_tmp1, qp_hdc_tmp1 = list_entry(qp_hdc_tmp1->list.next, struct ra_qp_handle, list)) {
        sent_wr = qp_hdc_tmp->send_wr_num;
        poll_cqe = sent_wr - qp_hdc_tmp->poll_cqe_num;

        if (poll_cqe == 0) {
            continue;
        }

        lite_wc = calloc(poll_cqe, sizeof(struct rdma_lite_wc));
        if (lite_wc == NULL) {
            hccp_err("[create][ra_hdc_period_poll]lite_wc calloc failed phy_id(%u)", qp_hdc_tmp->phy_id);
            break;
        }

        ret = ra_rdma_lite_poll_cq(qp_hdc_tmp->send_lite_cq, poll_cqe, lite_wc);
        if (ret < 0) {
            hccp_err("ra_rdma_lite_poll_cq failed ret %d", ret);
            goto poll_cq_err;
        }

        for (i = 0; i < ret; i++) {
            if (lite_wc[i].status != RDMA_LITE_WC_SUCCESS && lite_wc[i].status != RDMA_LITE_WC_WR_FLUSH_ERR) {
                hccp_err(
                    "[create][ra_hdc_period_poll]failed CQE status[%u], wr[%llu]", lite_wc[i].status, lite_wc[i].wr_id);
                ra_hdc_lite_save_cqe_err_info(qp_hdc_tmp, lite_wc[i].status);
                ra_hdc_lite_save_qp_cqe_err_info(qp_hdc_tmp, lite_wc[i].status);
                ra_retry_timeout_exception_check(rdma_handle, &lite_wc[i]);
            }
        }

        qp_hdc_tmp->poll_cqe_num += (unsigned int)ret;

poll_cq_err:
        free(lite_wc);
        lite_wc = NULL;
    }
}

STATIC void *ra_hdc_lite_pthread(void *arg)
{
    struct ra_rdma_handle *rdma_handle = (struct ra_rdma_handle *)arg;
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;

    hccp_run_info("lite thread begin! thread_id:%lu, pid:%d, ppid:%d, phy_id:%u",
        pthread_self(), getpid(), getppid(), phy_id);
    CHK_PRT_RETURN(pthread_detach(pthread_self()), hccp_err("pthread_detach failed! thread_id:%lu, errno:%d, phy_id:%u",
        pthread_self(), errno, phy_id), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_hdc_lite");

    while (1) {
        if (rdma_handle->thread_status == LITE_THREAD_STATUS_DESTROY) {
            break;
        }
        RA_PTHREAD_MUTEX_LOCK(&rdma_handle->rdev_mutex);
        if (rdma_handle->thread_status == LITE_THREAD_STATUS_SUSPEND) {
            RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);
            usleep(THREAD_SLEEP_TIME);
            continue;
        }
        ra_hdc_lite_period_poll_cqe(rdma_handle);
        RA_PTHREAD_MUTEX_UNLOCK(&rdma_handle->rdev_mutex);
        usleep(RA_LITE_POLL_CQE_PERIOD_TIME);
    }

    // thread quit, change status to finish running status(2)
    rdma_handle->thread_status = LITE_THREAD_STATUS_FINISH_RUNNING;
    hccp_run_info("lite QUIT thread_id:%lu, pid:%d, phy_id:%u", pthread_self(), getpid(), phy_id);

    return NULL;
}

int ra_hdc_lite_poll_cq(struct ra_qp_handle *qp_hdc, bool is_send_cq, unsigned int num_entries,
    struct rdma_lite_wc_v2 *lite_wc)
{
    int ret = 0;
    struct rdma_lite_cq *cq = is_send_cq ? qp_hdc->lite_qp->send_cq : qp_hdc->lite_qp->recv_cq;
    unsigned int *poll_cqe_num = is_send_cq ? &qp_hdc->poll_cqe_num : &qp_hdc->poll_recv_cqe_num;
    unsigned int wr_num = is_send_cq ? qp_hdc->send_wr_num : qp_hdc->recv_wr_num;
    int i;

    // no need to poll
    if ((wr_num - *poll_cqe_num) == 0) {
        return 0;
    }

    ret = ra_rdma_lite_poll_cq_v2(cq, (int)num_entries, lite_wc);
    CHK_PRT_RETURN(ret < 0, hccp_err("ra_rdma_lite_poll_cq_v2 failed, ret %d", ret), ret);
    CHK_PRT_RETURN(ret > (int)num_entries,
        hccp_err("ra_rdma_lite_poll_cq_v2 failed, expect maximum num_entries:%u but got %d", num_entries, ret), -EIO);

    for (i = 0; i < ret; i++) {
        ra_retry_timeout_exception_check(qp_hdc->rdma_handle, &lite_wc[i].wc);
    }

    *poll_cqe_num += (unsigned int)ret;
    return ret;
}

STATIC int ra_hdc_lite_post_send(struct ra_qp_handle *qp_hdc, struct lite_mr_info *local_mr,
    struct lite_mr_info *rem_mr, struct lite_send_wr *wr, struct send_wr_rsp *wr_rsp, u64 wr_id)
{
    int i;
    int ret;
    struct rdma_lite_sge list[RA_SGLIST_MAX];
    struct rdma_lite_send_wr lite_wr = {
        .sg_list    = list,
        .opcode     = wr->wr.op,
        .send_flags = wr->wr.send_flag,
    };
    struct rdma_lite_send_wr *bad_wr = NULL;
    struct rdma_lite_post_send_resp resp = { 0 };
    struct rdma_lite_post_send_attr attr = { 0 };

    for (i = 0; i < wr->wr.buf_num && i < RA_SGLIST_MAX; i++) {
        list[i].addr = (uintptr_t)wr->wr.buf_list[i].addr;
        list[i].length = wr->wr.buf_list[i].len;
        list[i].lkey = local_mr->key;
    }

    if (lite_wr.opcode == RDMA_LITE_WR_WRITE_WITH_NOTIFY ||
        lite_wr.opcode == RDMA_LITE_WR_REDUCE_WRITE ||
        lite_wr.opcode == RDMA_LITE_WR_REDUCE_WRITE_NOTIFY) {
        lite_wr.imm_data = htobe32((wr->aux.notify_offset & WRITE_NOTIFY_OFFSET_MASK) |
            WRITE_NOTIFY_VALUE_RECORD);
        attr.reduce_op = wr->aux.reduce_type;
        attr.reduce_type = wr->aux.data_type;
    }

    if (lite_wr.opcode == RDMA_LITE_WR_RDMA_WRITE_WITH_IMM ||
        lite_wr.opcode == RDMA_LITE_WR_SEND_WITH_IMM ||
        lite_wr.opcode == RDMA_LITE_WR_ATOMIC_WRITE) {
        lite_wr.imm_data = htobe32(wr->ext.imm_data);
    }

    lite_wr.num_sge = i;
    lite_wr.wr_id = wr_id;
    // send op has no rem_mr, no need to assign
    if (wr->wr.op != RA_WR_SEND && wr->wr.op != RA_WR_SEND_WITH_IMM) {
        lite_wr.rkey = rem_mr->key;
        lite_wr.remote_addr = wr->wr.dst_addr;
    }

    ret = ra_rdma_lite_post_send(qp_hdc->lite_qp, &lite_wr, &bad_wr, &attr, &resp);
    if (ret) {
        return ret;
    }

    wr_rsp->db.db_index = (unsigned int)qp_hdc->db_index;
    wr_rsp->db.db_info = resp.db.lite_db_info;

    return 0;
}

static int ra_hdc_lite_get_mr(struct ra_qp_handle *qp_hdc, unsigned long long addr, struct lite_mr_info **mr,
    struct lite_mr_info *src_mr, unsigned int mr_num)
{
    unsigned int i;

    RA_PTHREAD_MUTEX_LOCK(&qp_hdc->qp_mutex);

    for (i = 0; i < mr_num; i++) {
        if ((src_mr[i].addr <= addr) && (addr < src_mr[i].addr + src_mr[i].len)) {
            *mr = &src_mr[i];
            RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc->qp_mutex);
            return 0;
        }
    }

    RA_PTHREAD_MUTEX_UNLOCK(&qp_hdc->qp_mutex);

    return -EINVAL;
}

STATIC int ra_hdc_lite_handle_bp(struct ra_qp_handle *qp_hdc)
{
    u32 send_wr;

    if (qp_hdc->send_wr_num >= qp_hdc->poll_cqe_num) {
        send_wr = qp_hdc->send_wr_num - qp_hdc->poll_cqe_num;
    } else {
        send_wr = qp_hdc->send_wr_num + (0xFFFFFFFF - qp_hdc->poll_cqe_num);
    }

    /*
     * Due to driver limitations, the software pointer updates before the hardware pointer.
     * The software must reserve sq_depth(2^x - 2) to prevent the backpressure mechanism from failing.
     */
    if (send_wr < (qp_hdc->sq_depth - 2U)) {
        if (qp_hdc->bp_cnt != 0) {
            hccp_run_info("qpn:%u send_wr_num:%u poll_cqe_num:%u send_wr:%u sq_depth:%u "
                "bp_cnt:%u, back pressure relieved",
                qp_hdc->qpn, qp_hdc->send_wr_num, qp_hdc->poll_cqe_num, send_wr, qp_hdc->sq_depth,
                qp_hdc->bp_cnt);
            qp_hdc->bp_cnt = 0;
        }
        return 0;
    }

    // first time back pressure occurred
    if (qp_hdc->bp_cnt == 0) {
        hccp_run_warn("qpn:%u send_wr_num:%u poll_cqe_num:%u send_wr:%u sq_depth:%u, back pressure occurred",
            qp_hdc->qpn, qp_hdc->send_wr_num, qp_hdc->poll_cqe_num, send_wr, qp_hdc->sq_depth);
    } else {
        hccp_warn("qpn:%u send_wr_num:%u poll_cqe_num:%u send_wr:%u sq_depth:%u, back pressure continues bp_cnt:%u",
            qp_hdc->qpn, qp_hdc->send_wr_num, qp_hdc->poll_cqe_num, send_wr, qp_hdc->sq_depth, qp_hdc->bp_cnt);
    }

    qp_hdc->bp_cnt++;
    return -ENOMEM;
}

int ra_hdc_lite_typical_send_wr(struct ra_qp_handle *qp_hdc, struct lite_send_wr *wr, struct send_wr_rsp *op_rsp,
    unsigned long long wr_id)
{
    struct rdma_lite_post_send_resp resp = { 0 };
    struct rdma_lite_post_send_attr attr = { 0 };
    struct rdma_lite_sge list[RA_SGLIST_MAX];
    struct rdma_lite_send_wr lite_wr = {
        .sg_list    = list,
        .opcode     = wr->wr.op,
        .send_flags = wr->wr.send_flag,
    };
    struct rdma_lite_send_wr *bad_wr = NULL;
    int ret;
    int i;

    ret = ra_hdc_lite_handle_bp(qp_hdc);
    if (ret != 0) {
        return ret;
    }

    for (i = 0; i < wr->wr.buf_num && i < RA_SGLIST_MAX; i++) {
        list[i].addr = (uintptr_t)wr->wr.buf_list[i].addr;
        list[i].length = wr->wr.buf_list[i].len;
        list[i].lkey = wr->wr.buf_list[i].lkey;
    }

    lite_wr.num_sge = i;
    lite_wr.wr_id = wr_id;
    lite_wr.rkey = wr->wr.rkey;
    lite_wr.remote_addr = wr->wr.dst_addr;
    if (lite_wr.opcode == RDMA_LITE_WR_WRITE_WITH_NOTIFY ||
        lite_wr.opcode == RDMA_LITE_WR_REDUCE_WRITE ||
        lite_wr.opcode == RDMA_LITE_WR_REDUCE_WRITE_NOTIFY) {
        lite_wr.imm_data = htobe32((wr->aux.notify_offset & WRITE_NOTIFY_OFFSET_MASK) |
            WRITE_NOTIFY_VALUE_RECORD);
        attr.reduce_op = wr->aux.reduce_type;
        attr.reduce_type = wr->aux.data_type;
    }
    lite_wr.imm_data = htobe32(wr->ext.imm_data);

    ret = ra_rdma_lite_post_send(qp_hdc->lite_qp, &lite_wr, &bad_wr, &attr, &resp);
    if (ret) {
        if (ret == -ENOMEM) {
            hccp_warn("[send][ra_hdc_wr]ra hdc post send unsuccessful, ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
        } else {
            hccp_err("[send][ra_hdc_wr]ra hdc post send failed ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
        }

        return ret;
    }

    op_rsp->db.db_index = (unsigned int)qp_hdc->db_index;
    op_rsp->db.db_info = resp.db.lite_db_info;

    // user specify wr send_signal flag or user specify qp sq_sig_all flag
    if ((((uint32_t)wr->wr.send_flag & RA_SEND_SIGNALED) != 0) || (qp_hdc->sq_sig_all != 0)) {
        qp_hdc->send_wr_num++;
    }

    return 0;
}

int ra_hdc_lite_send_wr(struct ra_qp_handle *qp_hdc, struct lite_send_wr *wr, struct send_wr_rsp *op_rsp,
    unsigned long long wr_id)
{
    struct lite_mr_info *local_mr = NULL;
    struct lite_mr_info *rem_mr = NULL;
    int ret;

    ret = ra_hdc_lite_get_mr(qp_hdc, wr->wr.buf_list[0].addr, &local_mr, qp_hdc->local_mr, RA_MR_MAX_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_wr]ra hdc get local_mr failed ret(%d) phy_id(%u)",
        ret, qp_hdc->phy_id), ret);

    // send op no need to check & get remote mr
    if (wr->wr.op != RA_WR_SEND && wr->wr.op != RA_WR_SEND_WITH_IMM) {
        ret = ra_hdc_lite_get_mr(qp_hdc, wr->wr.dst_addr, &rem_mr, qp_hdc->rem_mr, RA_MR_MAX_NUM);
        CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_wr]ra hdc get rem_mr failed ret(%d) phy_id(%u)",
            ret, qp_hdc->phy_id), ret);
    }

    ret = ra_hdc_lite_handle_bp(qp_hdc);
    if (ret != 0) {
        return ret;
    }

    ret = ra_hdc_lite_post_send(qp_hdc, local_mr, rem_mr, wr, op_rsp, wr_id);
    if (ret) {
        if (ret == -ENOMEM) {
            hccp_warn("[send][ra_hdc_wr]ra hdc post send unsuccessful, ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
        } else {
            hccp_err("[send][ra_hdc_wr]ra hdc post send failed, ret(%d) phy_id(%u)", ret, qp_hdc->phy_id);
        }

        return ret;
    }

    // user specify wr send_signal flag or user specify qp sq_sig_all flag
    if ((((uint32_t)wr->wr.send_flag & RA_SEND_SIGNALED) != 0) || (qp_hdc->sq_sig_all != 0)) {
        qp_hdc->send_wr_num++;
    }

    return 0;
}

int ra_hdc_lite_send_wrlist(struct ra_qp_handle *qp_hdc, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    int ret;
    unsigned int i = 0;
    struct lite_send_wr normal_wr = { 0 };

    while (i < wrlist_num.send_num) {
        normal_wr.wr.buf_list = &(wr[i].mem_list);
        normal_wr.wr.buf_num = 1;
        normal_wr.wr.dst_addr = wr[i].dst_addr;
        normal_wr.wr.op = wr[i].op;
        normal_wr.wr.send_flag = wr[i].send_flags;
        ret = ra_hdc_lite_send_wr(qp_hdc, &normal_wr, &op_rsp[i], HDC_LITE_DEFAULT_WR_ID);
        if (ret) {
            if (ret == -ENOMEM) {
                hccp_warn("[send][ra_hdc_lite_wrlist]ra_hdc_lite_send_wr unsuccessful, ret(%d) phy_id(%u) "
                    "send_index(%u)", ret, qp_hdc->phy_id, i);
            } else {
                hccp_err("[send][ra_hdc_lite_wrlist]ra_hdc_lite_send_wr failed, ret(%d) phy_id(%u) send_index(%u)",
                    ret, qp_hdc->phy_id, i);
            }

            *(wrlist_num.complete_num) = i;
            return ret;
        }

        i++;
    }

    *(wrlist_num.complete_num) = i;

    return 0;
}

int ra_hdc_lite_send_wrlist_ext(struct ra_qp_handle *qp_hdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp op_rsp[], struct wrlist_send_complete_num wrlist_num)
{
    int ret;
    unsigned int i = 0;
    struct lite_send_wr normal_wr = { 0 };

    while (i < wrlist_num.send_num) {
        normal_wr.wr.buf_list = &(wr[i].mem_list);
        normal_wr.wr.buf_num = 1;
        normal_wr.wr.dst_addr = wr[i].dst_addr;
        normal_wr.wr.op = wr[i].op;
        normal_wr.wr.send_flag = wr[i].send_flags;
        normal_wr.aux = wr[i].aux;
        normal_wr.ext = wr[i].ext;
        ret = ra_hdc_lite_send_wr(qp_hdc, &normal_wr, &op_rsp[i], HDC_LITE_DEFAULT_WR_ID);
        if (ret) {
            if (ret == -ENOMEM) {
                hccp_warn("[send][ra_hdc_lite_send_wrlist_ext]ra_hdc_lite_send_wr unsuccessful, ret(%d) phy_id(%u) "
                    "send_index(%u)", ret, qp_hdc->phy_id, i);
            } else {
                hccp_err("[send][ra_hdc_lite_send_wrlist_ext]ra_hdc_lite_send_wr failed, ret(%d) phy_id(%u) "
                    "send_index(%u)", ret, qp_hdc->phy_id, i);
            }

            *(wrlist_num.complete_num) = i;
            return ret;
        }

        i++;
    }

    *(wrlist_num.complete_num) = i;

    return 0;
}

int ra_hdc_lite_send_normal_wrlist(struct ra_qp_handle *qp_hdc, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    struct lite_send_wr normal_wr = { 0 };
    unsigned int i = 0;
    int ret = 0;

    while (i < wrlist_num.send_num) {
        normal_wr.wr.send_flag = wr[i].send_flags;
        normal_wr.wr.rkey = wr[i].rkey;
        normal_wr.wr.op = wr[i].op;
        normal_wr.wr.dst_addr = wr[i].dst_addr;
        normal_wr.wr.buf_list = &(wr[i].mem_list);
        normal_wr.wr.buf_num = 1;
        normal_wr.aux = wr[i].aux;
        if (wr[i].op == RDMA_LITE_WR_RDMA_WRITE_WITH_IMM || wr[i].op == RDMA_LITE_WR_SEND_WITH_IMM ||
            wr[i].op == RDMA_LITE_WR_ATOMIC_WRITE) {
            normal_wr.ext.imm_data = wr[i].imm_data;
        }
        ret = ra_hdc_lite_typical_send_wr(qp_hdc, &normal_wr, &op_rsp[i], wr[i].wr_id);
        if (ret != 0) {
            if (ret == -ENOMEM) {
                hccp_warn("[send][send_wrlist]ra_hdc_lite_send_wr unsuccessful, ret(%d) phy_id(%u) send_index(%u)",
                    ret, qp_hdc->phy_id, i);
            } else {
                hccp_err("[send][send_wrlist]ra_hdc_lite_send_wr failed, ret(%d) phy_id(%u) send_index(%u)",
                    ret, qp_hdc->phy_id, i);
            }

            break;
        }

        i++;
    }

    *(wrlist_num.complete_num) = i;

    return ret;
}

STATIC void ra_hdc_lite_build_recv_wr(struct recv_wrlist_data *wr, struct rdma_lite_sge *list,
    struct rdma_lite_recv_wr *lite_wr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey = wr->mem_list.lkey;

    lite_wr->sg_list = list;
    lite_wr->wr_id = wr->wr_id;
    lite_wr->num_sge = 1; /* only support one sge */
}

int ra_hdc_lite_recv_wrlist(struct ra_qp_handle *qp_hdc, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num)
{
    struct rdma_lite_recv_wr *lite_wr = NULL;
    struct rdma_lite_recv_wr *bad_wr = NULL;
    struct rdma_lite_sge *list = NULL;
    unsigned int index;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(recv_num == 0, hccp_err("lite recv_num[%u] is invalid!", recv_num), -EINVAL);

    lite_wr = (struct rdma_lite_recv_wr *)calloc(recv_num, sizeof(struct rdma_lite_recv_wr));
    CHK_PRT_RETURN(lite_wr == NULL, hccp_err("lite calloc lite_wr failed!"), -ENOSPC);

    list = (struct rdma_lite_sge *)calloc(recv_num, sizeof(struct rdma_lite_sge));
    if (list == NULL) {
        hccp_err("lite calloc list failed!");
        ret = -ENOSPC;
        goto alloc_sge_fail;
    }

    // build up recv lite wr
    for (i = 0; i < recv_num; i++) {
        ra_hdc_lite_build_recv_wr(&wr[i], &list[i], &lite_wr[i]);
        index = i + 1;
        lite_wr[i].next = (i < recv_num - 1) ? &(lite_wr[index]) : NULL;
    }

    ret = ra_rdma_lite_post_recv(qp_hdc->lite_qp, lite_wr, &bad_wr);
    if (ret == 0) {
        *complete_num = recv_num;
    } else if (ret == -ENOMEM) {
        *complete_num = (unsigned int)((void *)bad_wr - (void *)lite_wr) / sizeof(struct rdma_lite_recv_wr);
        hccp_dbg("ra_rdma_lite_post_recv wqe overflow, complete_num[%d]", *complete_num);
    } else {
        *complete_num = 0;
        hccp_err("ra_rdma_lite_post_recv failed, ret[%d]", ret);
    }

    qp_hdc->recv_wr_num += *complete_num;

    free(list);
    list = NULL;

alloc_sge_fail:
    free(lite_wr);
    lite_wr = NULL;
    return (ret == -ENOMEM) ? 0 : ret;
}
