/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_UB_DFX_H
#define RS_UB_DFX_H

#include "hccp_ctx.h"
#include "rs_ctx_inner.h"
#include "rs_inner.h"

int RsUbCtxGetAuxInfo(struct RsUbDevCb *devCb, struct HccpAuxInfoIn *infoIn, struct HccpAuxInfoOut *infoOut);
int RsEpollEventJfcInHandle(struct rs_cb *rsCb, int fd);
int RsEpollEventUrmaAsyncEventInHandle(struct rs_cb *rsCb, int fd);
void RsUbCtxGetAsyncEvents(struct RsUbDevCb *devCb, struct AsyncEvent asyncEvents[], unsigned int *num);

#endif // RS_UB_DFX_H
