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

HCCP_ATTRI_VISI_DEF int ra_send_normal_wrlist(void *qp_handle, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    unsigned int send_num, unsigned int *complete_num)
{
    struct wrlist_send_complete_num wrlist_num = { 0 };
    struct ra_qp_handle *ra_qp_handle = NULL;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || wr == NULL || op_rsp == NULL || send_num == 0 || complete_num == NULL,
        hccp_err("[send][ra_wrlist]qp_handle or wr or op_rsp or complete_num is NULL or send_num is zero, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_send_normal_wrlist == NULL,
        hccp_err("[send][ra_wrlist]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_normal_wrlist is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    for (i = 0; i < send_num; i++) {
        CHK_PRT_RETURN(wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX,
            hccp_err("[send][ra_wrlist]wr[%u] mem_list.len(%u) > %d", i, wr[i].mem_list.len, MAX_SG_LIST_LEN_MAX),
            conver_return_code(RDMA_OP, -EINVAL));
    }

    wrlist_num.send_num = send_num;
    wrlist_num.complete_num = complete_num;
    ret = ra_qp_handle->rdma_ops->ra_send_normal_wrlist(ra_qp_handle, wr, op_rsp, wrlist_num);
    return conver_return_code(RDMA_OP, ret);
}

void ra_rdev_save_notify_mr(struct ra_rdma_handle *rdma_handle, int ret, uint64_t va, uint64_t size)
{
    // ret != 0 means unsuccessful, no need to save notify mr info
    if (ret != 0) {
        return;
    }

    rdma_handle->notify_va = va;
    rdma_handle->notify_size = size;
}

STATIC void ra_rdev_check_notify_mr(struct ra_rdma_handle *rdma_handle, uint64_t va, uint64_t size)
{
    if ((rdma_handle->notify_va <= (va + size)) && (va <= (rdma_handle->notify_va + rdma_handle->notify_size))) {
        hccp_run_warn("[check][ra_mr]phy_id:%u notify{va:0x%llx size:0x%llx} overlap input{va:0x%llx size:0x%llx}",
            rdma_handle->rdev_info.phy_id, rdma_handle->notify_va, rdma_handle->notify_size, va, size);
    }
}

HCCP_ATTRI_VISI_DEF int ra_register_mr(const void *rdma_handle, struct mr_info *info, void **mr_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(rdma_handle == NULL || info == NULL || mr_handle == NULL,
        hccp_err("[init][ra_mr]rdma_handle or info or mr_handle is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    rdma_handle_tmp = (struct ra_rdma_handle *)rdma_handle;
    CHK_PRT_RETURN(rdma_handle_tmp->rdma_ops == NULL || rdma_handle_tmp->rdma_ops->ra_register_mr == NULL,
        hccp_err("[init][ra_mr]rdma_ops or rdma_ops->ra_register_mr is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    ra_rdev_check_notify_mr(rdma_handle_tmp, (uint64_t)(uintptr_t)info->addr, info->size);
    ret = rdma_handle_tmp->rdma_ops->ra_register_mr(rdma_handle_tmp, info, mr_handle);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_remap_mr(const void *rdma_handle, struct mem_remap_info info[], unsigned int num)
{
    struct ra_rdma_handle *rdma_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(rdma_handle == NULL || info == NULL || num == 0 || num > REMAP_MR_MAX_NUM,
        hccp_err("[remap][ra_mr]rdma_handle or info is NULL or num:%u is out of range (0:%u]", num, REMAP_MR_MAX_NUM),
        conver_return_code(RDMA_OP, -EINVAL));

    rdma_handle_tmp = (struct ra_rdma_handle *)rdma_handle;
    CHK_PRT_RETURN(rdma_handle_tmp->rdma_ops == NULL || rdma_handle_tmp->rdma_ops->ra_remap_mr == NULL,
        hccp_err("[remap][ra_mr]rdma_ops or rdma_ops->ra_remap_mr is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    ret = rdma_handle_tmp->rdma_ops->ra_remap_mr(rdma_handle_tmp, info, num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_deregister_mr(const void *rdma_handle, void *mr_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(rdma_handle == NULL || mr_handle == NULL,
        hccp_err("[deinit][ra_mr]rdma_handle or mr_handle is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    rdma_handle_tmp = (struct ra_rdma_handle *)rdma_handle;
    CHK_PRT_RETURN(rdma_handle_tmp->rdma_ops == NULL || rdma_handle_tmp->rdma_ops->ra_deregister_mr == NULL,
        hccp_err("[deinit][ra_mr]rdma_ops or rdma_ops->ra_deregister_mr is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = rdma_handle_tmp->rdma_ops->ra_deregister_mr(rdma_handle_tmp, mr_handle);
    return conver_return_code(RDMA_OP, ret);
}
