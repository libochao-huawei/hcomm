/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include <hcomm_adapter_urma.h>
#include "log.h"
 
namespace hcomm {
 
HcclResult HrtUrmaPostJettySendWr(urma_jetty_t *jetty, urma_jfs_wr_t *wr, urma_jfs_wr_t **bad_wr)
{
    // 参数有效性检查
    if (jetty == nullptr || wr == nullptr) {
        HCCL_ERROR("HrtUrmaPostJettySendWr jetty[%p] or wr[%p] is nullptr", jetty, wr);
        return HCCL_E_PTR;
    }
    if (UNLIKELY(!DlUrmaFunction::GetInstance().DlUrmaFunctionIsInit())) {
        CHK_RET(DlUrmaFunction::GetInstance().DlUrmaFunctionInit());
    }
    urma_status_t ret = DlUrmaFunction::GetInstance().dlUrmaPostJettySendWr(jetty, wr, bad_wr);
    if (ret != 0) {
        HCCL_ERROR("HrtUrmaPostJettySendWr failed. ret:[%d]", ret);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}
 
int HrtUrmaPollJfc(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr)
{
    // 参数有效性检查
    if (jfc == nullptr || cr == nullptr) {
        HCCL_ERROR("HrtUrmaPollJfc jfc[%p] or cr[%p] is nullptr", jfc, cr);
        return HCCL_E_PTR;
    }
    if (UNLIKELY(!DlUrmaFunction::GetInstance().DlUrmaFunctionIsInit())) {
        CHK_RET(DlUrmaFunction::GetInstance().DlUrmaFunctionInit());
    }
    return DlUrmaFunction::GetInstance().dlUrmaPollJfc(jfc, cr_cnt, cr);
}
 
}