/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/hugetlb.h>
#include <linux/sched.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_umem_odp.h>

#include "roce_k_compat.h"
#include "hns_roce_sec.h"
#include "hns_roce_peer.h"

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

static DEFINE_MUTEX(peer_memory_mutex);
static LIST_HEAD(peer_memory_list);

static void complete_peer(struct kref *kref)
{
    struct ib_peer_memory_client *ib_peer_client =
        container_of(kref, struct ib_peer_memory_client, ref);

    complete(&ib_peer_client->unload_comp);
}

void *ib_register_peer_memory_client(const struct peer_memory_client *peer_client,
    invalidate_peer_memory *invalidate_callback)
{
    struct ib_peer_memory_client *ib_peer_client = NULL;

    if (peer_client == NULL) {
        pr_err("hns3: register peer client param err client is NULL\n");
        return NULL;
    }

    ib_peer_client = kzalloc(sizeof(*ib_peer_client), GFP_KERNEL);
    if (ib_peer_client == NULL) {
        pr_err("hns3: kzalloc ib peer memory client failed\n");
        return NULL;
    }

    INIT_LIST_HEAD(&ib_peer_client->core_ticket_list);
    init_completion(&ib_peer_client->unload_comp);
    kref_init(&ib_peer_client->ref);
    ib_peer_client->peer_mem = peer_client;

    /* Once peer supplied a non NULL callback it's an indication that
     * invalidation support is required for any memory owning.
     */
    if (invalidate_callback != NULL) {
        *invalidate_callback = NULL;
        ib_peer_client->invalidation_required = 0;
    }
    ib_peer_client->last_ticket = 1;

    mutex_lock(&peer_memory_mutex);
    list_add_tail(&ib_peer_client->core_peer_list, &peer_memory_list);
    mutex_unlock(&peer_memory_mutex);

    return ib_peer_client;
}
EXPORT_SYMBOL(ib_register_peer_memory_client);

void ib_unregister_peer_memory_client(void *reg_handle)
{
    struct ib_peer_memory_client *ib_peer_client = NULL;
    if (reg_handle == NULL) {
        pr_err("hns3: unregister peer client param err reg_handle is NULL\n");
        return;
    }

    ib_peer_client = reg_handle;

    mutex_lock(&peer_memory_mutex);
    list_del(&ib_peer_client->core_peer_list);
    mutex_unlock(&peer_memory_mutex);

    kref_put(&ib_peer_client->ref, complete_peer);
    wait_for_completion(&ib_peer_client->unload_comp);
    kfree(ib_peer_client);
    ib_peer_client = NULL;
}
EXPORT_SYMBOL(ib_unregister_peer_memory_client);

STATIC struct ib_peer_memory_client *ib_get_peer_client(struct ib_ucontext *context, unsigned long addr,
                                                        size_t size, unsigned long peer_mem_flags,
                                                        struct hns_roce_umem *hr_umem)
{
    struct ib_peer_memory_client *ib_peer_client = NULL;
    struct peer_memory_data peer_memory_data;
    int ret;
    struct hns_roce_dev *hr_dev = NULL;
    void *peer_client_context = NULL;

    hr_dev = (struct hns_roce_dev *)context->device;

    peer_memory_data.host_pid = hr_umem->psign.tgid;
    peer_memory_data.devid = hr_umem->psign.devid;
    peer_memory_data.vfid = hr_umem->psign.vfid;
    ret = memcpy_s(peer_memory_data.pid_sign, PROCESS_SIGN_LENGTH, hr_umem->psign.sign, PROCESS_SIGN_LENGTH);
    if (ret) {
        pr_err("hns3: ib_get_peer_client memcpy failed, ret[%d]\n", ret);
        return NULL;
    }

    mutex_lock(&peer_memory_mutex);
    list_for_each_entry(ib_peer_client, &peer_memory_list, core_peer_list) {
        ret = ib_peer_client->peer_mem->acquire(addr, size,
                                                &peer_memory_data,
                                                hr_dev->peer_mem_name,
                                                &peer_client_context);
        if (ret > 0) {
            goto found;
        }
    }

