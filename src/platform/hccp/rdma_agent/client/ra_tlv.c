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

STATIC struct RaTlvOps gRaHdcTlvOps = {
    .raTlvInit = RaHdcTlvInit,
    .raTlvDeinit = RaHdcTlvDeinit,
    .raTlvRequest = RaHdcTlvRequest,
};

HCCP_ATTRI_VISI_DEF int RaTlvInit(struct TlvInitInfo *initInfo, unsigned int moduleType,
    unsigned int *bufferSize, void **tlvHandle)
{
    struct RaTlvHandle *tlvHandleTmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(initInfo == NULL || bufferSize == NULL || tlvHandle == NULL,
        hccp_err("[init][ra_tlv]init_info or buffer_size or tlv_handle is NULL"),
            ConverReturnCode(HCCP_INIT, -EINVAL));

    CHK_PRT_RETURN(moduleType >= TLV_MODULE_TYPE_MAX,
        hccp_err("[init][ra_tlv]module_type(%u) invalid, must smaller than TLV_MODULE_TYPE_MAX(%u)",
        moduleType, TLV_MODULE_TYPE_MAX), ConverReturnCode(HCCP_INIT, -EINVAL));
    CHK_PRT_RETURN(initInfo->nicPosition != NETWORK_OFFLINE, hccp_err("[init][ra_tlv]mode(%u) not support",
        initInfo->nicPosition), ConverReturnCode(HCCP_INIT, -EINVAL));
    CHK_PRT_RETURN(initInfo->phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[init][ra_tlv]phy_id(%u) must smaller than %u", initInfo->phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(HCCP_INIT, -EINVAL));

    tlvHandleTmp = calloc(1, sizeof(struct RaTlvHandle));
    CHK_PRT_RETURN(tlvHandleTmp == NULL, hccp_err("[init][ra_tlv]calloc for tlv_handle failed"),
        ConverReturnCode(HCCP_INIT, -ENOMEM));

    (void)memcpy_s(&(tlvHandleTmp->initInfo), sizeof(struct TlvInitInfo), initInfo, sizeof(struct TlvInitInfo));
    tlvHandleTmp->moduleType = moduleType;
    tlvHandleTmp->tlvOps = &gRaHdcTlvOps;
    if (tlvHandleTmp->tlvOps->raTlvInit == NULL) {
        ret = -EINVAL;
        hccp_err("[init][ra_tlv]ra_tlv_init is NULL");
        goto ra_tlv_init_err;
    }

    hccp_run_info("Input parameters: phy_id[%u], nicPosition[%u], moduleType[%u]", initInfo->phyId,
        initInfo->nicPosition, moduleType);
    ret = tlvHandleTmp->tlvOps->raTlvInit(tlvHandleTmp);
    if (ret == -ENOTSUPP) {
        hccp_run_warn("[init][ra_tlv]ra_tlv_init unsuccessful, ret(%d), phyId(%u)", ret, initInfo->phyId);
        goto ra_tlv_init_err;
    } else if (ret != 0) {
        hccp_err("[init][ra_tlv]ra_tlv_init failed, ret(%d), phyId(%u)", ret, initInfo->phyId);
        goto ra_tlv_init_err;
    }

    *bufferSize = tlvHandleTmp->bufferSize;
    *tlvHandle = (void *)tlvHandleTmp;
    return 0;

ra_tlv_init_err:
    free(tlvHandleTmp);
    tlvHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaTlvDeinit(void *tlvHandle)
{
    struct RaTlvHandle *tlvHandleTmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(tlvHandle == NULL, hccp_err("[deinit][ra_tlv]tlv_handle is NULL"),
        ConverReturnCode(HCCP_INIT, -EINVAL));

    tlvHandleTmp = (struct RaTlvHandle *)tlvHandle;
    if (tlvHandleTmp->tlvOps->raTlvDeinit == NULL) {
        ret = -EINVAL;
        hccp_err("[deinit][ra_tlv]ra_tlv_deinit is NULL, ret(%d), phyId(%u)", ret, tlvHandleTmp->initInfo.phyId);
        goto ra_tlv_deinit_fail;
    }

    hccp_run_info("Input parameters: phy_id[%u], nicPosition[%u], moduleType[%u]", tlvHandleTmp->initInfo.phyId,
        tlvHandleTmp->initInfo.nicPosition, tlvHandleTmp->moduleType);
    ret = tlvHandleTmp->tlvOps->raTlvDeinit(tlvHandleTmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_tlv]ra_tlv_deinit failed, ret(%d), phyId(%u)", ret, tlvHandleTmp->initInfo.phyId);
        goto ra_tlv_deinit_fail;
    }

ra_tlv_deinit_fail:
    free(tlvHandleTmp);
    tlvHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaTlvRequest(void *tlvHandle, struct TlvMsg *sendMsg, struct TlvMsg *recvMsg)
{
    struct RaTlvHandle *tlvHandleTmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(tlvHandle == NULL || sendMsg == NULL || recvMsg == NULL,
        hccp_err("[request][ra_tlv]tlv_handle or send_msg or recv_msg is NULL"), ConverReturnCode(OTHERS, -EINVAL));

    tlvHandleTmp = (struct RaTlvHandle *)tlvHandle;
    CHK_PRT_RETURN(sendMsg->length > tlvHandleTmp->bufferSize || sendMsg->length == 0,
        hccp_err("[request][ra_tlv]send length(%u) out of range(%u) or equal to 0",
        sendMsg->length, tlvHandleTmp->bufferSize), ConverReturnCode(OTHERS, -EINVAL));
    CHK_PRT_RETURN(tlvHandleTmp->tlvOps->raTlvRequest == NULL,
        hccp_err("[request][ra_tlv]ra_tlv_request is NULL"), ConverReturnCode(OTHERS, -EINVAL));

    ret = tlvHandleTmp->tlvOps->raTlvRequest(tlvHandleTmp, sendMsg, recvMsg);
    if (ret != 0) {
        hccp_err("[request][ra_tlv]ra_tlv_request failed, ret(%d), phyId(%u)", ret, tlvHandleTmp->initInfo.phyId);
    }

    return ConverReturnCode(OTHERS, ret);
}
