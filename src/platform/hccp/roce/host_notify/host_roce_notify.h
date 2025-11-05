/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HOST_ROCE_NOTIFY_H
#define _HOST_ROCE_NOTIFY_H

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include "host_roce_cdev.h"

#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX 16
#define PEER_MEM_U64_CORE_CONTEXT

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

struct host_roce_notify_node {
    unsigned long long va;
    unsigned long long pa;
    unsigned long long sz;
    pid_t tgid;
    struct list_head node;
};

struct hns_roce_notify_block {
    unsigned long   pa;
    unsigned int    sz;
    struct page     *page;
    dma_addr_t  dma;
};

struct hns_roce_notify_context {
    u32 inited_flag;
    int is_callback;
    int sg_allocated;

    u64 va;
    u64 len;
    u64 va_aligned_start;
    u64 va_aligned_end;
    u32 blk_num;
    struct host_roce_notify_node *notify_node;
    struct hns_roce_notify_block blks[0];
};

#define PROCESS_SIGN_LENGTH 49

struct peer_memory_data {
    pid_t host_pid;
    unsigned int devid; /* chip_id */
    unsigned int vfid;
    char pid_sign[PROCESS_SIGN_LENGTH];
};

struct mlx_peer_memory_client {
    char    name[IB_PEER_MEMORY_NAME_MAX];
    char    version[IB_PEER_MEMORY_VER_MAX];
    int (*acquire)(unsigned long addr, size_t size, void *peer_mem_private_data,
                   char *peer_mem_name, void **client_context);
    int (*get_pages)(unsigned long addr,
                     size_t size, int write, int force,
                     struct sg_table *sg_head,
                     void *client_context, u64 core_context);
    int (*dma_map)(struct sg_table *sg_head, void *client_context,
                   struct device *dma_device, int dmasync, int *nmap);
    int (*dma_unmap)(struct sg_table *sg_head, void *client_context,
                     struct device  *dma_device);
    void (*put_pages)(struct sg_table *sg_head, void *client_context);
    unsigned long (*get_page_size)(void *client_context);
    void (*release)(void *client_context);
};

typedef int (*invalidate_peer_memory)(void *reg_handle, u64 core_context);

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);

typedef void* (*ib_register_peer_memory_client_fun)(struct mlx_peer_memory_client *peer_client,
    invalidate_peer_memory *invalidate_callback);

typedef void (*ib_unregister_peer_memory_client_fun)(void *reg_handle);

void *ib_register_peer_memory_client(const struct mlx_peer_memory_client *peer_client,
    invalidate_peer_memory *invalidate_callback);

void ib_unregister_peer_memory_client(void *reg_handle);

void *host_roce_notify_client_init(ib_register_peer_memory_client_fun func_ib_register_peer_memory_client);

void host_roce_notify_client_uninit(char *priv);

int host_roce_notify_add(struct host_roce_notify_node *notify_node);

void host_roce_notify_del(struct host_roce_notify_info *notify_node);

void host_roce_notify_clear(void);

#endif
