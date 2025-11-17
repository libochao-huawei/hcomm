/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_PEER_H
#define _HNS_ROCE_PEER_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/scatterlist.h>

#include "hns_roce_device.h"

#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX 16

struct peer_memory_data {
    pid_t host_pid;
    unsigned int devid; /* chip_id */
    unsigned int vfid;
    char pid_sign[PROCESS_SIGN_LENGTH];
};

struct peer_memory_client {
    char    name[IB_PEER_MEMORY_NAME_MAX];
    char    version[IB_PEER_MEMORY_VER_MAX];
    int (*acquire)(unsigned long addr, size_t size, void *peer_mem_private_data,
                   char *peer_mem_name, void **client_context);
    int (*get_pages)(unsigned long addr,
                     size_t size, int write, int force,
                     struct sg_table *sg_head,
                     void *client_context, u64 core_context);
    void (*put_pages)(struct sg_table *sg_head, void *client_context);
    int (*dma_map)(struct sg_table *sg_head, void *client_context,
                   struct device *dma_device, int dmasync, int *nmap);
    int (*dma_unmap)(struct sg_table *sg_head, void *client_context,
                     struct device  *dma_device);
    unsigned long (*get_page_size)(void *client_context);
    void (*release)(void *client_context);
    void *(*get_context_private_data)(u64 peer_id);
    void (*put_context_private_data)(void *context);
};

struct ib_peer_memory_statistics {
    atomic64_t num_alloc_mrs;
    atomic64_t num_dealloc_mrs;
    atomic64_t num_reg_pages;
    atomic64_t num_dereg_pages;
    atomic64_t num_reg_bytes;
    atomic64_t num_dereg_bytes;
    unsigned long num_free_callbacks;
};

#ifndef CONFIG_INFINIBAND_PEER_MEMORY
enum ib_peer_mem_flags {
    IB_PEER_MEM_ALLOW   = 1,
    IB_PEER_MEM_INVAL_SUPP = (1 << 1),
};

typedef void (*umem_invalidate_func_t)(void *invalidation_cookie,
                                       struct ib_umem *umem,
                                       unsigned long addr, size_t size);

struct invalidation_ctx {
    struct ib_umem *umem;
    u64 context_ticket;
    umem_invalidate_func_t func;
    void *cookie;
    int peer_callback;
    int inflight_invalidation;
    int peer_invalidated;
    struct completion comp;
};
#endif

struct ib_peer_memory_client {
    const struct peer_memory_client *peer_mem;
    struct list_head    core_peer_list;
    int invalidation_required;
    struct kref ref;
    struct completion unload_comp;
    /* lock is used via the invalidation flow */
    struct mutex lock;
    struct list_head   core_ticket_list;
    u64 last_ticket;
    struct kobject *kobj;
    struct attribute_group peer_mem_attr_group;
    struct ib_peer_memory_statistics stats;
};

typedef int (*invalidate_peer_memory)(void *reg_handle, u64 core_context);

struct core_ticket {
    unsigned long key;
    void *context;
    struct list_head   ticket_list;
};

int hns_roce_init_atu_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_atu_table(struct hns_roce_dev *hr_dev);
int hns_roce_atu_alloc(struct hns_roce_dev *hr_dev, unsigned long *idx);
void hns_roce_atu_free(struct hns_roce_dev *hr_dev, unsigned long idx);

struct ib_umem *hns_roce_umem_get(struct ib_ucontext *context, struct hns_roce_user_mr_info user_mr,
                                  int dmasync, unsigned long peer_mem_flags, struct hns_roce_umem *hr_umem);
void hns_roce_umem_release(struct ib_umem *umem, struct hns_roce_umem *hr_umem);

void *ib_register_peer_memory_client(const struct peer_memory_client *peer_client,
                                     invalidate_peer_memory *invalidate_callback);
void ib_unregister_peer_memory_client(void *reg_handle);

#endif /* _HNS_ROCE_PEER_H */
