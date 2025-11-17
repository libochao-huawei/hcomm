/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdbool.h>
#include <errno.h>
#include "user_log.h"
#include "hccp.h"
#include "hccp_common.h"
#include "ra_client_host.h"
#include "ra.h"
#include "ra_peer.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma.h"
#include "ra_rs_err.h"
#include "ra_rs_comm.h"

/* rdma ops for ra_restore_snapshot: The use of RDMA-lite related interfaces is prohibited. */
static struct ra_rdma_ops g_ra_restore_rdma_ops = {
    .ra_rdev_deinit = ra_hdc_rdev_restore_deinit,
    .ra_qp_destroy = ra_hdc_qp_destroy,
    .ra_deregister_mr = ra_hdc_typical_mr_dereg,
};

HCCP_ATTRI_VISI_DEF int ra_get_tls_enable(struct ra_info *info, bool *tls_enable)
{
    int ret;

    CHK_PRT_RETURN(info == NULL || tls_enable == NULL, hccp_err("[get][tls_enable]info or tls_enable is NULL"),
        conver_return_code(OTHERS, -EINVAL));
    CHK_PRT_RETURN(info->phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][tls_enable]phy_id(%u) must smaller than %u",
        info->phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", info->phy_id, info->mode);
    if (info->mode == NETWORK_PEER_ONLINE) {
        ret = ra_peer_get_tls_enable(info->phy_id, tls_enable);
    } else if (info->mode == NETWORK_OFFLINE) {
        ret = ra_hdc_get_tls_enable(info->phy_id, tls_enable);
    } else {
        hccp_err("[get][tls_enable]do not support mode(%d) phy_id(%u)", info->mode, info->phy_id);
        return conver_return_code(OTHERS, -ENOTSUPP);
    }
    return conver_return_code(OTHERS, ret);
}

HCCP_ATTRI_VISI_DEF int ra_save_snapshot(struct ra_info *info, enum save_snapshot_action action)
{
    struct ra_rdma_handle *rdma_handle = NULL;
    int ret;

    CHK_PRT_RETURN(info == NULL, hccp_err("[save][snapshot]info is NULL"), conver_return_code(OTHERS, -EINVAL));
    CHK_PRT_RETURN(action < SAVE_SNAPSHOT_ACTION_PRE_PROCESSING || action >= SAVE_SNAPSHOT_ACTION_MAX,
        hccp_err("[save][snapshot]invalid action(%d)", action), conver_return_code(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d], action:[%d]", info->phy_id, info->mode, action);
    if (info->mode == NETWORK_PEER_ONLINE) {
        return 0;
    } else if (info->mode == NETWORK_OFFLINE) {
        ret = ra_rdev_get_handle(info->phy_id, (void **)&rdma_handle);
        CHK_PRT_RETURN(ret != 0 && ret != -ENODEV, hccp_err("[save][snapshot]ra_rdev_get_handle failed, ret[%d]", ret),
            conver_return_code(OTHERS, ret));
        ret = ra_hdc_rdma_save_snapshot(rdma_handle, action);
        CHK_PRT_RETURN(ret != 0, hccp_err("[save][snapshot]ra_hdc_rdma_save_snapshot failed, ret[%d]", ret),
            conver_return_code(OTHERS, ret));

        ret = ra_hdc_save_snapshot(info->phy_id, action);
        CHK_PRT_RETURN(ret != 0, hccp_err("[save][snapshot]ra_hdc_save_snapshot failed, ret[%d]", ret),
            conver_return_code(OTHERS, ret));
    } else {
        hccp_err("[save][snapshot]do not support mode[%d] phy_id[%u]", info->mode, info->phy_id);
        return conver_return_code(OTHERS, -ENOTSUPP);
    }

    return conver_return_code(OTHERS, ret);
}

HCCP_ATTRI_VISI_DEF int ra_restore_snapshot(struct ra_info *info)
{
    struct ra_rdma_handle *rdma_handle = NULL;
    int ret;

    CHK_PRT_RETURN(info == NULL, hccp_err("[restore][snapshot]info is NULL"), conver_return_code(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", info->phy_id, info->mode);
    if (info->mode == NETWORK_PEER_ONLINE) {
        return 0;
    } else if (info->mode == NETWORK_OFFLINE) {
        ret = ra_rdev_get_handle(info->phy_id, (void **)&rdma_handle);
        CHK_PRT_RETURN(ret != 0 && ret != -ENODEV, hccp_err("[restore][snapshot]ra_rdev_get_handle failed, ret[%d]",
            ret),conver_return_code(OTHERS, ret));
        ret = ra_hdc_rdma_restore_snapshot(rdma_handle, &g_ra_restore_rdma_ops);
        CHK_PRT_RETURN(ret != 0, hccp_err("[restore][snapshot]ra_hdc_rdma_restore_snapshot failed, ret[%d]", ret),
            conver_return_code(OTHERS, ret));

        ret = ra_hdc_restore_snapshot(info->phy_id);
        CHK_PRT_RETURN(ret != 0, hccp_err("[restore][snapshot]ra_hdc_restore_snapshot failed, ret[%d]", ret),
            conver_return_code(OTHERS, ret));
    } else {
        hccp_err("[restore][snapshot]do not support mode[%d] phy_id[%u]", info->mode, info->phy_id);
        return conver_return_code(OTHERS, -ENOTSUPP);
    }

    return conver_return_code(OTHERS, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_sec_random(struct ra_info *info, u32 *value)
{
    int ret;

    CHK_PRT_RETURN(info == NULL || value == NULL, hccp_err("[get][sec_random]info or value is NULL"),
        conver_return_code(OTHERS, -EINVAL));
    hccp_run_info("Input parameters: phy_id[%u], info.mode:[%d]", info->phy_id, info->mode);

    ret = ra_peer_get_sec_random(value);
    if(ret != 0 && info->mode == NETWORK_OFFLINE) {
        CHK_PRT_RETURN(info->phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][sec_random]phy_id(%u) must smaller than %u",
            info->phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(OTHERS, -EINVAL));
        ret = ra_hdc_get_sec_random(info->phy_id, value);
    } else if (ret != 0 && info->mode != NETWORK_OFFLINE) {
        hccp_err("[get][sec_random] failed, mode[%u], ret[%d]", info->mode, ret);
    } else {
        hccp_run_info("[get][sec_random] success, mode[%u]", info->mode);
    }

    return conver_return_code(OTHERS, ret);
}
