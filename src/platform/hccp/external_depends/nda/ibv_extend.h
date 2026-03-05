/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef IBV_EXTEND_H
#define IBV_EXTEND_H

#include <infiniband/verbs.h>
#include <stdint.h>

#define IBV_EXTEND_VERSION_MAJOR 1
#define IBV_EXTEND_VERSION_MINOR 4
#define IBV_EXTEND_VERSION_PATCH 0
#define IBV_EXTEND_VERSION_STRING "1.4.0"

enum queue_buf_dma_mode {
    QU_BUF_DMA_MODE_DEFAULT = 0,
    QU_BUF_DMA_MODE_INDEP_UB,
    QU_BUF_DMA_MODE_MAX
};

enum doorbell_map_mode {
    DB_MAP_MODE_HOST_VA = 0,
    DB_MAP_MODE_UB_RES,
    DB_MAP_MODE_UB_MAX
};

struct doorbell_map_desc {
    uint32_t type;

    union {
        uint64_t hva;
        struct {
            uint64_t guid_l;
            uint64_t guid_h;
            struct {
                uint32_t resource_id : 4;
                uint32_t offset : 24;
                uint32_t rsvd : 24;
            } bits;
        };
        
    };
    
};


#endif // IBV_EXTEND_H