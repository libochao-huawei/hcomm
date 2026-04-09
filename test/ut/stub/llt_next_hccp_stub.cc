/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hccp.h"
#include "hccp_ctx.h"
#include "hccp_async.h"
#include "hccp_async_ctx.h"
#include "orion_adapter_hccp.h"
#include "hccl/base.h"
 
int RaCtxQpCreate(void *ctx_handle, struct QpCreateAttr *attr, struct QpCreateInfo *info,
    void **qp_handle)
{
    return 0;
}
 
int RaCtxQpDestroy(void *qp_handle)
{
    return 0;
}
 
int RaCtxQpImport(void *ctx_handle, struct QpImportInfoT *qp_info, void **rem_qp_handle)
{
    return 0;
}
 
int RaCtxQpUnimport(void *ctx_handle, void *rem_qp_handle)
{
    return 0;
}
 
int RaGetAsyncReqResult(void *reqHandle, int *reqResult)
{
    *reqResult = 0;
    return 0;
}
 
int RaCtxQpCreateAsync(void *ctx_handle, struct QpCreateAttr *attr,
    struct QpCreateInfo *info, void **qp_handle, void **req_handle)
{
    int a = 12378;
    *req_handle = &a;
    return 0;
}
 
int RaCtxQpImportAsync(void *ctx_handle, struct QpImportInfoT *info, void **rem_qp_handle,
    void **req_handle)
{
    int a = 12378;
    *req_handle = &a;
    return 0;
}
 
int RaGetTpInfoListAsync(void *ctx_handle, struct GetTpCfg *cfg, struct HccpTpInfo info_list[],
    unsigned int *num, void **req_handle)
{
    int a = 12378;
    *req_handle = &a;
    return 0;
}
 
int RaCustomChannel(struct RaInfo info, struct CustomChanInfoIn *in,
    struct CustomChanInfoOut *out)
{
    return 0;
}
 
int RaGetDevEidInfoNum(struct RaInfo info, unsigned int *num)
{
    *num = 2;
    return 0;
}
 
int RaGetDevEidInfoList(struct RaInfo info, struct HccpDevEidInfo info_list[],
    unsigned int *num)
{
    if (info.phyId == 0) {
        info_list[0].eid.in4.addr = 167772383;
    } else {
        info_list[0].eid.in4.addr = 469762271;
    }
    
    info_list[0].dieId = 0;
    info_list[0].chipId = 0;
    info_list[0].funcId = 2;
 
    info_list[1].eid.in4.addr = 12346;
    info_list[1].dieId = 1;
    info_list[1].chipId = 0;
    info_list[1].funcId = 3;
 
    return 0;
}

int RaGetSecRandom(struct RaInfo *info, uint32_t *value)
{
    return 0;
}

int RaCtxGetAuxInfo(void *ctx_handle, struct HccpAuxInfoIn *in, struct HccpAuxInfoOut *out) {
    return 0;
}

int RaCtxQpQueryBatch(void *qp_handle[], struct JettyAttr attr[], unsigned int *num) {
    return 0;
}

namespace Hccl {
void HrtRaCustomChannel(const HRaInfo &raInfo, void *customIn, void *customOut)
{
    return;
}

void HrtDeviceAbortRegCallBack(aclrtDeviceTaskAbortCallback callback, void *args, const std::string& name)
{
    return;
}
} // namespace Hccl

// Stub implementations for test
HcclResult hrtRaAiQpCreate(u32 deviceId, void* nicRdmaHandle, struct RaQpCreateAttr *attrs, 
    struct AiQpInfo *aiQpInfo, void **qpHandle)
{
    (void)deviceId;
    (void)nicRdmaHandle;
    (void)attrs;
    (void)aiQpInfo;
    static u8 mockQpHandle[16] = {0};
    *qpHandle = mockQpHandle;
    return HCCL_SUCCESS;
}

HcclResult hrtRaQpCreateWithAttrs(void* nicRdmaHandle, struct RaQpCreateAttr *attrs, void **qpHandle)
{
    (void)nicRdmaHandle;
    (void)attrs;
    static u8 mockQpHandle[16] = {0};
    *qpHandle = mockQpHandle;
    return HCCL_SUCCESS;
}

HcclResult hrtRaGetQpAttr(void* qpHandle, struct QpAttr *attr)
{
    (void)qpHandle;
    if (attr != nullptr) {
        attr->qpn = 12345;
        attr->udpSport = 10000;
    }
    return HCCL_SUCCESS;
}

HcclResult HrtRaQpDestroy(void* qpHandle)
{
    (void)qpHandle;
    return HCCL_SUCCESS;
}
