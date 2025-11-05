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
#include <dlfcn.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "hccp.h"
#include "hccp_common.h"
#include "ra.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_hdc.h"
#include "ra_peer.h"
#include "ra_hdc_async.h"
#include "ra_init.h"

static unsigned int g_send_wr_num = 0;
static void *g_ra_rdev_handle[RA_MAX_PHY_ID_NUM] = { 0 };
static ra_instance g_ref_instances[RA_MAX_INSTANCES] = { { 0, PTHREAD_MUTEX_INITIALIZER } };

HCCP_ATTRI_VISI_DEF int ra_is_first_used(int ins_id)
{
    int is_first = 0;

    CHK_PRT_RETURN(ins_id < 0 || ins_id >= RA_MAX_INSTANCES, hccp_err("[ra]ins_id(%d) must be in [0, %u)",
        ins_id, RA_MAX_INSTANCES), -EINVAL);

    pthread_mutex_lock(&g_ref_instances[ins_id].mutex);
    if (g_ref_instances[ins_id].ref_count == 0) {
        is_first++;
        hccp_run_info("[ra]ins_id(%d) is first used", ins_id);
    }

    g_ref_instances[ins_id].ref_count++;
    hccp_info("[ra]ins_id[%d] is %d", ins_id, g_ref_instances[ins_id].ref_count);
    pthread_mutex_unlock(&g_ref_instances[ins_id].mutex);

    return is_first;
}

HCCP_ATTRI_VISI_DEF int ra_is_last_used(int ins_id)
{
    int is_last = 0;

    CHK_PRT_RETURN(ins_id < 0 || ins_id >= RA_MAX_INSTANCES, hccp_err("[ra]ins_id(%d) must be in [0, %u)",
        ins_id, RA_MAX_INSTANCES), -EINVAL);

    pthread_mutex_lock(&g_ref_instances[ins_id].mutex);
    if (g_ref_instances[ins_id].ref_count == 0) {
        hccp_err("[ra]ins_id %d has not been used", ins_id);
        pthread_mutex_unlock(&g_ref_instances[ins_id].mutex);
        return -EINVAL;
    }

    if (g_ref_instances[ins_id].ref_count == 1) {
        is_last++;
        hccp_run_info("[ra]ins_id(%d) is last used", ins_id);
    }

    hccp_info("[ra]ins_id[%d] is %d", ins_id, g_ref_instances[ins_id].ref_count);
    g_ref_instances[ins_id].ref_count--;
    pthread_mutex_unlock(&g_ref_instances[ins_id].mutex);

    return is_last;
}

HCCP_ATTRI_VISI_DEF int ra_rdev_get_handle(unsigned int phy_id, void **rdma_handle)
{
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[get][ra_rdev]phy_id(%u) must smaller than %u",
        phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);
    CHK_PRT_RETURN(rdma_handle == NULL, hccp_err("[get][ra_rdev]rdma_handle is NULL, phy_id(%u)",
        phy_id), -EINVAL);
    CHK_PRT_RETURN(g_ra_rdev_handle[phy_id] == NULL, hccp_run_info("[get][ra_rdev]handle is NULL, phy_id(%u)", phy_id),
        -ENODEV);

    *rdma_handle = g_ra_rdev_handle[phy_id];
    return 0;
}

void ra_rdev_set_handle(unsigned int phy_id, void *rdma_handle)
{
    if (phy_id >= RA_MAX_PHY_ID_NUM) {
        hccp_warn("[set][ra_rdev]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM);
        return;
    }

    g_ra_rdev_handle[phy_id] = rdma_handle;
}

void ra_rdev_inc_send_wr_num(void)
{
    g_send_wr_num++;
}

STATIC int ra_init_hdc(struct ra_init_config *config)
{
    struct process_ra_sign p_ra_sign = {0};
    unsigned int phy_id = config->phy_id;
    struct process_sign psign = {0};
    int ret;

    ret = dl_drv_get_process_sign(&psign);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra]Get process sign failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    p_ra_sign.tgid = psign.tgid;
    ret = strcpy_s(p_ra_sign.sign, PROCESS_RA_SIGN_LENGTH, psign.sign);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra]Invalid pid sign, ret(%d) phy_id(%u)", ret, phy_id), -ESAFEFUNC);

    ret = ra_hdc_init(config, p_ra_sign);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra]ra hdc init failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    ra_hdc_get_all_opcode_version(phy_id);

    // init async hdc session via sync hdc session
    ret = ra_hdc_init_async(config);
    if (ret != 0) {
        hccp_err("[init][ra]ra_hdc_init_async failed, ret(%d) phy_id(%u)", ret, phy_id);
        (void)ra_hdc_deinit(config);
    }

    return ret;
}

