/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_U_ABI_H
#define _HNS_ROCE_U_ABI_H

#include "verbs_exp.h"
struct hns_roce_alloc_ucontext_resp {
    struct ib_uverbs_get_context_resp   ibv_resp;
    __u32               qp_tab_size;
    __u32               chip_id;
    __u64               notify_pa;
    __u64               notify_size;
    __u32               port_num;
    __u32               port_id;
};

struct hns_roce_alloc_pd_resp {
    struct ib_uverbs_alloc_pd_resp  ibv_resp;
    __u32               pdn;
    __u32               reserved;
};

struct hns_roce_create_cq {
    struct ibv_create_cq        ibv_cmd;
    __u64               buf_addr;
    __u64               db_addr;
};

struct hns_roce_create_cq_resp {
    struct ib_uverbs_create_cq_resp ibv_resp;
    __u32               cqn;
    __u32               reserved;
    __u32               cap_flags;
    __u32               reserved1;
};

struct hns_roce_create_srq {
    struct ibv_create_srq       ibv_cmd;
    __u64               buf_addr;
    __u64               db_addr;
    __u64               que_addr;
};

struct hns_roce_create_srq_resp {
    struct ib_uverbs_create_srq_resp    ibv_resp;
    __u32                   srqn;
    __u32                   reserved;
};

#define RESERVED_SIZE        5
struct hns_roce_create_qp {
    struct ibv_create_qp        ibv_cmd;
    __u64               buf_addr;
    __u64               db_addr;
    __u8                log_sq_bb_count;
    __u8                log_sq_stride;
    __u8                sq_no_prefetch;
    __u8                reserved[RESERVED_SIZE];
    __u64               sdb_addr;
    __u32               gdr_enable;
    __u32               reserve;
    __u64               share_addr_base;
};

struct hns_roce_reg_mr {
    struct ibv_reg_mr       cmd;
    struct roce_process_sign psign;
    __u32               reserved;
};


struct hns_roce_create_qp_resp {
    struct ib_uverbs_create_qp_resp base;
    __u32               cap_flags;
    __u32               reserved;
    __u32               share_sq_index;
    __u32               share_sq_offset;
    __u32               share_temp_offset;
    __u32               share_db_offset;
    __u32               share_dfx_offset;
    __u32               share_sq_depth;
    __u32               share_temp_depth;
    __u32               share_db_depth;
    __u32               share_dfx_depth;
    __u32               share_max_sq_num;
};

struct hns_roce_modify_qp {
    struct ibv_modify_qp        ibv_cmd;
    __u32               sq_head;
    __u32               rq_head;
};
#endif /* _HNS_ROCE_U_ABI_H */
