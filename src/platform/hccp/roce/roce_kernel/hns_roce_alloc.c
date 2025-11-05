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
#include <linux/vmalloc.h>
#include "roce_k_compat.h"
#include "hns_roce_device.h"

#define HNS_ROCE_BUF_NUM_TWO        2

int hns_roce_bitmap_alloc(struct hns_roce_bitmap *bitmap, unsigned long *obj)
{
    int ret = 0;

    spin_lock(&bitmap->lock);
    *obj = find_next_zero_bit(bitmap->table, bitmap->max, bitmap->last);
    if (*obj >= bitmap->max) {
        bitmap->top = (bitmap->top + bitmap->max + bitmap->reserved_top)
                      & bitmap->mask;
        *obj = find_first_zero_bit(bitmap->table, bitmap->max);
    }

    if (*obj < bitmap->max) {
        set_bit(*obj, bitmap->table);
        bitmap->last = (*obj + 1);
        if (bitmap->last == bitmap->max) {
            bitmap->last = 0;
        }
        *obj |= bitmap->top;
    } else {
        ret = -1;
    }

    spin_unlock(&bitmap->lock);

    return ret;
}

void hns_roce_bitmap_free(struct hns_roce_bitmap *bitmap, unsigned long obj,
                          int rr)
{
    hns_roce_bitmap_free_range(bitmap, obj, 1, rr);
}

int hns_roce_bitmap_alloc_range(struct hns_roce_bitmap *bitmap, int cnt,
                                int align, unsigned long *obj)
{
    int ret = 0;
    int i;

    if (likely(cnt == 1 && align == 1)) {
        return hns_roce_bitmap_alloc(bitmap, obj);
    }

    spin_lock(&bitmap->lock);

    *obj = bitmap_find_next_zero_area(bitmap->table, bitmap->max,
                                      bitmap->last, cnt, align - 1);
    if (*obj >= bitmap->max) {
        bitmap->top = (bitmap->top + bitmap->max + bitmap->reserved_top)
                      & bitmap->mask;
        *obj = bitmap_find_next_zero_area(bitmap->table, bitmap->max, 0,
                                          cnt, align - 1);
    }

    if (*obj < bitmap->max) {
        for (i = 0; i < cnt; i++) {
            set_bit(*obj + i, bitmap->table);
        }

        if (*obj == bitmap->last) {
            bitmap->last = (*obj + cnt);
            if (bitmap->last >= bitmap->max) {
                bitmap->last = 0;
            }
        }
        *obj |= bitmap->top;
    } else {
        ret = -1;
    }

    spin_unlock(&bitmap->lock);

    return ret;
}

void hns_roce_bitmap_free_range(struct hns_roce_bitmap *bitmap,
                                unsigned long obj, int cnt,
                                int rr)
{
    unsigned long base;
    int i;

    spin_lock(&bitmap->lock);
    base = obj & (bitmap->max + bitmap->reserved_top - 1);
    for (i = 0; i < cnt; i++) {
        clear_bit(base + i, bitmap->table);
    }

    if (!rr) {
        bitmap->last = min(bitmap->last, base);
    }
    bitmap->top = (bitmap->top + bitmap->max + bitmap->reserved_top)
                  & bitmap->mask;
    spin_unlock(&bitmap->lock);
}

int hns_roce_bitmap_init(struct hns_roce_bitmap *bitmap, u32 num, u32 mask,
                         u32 reserved_bot, u32 reserved_top)
{
    u32 i;

    if (num != (u32)roundup_pow_of_two(num)) {
        pr_err("hns3: Invalid num : %u\n", num);
        return -EINVAL;
    }

    bitmap->last = 0;
    bitmap->top = 0;
    bitmap->max = num - reserved_top;
    bitmap->mask = mask;
    bitmap->reserved_top = reserved_top;
    spin_lock_init(&bitmap->lock);
    bitmap->table = kcalloc(BITS_TO_LONGS(bitmap->max), sizeof(long),
                            GFP_KERNEL);
    if (bitmap->table == NULL) {
        pr_err("hns3: alloc hns_roce_bitmap table failed\n");
        return -ENOMEM;
    }

    for (i = 0; i < reserved_bot; ++i) {
        set_bit(i, bitmap->table);
    }

    return 0;
}

void hns_roce_bitmap_cleanup(struct hns_roce_bitmap *bitmap)
{
    kfree(bitmap->table);
    bitmap->table = NULL;
}

