/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INFINIBAND_MARSHALL_H
#define INFINIBAND_MARSHALL_H

#include <infiniband/verbs.h>
#include <infiniband/sa.h>
#include <infiniband/kern-abi.h>
#include <rdma/ib_user_sa.h>

#ifdef __cplusplus
extern "C" {
#endif

void ibv_copy_qp_attr_from_kern(struct ibv_qp_attr *dst,
				struct ib_uverbs_qp_attr *src);

void ibv_copy_ah_attr_from_kern(struct ibv_ah_attr *dst,
				struct ib_uverbs_ah_attr *src);

void ibv_copy_path_rec_from_kern(struct ibv_sa_path_rec *dst,
				 struct ib_user_path_rec *src);

void ibv_copy_path_rec_to_kern(struct ib_user_path_rec *dst,
			       struct ibv_sa_path_rec *src);

#ifdef __cplusplus
}
#endif

#endif /* INFINIBAND_MARSHALL_H */
