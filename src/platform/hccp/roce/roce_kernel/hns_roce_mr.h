/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_MR_H__
#define _HNS_ROCE_MR_H__

#define MHOP_NUM_3LEVELS    3
#define MHOP_NUM_2LEVELS    2
#define ERRLOP_INDEX2       2
#define ERRLOP_INDEX1       1
#define PBLSZ_BASE          8
#define PG4K_OFFSET         12
#define HIGH_OFFSET         24
#define LOW_OFFSET          8

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

struct hns_roce_user_reregmr_info {
    struct ib_mr *ibmr;
    struct ib_pd *pd;
    u64 start;
    u64 length;
    u64 virt_addr;
    int access_flags;
    int flags;
};

struct hns_roce_ib_umem_page_info {
    u64 *pages;
    int i;
    int npage;
    unsigned int order;
};
#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

#endif
