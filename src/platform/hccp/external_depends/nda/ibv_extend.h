/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef IBV_EXTEND_H
#define IBV_EXTEND_H

#include <sys/uio.h>
#include <infiniband/verbs.h>
#include <stdint.h>

#define IBV_EXTEND_VERSION_MAJOR 2
#define IBV_EXTEND_VERSION_MINOR 1
#define IBV_EXTEND_VERSION_PATCH 0
#define IBV_EXTEND_VERSION_STRING "2.1.0"

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

enum memcpy_direction {
    MEMCPY_DIR_HOST_TO_HOST = 0,
    MEMCPY_DIR_HOST_TO_DEVICE,
    MEMCPY_DIR_DEVICE_TO_HOST,
    MEMCPY_DIR_DEVICE_TO_DEVICE,
};

enum ibv_qp_init_cap {
    QP_ENABLE_DIRECT_WQE = 1 << 0,
};

enum ibv_extend_device_cap {
    IBV_EXTEND_DEV_NDR = 1 << 0,
    IBV_EXTEND_DEV_NDA = 1 << 1,
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
                uint32_t rsvd : 4;
            } bits;
            uint32_t rsvd;
        } ub_res;
    };
    uint64_t size;
};

struct ibv_extend_ops {
    void *(*alloc)(size_t size);
    void (*free)(void *ptr);

    void (*memset_s)(void *dst, int value, size_t count);
    int (*memcpy_s)(void *dst, size_t dst_max_size, void *src, size_t size, uint32_t direct);

    void *(*db_mmap)(struct doorbell_map_desc *desc);
    int (*db_unmap)(void *ptr, struct doorbell_map_desc *desc);
};

struct queue_buf {
    uint64_t base;
    uint32_t entry_cnt;
    uint32_t entry_size;
};

struct queue_info {
    struct queue_buf qbuf;
    struct iovec dbr_pi_va;
    struct iovec dbr_ci_va;
    struct iovec db_hw_va;
};

struct ibv_qp_extend {
    struct ibv_qp *qp;
    struct queue_info sq_info;
    struct queue_info rq_info;

    uint64_t resv[32];
};

struct ibv_cq_extend {
    struct ibv_cq *cq;
    struct queue_info cq_info;

    uint64_t resv[32];
};

struct ibv_srq_extend {
    struct ibv_srq *srq;
    struct queue_info srq_info;

    uint64_t resv[32];
};

struct ibv_qp_init_attr_extend {
    struct ibv_pd *pd;
    struct ibv_qp_init_attr attr;

    uint32_t qp_cap_flag;
    enum queue_buf_dma_mode type;
    struct ibv_extend_ops *ops;
};

struct ibv_cq_init_attr_extend {
    struct ibv_cq_init_attr_ex attr;

    uint32_t cq_cap_flag;
    enum queue_buf_dma_mode type;
    struct ibv_extend_ops *ops;
};

struct ibv_srq_init_attr_extend {
    struct ibv_pd *pd;
    struct ibv_srq_init_attr attr;

    uint32_t comp_mask;

    uint32_t srq_cap_flag;
    enum queue_buf_dma_mode type;
    struct ibv_extend_ops *ops;
};

struct ibv_device_attr_extend {
    uint32_t ext_cap;

    uint32_t resv[32];
};

struct ibv_context_extend {
    struct ibv_context *context;
    struct ibv_context_extend_ops *ops;
};

const char *ibv_extend_get_version(uint32_t *major, uint32_t *minor, uint32_t *patch);

int ibv_extend_check_version(uint32_t driver_major, uint32_t driver_minor, uint32_t driver_patch);

struct ibv_context_extend *ibv_open_extend(struct ibv_context *context);

int ibv_close_extend(struct ibv_context_extend *context);

struct ibv_qp_extend *ibv_create_qp_extend(struct ibv_context_extend *context,
                                           struct ibv_qp_init_attr_extend *qp_init_attr);

struct ibv_cq_extend *ibv_create_cq_extend(struct ibv_context_extend *context,
                                           struct ibv_cq_init_attr_extend *cq_init_attr);

struct ibv_srq_extend *ibv_create_srq_extend(struct ibv_context_extend *context,
                                             struct ibv_srq_init_attr_extend *srq_init_attr);

int ibv_destroy_qp_extend(struct ibv_context_extend *context, struct ibv_qp_extend *qp_extend);

int ibv_destroy_cq_extend(struct ibv_context_extend *context, struct ibv_cq_extend *cq_extend);

int ibv_destroy_srq_extend(struct ibv_context_extend *context, struct ibv_srq_extend *srq_extend);

struct ibv_context_extend_ops {
    struct ibv_qp_extend *(*create_qp)(struct ibv_context *context,
                                       struct ibv_qp_init_attr_extend *qp_init_attr);
    struct ibv_cq_extend *(*create_cq)(struct ibv_context *context,
                                       struct ibv_cq_init_attr_extend *cq_init_attr);
    struct ibv_srq_extend *(*create_srq)(struct ibv_context *context,
                                         struct ibv_srq_init_attr_extend *srq_init_attr);

    int (*destroy_qp)(struct ibv_qp_extend *qp_extend);
    int (*destroy_cq)(struct ibv_cq_extend *cq_extend);
    int (*destroy_srq)(struct ibv_srq_extend *srq_extend);

    int (*query_device)(struct ibv_context_extend *context,
                        struct ibv_device_attr_extend *ext_dev_attr);
};

struct verbs_device_extend_ops {
    const char *name;

    struct ibv_context_extend *(*alloc_context)(struct ibv_context *context);
    void (*free_context)(struct ibv_context_extend *context);
};

void verbs_register_driver_extend(const struct verbs_device_extend_ops *ops);

#define PROVIDER_EXTEND_DRIVER(drv)                                              \
    static __attribute__((constructor)) void drv##__register_extend_driver(void) \
    {                                                                            \
        verbs_register_driver_extend(&drv);                                      \
    }

#endif // IBV_EXTEND_H