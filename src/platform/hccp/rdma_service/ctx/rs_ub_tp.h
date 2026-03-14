/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_UB_TP_H
#define RS_UB_TP_H

#include "hccp_async_ctx.h"
#include "hccp_ctx.h"
#include "rs_ctx_inner.h"

#define IPV4_MAP_START_IDX 12
#define TP_ATTR_SIP_MASK 0x4
#define TP_ATTR_DIP_MASK 0x8
#define TP_ATTR_SMAC_MASK 0x10
#define TP_ATTR_DMAC_MASK 0x20

int RsUbGetTpInfoList(struct RsUbDevCb *devCb, struct GetTpCfg *cfg, struct HccpTpInfo infoList[],
    unsigned int *num);
int RsUbGetTpAttr(struct RsUbDevCb *devCb, unsigned int *attrBitmap, const uint64_t tpHandle,
    struct TpAttr *attr);
int RsUbSetTpAttr(struct RsUbDevCb *devCb, const unsigned int attrBitmap, const uint64_t tpHandle,
    struct TpAttr *attr);

#endif // RS_UB_TP_H
