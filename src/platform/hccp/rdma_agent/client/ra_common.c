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
static struct ra_rdma_ops gRaRestoreRdmaOps = {
    .ra_rdev_deinit = RaHdcRdevRestoreDeinit,
    .ra_qp_destroy = RaHdcQpDestroy,
    .ra_deregister_mr = RaHdcTypicalMrDereg,
};

HCCP_ATTRI_VISI_DEF int RaGetTlsEnable(struct ra_info *info, bool *tlsEnable)
{
    int ret;

    CHK_PRT_RETURN(info == NULL || tlsEnable == NULL, hccp_err("[get][tls_enable]info or tls_enable is NULL"),
        ConverReturnCode(OTHERS, -EINVAL));
    CHK_PRT_RETURN(info->phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][tls_enable]phy_id(%u) must smaller than %u",
        info->phy_id, RA_MAX_PHY_ID_NUM), ConverReturnCode(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", info->phy_id, info->mode);
    if (info->mode == NETWORK_PEER_ONLINE) {
        ret = RaPeerGetTlsEnable(info->phy_id, tlsEnable);
    } else if (info->mode == NETWORK_OFFLINE) {
        ret = RaHdcGetTlsEnable(info->phy_id, tlsEnable);
    } else {
        hccp_err("[get][tls_enable]do not support mode(%d) phy_id(%u)", info->mode, info->phy_id);
        return ConverReturnCode(OTHERS, -ENOTSUPP);
    }
    return ConverReturnCode(OTHERS, ret);
}

HCCP_ATTRI_VISI_DEF int RaSaveSnapshot(struct ra_info *info, enum save_snapshot_action action)
{
    struct ra_rdma_handle *rdmaHandle = NULL;
    int ret;

    CHK_PRT_RETURN(info == NULL, hccp_err("[save][snapshot]info is NULL"), ConverReturnCode(OTHERS, -EINVAL));
    CHK_PRT_RETURN(action < SAVE_SNAPSHOT_ACTION_PRE_PROCESSING || action >= SAVE_SNAPSHOT_ACTION_MAX,
        hccp_err("[save][snapshot]invalid action(%d)", action), ConverReturnCode(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d], action:[%d]", info->phy_id, info->mode, action);
    if (info->mode == NETWORK_PEER_ONLINE) {
        return 0;
    } else if (info->mode == NETWORK_OFFLINE) {
        ret = RaRdevGetHandle(info->phy_id, (void **)&rdmaHandle);
        CHK_PRT_RETURN(ret != 0 && ret != -ENODEV, hccp_err("[save][snapshot]ra_rdev_get_handle failed, ret[%d]", ret),
            ConverReturnCode(OTHERS, ret));
        ret = RaHdcRdmaSaveSnapshot(rdmaHandle, action);
        CHK_PRT_RETURN(ret != 0, hccp_err("[save][snapshot]ra_hdc_rdma_save_snapshot failed, ret[%d]", ret),
            ConverReturnCode(OTHERS, ret));

        ret = RaHdcSaveSnapshot(info->phy_id, action);
        CHK_PRT_RETURN(ret != 0, hccp_err("[save][snapshot]ra_hdc_save_snapshot failed, ret[%d]", ret),
            ConverReturnCode(OTHERS, ret));
    } else {
        hccp_err("[save][snapshot]do not support mode[%d] phy_id[%u]", info->mode, info->phy_id);
        return ConverReturnCode(OTHERS, -ENOTSUPP);
    }

    return ConverReturnCode(OTHERS, ret);
}

HCCP_ATTRI_VISI_DEF int RaRestoreSnapshot(struct ra_info *info)
{
    struct ra_rdma_handle *rdmaHandle = NULL;
    int ret;

    CHK_PRT_RETURN(info == NULL, hccp_err("[restore][snapshot]info is NULL"), ConverReturnCode(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", info->phy_id, info->mode);
    if (info->mode == NETWORK_PEER_ONLINE) {
        return 0;
    } else if (info->mode == NETWORK_OFFLINE) {
        ret = RaRdevGetHandle(info->phy_id, (void **)&rdmaHandle);
        CHK_PRT_RETURN(ret != 0 && ret != -ENODEV, hccp_err("[restore][snapshot]ra_rdev_get_handle failed, ret[%d]",
            ret),ConverReturnCode(OTHERS, ret));
        ret = RaHdcRdmaRestoreSnapshot(rdmaHandle, &gRaRestoreRdmaOps);
        CHK_PRT_RETURN(ret != 0, hccp_err("[restore][snapshot]ra_hdc_rdma_restore_snapshot failed, ret[%d]", ret),
            ConverReturnCode(OTHERS, ret));

        ret = RaHdcRestoreSnapshot(info->phy_id);
        CHK_PRT_RETURN(ret != 0, hccp_err("[restore][snapshot]ra_hdc_restore_snapshot failed, ret[%d]", ret),
            ConverReturnCode(OTHERS, ret));
    } else {
        hccp_err("[restore][snapshot]do not support mode[%d] phy_id[%u]", info->mode, info->phy_id);
        return ConverReturnCode(OTHERS, -ENOTSUPP);
    }

    return ConverReturnCode(OTHERS, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetHccnCfg(struct ra_info *info, enum hccn_cfg_key key, char *value, unsigned int *valueLen)
{
    int ret;

    CHK_PRT_RETURN(info == NULL || value == NULL || valueLen == NULL, hccp_err("[get][hccn_cfg]info or value or value_len is NULL"),
        ConverReturnCode(OTHERS, -EINVAL));
    CHK_PRT_RETURN(*valueLen < HCCN_CFG_MSG_DATA_LEN,
        hccp_err("[get][hccn_cfg] failed, valueLen[%d] < min_len[%d]", *valueLen, HCCN_CFG_MSG_DATA_LEN),
        ConverReturnCode(OTHERS, -EINVAL));
    CHK_PRT_RETURN(info->phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][hccn_cfg]phy_id(%u) must smaller than %u",
        info->phy_id, RA_MAX_PHY_ID_NUM), ConverReturnCode(OTHERS, -EINVAL));
    CHK_PRT_RETURN(info->mode != NETWORK_OFFLINE, hccp_err("[get][hccn_cfg]do not support mode(%u)", info->mode),
        ConverReturnCode(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d], hccn_cfg_key[%d]",
        info->phy_id, info->mode, key);
    ret = RaHdcGetHccnCfg(info->phy_id, key, value, valueLen);
    if (ret != 0) {
        hccp_err("[get][hccn_cfg] failed, phy_id[%u], ret[%d]", info->phy_id, ret);
    }

    return ConverReturnCode(OTHERS, ret);
}
