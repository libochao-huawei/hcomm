/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/export.h>

#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/kallsyms.h>

#include "host_roce_cdev.h"
#include "host_roce_notify.h"

invalidate_peer_memory g_host_invalidate_callback;
#define NOTIFY_HOST_NAME         "HOST RoCE Notify"
#define NOTIFY_HOST_VERSION      "1.0"
#define NOTIFY_INITED_FLAG      0xF1234567UL

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

static LIST_HEAD(host_roce_notify_list);
static DEFINE_MUTEX(host_roce_notify_lock);

STATIC unsigned long host_roce_notify_get_page_size(void *context)
{
    return PAGE_SIZE;
}

STATIC int host_roce_notify_acquire(unsigned long addr, size_t size, void *peer_mem_private_data,
    char *peer_mem_name, void **client_context)
{
    int page_num;
    unsigned long va_aligned_start, va_aligned_end;
    struct hns_roce_notify_context *host_context = NULL;
    struct host_roce_notify_node *notify_node = NULL;

    pr_info("enter addr=0x%lx, size=%lu.\n", addr, size);
    mutex_lock(&host_roce_notify_lock);
    list_for_each_entry(notify_node, &host_roce_notify_list, node) {
        if ((addr < (notify_node->va + notify_node->sz)) && (addr >= notify_node->va) &&
            (current->tgid == notify_node->tgid)) {
            va_aligned_start = addr & PAGE_MASK;
            va_aligned_end = (addr + size + PAGE_SIZE - 1) & PAGE_MASK;
            page_num = (va_aligned_end - va_aligned_start) / PAGE_SIZE;
            host_context = kzalloc((sizeof(*host_context)) +
                page_num * sizeof(struct hns_roce_notify_block), GFP_ATOMIC | __GFP_ACCOUNT);
            if (host_context == NULL) {
                mutex_unlock(&host_roce_notify_lock);
                pr_err("kzalloc err. page_num=%d\n", page_num);
                return 0;
            }

            host_context->inited_flag  = NOTIFY_INITED_FLAG;
            host_context->is_callback  = 0;
            host_context->sg_allocated = 0;
            host_context->va = addr;
            host_context->len = size;
            host_context->va_aligned_start = va_aligned_start;
            host_context->va_aligned_end = va_aligned_end;
            host_context->blk_num = page_num;
            host_context->notify_node = notify_node;
            *client_context = host_context;
            pr_info("acquire exit. succ.\n");
            mutex_unlock(&host_roce_notify_lock);
            return 1;
        }
    }
    mutex_unlock(&host_roce_notify_lock);

    return 0;
}

STATIC int host_roce_notify_dma_map(struct sg_table *sg_head, void *context,
    struct device *dma_device, int dmasync,
    int *nmap)
{
    int k, ret;
    struct hns_roce_notify_context *host_context =
        (struct hns_roce_notify_context *) context;
    struct scatterlist *sg = NULL;

    if (sg_head == NULL || nmap == NULL) {
        pr_err("hns3: get null ptr, sg_head: %pK, nmap: %pK\n", sg_head, nmap);
        return -EINVAL;
    }

    if ((host_context == NULL) || host_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: mm_context is not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return -EINVAL;
    }

    ret = sg_alloc_table(sg_head, host_context->blk_num, GFP_KERNEL | __GFP_ACCOUNT);
    if (ret) {
        pr_err("hns3: sg alloc table failed, blk_num=%u.\n", host_context->blk_num);
        return ret;
    }

    host_context->sg_allocated = 1;
    for_each_sg(sg_head->sgl, sg, (int)host_context->blk_num, k) {
        sg_set_page(sg, NULL, host_roce_notify_get_page_size(host_context), 0);
        sg->dma_address = host_context->blks[k].dma;
        sg->dma_length = host_context->blks[k].sz;
    }

    *nmap = host_context->blk_num;

    pr_info("hns3: dma map exit. suc\n");
    return 0;
}

STATIC int host_roce_notify_dma_unmap(struct sg_table *sg_head, void *context,
    struct device  *dma_device)
{
    struct hns_roce_notify_context *host_context =
        (struct hns_roce_notify_context *) context;

    if ((host_context == NULL) || host_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: invalid host_context or not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return -EINVAL;
    }

    if (host_context->sg_allocated) {
        sg_free_table(sg_head);
        host_context->sg_allocated = 0;
    }

    pr_info("hns3: dma unmap exit. suc\n");
    return 0;
}

STATIC void host_roce_notify_put_pages(struct sg_table *sg_head, void *context)
{
    struct hns_roce_notify_context *host_context =
        (struct hns_roce_notify_context *) context;

    if ((host_context == NULL) || host_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: invalid host_context or not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return;
    }

    if (host_context->sg_allocated) {
        sg_free_table(sg_head);
        host_context->sg_allocated = 0;
    }

    pr_info("hns3: put page exit. suc\n");
}

STATIC void host_roce_notify_release(void *context)
{
    kfree(context);
    context = NULL;
    pr_info("hns3: release exit.\n");
}

STATIC int host_roce_notify_get_pages(unsigned long addr,
    size_t size, int write, int force,
    struct sg_table *sg_head,
    void *client_context,
#ifndef PEER_MEM_U64_CORE_CONTEXT
    void *core_context)
#else
    u64 core_context)