void hns_roce_buf_free(struct hns_roce_dev *hr_dev, u32 size,
                       struct hns_roce_buf *buf)
{
    int i;
    struct device *dev = hr_dev->dev;

    if (buf->nbufs == 1) {
        dma_free_coherent(dev, size, buf->direct.buf, buf->direct.map);
        buf->direct.buf = NULL;
    } else {
        for (i = 0; i < buf->nbufs; ++i) {
            if (buf->page_list[i].buf) {
                dma_free_coherent(dev, 1U << (unsigned int)buf->page_shift, buf->page_list[i].buf,
                    buf->page_list[i].map);
                buf->page_list[i].buf = NULL;
            }
        }
        kfree(buf->page_list);
        buf->page_list = NULL;
    }
}

#ifndef DEFINE_HNS_LLT
static void hns_roce_nbufs_free(struct hns_roce_dev *hr_dev, int i, struct hns_roce_buf *buf)
{
    for (i--; i >= 0; --i) {
        dma_free_coherent(hr_dev->dev, 1U << (unsigned int)buf->page_shift,
            buf->page_list[i].buf, buf->page_list[i].map);
        buf->page_list[i].buf = NULL;
    }
    kfree(buf->page_list);
    buf->page_list = NULL;
}
#endif

int hns_roce_buf_alloc(struct hns_roce_dev *hr_dev, u32 size, u32 max_direct,
                       struct hns_roce_buf *buf, u32 page_shift)
{
    int i = 0;
    dma_addr_t t;
    struct device *dev = hr_dev->dev;
    u32 page_size = 1U << page_shift;
    u32 order;

    /* buf for SQ/RQ both at lease one page, SQ + RQ is 2 pages */
    if (size <= max_direct) {
        buf->nbufs = 1;
        /* Npages calculated by page_size */
        order = get_order(size);
        order = (order <= (page_shift - PAGE_SHIFT)) ? 0 : order - (page_shift - PAGE_SHIFT);
        buf->npages = 1U << order;
        buf->page_shift = page_shift;
        /* MTT PA must be recorded in 4k alignment, t is 4k aligned */
        buf->direct.buf = dma_zalloc_coherent(dev, size, &t, GFP_KERNEL);
        if (!buf->direct.buf) {
            dev_err(dev, "dma_zalloc_coherent hnc_roce_buf failed\n");
            return -ENOMEM;
        }

        buf->direct.map = t;

        while (t & ((1UL << (unsigned int)buf->page_shift) - 1)) {
            --buf->page_shift;
            buf->npages *= HNS_ROCE_BUF_NUM_TWO;
        }
    } else {
        buf->nbufs = (size + page_size - 1) / page_size;
        buf->npages = buf->nbufs;
        buf->page_shift = page_shift;
        buf->page_list = kcalloc(buf->nbufs, sizeof(*buf->page_list), GFP_KERNEL);

        if (buf->page_list == NULL) {
            dev_err(dev, "dma_zalloc_coherent hns_roce_buff page_list failed\n");
            return -ENOMEM;
        }

        for (i = 0; i < buf->nbufs; ++i) {
            buf->page_list[i].buf = dma_zalloc_coherent(dev, page_size, &t, GFP_KERNEL);
            if (buf->page_list[i].buf == NULL) {
                dev_err(dev, "dma_zalloc_coherent hns_roce_buff page's buff failed\n");
                goto err_free;
            }

            buf->page_list[i].map = t;
        }
    }

    return 0;

err_free:
#ifndef DEFINE_HNS_LLT
    hns_roce_nbufs_free(hr_dev, i, buf);
#endif
    return -ENOMEM;
}

void hns_roce_cleanup_bitmap(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SRQ) {
        hns_roce_cleanup_srq_table(hr_dev);
    }
    hns_roce_cleanup_atu_table(hr_dev);
    hns_roce_cleanup_qp_table(hr_dev);
    hns_roce_cleanup_cq_table(hr_dev);
    hns_roce_cleanup_mr_table(hr_dev);
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC) {
        hns_roce_cleanup_xrcd_table(hr_dev);
    }
    hns_roce_cleanup_pd_table(hr_dev);
    hns_roce_uar_free(hr_dev, &hr_dev->priv_uar);
    hns_roce_cleanup_uar_table(hr_dev);
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
        mutex_destroy(&hr_dev->pgdir_mutex);
    }
}
