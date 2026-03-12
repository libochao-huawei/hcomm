/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_NDA_H
#define RS_NDA_H

#include "hccp_nda.h"
#include "rs_list.h"

#define RS_VENDOR_ID_19E5 0x19E5
#define RS_NDA_4K 4096

struct NdaDbCb {
    uint64_t hva;
    uint64_t dva;
    uint32_t refCnt;
    uint32_t resv;

    struct RsListHead list;
};

struct NdaGuidCb {
    uint64_t guidL;
    uint64_t guidH;
    uint64_t dva;
    uint16_t guidIdx;
    uint16_t resv;
    uint32_t refCnt;

    struct RsListHead list;
};

union DbUbResInfo {
    struct {
        uint32_t dbIdx : 12;
        uint32_t guidIdx : 16;
        uint32_t resv : 4;
    };
    uint32_t resId;
};

// 底软没有提供这个结构体
struct ascend_nda_res_map_in {
    unsigned long long guid_l;
    unsigned long long guid_h;
    unsigned int db_idx;
    unsigned int db_num;
    unsigned int resv[8];
};

static inline int RsNdaGetDirectFlagByVendorId(uint32_t vendorId)
{
    return (vendorId == RS_VENDOR_ID_19E5) ? DIRECT_FLAG_UB : DIRECT_FLAG_PCIE;
}

static inline unsigned int RsNdaGenerateResId(uint32_t dbIdx, uint16_t guidIdx)
{
    union DbUbResInfo resInfo = {0};
    resInfo.dbIdx = dbIdx;
    resInfo.guidIdx = guidIdx;

    return resInfo.resId;
}

#endif // RS_NDA_H
