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
	__u64   buf_addr;
};

struct hns_roce_ib_create_qp {
	__u64	buf_addr;
	__u64   db_addr;
	__u8    log_sq_bb_count;
	__u8    log_sq_stride;
	__u8    sq_no_prefetch;
	__u8    reserved[5];
};

struct hns_roce_ib_alloc_ucontext_resp {
	__u32	qp_tab_size;
};
#endif /* HNS_ABI_USER_H */
