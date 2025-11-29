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
#include <errno.h>
#include "securec.h"
#include "hccp_ping.h"
#include "ra.h"
#include "ra_client_host.h"
#include "user_log.h"
#include "ra_hdc_ping.h"
#include "ra_rs_comm.h"
#include "ra_ping.h"
#include "ra_rs_err.h"

struct ra_ping_ops gRaHdcPingOps = {
    .ra_ping_init = RaHdcPingInit,
    .ra_ping_target_add = RaHdcPingTargetAdd,
    .ra_ping_task_start = RaHdcPingTaskStart,
    .ra_ping_get_results = RaHdcPingGetResults,
    .ra_ping_target_del = RaHdcPingTargetDel,
    .ra_ping_task_stop = RaHdcPingTaskStop,
    .ra_ping_deinit = RaHdcPingDeinit,
};

STATIC int RaUdevInitCheck(unsigned int phyId, void *pingHandle)
{
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[check][ra_ping_init]phy_id(%u) is invalid! "
        "it must greater or equal to 0 and less than %d!", phyId, RA_MAX_PHY_ID_NUM), -EINVAL);
    CHK_PRT_RETURN(pingHandle == NULL, hccp_err("[check][ra_ping_init]phy_id(%u) ping_handle is null!", phyId),
        -EINVAL);
    return 0;
}

STATIC int RaPingInitGetHandle(struct ping_init_attr *initAttr, struct ping_init_info *initInfo,
    struct ra_ping_handle *pingHandle)
{
    char localIp[MAX_IP_LEN] = {0};
    int ret;

    CHK_PRT_RETURN(initAttr == NULL || initInfo == NULL,
        hccp_err("[init][ra_ping]init_attr or init_info is NULL"), -EINVAL);
    CHK_PRT_RETURN(initAttr->mode != NETWORK_OFFLINE,
        hccp_err("[init][ra_ping]mode:%d do not support", initAttr->mode), -EINVAL);

    if (initAttr->protocol == PROTOCOL_RDMA) {
        CHK_PRT_RETURN(initAttr->comm_info.rdma.udp_sport > MAX_PORT_NUM,
            hccp_err("[init][ra_ping]udp_sport %u invalid", initAttr->comm_info.rdma.udp_sport), -EINVAL);
        ret = RaRdevInitCheck(initAttr->mode, initAttr->dev.rdma, localIp, MAX_IP_LEN, pingHandle);
        CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_ping]ra_rdev_init_check failed, ret(%d)", ret), -EINVAL);

        pingHandle->phy_id = initAttr->dev.rdma.phy_id;
        hccp_run_info("Input parameters: phy_id[%u], nic_position[%d] family[%d] ip[%s] buffer_size[0x%x]",
            initAttr->dev.rdma.phy_id, initAttr->mode, initAttr->dev.rdma.family, localIp, initAttr->buffer_size);
    } else {
        hccp_err("[init][ra_ping]protocol:%d do not support", initAttr->protocol);
        return -ENOTSUPP;
    }
    pingHandle->protocol = initAttr->protocol;

    pingHandle->ping_ops = &gRaHdcPingOps;
    CHK_PRT_RETURN(pingHandle->ping_ops->ra_ping_init == NULL, hccp_err("[init][ra_ping]ra_ping_init is NULL"),
        -EINVAL);
    CHK_PRT_RETURN(initAttr->buffer_size == 0 || initAttr->buffer_size % RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE != 0,
        hccp_err("[init][ra_ping]init_attr->buffer_size:0x%x not 0x%xB aligned", initAttr->buffer_size,
        RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE), -EINVAL);
    pingHandle->buffer_size = initAttr->buffer_size;

    ret = pthread_mutex_init(&pingHandle->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_ping]init mutext failed, ret:%d", ret), ret);

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaPingInit(struct ping_init_attr *initAttr, struct ping_init_info *initInfo,
    void **pingHandle)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    int ret;

    CHK_PRT_RETURN(pingHandle == NULL, hccp_err("[init][ra_ping]ping_handle is NULL"), -EINVAL);

    pingHandleTmp = calloc(1, sizeof(struct ra_ping_handle));
    CHK_PRT_RETURN(pingHandleTmp == NULL, hccp_err("[init][ra_ping]calloc for ping_handle failed"),
        ConverReturnCode(HCCP_INIT, -ENOMEM));

    ret = RaPingInitGetHandle(initAttr, initInfo, pingHandleTmp);
    if (ret != 0) {
        hccp_err("[init][ra_ping]ra_ping_init_get_handle failed, ret(%d)", ret);
        goto err;
    }

    ret = pingHandleTmp->ping_ops->ra_ping_init(pingHandleTmp, initAttr, initInfo);
    if (ret != 0) {
        (void)pthread_mutex_destroy(&pingHandleTmp->mutex);
        goto err;
    }

    *pingHandle = (void *)pingHandleTmp;

    return 0;

