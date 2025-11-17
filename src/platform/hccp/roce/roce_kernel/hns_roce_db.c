/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/platform_device.h>
#include <rdma/ib_umem.h>
#include "roce_k_compat.h"
#include "hns_roce_device.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

int hns_roce_db_map_user(struct hns_roce_ucontext *context, unsigned long virt,
                         struct hns_roce_db *db)
{
    struct hns_roce_user_db_page *page = NULL;
    int ret = 0;

    mutex_lock(&context->page_mutex);

    list_for_each_entry(page, &context->page_list, list)
    if (page->user_virt == (virt & PAGE_MASK)) {
        goto found;
    }

    page = kzalloc(sizeof(*page), GFP_KERNEL);
    if (page == NULL) {
        pr_err("hns3: kmalloc hns_roce_user_db_page failed\n");
        ret = -ENOMEM;
        goto out;
    }

    refcount_set(&page->refcount, 1);
    page->user_virt = (virt & PAGE_MASK);
    page->umem = ib_umem_get(context->ibucontext.device, virt & PAGE_MASK,
                             PAGE_SIZE, 0);
    if (IS_ERR_OR_NULL(page->umem)) {
        ret = PTR_ERR(page->umem);
        kfree(page);
        page = NULL;
        pr_err("hns3: ib_umem_get fail, ret:[%d]\n", ret);
        goto out;
    }

    list_add(&page->list, &context->page_list);

found:
    db->dma = sg_dma_address(page->umem->sg_head.sgl) +
              (virt & ~PAGE_MASK);
    page->umem->sg_head.sgl->offset = virt & ~PAGE_MASK;
    db->virt_addr = sg_virt(page->umem->sg_head.sgl);
    db->u.user_page = page;
    atomic_inc(&page->refcount.refs);

out:
    mutex_unlock(&context->page_mutex);

    return ret;
}

void hns_roce_db_unmap_user(struct hns_roce_ucontext *context,
                            struct hns_roce_db *db)
{
    mutex_lock(&context->page_mutex);

    atomic_dec(&db->u.user_page->refcount.refs);
    if (atomic_cmpxchg_release(&db->u.user_page->refcount.refs, 1, 0) == 1) {
        list_del(&db->u.user_page->list);
        ib_umem_release(db->u.user_page->umem);
        kfree(db->u.user_page);
        db->u.user_page = NULL;
    }

    mutex_unlock(&context->page_mutex);
}

static struct hns_roce_db_pgdir *hns_roce_alloc_db_pgdir(
    struct device *dma_device)
{
    struct hns_roce_db_pgdir *pgdir;

    pgdir = kzalloc(sizeof(*pgdir), GFP_KERNEL);
    if (pgdir == NULL) {
        dev_err(dma_device, "kzalloc hns_roce_db_pgdir failed\n");
        return NULL;
    }

    bitmap_fill(pgdir->order1,
                HNS_ROCE_DB_PER_PAGE / HNS_ROCE_DB_TYPE_COUNT);
    pgdir->bits[0] = pgdir->order0;
    pgdir->bits[1] = pgdir->order1;
    pgdir->page = dma_alloc_coherent(dma_device, PAGE_SIZE,
                                     &pgdir->db_dma, GFP_KERNEL | __GFP_NOWARN);
    if (pgdir->page == NULL) {
        kfree(pgdir);
        pgdir = NULL;
        dev_err(dma_device, "dma_alloc_coherent hns_roce_db_pgdir page failed\n");
        return NULL;
    }

    return pgdir;
}

static int hns_roce_alloc_db_from_pgdir(struct hns_roce_db_pgdir *pgdir,
                                        struct hns_roce_db *db, int order)
{
    unsigned int o;
    unsigned int i;

    if (db == NULL) {
        return -ENOMEM;
    }

    for (o = order; o <= 1; ++o) {
        i = find_first_bit(pgdir->bits[o], HNS_ROCE_DB_PER_PAGE >> o);
        if (i < (HNS_ROCE_DB_PER_PAGE >> o)) {
            goto found;
        }
    }

    return -ENOMEM;

found:
    clear_bit(i, pgdir->bits[o]);

    i <<= o;

    if ((int)o > order) {
        set_bit(i ^ 1, pgdir->bits[order]);
    }

    db->u.pgdir   = pgdir;
    db->index     = i;
    db->db_record = pgdir->page + db->index;
    db->dma       = pgdir->db_dma + db->index * HNS_ROCE_DB_UNIT_SIZE;
    db->order     = order;

    return 0;
}

int hns_roce_alloc_db(struct hns_roce_dev *hr_dev, struct hns_roce_db *db,
                      int order)
{
    struct hns_roce_db_pgdir *pgdir = NULL;
    int ret = 0;

    mutex_lock(&hr_dev->pgdir_mutex);

    list_for_each_entry(pgdir, &hr_dev->pgdir_list, list)
    if (!hns_roce_alloc_db_from_pgdir(pgdir, db, order)) {
        goto out;
    }

    pgdir = hns_roce_alloc_db_pgdir(hr_dev->dev);
    if (pgdir == NULL) {
        dev_err(hr_dev->dev, "alloc db_pgdir failed\n");
        ret = -ENOMEM;
        goto out;
    }

    list_add(&pgdir->list, &hr_dev->pgdir_list);

    /* This should never fail -- we just allocated an empty page: */
    WARN_ON(hns_roce_alloc_db_from_pgdir(pgdir, db, order));

out:
    mutex_unlock(&hr_dev->pgdir_mutex);

    return ret;
}

void hns_roce_free_db(struct hns_roce_dev *hr_dev, struct hns_roce_db *db)
{
    int o;
    int i;

    mutex_lock(&hr_dev->pgdir_mutex);

    o = db->order;
    i = db->index;

    if (db->order == 0 && test_bit((unsigned int)i ^ 1, db->u.pgdir->order0)) {
        clear_bit((unsigned int)i ^ 1, db->u.pgdir->order0);
        ++o;
    }

    i = (unsigned int)i >> (unsigned int)o;
    set_bit(i, db->u.pgdir->bits[o]);

    if (bitmap_full(db->u.pgdir->order1,
                    HNS_ROCE_DB_PER_PAGE / HNS_ROCE_DB_TYPE_COUNT)) {
        dma_free_coherent(hr_dev->dev, PAGE_SIZE, db->u.pgdir->page,
                          db->u.pgdir->db_dma);
        list_del(&db->u.pgdir->list);
        kfree(db->u.pgdir);
        db->u.pgdir = NULL;
    }

    mutex_unlock(&hr_dev->pgdir_mutex);
}
