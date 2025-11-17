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

struct ra_ping_ops g_ra_hdc_ping_ops = {
    .ra_ping_init = ra_hdc_ping_init,
    .ra_ping_target_add = ra_hdc_ping_target_add,
    .ra_ping_task_start = ra_hdc_ping_task_start,
    .ra_ping_get_results = ra_hdc_ping_get_results,
    .ra_ping_target_del = ra_hdc_ping_target_del,
    .ra_ping_task_stop = ra_hdc_ping_task_stop,
    .ra_ping_deinit = ra_hdc_ping_deinit,
};

STATIC int ra_udev_init_check(unsigned int phy_id, void *ping_handle)
{
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[check][ra_ping_init]phy_id(%u) is invalid! "
        "it must greater or equal to 0 and less than %d!", phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);
    CHK_PRT_RETURN(ping_handle == NULL, hccp_err("[check][ra_ping_init]phy_id(%u) ping_handle is null!", phy_id),
        -EINVAL);
    return 0;
}

STATIC int ra_ping_init_get_handle(struct ping_init_attr *init_attr, struct ping_init_info *init_info,
    struct ra_ping_handle *ping_handle)
{
    char local_ip[MAX_IP_LEN] = {0};
    int ret;

    CHK_PRT_RETURN(init_attr == NULL || init_info == NULL,
        hccp_err("[init][ra_ping]init_attr or init_info is NULL"), -EINVAL);
    CHK_PRT_RETURN(init_attr->mode != NETWORK_OFFLINE,
        hccp_err("[init][ra_ping]mode:%d do not support", init_attr->mode), -EINVAL);

    if (init_attr->protocol == PROTOCOL_RDMA) {
        CHK_PRT_RETURN(init_attr->comm_info.rdma.udp_sport > MAX_PORT_NUM,
            hccp_err("[init][ra_ping]udp_sport %u invalid", init_attr->comm_info.rdma.udp_sport), -EINVAL);
        ret = ra_rdev_init_check(init_attr->mode, init_attr->dev.rdma, local_ip, MAX_IP_LEN, ping_handle);
        CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_ping]ra_rdev_init_check failed, ret(%d)", ret), -EINVAL);

        ping_handle->phy_id = init_attr->dev.rdma.phy_id;
        hccp_run_info("Input parameters: phy_id[%u], nic_position[%d] family[%d] ip[%s] buffer_size[0x%x]",
            init_attr->dev.rdma.phy_id, init_attr->mode, init_attr->dev.rdma.family, local_ip, init_attr->buffer_size);
    } else if (init_attr->protocol == PROTOCOL_UDMA) {
        ret = ra_udev_init_check(init_attr->ub.phy_id, ping_handle);
        CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_ping]ra_ub_dev_init_check failed, ret(%d)", ret), -EINVAL);

        ping_handle->phy_id = init_attr->ub.phy_id;
        hccp_run_info("Input parameters: phy_id[%u], nic_position[%d], eid_index[%u] buffer_size[0x%x]",
            init_attr->ub.phy_id, init_attr->mode, init_attr->dev.ub.eid_index, init_attr->buffer_size);
    } else {
        hccp_err("[init][ra_ping]protocol:%d do not support", init_attr->protocol);
        return -ENOTSUPP;
    }
    ping_handle->protocol = init_attr->protocol;

    ping_handle->ping_ops = &g_ra_hdc_ping_ops;
    CHK_PRT_RETURN(ping_handle->ping_ops->ra_ping_init == NULL, hccp_err("[init][ra_ping]ra_ping_init is NULL"),
        -EINVAL);
    CHK_PRT_RETURN(init_attr->buffer_size == 0 || init_attr->buffer_size % RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE != 0,
        hccp_err("[init][ra_ping]init_attr->buffer_size:0x%x not 0x%xB aligned", init_attr->buffer_size,
        RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE), -EINVAL);
    ping_handle->buffer_size = init_attr->buffer_size;

    ret = pthread_mutex_init(&ping_handle->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_ping]init mutext failed, ret:%d", ret), ret);

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ping_init(struct ping_init_attr *init_attr, struct ping_init_info *init_info,
    void **ping_handle)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ping_handle == NULL, hccp_err("[init][ra_ping]ping_handle is NULL"), -EINVAL);

    ping_handle_tmp = calloc(1, sizeof(struct ra_ping_handle));
    CHK_PRT_RETURN(ping_handle_tmp == NULL, hccp_err("[init][ra_ping]calloc for ping_handle failed"),
        conver_return_code(HCCP_INIT, -ENOMEM));

    ret = ra_ping_init_get_handle(init_attr, init_info, ping_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_ping]ra_ping_init_get_handle failed, ret(%d)", ret);
        goto err;
    }

    ret = ping_handle_tmp->ping_ops->ra_ping_init(ping_handle_tmp, init_attr, init_info);
    if (ret != 0) {
        (void)pthread_mutex_destroy(&ping_handle_tmp->mutex);
        goto err;
    }

    *ping_handle = (void *)ping_handle_tmp;

    return 0;

