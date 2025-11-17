/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/scatterlist.h>

#include <linux/vmalloc.h>
#include <linux/types.h>

#include "roce_k_compat.h"
#include "hns_roce_peer.h"
#include "hns_roce_sec.h"
#include "hns_roce_common.h"
#include "hns_roce_notify.h"

#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX 16
#define PEER_MEM_U64_CORE_CONTEXT

invalidate_peer_memory g_hns_invalidate_callback;
#define NOTIFY_DRV_NAME         "HNS RoCE Notify"
#define NOTIFY_DRV_VERSION      "1.0"
#define NOTIFY_INITED_FLAG      0xF1234567UL

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
    struct hns_roce_notify_node *notify_node;
    struct hns_roce_notify_block blks[0];
};

static LIST_HEAD(hns_roce_notify_list);
static DEFINE_MUTEX(hns_roce_notify_lock);

STATIC unsigned long hns_roce_notify_get_page_size(void *context)
{
    return PAGE_SIZE;
}

STATIC int hns_roce_notify_acquire(unsigned long addr, size_t size, void *peer_mem_private_data,
    char *peer_mem_name, void **client_context)
{
    int page_num;
    unsigned long va_aligned_start, va_aligned_end;
    struct hns_roce_notify_context *hns_context = NULL;
    struct hns_roce_notify_node *notify_node = NULL;

    pr_info("hns3: enter addr=0x%lx, size=%lu.\n", addr, size);
    mutex_lock(&hns_roce_notify_lock);
    list_for_each_entry(notify_node, &hns_roce_notify_list, node) {
        if ((addr < (notify_node->va + notify_node->sz)) && (addr >= notify_node->va) &&
            (current->pid == notify_node->pid)) {
            va_aligned_start = addr & PAGE_MASK;
            va_aligned_end = (addr + size + PAGE_SIZE - 1) & PAGE_MASK;
            page_num = (va_aligned_end - va_aligned_start) / PAGE_SIZE;
            hns_context = kzalloc((sizeof(*hns_context)) +
                                  page_num * sizeof(struct hns_roce_notify_block),
                                  GFP_ATOMIC | __GFP_ACCOUNT);
            if (hns_context == NULL) {
                mutex_unlock(&hns_roce_notify_lock);
                pr_err("hns3: kzalloc err. page_num=%d\n", page_num);
                return 0;
            }

            hns_context->inited_flag  = NOTIFY_INITED_FLAG;
            hns_context->is_callback  = 0;
            hns_context->sg_allocated = 0;
            hns_context->va = addr;
            hns_context->len = size;
            hns_context->va_aligned_start = va_aligned_start;
            hns_context->va_aligned_end = va_aligned_end;
            hns_context->blk_num = page_num;
            hns_context->notify_node = notify_node;
            *client_context = hns_context;
            pr_info("hns3: acquire exit. succ.\n");
            mutex_unlock(&hns_roce_notify_lock);
            return 1;
        }
    }
    mutex_unlock(&hns_roce_notify_lock);

    return 0;
}

STATIC int hns_roce_notify_dma_map(struct sg_table *sg_head, void *context,
    struct device *dma_device, int dmasync,
    int *nmap)
{
    int k, ret;
    struct hns_roce_notify_context *hns_context =
        (struct hns_roce_notify_context *) context;
    struct scatterlist *sg = NULL;

    if (sg_head == NULL || nmap == NULL) {
        pr_err("hns3: get null ptr, sg_head: %pK, nmap: %pK\n", sg_head, nmap);
        return -EINVAL;
    }

    if ((hns_context == NULL) || hns_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: mm_context is not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return -EINVAL;
    }

    ret = sg_alloc_table(sg_head, hns_context->blk_num, GFP_KERNEL | __GFP_ACCOUNT);
    if (ret) {
        pr_err("hns3: sg alloc table failed, blk_num=%u.\n", hns_context->blk_num);
        return ret;
    }

    hns_context->sg_allocated = 1;
    for_each_sg(sg_head->sgl, sg, (int)hns_context->blk_num, k) {
        sg_set_page(sg, NULL, hns_roce_notify_get_page_size(hns_context), 0);
        sg->dma_address = hns_context->blks[k].dma;
        sg->dma_length = hns_context->blks[k].sz;
    }

    *nmap = hns_context->blk_num;

    pr_info("hns3: dma map exit. suc\n");
    return 0;
}

STATIC int hns_roce_notify_dma_unmap(struct sg_table *sg_head, void *context,
    struct device  *dma_device)
{
    struct hns_roce_notify_context *hns_context =
        (struct hns_roce_notify_context *) context;

    if ((hns_context == NULL) || hns_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: invalid hns_context or not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return -EINVAL;
    }

    if (hns_context->sg_allocated) {
        sg_free_table(sg_head);
        hns_context->sg_allocated = 0;
    }

    pr_info("hns3: dma unmap exit. suc\n");
    return 0;
}

