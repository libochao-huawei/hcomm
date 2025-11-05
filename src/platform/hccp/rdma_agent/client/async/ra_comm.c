/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "user_log.h"
#include "ra_async.h"
#include "ra_rs_comm.h"
#include "ra_hdc_async.h"
#include "hccp_async.h"

HCCP_ATTRI_VISI_DEF int ra_get_async_req_result(void *req_handle, int *req_result)
{
    struct ra_request_handle *req_handle_tmp = NULL;

    CHK_PRT_RETURN(req_handle == NULL || req_result == NULL, hccp_err("[get][async]req_handle or req_result is NULL"),
        conver_return_code(OTHERS, -EINVAL));

    req_handle_tmp = (struct ra_request_handle *)req_handle;
    if (!req_handle_tmp->is_done){
        return conver_return_code(OTHERS, -EAGAIN);
    }

    *req_result = conver_return_code(req_handle_tmp->op_handle->op_module, req_handle_tmp->op_ret);
    hdc_async_del_response(req_handle_tmp);
    return 0;
}
