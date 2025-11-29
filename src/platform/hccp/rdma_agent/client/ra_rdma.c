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
#include "hccp.h"
#include "ra.h"
#include "ra_rs_comm.h"
#include "ra_client_host.h"
#include "ra_rdma.h"

HCCP_ATTRI_VISI_DEF int RaSendNormalWrlist(void *qpHandle, struct wr_info wr[], struct send_wr_rsp opRsp[],
    unsigned int sendNum, unsigned int *completeNum)
{
    struct wrlist_send_complete_num wrlistNum = { 0 };
    struct ra_qp_handle *raQpHandle = NULL;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || wr == NULL || opRsp == NULL || sendNum == 0 || completeNum == NULL,
        hccp_err("[send][ra_wrlist]qp_handle or wr or op_rsp or complete_num is NULL or send_num is zero, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    raQpHandle = (struct ra_qp_handle *)qpHandle;
    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_send_normal_wrlist == NULL,
        hccp_err("[send][ra_wrlist]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_normal_wrlist is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    for (i = 0; i < sendNum; i++) {
        CHK_PRT_RETURN(wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX,
            hccp_err("[send][ra_wrlist]wr[%u] mem_list.len(%u) > %d", i, wr[i].mem_list.len, MAX_SG_LIST_LEN_MAX),
            ConverReturnCode(RDMA_OP, -EINVAL));
    }

    wrlistNum.send_num = sendNum;
    wrlistNum.complete_num = completeNum;
    ret = raQpHandle->rdma_ops->ra_send_normal_wrlist(raQpHandle, wr, opRsp, wrlistNum);
    return ConverReturnCode(RDMA_OP, ret);
}

void RaRdevSaveNotifyMr(struct ra_rdma_handle *rdmaHandle, int ret, uint64_t va, uint64_t size)
{
    // ret != 0 means unsuccessful, no need to save notify mr info
    if (ret != 0) {
        return;
    }

    rdmaHandle->notify_va = va;
    rdmaHandle->notify_size = size;
}

STATIC void RaRdevCheckNotifyMr(struct ra_rdma_handle *rdmaHandle, uint64_t va, uint64_t size)
{
    if ((rdmaHandle->notify_va <= (va + size)) && (va <= (rdmaHandle->notify_va + rdmaHandle->notify_size))) {
        hccp_run_warn("[check][ra_mr]phy_id:%u notify{va:0x%llx size:0x%llx} overlap input{va:0x%llx size:0x%llx}",
            rdmaHandle->rdev_info.phy_id, rdmaHandle->notify_va, rdmaHandle->notify_size, va, size);
    }
}

HCCP_ATTRI_VISI_DEF int RaRegisterMr(const void *rdmaHandle, struct mr_info *info, void **mrHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = NULL;
    int ret;

    CHK_PRT_RETURN(rdmaHandle == NULL || info == NULL || mrHandle == NULL,
        hccp_err("[init][ra_mr]rdma_handle or info or mr_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rdmaHandleTmp = (struct ra_rdma_handle *)rdmaHandle;
    CHK_PRT_RETURN(rdmaHandleTmp->rdma_ops == NULL || rdmaHandleTmp->rdma_ops->ra_register_mr == NULL,
        hccp_err("[init][ra_mr]rdma_ops or rdma_ops->ra_register_mr is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    RaRdevCheckNotifyMr(rdmaHandleTmp, (uint64_t)(uintptr_t)info->addr, info->size);
    ret = rdmaHandleTmp->rdma_ops->ra_register_mr(rdmaHandleTmp, info, mrHandle);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaRemapMr(const void *rdmaHandle, struct mem_remap_info info[], unsigned int num)
{
    struct ra_rdma_handle *rdmaHandleTmp = NULL;
    int ret;

    CHK_PRT_RETURN(rdmaHandle == NULL || info == NULL || num == 0 || num > REMAP_MR_MAX_NUM,
        hccp_err("[remap][ra_mr]rdma_handle or info is NULL or num:%u is out of range (0:%u]", num, REMAP_MR_MAX_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    rdmaHandleTmp = (struct ra_rdma_handle *)rdmaHandle;
    CHK_PRT_RETURN(rdmaHandleTmp->rdma_ops == NULL || rdmaHandleTmp->rdma_ops->ra_remap_mr == NULL,
        hccp_err("[remap][ra_mr]rdma_ops or rdma_ops->ra_remap_mr is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = rdmaHandleTmp->rdma_ops->ra_remap_mr(rdmaHandleTmp, info, num);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaDeregisterMr(const void *rdmaHandle, void *mrHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = NULL;
    int ret;

    CHK_PRT_RETURN(rdmaHandle == NULL || mrHandle == NULL,
        hccp_err("[deinit][ra_mr]rdma_handle or mr_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rdmaHandleTmp = (struct ra_rdma_handle *)rdmaHandle;
    CHK_PRT_RETURN(rdmaHandleTmp->rdma_ops == NULL || rdmaHandleTmp->rdma_ops->ra_deregister_mr == NULL,
        hccp_err("[deinit][ra_mr]rdma_ops or rdma_ops->ra_deregister_mr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = rdmaHandleTmp->rdma_ops->ra_deregister_mr(rdmaHandleTmp, mrHandle);
    return ConverReturnCode(RDMA_OP, ret);
}