    ib_peer_client = NULL;

found:
    if (ib_peer_client != NULL) {
        kref_get(&ib_peer_client->ref);
    }

    hr_umem->peer_mem_client = ib_peer_client;
    hr_umem->peer_mem_client_context = peer_client_context;
    mutex_unlock(&peer_memory_mutex);
    return ib_peer_client;
}

STATIC void ib_put_peer_client(struct ib_peer_memory_client *ib_peer_client,
    char *peer_client_context)
{
    if (ib_peer_client->peer_mem->release) {
        ib_peer_client->peer_mem->release(peer_client_context);
    }

    kref_put(&ib_peer_client->ref, complete_peer);
}

int hns_roce_init_atu_table(struct hns_roce_dev *hr_dev)
{
    return hns_roce_bitmap_init(&hr_dev->atu_bitmap, hr_dev->caps.num_pds,
                                hr_dev->caps.num_pds - 1,
                                hr_dev->caps.reserved_pds, 0);
}

void hns_roce_cleanup_atu_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_bitmap_cleanup(&hr_dev->atu_bitmap);
}

int hns_roce_atu_alloc(struct hns_roce_dev *hr_dev, unsigned long *idx)
{
    return hns_roce_bitmap_alloc(&hr_dev->atu_bitmap, idx);
}

void hns_roce_atu_free(struct hns_roce_dev *hr_dev, unsigned long idx)
{
    hns_roce_bitmap_free(&hr_dev->atu_bitmap, idx, BITMAP_NO_RR);
}

STATIC struct ib_umem *peer_umem_get(struct ib_peer_memory_client *ib_peer_mem, struct ib_umem *umem,
    unsigned long addr, int dmasync, char *peer_client_context, unsigned int *page_shift)
{
    const struct peer_memory_client *peer_mem = ib_peer_mem->peer_mem;
    unsigned int npages;
    int ret;

    /*
     * We always request write permissions to the pages, to force breaking of any CoW
     * during the registration of the MR. For read-only MRs we use the "force" flag to
     * indicate that CoW breaking is required but the registration should not fail if
     * referencing read-only areas.
     */
    ret = peer_mem->get_pages(addr, umem->length,
                              1, !umem->writable,
                              &umem->sg_head,
                              peer_client_context,
                              0);
    if (ret) {
        pr_err("hns3: peer_mem get pages failed, ret[%d]\n", ret);
        goto end;
    }

    *page_shift = ilog2(peer_mem->get_page_size(peer_client_context));

    ret = peer_mem->dma_map(&umem->sg_head,
                            peer_client_context,
                            NULL,
                            dmasync,
                            &umem->nmap);
    if (ret) {
        pr_err("hns3: peer_mem dma map failed, ret[%d]\n", ret);
        goto put_pages;
    }

    npages = (umem->length + (1 << *page_shift) - 1) / (1 << *page_shift);

    atomic64_add(npages, &ib_peer_mem->stats.num_reg_pages);
    atomic64_add(npages << *page_shift, &ib_peer_mem->stats.num_reg_bytes);
    atomic64_inc(&ib_peer_mem->stats.num_alloc_mrs);
    return umem;

put_pages:
    peer_mem->put_pages(&umem->sg_head, peer_client_context);
end:
    ib_put_peer_client(ib_peer_mem, peer_client_context);
    kfree(umem);
    umem = NULL;
    return ERR_PTR(ret);
}

STATIC void peer_umem_release(struct ib_umem *umem, struct hns_roce_umem *hr_umem)
{
    struct ib_peer_memory_client *ib_peer_mem = NULL;
    const struct peer_memory_client *peer_mem = NULL;
    int ret;

    ib_peer_mem = hr_umem->peer_mem_client;
    peer_mem = ib_peer_mem->peer_mem;

    ret = peer_mem->dma_unmap(&umem->sg_head,
        hr_umem->peer_mem_client_context,
        NULL);
    if (ret) {
        pr_err("hns3: peer_mem dma unmap failed, ret[%d]\n", ret);
    }

    peer_mem->put_pages(&umem->sg_head,
                        hr_umem->peer_mem_client_context);

    atomic64_add(hr_umem->npages, &ib_peer_mem->stats.num_dereg_pages);
    atomic64_add(hr_umem->npages << hr_umem->page_shift, &ib_peer_mem->stats.num_dereg_bytes);
    atomic64_inc(&ib_peer_mem->stats.num_dealloc_mrs);
    ib_put_peer_client(ib_peer_mem, hr_umem->peer_mem_client_context);
    kfree(umem);
    umem = NULL;
}

