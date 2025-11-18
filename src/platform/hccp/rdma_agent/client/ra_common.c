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

HCCP_ATTRI_VISI_DEF int ra_get_hccn_cfg(struct ra_info *info, enum hccn_cfg_key key, char *value, unsigned int *value_len)
{
    int ret;

    CHK_PRT_RETURN(info == NULL || value == NULL || value_len == NULL, hccp_err("[get][hccn_cfg]info or value or value_len is NULL"),
        conver_return_code(OTHERS, -EINVAL));
    CHK_PRT_RETURN(*value_len < HCCN_CFG_MSG_DATA_LEN,
        hccp_err("[get][hccn_cfg] failed, value_len[%d] < min_len[%d]", *value_len, HCCN_CFG_MSG_DATA_LEN),
        conver_return_code(OTHERS, -EINVAL));
    CHK_PRT_RETURN(info->phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][hccn_cfg]phy_id(%u) must smaller than %u",
        info->phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(OTHERS, -EINVAL));
    CHK_PRT_RETURN(info->mode != NETWORK_OFFLINE, hccp_err("[get][hccn_cfg]do not support mode(%u)", info->mode),
        conver_return_code(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d], hccn_cfg_key[%d]",
        info->phy_id, info->mode, key);
    ret = ra_hdc_get_hccn_cfg(info->phy_id, key, value, value_len);
    if (ret != 0) {
        hccp_err("[get][hccn_cfg] failed, phy_id[%u], ret[%d]", info->phy_id, ret);
    }

    return conver_return_code(OTHERS, ret);
}
