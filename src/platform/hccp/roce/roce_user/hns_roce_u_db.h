/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_U_DB_H
#define _HNS_ROCE_U_DB_H
#include <linux/types.h>
#include "hns_roce_u.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define HNS_ROCE_PAIR_TO_64(val) (((uint64_t) ((val)[1]) << 32U) | ((val)[0]))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define HNS_ROCE_PAIR_TO_64(val) (((uint64_t) ((val)[0]) << 32U) | ((val)[1]))
#else
#error __BYTE_ORDER not defined
#endif

#define VAL_SIZE        2

static inline void hns_roce_write64(const uint32_t val[VAL_SIZE],
                                    struct hns_roce_context *ctx, int offset)
{
    *((volatile uint64_t *)((char *)ctx->uar + offset)) = HNS_ROCE_PAIR_TO_64(val);
}

void *hns_roce_alloc_db(struct hns_roce_context *ctx, enum hns_roce_db_type type,
                        enum hns_roce_buf_type buf_type, uint32_t mem_idx);
void hns_roce_free_db(struct hns_roce_context *ctx, const unsigned int *db,
                      enum hns_roce_db_type type);

#endif /* _HNS_ROCE_U_DB_H */
