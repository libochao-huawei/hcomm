/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include <string.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "ra_hdc.h"
#include "ra_hdc_tlv.h"
#include "ra_rs_err.h"
#include "ra.h"
#include "ra_tlv.h"

STATIC struct ra_tlv_ops g_ra_hdc_tlv_ops = {
    .ra_tlv_init = ra_hdc_tlv_init,
    .ra_tlv_deinit = ra_hdc_tlv_deinit,
    .ra_tlv_request = ra_hdc_tlv_request,
};

HCCP_ATTRI_VISI_DEF int ra_tlv_init(struct tlv_init_info *init_info, unsigned int module_type,
    unsigned int *buffer_size, void **tlv_handle)
{
    struct ra_tlv_handle *tlv_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(init_info == NULL || buffer_size == NULL || tlv_handle == NULL,
        hccp_err("[init][ra_tlv]init_info or buffer_size or tlv_handle is NULL"),
            conver_return_code(HCCP_INIT, -EINVAL));

    CHK_PRT_RETURN(module_type >= TLV_MODULE_TYPE_MAX,
        hccp_err("[init][ra_tlv]module_type(%u) invalid, must smaller than TLV_MODULE_TYPE_MAX(%u)",
        module_type, TLV_MODULE_TYPE_MAX), conver_return_code(HCCP_INIT, -EINVAL));
    CHK_PRT_RETURN(init_info->nic_position != NETWORK_OFFLINE, hccp_err("[init][ra_tlv]mode(%u) not support",
        init_info->nic_position), conver_return_code(HCCP_INIT, -EINVAL));
    CHK_PRT_RETURN(init_info->phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[init][ra_tlv]phy_id(%u) must smaller than %u", init_info->phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(HCCP_INIT, -EINVAL));

    tlv_handle_tmp = calloc(1, sizeof(struct ra_tlv_handle));
    CHK_PRT_RETURN(tlv_handle_tmp == NULL, hccp_err("[init][ra_tlv]calloc for tlv_handle failed"),
        conver_return_code(HCCP_INIT, -ENOMEM));

    (void)memcpy_s(&(tlv_handle_tmp->init_info), sizeof(struct tlv_init_info), init_info, sizeof(struct tlv_init_info));
    tlv_handle_tmp->module_type = module_type;
    tlv_handle_tmp->tlv_ops = &g_ra_hdc_tlv_ops;
    if (tlv_handle_tmp->tlv_ops->ra_tlv_init == NULL) {
        ret = -EINVAL;
        hccp_err("[init][ra_tlv]ra_tlv_init is NULL");
        goto ra_tlv_init_err;
    }

    hccp_run_info("Input parameters: phy_id[%u], nic_position[%u], module_type[%u]", init_info->phy_id,
        init_info->nic_position, module_type);
    ret = tlv_handle_tmp->tlv_ops->ra_tlv_init(tlv_handle_tmp);
    if (ret == -ENOTSUPP) {
        hccp_run_warn("[init][ra_tlv]ra_tlv_init unsuccessful, ret(%d), phy_id(%u)", ret, init_info->phy_id);
        goto ra_tlv_init_err;
    } else if (ret != 0) {
        hccp_err("[init][ra_tlv]ra_tlv_init failed, ret(%d), phy_id(%u)", ret, init_info->phy_id);
        goto ra_tlv_init_err;
    }

    *buffer_size = tlv_handle_tmp->buffer_size;
    *tlv_handle = (void *)tlv_handle_tmp;
    return 0;

ra_tlv_init_err:
    free(tlv_handle_tmp);
    tlv_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_tlv_deinit(void *tlv_handle)
{
    struct ra_tlv_handle *tlv_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(tlv_handle == NULL, hccp_err("[deinit][ra_tlv]tlv_handle is NULL"),
        conver_return_code(HCCP_INIT, -EINVAL));

    tlv_handle_tmp = (struct ra_tlv_handle *)tlv_handle;
    if (tlv_handle_tmp->tlv_ops->ra_tlv_deinit == NULL) {
        ret = -EINVAL;
        hccp_err("[deinit][ra_tlv]ra_tlv_deinit is NULL, ret(%d), phy_id(%u)", ret, tlv_handle_tmp->init_info.phy_id);
        goto ra_tlv_deinit_fail;
    }

    hccp_run_info("Input parameters: phy_id[%u], nic_position[%u], module_type[%u]", tlv_handle_tmp->init_info.phy_id,
        tlv_handle_tmp->init_info.nic_position, tlv_handle_tmp->module_type);
    ret = tlv_handle_tmp->tlv_ops->ra_tlv_deinit(tlv_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_tlv]ra_tlv_deinit fail, ret(%d), phy_id(%u)", ret, tlv_handle_tmp->init_info.phy_id);
        goto ra_tlv_deinit_fail;
    }

ra_tlv_deinit_fail:
    free(tlv_handle_tmp);
    tlv_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_tlv_request(void *tlv_handle, struct tlv_msg *send_msg, struct tlv_msg *recv_msg)
{
    struct ra_tlv_handle *tlv_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(tlv_handle == NULL || send_msg == NULL || recv_msg == NULL,
        hccp_err("[request][ra_tlv]tlv_handle or send_msg or recv_msg is NULL"), conver_return_code(OTHERS, -EINVAL));

    tlv_handle_tmp = (struct ra_tlv_handle *)tlv_handle;
    CHK_PRT_RETURN(send_msg->length > tlv_handle_tmp->buffer_size || send_msg->length == 0,
        hccp_err("[request][ra_tlv]send length(%u) out of range(%u) or equal to 0",
        send_msg->length, tlv_handle_tmp->buffer_size), conver_return_code(OTHERS, -EINVAL));
    CHK_PRT_RETURN(tlv_handle_tmp->tlv_ops->ra_tlv_request == NULL,
        hccp_err("[request][ra_tlv]ra_tlv_request is NULL"), conver_return_code(OTHERS, -EINVAL));

    ret = tlv_handle_tmp->tlv_ops->ra_tlv_request(tlv_handle_tmp, send_msg, recv_msg);
    if (ret != 0) {
        hccp_err("[request][ra_tlv]ra_tlv_request failed, ret(%d), phy_id(%u)", ret, tlv_handle_tmp->init_info.phy_id);
    }

    return conver_return_code(OTHERS, ret);
}