err:
    free(ping_handle_tmp);
    ping_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ping_target_add(void *ping_handle, struct ping_target_info target[], uint32_t num)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(ping_handle == NULL || target == NULL || num == 0, hccp_err("[add][ra_ping]ping_handle or target "
        "is NULL, or num is 0"), conver_return_code(RDMA_OP, -EINVAL));

    ping_handle_tmp = (struct ra_ping_handle *)ping_handle;
    CHK_PRT_RETURN(ping_handle_tmp->ping_ops == NULL || ping_handle_tmp->ping_ops->ra_ping_target_add == NULL,
        hccp_err("[add][ra_ping]ping_ops or ra_ping_target_add is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    phy_id = ping_handle_tmp->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[add][ra_ping]phy_id(%u) must less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
    // disallow add target when task is running
    if (ping_handle_tmp->task_cnt != 0) {
        hccp_err("[add][ra_ping]task_cnt:%u != 0 invalid, task already running", ping_handle_tmp->task_cnt);
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, -EEXIST);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);

    ret = ping_handle_tmp->ping_ops->ra_ping_target_add(ping_handle_tmp, target, num);
    if (ret != 0) {
        return conver_return_code(RDMA_OP, ret);
    }

    // increase target cnt
    RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
    ping_handle_tmp->target_cnt += num;
    RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ping_task_start(void *ping_handle, struct ping_task_attr *attr)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(ping_handle == NULL || attr == NULL, hccp_err("[start][ra_ping]ping_handle is NULL or attr is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attr->packet_cnt == 0 || attr->packet_interval == 0 || attr->timeout_interval == 0,
        hccp_err("[start][ra_ping]packet_cnt:%u or packet_interval:%u or timeout_interval:%u is 0", attr->packet_cnt,
        attr->packet_interval, attr->timeout_interval), conver_return_code(RDMA_OP, -EINVAL));

    ping_handle_tmp = (struct ra_ping_handle *)ping_handle;
    CHK_PRT_RETURN(ping_handle_tmp->ping_ops == NULL || ping_handle_tmp->ping_ops->ra_ping_task_start == NULL,
        hccp_err("[start][ra_ping]ping_ops is NULL or ping_ops->ra_ping_task_start is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = ping_handle_tmp->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[start][ra_ping]phy_id(%u) must less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
    // disallow multi task running or no target to start
    if (ping_handle_tmp->task_cnt != 0) {
        hccp_warn("[start][ra_ping]task_cnt:%u != 0 invalid, task already running", ping_handle_tmp->task_cnt);
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, -EEXIST);
    }
    if (ping_handle_tmp->target_cnt == 0) {
        hccp_warn("[start][ra_ping]target_cnt is 0 invalid, no target exist");
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, -ENODEV);
    }

    // increase task cnt to avoid lock contention, trigger task running
    ping_handle_tmp->task_cnt++;
    RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);

    hccp_run_info("Input parameters: phy_id[%u], packet_cnt[%u] interval[%u] timeout_interval[%u], target_cnt[%u]",
        phy_id, attr->packet_cnt, attr->packet_interval, attr->timeout_interval, ping_handle_tmp->target_cnt);

    ret = ping_handle_tmp->ping_ops->ra_ping_task_start(ping_handle_tmp, attr);
    if (ret != 0) {
        // trigger failed, decrease task cnt
        hccp_err("[start][ra_ping]ping_task_start failed, ret(%d)", ret);
        RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
        ping_handle_tmp->task_cnt--;
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ping_get_results(void *ping_handle, struct ping_target_result target[], uint32_t *num)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    unsigned int phy_id;
    int ret;

    if (ping_handle == NULL || target == NULL || num == NULL || *num == 0) {
        hccp_err("[get][ra_ping]ping_handle is NULL or target is NULL or num is NULL or *num is 0");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ping_handle_tmp = (struct ra_ping_handle *)ping_handle;
    if (ping_handle_tmp->ping_ops == NULL || ping_handle_tmp->ping_ops->ra_ping_get_results == NULL) {
        hccp_err("[get][ra_ping]ping_ops is NULL or ping_ops->ra_ping_get_results is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    phy_id = ping_handle_tmp->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][ra_ping]phy_id(%u) must less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    // num invalid, bigger than target exist
    RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
    if (ping_handle_tmp->target_cnt < *num) {
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        hccp_err("[get][ra_ping]target_cnt:%u < num:%u, invalid", ping_handle_tmp->target_cnt, *num);
        return conver_return_code(RDMA_OP, -EINVAL);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);

    ret = ping_handle_tmp->ping_ops->ra_ping_get_results(ping_handle_tmp, target, num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ping_target_del(void *ping_handle, struct ping_target_comm_info target[], uint32_t num)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    unsigned int phy_id;
    int ret;

    if (ping_handle == NULL || target == NULL || num == 0) {
        hccp_err("[del][ra_ping]ping_handle is NULL or target is NULL or num:%u is 0", num);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ping_handle_tmp = (struct ra_ping_handle *)ping_handle;
    if (ping_handle_tmp->ping_ops == NULL || ping_handle_tmp->ping_ops->ra_ping_target_del == NULL) {
        hccp_err("[del][ra_ping]ping_ops is NULL or ping_ops->ra_ping_target_del is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
    // disallow del target when task is running
    if (ping_handle_tmp->task_cnt != 0) {
        hccp_err("[del][ra_ping]task_cnt:%u != 0 invalid, task already running", ping_handle_tmp->task_cnt);
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, -EEXIST);
    }
    // num invalid, bigger than target exist
    if (ping_handle_tmp->target_cnt < num) {
        hccp_err("[del][ra_ping]target_cnt:%u < num:%u, invalid", ping_handle_tmp->target_cnt, num);
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, -EINVAL);
    }
    // decrease target cnt to avoid lock contention, trigger target del
    ping_handle_tmp->target_cnt -= num;
    RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);

    phy_id = ping_handle_tmp->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[del][ra_ping]phy_id(%u) must less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    ret = ping_handle_tmp->ping_ops->ra_ping_target_del(ping_handle_tmp, target, num);
    if (ret != 0) {
        // trigger del failed, increase target cnt
        RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
        ping_handle_tmp->target_cnt += num;
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ping_task_stop(void *ping_handle)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    unsigned int phy_id;
    int ret;

    if (ping_handle == NULL) {
        hccp_err("[stop][ra_ping]ping_handle is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ping_handle_tmp = (struct ra_ping_handle *)ping_handle;
    if (ping_handle_tmp->ping_ops == NULL || ping_handle_tmp->ping_ops->ra_ping_task_stop == NULL) {
        hccp_err("[stop][ra_ping]ping_ops is NULL or ping_ops->ra_ping_task_stop is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    phy_id = ping_handle_tmp->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[stop][ra_ping]phy_id(%u) must less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    // no task to stop
    RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
    if (ping_handle_tmp->task_cnt == 0) {
        hccp_warn("[stop][ra_ping]task_cnt is 0 invalid, no task running");
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, -ENODEV);
    }
    // decrease task cnt to avoid lock contention, trigger task stop
    ping_handle_tmp->task_cnt--;
    RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);

    hccp_run_info("Input parameters: phy_id[%u], target_cnt[%u]", phy_id, ping_handle_tmp->target_cnt);

    ret = ping_handle_tmp->ping_ops->ra_ping_task_stop(ping_handle_tmp);
    if (ret != 0) {
        // trigger failed, increase task cnt
        RA_PTHREAD_MUTEX_LOCK(&ping_handle_tmp->mutex);
        ping_handle_tmp->task_cnt++;
        RA_PTHREAD_MUTEX_UNLOCK(&ping_handle_tmp->mutex);
        return conver_return_code(RDMA_OP, ret);
    }

    return 0;
}

STATIC int ra_ping_deinit_para_check(struct ra_ping_handle *ping_handle)
{
    char local_ip[MAX_IP_LEN] = {0};
    union ping_dev dev_info;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(ping_handle->ping_ops == NULL || ping_handle->ping_ops->ra_ping_deinit == NULL,
        hccp_err("[deinit][ra_ping]ping_ops is NULL or ra_ping_deinit is NULL"), -EINVAL);

    phy_id = ping_handle->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[deinit][ra_ping]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);

    dev_info = ping_handle->dev;
    if (ping_handle->protocol == PROTOCOL_RDMA) {
        ret = ra_inet_pton(dev_info.rdma.family, dev_info.rdma.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_ping]ra_inet_pton for local_ip failed, ret(%d)", ret), ret);
        hccp_run_info("Input parameters: phy_id[%u] dev_index[%u] family[%d] local_ip[%s] target_cnt[%u] task_cnt[%u]",
            phy_id, ping_handle->dev_index, dev_info.rdma.family, local_ip, ping_handle->target_cnt,
            ping_handle->task_cnt);
    } else if (ping_handle->protocol == PROTOCOL_UDMA) {
        hccp_run_info("Input parameters: eid_index[%u] eid[0x%016llx%016llx] target_cnt[%u] task_cnt[%u]",
            dev_info.ub.eid_index, dev_info.ub.eid.in6.subnet_prefix, dev_info.ub.eid.in6.interface_id,
            ping_handle->target_cnt, ping_handle->task_cnt);
    } else {
        hccp_err("[deinit][ra_ping]protocol:%d do not support", ping_handle->protocol);
        return -ENOTSUPP;
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ping_deinit(void *ping_handle)
{
    struct ra_ping_handle *ping_handle_tmp = NULL;
    int ret;

    if (ping_handle == NULL) {
        hccp_err("[deinit][ra_ping]ping_handle is NULL");
        return conver_return_code(HCCP_INIT, -EINVAL);
    }
    ping_handle_tmp = (struct ra_ping_handle *)ping_handle;

    ret = ra_ping_deinit_para_check(ping_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_ping]ra_ping_deinit_para_check failed, ret(%d)", ret);
        return conver_return_code(HCCP_INIT, ret);
    }

    ret = ping_handle_tmp->ping_ops->ra_ping_deinit(ping_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_ping]ra_ping_deinit failed, ret(%d)", ret);
    }

    ping_handle_tmp->ping_ops = NULL;
    (void)pthread_mutex_destroy(&ping_handle_tmp->mutex);
    free(ping_handle_tmp);
    ping_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}