STATIC void hns_roce_notify_put_pages(struct sg_table *sg_head, void *context)
{
    struct hns_roce_notify_context *hns_context =
        (struct hns_roce_notify_context *) context;

    if ((hns_context == NULL) || hns_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: invalid hns_context or not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return;
    }

    if (hns_context->sg_allocated) {
        sg_free_table(sg_head);
        hns_context->sg_allocated = 0;
    }

    pr_info("hns3: put page exit. suc\n");
}

STATIC void hns_roce_notify_release(void *context)
{
    kfree(context);
    context = NULL;
    pr_info("hns3: release exit.\n");
}

STATIC int hns_roce_notify_get_pages(unsigned long addr,
    size_t size, int write, int force,
    struct sg_table *sg_head,
    void *client_context,
    u64 core_context)
{
    int k;
    struct hns_roce_notify_context *hns_context = NULL;

    pr_info("hns3: get page enter,addr=0x%lx,size=%lu.\n", addr, size);
    hns_context = (struct hns_roce_notify_context *)client_context;
    if (hns_context == NULL || hns_context->inited_flag != NOTIFY_INITED_FLAG) {
        pr_err("hns3: invalid hns_context or not inited, inited flag[%lu]\n", NOTIFY_INITED_FLAG);
        return -EINVAL;
    }

    hns_context->blk_num = (hns_context->va_aligned_end -
                            hns_context->va_aligned_start) / PAGE_SIZE;

    for (k = 0; k < (int)hns_context->blk_num; k++) {
        hns_context->blks[k].pa = hns_context->notify_node->pa + k * hns_roce_notify_get_page_size(hns_context);
        hns_context->blks[k].sz = hns_roce_notify_get_page_size(hns_context);
        hns_context->blks[k].dma = hns_context->blks[k].pa;
        pr_debug("k[%d] pa=0x%lx dma=0x%llx\n",
               k, (unsigned long)hns_context->blks[k].pa, hns_context->blks[k].dma);
    }

    return 0;
}

static struct peer_memory_client g_hns_roce_notify_client = {
    .acquire    = hns_roce_notify_acquire,
    .get_pages  = hns_roce_notify_get_pages,
    .dma_map    = hns_roce_notify_dma_map,
    .dma_unmap  = hns_roce_notify_dma_unmap,
    .put_pages  = hns_roce_notify_put_pages,
    .get_page_size  = hns_roce_notify_get_page_size,
    .release    = hns_roce_notify_release,
};

void *hns_roce_notify_client_init(void)
{
    size_t lenth;
    int ret;
    lenth = strlen(NOTIFY_DRV_NAME);
    lenth = lenth > IB_PEER_MEMORY_NAME_MAX ? IB_PEER_MEMORY_NAME_MAX : lenth;
    ret = memcpy_s(g_hns_roce_notify_client.name, sizeof(g_hns_roce_notify_client.name),
                   NOTIFY_DRV_NAME, lenth);
    HNS_ROCE_NOTIFY_SEC_CHECK_RET_NULL(ret);

    lenth = strlen(NOTIFY_DRV_VERSION);
    lenth = lenth > IB_PEER_MEMORY_VER_MAX ? IB_PEER_MEMORY_VER_MAX : lenth;
    ret = memcpy_s(g_hns_roce_notify_client.version, sizeof(g_hns_roce_notify_client.version),
                   NOTIFY_DRV_VERSION, lenth);
    HNS_ROCE_NOTIFY_SEC_CHECK_RET_NULL(ret);
    return ib_register_peer_memory_client(&g_hns_roce_notify_client,
                                          &g_hns_invalidate_callback);
}

void hns_roce_notify_client_cleanup(char *priv)
{
    ib_unregister_peer_memory_client(priv);
}

int hns_roce_notify_add(struct hns_roce_notify_node *notify_node)
{
    struct hns_roce_notify_node *notify_node_tmp = NULL;
    pr_info("hns3: notify_va[0x%llx] notify_sz[0x%llx] pid[%d]\n", notify_node->va,
        notify_node->sz, current->pid);
    notify_node->pid = current->pid;

    mutex_lock(&hns_roce_notify_lock);
    list_for_each_entry(notify_node_tmp, &hns_roce_notify_list, node) {
        if ((notify_node_tmp == notify_node)
                || (notify_node_tmp->va == notify_node->va && notify_node_tmp->pa == notify_node->pa &&
                    notify_node_tmp->pid == notify_node->pid)) {
            mutex_unlock(&hns_roce_notify_lock);
            pr_debug("notify_pid[%d] notify_va[0x%llx] notify_pa[0x%llx] notify_sz[0x%llx] have exit\n",
                notify_node->pid, notify_node->va, notify_node->pa, notify_node->sz);
            return 0;
        }
    }

    list_add_tail(&notify_node->node, &hns_roce_notify_list);
    mutex_unlock(&hns_roce_notify_lock);
    return 0;
}

void hns_roce_notify_del(struct hns_roce_notify_node *notify_node)
{
    struct hns_roce_notify_node *notify_node_tmp = NULL;

    mutex_lock(&hns_roce_notify_lock);

    list_for_each_entry(notify_node_tmp, &hns_roce_notify_list, node) {
        if (notify_node_tmp == notify_node) {
            list_del(&notify_node->node);
            break;
        }
    }

    mutex_unlock(&hns_roce_notify_lock);
}
