/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HNS_ABI_USER_H
#define HNS_ABI_USER_H

#include <linux/types.h>

struct hns_roce_ib_create_cq {
    __aligned_u64 buf_addr;
    __aligned_u64 db_addr;
};

enum hns_roce_cq_cap_flags {
    HNS_ROCE_CQ_FLAG_RECORD_DB = 1 << 0,
};

struct hns_roce_ib_create_cq_resp {
    __aligned_u64 cqn; /* Only 32 bits used, 64 for compat */
    __aligned_u64 cap_flags;
};

struct hns_roce_ib_create_srq {
    __aligned_u64 buf_addr;
    __aligned_u64 db_addr;
    __aligned_u64 que_addr;
};

struct hns_roce_ib_create_srq_resp {
    __u32    srqn;
    __u32    reserved;
};

struct hns_roce_ib_create_qp {
    __aligned_u64 buf_addr;
    __aligned_u64 db_addr;
    __u8    log_sq_bb_count;
    __u8    log_sq_stride;
    __u8    sq_no_prefetch;
    __u8    reserved[5];
    __aligned_u64 sdb_addr;
    __u32         gdr_enable;
    __u32         reserve;
    __aligned_u64 share_addr_base;
};

#define PROCESS_CMD_SIGN_LENGTH 49

struct hns_roce_ib_reg_mr {
    u32 tgid;
    u32 devid;
    u32 vfid;
    char sign[PROCESS_CMD_SIGN_LENGTH];
    __u32    reserved;
};

enum hns_roce_qp_cap_flags {
    HNS_ROCE_QP_CAP_RQ_RECORD_DB = 1 << 0,
    HNS_ROCE_QP_CAP_SQ_RECORD_DB = 1 << 1,
    HNS_ROCE_QP_CAP_OWNER_DB = 1 << 2,
    HNS_ROCE_QP_CAP_DIRECT_WQE = 1 << 5,
};

struct hns_roce_ib_create_qp_resp {
    __aligned_u64 cap_flags;
    __u32         share_sq_index;
    __u32         share_sq_offset;
    __u32         share_temp_offset;
    __u32         share_db_offset;
    __u32         share_dfx_offset;
    __u32         share_sq_depth;
    __u32         share_temp_depth;
    __u32         share_db_depth;
    __u32         share_dfx_depth;
    __u32         share_max_sq_num;
};

struct hns_roce_ib_alloc_ucontext_resp {
    __u32    qp_tab_size;
    __u32    chip_id;
    __aligned_u64    notify_pa;
    __aligned_u64    notify_size;
    __u32    port_num;
    __u32    port_id;
};

struct hns_roce_ib_alloc_pd_resp {
    __u32 pdn;
};

#endif /* HNS_ABI_USER_H */
