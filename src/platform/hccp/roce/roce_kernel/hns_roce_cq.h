/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_CQ_H_
#define _HNS_ROCE_CQ_H_
#include "hns-abi.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#define HNS_ROCE_IB_NUM_TWO     2

struct hns_roce_user_cq {
    struct ib_ucontext *context;
    struct ib_udata *udata;
    struct hns_roce_ib_create_cq_resp resp;
};

#define HNS_ROCE_WRITE_CQC(hr_dev) do { \
    wrt_cqc_info.dma_handle = dma_handle; \
    wrt_cqc_info.nent = nent; \
    wrt_cqc_info.vector = vector; \
    hr_dev->hw->write_cqc(hr_dev, hr_cq, mailbox->buf, mtts, wrt_cqc_info); \
} while (0)

#define HNS_ROCE_LOC_VAR_INIT(hr_dev, ib_dev, attr) do { \
    hr_dev = to_hr_dev(ib_dev); \
    dev = hr_dev->dev; \
    vector = attr->comp_vector; \
    cq_entries = attr->cqe; \
} while (0)

#define HNS_ROCE_INIT_HR_CQ_TPTR() do { \
    if (context == NULL && hr_cq->tptr_addr) { \
        *hr_cq->tptr_addr = 0; \
    } \
    hr_cq->cq_depth = cq_entries; \
} while (0)

#endif