STATIC int ra_init_peer(struct ra_init_config *config)
{
    unsigned int phy_id = config->phy_id;
    unsigned int white_list_switch = 0;
    int ret;

    ret = ra_socket_get_white_list_status(&white_list_switch);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra]get white_list_status failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    ret = ra_peer_init(config, white_list_switch);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra]ra_peer_init failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_init(struct ra_init_config *config)
{
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(config == NULL, hccp_err("[init][ra]config is NULL"), conver_return_code(HCCP_INIT, -EINVAL));

    phy_id = config->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[init][ra]phy_id(%u) is invalid! it must greater or "
        "equal to 0 and less than %d!", phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(HCCP_INIT, -EINVAL));

    if (config->hdc_type != HDC_SERVICE_TYPE_RDMA && config->hdc_type != HDC_SERVICE_TYPE_RDMA_V2) {
        hccp_warn("[init][ra]hdc_type(%d) is invalid, set it to default hdc_type(%d)",
            config->hdc_type, HDC_SERVICE_TYPE_RDMA);
        config->hdc_type = HDC_SERVICE_TYPE_RDMA;
    }

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%u] hdc_type:[%d] enable_hdc_async[%d]",
        phy_id, config->nic_position, config->hdc_type, config->enable_hdc_async);
    ret = dl_hal_init();
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra]dl_hal_init failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    if (config->nic_position == NETWORK_OFFLINE) {
        ret = ra_init_hdc(config);
        if (ret != 0) {
            hccp_err("[init][ra]ra_init_hdc failed, ret(%d) phy_id(%u)", ret, phy_id);
            goto err;
        }
    } else if (config->nic_position == NETWORK_PEER_ONLINE) {
        ret = ra_init_peer(config);
        if (ret != 0) {
            hccp_err("[init][ra]ra_init_peer failed, ret(%d) phy_id(%u)", ret, phy_id);
            goto err;
        }
    } else {
        hccp_err("[init][ra]do not support nic_position(%u) phy_id(%u)", config->nic_position, phy_id);
        ret = -EPROTONOSUPPORT;
        goto err;
    }

    return 0;

err:
    dl_hal_deinit();
    return conver_return_code(HCCP_INIT, ret);
}

STATIC int ra_deinit_hdc(struct ra_init_config *config)
{
    int ret;

    ret = ra_hdc_deinit_async(config->phy_id);
    CHK_PRT_RETURN(ret != 0 && ret != -ENODEV, hccp_err("[deinit][ra]ra_hdc_deinit_async failed, ret(%d)", ret), ret);

    ret = ra_hdc_deinit(config);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra]ra hdc deinit failed, ret(%d), g_send_wr_num(%u)",
        ret, g_send_wr_num), ret);
    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_deinit(struct ra_init_config *config)
{
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(config == NULL, hccp_err("[deinit][ra]config is NULL, invalid"),
        conver_return_code(HCCP_INIT, -EINVAL));

    phy_id = config->phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[deinit][ra]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(HCCP_INIT, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%u]", phy_id, config->nic_position);

    if (config->nic_position == NETWORK_OFFLINE) {
        ret = ra_deinit_hdc(config);
        CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra]ra_deinit_hdc failed, ret(%d) phy_id(%u)", ret, phy_id),
            conver_return_code(HCCP_INIT, ret));
    } else if (config->nic_position == NETWORK_PEER_ONLINE) {
        ret = ra_peer_deinit(config);
        CHK_PRT_RETURN(ret == -EAGAIN, hccp_warn("[deinit][ra]ra_peer_deinit unsuccessful, ret(%d) phy_id(%u)",
            ret, phy_id), conver_return_code(HCCP_INIT, ret));
        CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra]ra_peer_deinit failed, ret(%d) phy_id(%u)", ret, phy_id),
            conver_return_code(HCCP_INIT, ret));
    } else {
        hccp_err("[deinit][ra]do not support nic_position(%u) phy_id(%u)", config->nic_position, phy_id);
        return conver_return_code(HCCP_INIT, -EPROTONOSUPPORT);
    }

    ra_socket_set_white_list_status(WHITE_LIST_DISABLE);
    g_send_wr_num = 0;
    dl_hal_deinit();
    return 0;
}