err:
    free(pingHandleTmp);
    pingHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaPingTargetAdd(void *pingHandle, struct ping_target_info target[], uint32_t num)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(pingHandle == NULL || target == NULL || num == 0, hccp_err("[add][ra_ping]ping_handle or target "
        "is NULL, or num is 0"), ConverReturnCode(RDMA_OP, -EINVAL));

    pingHandleTmp = (struct ra_ping_handle *)pingHandle;
    CHK_PRT_RETURN(pingHandleTmp->ping_ops == NULL || pingHandleTmp->ping_ops->ra_ping_target_add == NULL,
        hccp_err("[add][ra_ping]ping_ops or ra_ping_target_add is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = pingHandleTmp->phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[add][ra_ping]phy_id(%u) must less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
    // disallow add target when task is running
    if (pingHandleTmp->task_cnt != 0) {
        hccp_err("[add][ra_ping]task_cnt:%u != 0 invalid, task already running", pingHandleTmp->task_cnt);
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, -EEXIST);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);

    ret = pingHandleTmp->ping_ops->ra_ping_target_add(pingHandleTmp, target, num);
    if (ret != 0) {
        return ConverReturnCode(RDMA_OP, ret);
    }

    // increase target cnt
    RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
    pingHandleTmp->target_cnt += num;
    RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaPingTaskStart(void *pingHandle, struct ping_task_attr *attr)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(pingHandle == NULL || attr == NULL, hccp_err("[start][ra_ping]ping_handle is NULL or attr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attr->packet_cnt == 0 || attr->packet_interval == 0 || attr->timeout_interval == 0,
        hccp_err("[start][ra_ping]packet_cnt:%u or packet_interval:%u or timeout_interval:%u is 0", attr->packet_cnt,
        attr->packet_interval, attr->timeout_interval), ConverReturnCode(RDMA_OP, -EINVAL));

    pingHandleTmp = (struct ra_ping_handle *)pingHandle;
    CHK_PRT_RETURN(pingHandleTmp->ping_ops == NULL || pingHandleTmp->ping_ops->ra_ping_task_start == NULL,
        hccp_err("[start][ra_ping]ping_ops is NULL or ping_ops->ra_ping_task_start is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = pingHandleTmp->phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[start][ra_ping]phy_id(%u) must less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
    // disallow multi task running or no target to start
    if (pingHandleTmp->task_cnt != 0) {
        hccp_warn("[start][ra_ping]task_cnt:%u != 0 invalid, task already running", pingHandleTmp->task_cnt);
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, -EEXIST);
    }
    if (pingHandleTmp->target_cnt == 0) {
        hccp_warn("[start][ra_ping]target_cnt is 0 invalid, no target exist");
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, -ENODEV);
    }

    // increase task cnt to avoid lock contention, trigger task running
    pingHandleTmp->task_cnt++;
    RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);

    hccp_run_info("Input parameters: phy_id[%u], packet_cnt[%u] interval[%u] timeout_interval[%u], target_cnt[%u]",
        phyId, attr->packet_cnt, attr->packet_interval, attr->timeout_interval, pingHandleTmp->target_cnt);

    ret = pingHandleTmp->ping_ops->ra_ping_task_start(pingHandleTmp, attr);
    if (ret != 0) {
        // trigger failed, decrease task cnt
        hccp_err("[start][ra_ping]ping_task_start failed, ret(%d)", ret);
        RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
        pingHandleTmp->task_cnt--;
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaPingGetResults(void *pingHandle, struct ping_target_result target[], uint32_t *num)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    unsigned int phyId;
    int ret;

    if (pingHandle == NULL || target == NULL || num == NULL || *num == 0) {
        hccp_err("[get][ra_ping]ping_handle is NULL or target is NULL or num is NULL or *num is 0");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    pingHandleTmp = (struct ra_ping_handle *)pingHandle;
    if (pingHandleTmp->ping_ops == NULL || pingHandleTmp->ping_ops->ra_ping_get_results == NULL) {
        hccp_err("[get][ra_ping]ping_ops is NULL or ping_ops->ra_ping_get_results is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    phyId = pingHandleTmp->phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[get][ra_ping]phy_id(%u) must less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    // num invalid, bigger than target exist
    RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
    if (pingHandleTmp->target_cnt < *num) {
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        hccp_err("[get][ra_ping]target_cnt:%u < num:%u, invalid", pingHandleTmp->target_cnt, *num);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);

    ret = pingHandleTmp->ping_ops->ra_ping_get_results(pingHandleTmp, target, num);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaPingTargetDel(void *pingHandle, struct ping_target_comm_info target[], uint32_t num)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    unsigned int phyId;
    int ret;

    if (pingHandle == NULL || target == NULL || num == 0) {
        hccp_err("[del][ra_ping]ping_handle is NULL or target is NULL or num:%u is 0", num);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    pingHandleTmp = (struct ra_ping_handle *)pingHandle;
    if (pingHandleTmp->ping_ops == NULL || pingHandleTmp->ping_ops->ra_ping_target_del == NULL) {
        hccp_err("[del][ra_ping]ping_ops is NULL or ping_ops->ra_ping_target_del is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
    // disallow del target when task is running
    if (pingHandleTmp->task_cnt != 0) {
        hccp_err("[del][ra_ping]task_cnt:%u != 0 invalid, task already running", pingHandleTmp->task_cnt);
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, -EEXIST);
    }
    // num invalid, bigger than target exist
    if (pingHandleTmp->target_cnt < num) {
        hccp_err("[del][ra_ping]target_cnt:%u < num:%u, invalid", pingHandleTmp->target_cnt, num);
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }
    // decrease target cnt to avoid lock contention, trigger target del
    pingHandleTmp->target_cnt -= num;
    RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);

    phyId = pingHandleTmp->phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[del][ra_ping]phy_id(%u) must less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = pingHandleTmp->ping_ops->ra_ping_target_del(pingHandleTmp, target, num);
    if (ret != 0) {
        // trigger del failed, increase target cnt
        RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
        pingHandleTmp->target_cnt += num;
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaPingTaskStop(void *pingHandle)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    unsigned int phyId;
    int ret;

    if (pingHandle == NULL) {
        hccp_err("[stop][ra_ping]ping_handle is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    pingHandleTmp = (struct ra_ping_handle *)pingHandle;
    if (pingHandleTmp->ping_ops == NULL || pingHandleTmp->ping_ops->ra_ping_task_stop == NULL) {
        hccp_err("[stop][ra_ping]ping_ops is NULL or ping_ops->ra_ping_task_stop is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    phyId = pingHandleTmp->phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[stop][ra_ping]phy_id(%u) must less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    // no task to stop
    RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
    if (pingHandleTmp->task_cnt == 0) {
        hccp_warn("[stop][ra_ping]task_cnt is 0 invalid, no task running");
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, -ENODEV);
    }
    // decrease task cnt to avoid lock contention, trigger task stop
    pingHandleTmp->task_cnt--;
    RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);

    hccp_run_info("Input parameters: phy_id[%u], target_cnt[%u]", phyId, pingHandleTmp->target_cnt);

    ret = pingHandleTmp->ping_ops->ra_ping_task_stop(pingHandleTmp);
    if (ret != 0) {
        // trigger failed, increase task cnt
        RA_PTHREAD_MUTEX_LOCK(&pingHandleTmp->mutex);
        pingHandleTmp->task_cnt++;
        RA_PTHREAD_MUTEX_UNLOCK(&pingHandleTmp->mutex);
        return ConverReturnCode(RDMA_OP, ret);
    }

    return 0;
}

STATIC int RaPingDeinitParaCheck(struct ra_ping_handle *pingHandle)
{
    char localIp[MAX_IP_LEN] = {0};
    union ping_dev devInfo;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(pingHandle->ping_ops == NULL || pingHandle->ping_ops->ra_ping_deinit == NULL,
        hccp_err("[deinit][ra_ping]ping_ops is NULL or ra_ping_deinit is NULL"), -EINVAL);

    phyId = pingHandle->phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[deinit][ra_ping]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM), -EINVAL);

    devInfo = pingHandle->dev;
    if (pingHandle->protocol == PROTOCOL_RDMA) {
        ret = RaInetPton(devInfo.rdma.family, devInfo.rdma.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_ping]ra_inet_pton for local_ip failed, ret(%d)", ret), ret);
        hccp_run_info("Input parameters: phy_id[%u] dev_index[%u] family[%d] local_ip[%s] target_cnt[%u] task_cnt[%u]",
            phyId, pingHandle->dev_index, devInfo.rdma.family, localIp, pingHandle->target_cnt,
            pingHandle->task_cnt);
    } else {
        hccp_err("[deinit][ra_ping]protocol:%d do not support", pingHandle->protocol);
        return -ENOTSUPP;
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaPingDeinit(void *pingHandle)
{
    struct ra_ping_handle *pingHandleTmp = NULL;
    int ret;

    if (pingHandle == NULL) {
        hccp_err("[deinit][ra_ping]ping_handle is NULL");
        return ConverReturnCode(HCCP_INIT, -EINVAL);
    }
    pingHandleTmp = (struct ra_ping_handle *)pingHandle;

    ret = RaPingDeinitParaCheck(pingHandleTmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_ping]ra_ping_deinit_para_check failed, ret(%d)", ret);
        return ConverReturnCode(HCCP_INIT, ret);
    }

    ret = pingHandleTmp->ping_ops->ra_ping_deinit(pingHandleTmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_ping]ra_ping_deinit failed, ret(%d)", ret);
    }

    pingHandleTmp->ping_ops = NULL;
    (void)pthread_mutex_destroy(&pingHandleTmp->mutex);
    free(pingHandleTmp);
    pingHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}