#endif
{
    int k;
    struct hns_roce_notify_context *host_context = NULL;

    pr_info("hns3: get page enter,addr=0x%lx,size=%lu.\n", addr, size);
    host_context = (struct hns_roce_notify_context *)client_context;
    if (host_context == NULL || host_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: invalid host_context or not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return -EINVAL;
    }

    host_context->blk_num = (host_context->va_aligned_end -
                            host_context->va_aligned_start) / PAGE_SIZE;

    for (k = 0; k < (int)host_context->blk_num; k++) {
        host_context->blks[k].pa = host_context->notify_node->pa + k * host_roce_notify_get_page_size(host_context);
        host_context->blks[k].sz = host_roce_notify_get_page_size(host_context);
        host_context->blks[k].dma = host_context->blks[k].pa;
        pr_debug("k[%d] pa=0x%lx dma=0x%llx\n",
               k, (unsigned long)host_context->blks[k].pa, host_context->blks[k].dma);
    }

    return 0;
}

static struct mlx_peer_memory_client g_host_roce_notify_client = {
    .acquire    = host_roce_notify_acquire,
    .get_pages  = host_roce_notify_get_pages,
    .dma_map    = host_roce_notify_dma_map,
    .dma_unmap  = host_roce_notify_dma_unmap,
    .put_pages  = host_roce_notify_put_pages,
    .get_page_size  = host_roce_notify_get_page_size,
    .release    = host_roce_notify_release,
};

void *host_roce_notify_client_init(ib_register_peer_memory_client_fun func_ib_register_peer_memory_client)
{
    size_t lenth;
    int ret;

    lenth = strlen(NOTIFY_HOST_NAME);
    lenth = lenth > IB_PEER_MEMORY_NAME_MAX ? IB_PEER_MEMORY_NAME_MAX : lenth;
    ret = memcpy_s(g_host_roce_notify_client.name, sizeof(g_host_roce_notify_client.name),
                   NOTIFY_HOST_NAME, lenth);
    if (ret) {
        pr_err("host_roce_notify_client_init name memcpy_s err, ret[%d].\n", ret);
        return NULL;
    }

    lenth = strlen(NOTIFY_HOST_VERSION);
    lenth = lenth > IB_PEER_MEMORY_VER_MAX ? IB_PEER_MEMORY_VER_MAX : lenth;
    ret = memcpy_s(g_host_roce_notify_client.version, sizeof(g_host_roce_notify_client.version),
                   NOTIFY_HOST_VERSION, lenth);
    if (ret) {
        pr_err("host_roce_notify_client_init, version memcpy_s err, ret[%d].\n", ret);
        return NULL;
    }
    return func_ib_register_peer_memory_client(&g_host_roce_notify_client,
                                               &g_host_invalidate_callback);
}

void host_roce_notify_client_uninit(char *priv)
{
#ifndef DEFINE_HNS_LLT
    ib_unregister_peer_memory_client_fun func_ib_unregister_peer_memory_client =
        (ib_unregister_peer_memory_client_fun)(uintptr_t)symbol_get(ib_unregister_peer_memory_client);
    if (func_ib_unregister_peer_memory_client == NULL) {
        pr_err("ib_unregister_peer_memory_client syms not found.\n");
        return;
    }
#else
    ib_unregister_peer_memory_client_fun func_ib_unregister_peer_memory_client =
        ib_unregister_peer_memory_client;
#endif

    func_ib_unregister_peer_memory_client(priv);

#ifndef DEFINE_HNS_LLT
    if (func_ib_unregister_peer_memory_client != NULL) {
        symbol_put(ib_unregister_peer_memory_client);
    }
#endif
}

int host_roce_notify_add(struct host_roce_notify_node *notify_node)
{
    struct host_roce_notify_node *notify_node_tmp = NULL;

    pr_info("notify_va[0x%llx] notify_sz[0x%llx] tgid[%d]\n", notify_node->va,
        notify_node->sz, current->tgid);
    notify_node->tgid = current->tgid;

    mutex_lock(&host_roce_notify_lock);
    list_for_each_entry(notify_node_tmp, &host_roce_notify_list, node) {
        if ((notify_node_tmp == notify_node) ||
            (notify_node_tmp->va == notify_node->va && notify_node_tmp->pa == notify_node->pa &&
             notify_node_tmp->tgid == notify_node->tgid)) {
            mutex_unlock(&host_roce_notify_lock);
            pr_debug("notify_tgid[%d] notify_va[0x%llx] notify_sz[0x%llx] have exit\n",
                notify_node->tgid, notify_node->va, notify_node->sz);
            return -EINVAL;
        }
    }

    list_add_tail(&notify_node->node, &host_roce_notify_list);
    mutex_unlock(&host_roce_notify_lock);
    return 0;
}

void host_roce_notify_del(struct host_roce_notify_info *notify_node)
{
    struct host_roce_notify_node *notify_node_tmp = NULL;

    mutex_lock(&host_roce_notify_lock);

    list_for_each_entry(notify_node_tmp, &host_roce_notify_list, node) {
        if (notify_node_tmp->va == notify_node->va && notify_node_tmp->sz == notify_node->sz &&
            notify_node_tmp->tgid == current->tgid) {
            list_del(&notify_node_tmp->node);
            kfree(notify_node_tmp);
            notify_node_tmp = NULL;
            break;
        }
    }

    mutex_unlock(&host_roce_notify_lock);
    return;
}

void host_roce_notify_clear(void)
{
    struct host_roce_notify_node *notify_node_tmp = NULL;
    struct host_roce_notify_node *n = NULL;

    mutex_lock(&host_roce_notify_lock);

    list_for_each_entry_safe(notify_node_tmp, n, &host_roce_notify_list, node) {
        if (notify_node_tmp->tgid == current->tgid) {
            list_del(&notify_node_tmp->node);
            kfree(notify_node_tmp);
            notify_node_tmp = NULL;
        }
    }

    mutex_unlock(&host_roce_notify_lock);
    return;
}