/**
 * hns_roce_umem_get - Pin and DMA map userspace memory.
 *
 * If access flags indicate ODP memory, avoid pinning. Instead, stores
 * the mm for future page fault handling in conjunction with MMU notifiers.
 *
 * @context: userspace context to pin memory for
 * @user_mr: start is userspace virtual address to start at
 * length of region to pin,  access_flags is IB_ACCESS_xxx flags for memory being pinned
 * @dmasync: flush in-flight DMA when the memory region is written
 * @peer_mem_flags: IB_PEER_MEM_xxx flags for memory being used
 */
struct ib_umem *hns_roce_umem_get(struct ib_ucontext *context, struct hns_roce_user_mr_info user_mr,
                                  int dmasync, unsigned long peer_mem_flags, struct hns_roce_umem *hr_umem)
{
    unsigned long va_aligned_start, va_aligned_end, aligned_size;
    unsigned long endaddr = user_mr.start + user_mr.length;
    struct ib_umem *umem = NULL;

    /*
     * If the combination of the addr and size requested for this memory
     * region causes an integer overflow, return error.
     */
    if ((endaddr < user_mr.start) ||
            PAGE_ALIGN(endaddr) < endaddr) {
        pr_err("hns3: %s: integer overflow, size=%lld\n", __func__, user_mr.length);
        return ERR_PTR(-EINVAL);
    }

    if (!can_do_mlock()) {
        pr_err("hns3: %s: no mlock permission\n", __func__);
        return ERR_PTR(-EPERM);
    }

    hr_umem->page_shift = PAGE_SHIFT;
    hr_umem->npages = (user_mr.length + (1 << PAGE_SHIFT) - 1) / (1 << PAGE_SHIFT);
    if (peer_mem_flags & IB_PEER_MEM_ALLOW) {
        hr_umem->peer_mem_client = ib_get_peer_client(context, user_mr.start, user_mr.length,
                                                      peer_mem_flags, hr_umem);
        if (hr_umem->peer_mem_client != NULL) {
            umem = kzalloc(sizeof(*umem), GFP_KERNEL);
            if (umem == NULL) {
                ib_put_peer_client(hr_umem->peer_mem_client, hr_umem->peer_mem_client_context);
                return ERR_PTR(-ENOMEM);
            }

            umem->length     = user_mr.length;
            umem->address    = user_mr.start;
            umem->writable  = !!((unsigned int)user_mr.access_flags &
                                 (IB_ACCESS_LOCAL_WRITE   | IB_ACCESS_REMOTE_WRITE |
                                  IB_ACCESS_REMOTE_ATOMIC | IB_ACCESS_MW_BIND));
            umem = peer_umem_get(hr_umem->peer_mem_client, umem, user_mr.start,
                                 dmasync, hr_umem->peer_mem_client_context, &hr_umem->page_shift);

            // recalc npages by va_aligned with updated page_shift
            va_aligned_start = round_down(user_mr.start, (1 << hr_umem->page_shift));
            va_aligned_end = round_up(endaddr, (1 << hr_umem->page_shift));
            aligned_size = va_aligned_end - va_aligned_start;
            hr_umem->npages = (aligned_size + (1 << hr_umem->page_shift) - 1) / (1 << hr_umem->page_shift);

            return umem;
        }
    }

    return ib_umem_get(context->device, user_mr.start, user_mr.length, user_mr.access_flags);
}

/**
 * hns_roce_umem_release - release memory pinned
 * @umem: umem struct to release
 */
void hns_roce_umem_release(struct ib_umem *umem, struct hns_roce_umem *hr_umem)
{
    if (hr_umem != NULL && hr_umem->peer_mem_client != NULL) {
        peer_umem_release(umem, hr_umem);
        umem = NULL;
        return;
    }

    ib_umem_release(umem);
}

