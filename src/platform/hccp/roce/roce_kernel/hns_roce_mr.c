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
#include <linux/version.h>
#include <rdma/ib_umem.h>
#include "hns-abi.h"
#include "roce_k_compat.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"
#include "hns_roce_peer.h"
#include "hns_roce_sec.h"
#ifdef CONFIG_INFINIBAND_HNS_TEST
#include "hns_roce_test.h"
#endif
#include "hns_roce_mr.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

static u32 hw_index_to_key(unsigned long ind)
{
    return (u32)(ind >> HIGH_OFFSET) | (ind << LOW_OFFSET);
}

STATIC unsigned long key_to_hw_index(u32 key)
{
    return (key << HIGH_OFFSET) | (key >> LOW_OFFSET);
}

static int hns_roce_sw2hw_mpt(struct hns_roce_dev *hr_dev,
                              struct hns_roce_cmd_mailbox *mailbox,
                              unsigned long mpt_index)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = mpt_index;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_SW2HW_MPT;
    return hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

STATIC int hns_roce_hw2sw_mpt(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox,
                              unsigned long mpt_index)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox ? mailbox->dma : 0;
    mbox_pst_params.in_modifier = mpt_index;
    mbox_pst_params.op_modifier = !mailbox;
    mbox_pst_params.op = HNS_ROCE_CMD_HW2SW_MPT;
    return hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

static int hns_roce_buddy_alloc(struct hns_roce_buddy *buddy, int order,
    unsigned long *seg)
{
    int o;
    u32 m;

    spin_lock(&buddy->lock);

    for (o = order; o <= buddy->max_order; ++o) {
        if (buddy->num_free[o]) {
            m = 1U << (unsigned int)(buddy->max_order - o);
            *seg = find_first_bit(buddy->bits[o], m);
            if (*seg < m) {
                goto found;
            }
        }
    }
    spin_unlock(&buddy->lock);
    pr_err("hns3: failed to find invalid seg\n");
    return -1;

found:
    clear_bit(*seg, buddy->bits[o]);
    --buddy->num_free[o];

    while (o > order) {
        --o;
        *seg <<= 1;
        set_bit(*seg ^ 1, buddy->bits[o]);
        ++buddy->num_free[o];
    }

    spin_unlock(&buddy->lock);

    *seg <<= (unsigned int)order;
    return 0;
}

static void hns_roce_buddy_free(struct hns_roce_buddy *buddy, unsigned long seg,
    int order)
{
    seg >>= (unsigned int)order;

    spin_lock(&buddy->lock);

    while (test_bit(seg ^ 1, buddy->bits[order])) {
        clear_bit(seg ^ 1, buddy->bits[order]);
        --buddy->num_free[order];
        seg >>= 1;
        ++order;
    }

    set_bit(seg, buddy->bits[order]);
    ++buddy->num_free[order];

    spin_unlock(&buddy->lock);
}

static int hns_roce_buddy_init(struct hns_roce_buddy *buddy, int max_order)
{
    size_t s;
    int i;

    buddy->max_order = max_order;
    spin_lock_init(&buddy->lock);
    buddy->bits = kcalloc(buddy->max_order + 1, sizeof(*buddy->bits), GFP_KERNEL);
    if (buddy->bits == NULL) {
        pr_err("hns3: alloc hns_roce buddy's bits failed\n");
        return -ENOMEM;
    }

    buddy->num_free = kcalloc(buddy->max_order + 1, sizeof(*buddy->num_free), GFP_KERNEL);
    if (buddy->num_free == NULL) {
        pr_err("hns3: alloc hns_roce buddy's member failed\n");
        goto err_num_free;
    }

    for (i = 0; i <= (int)buddy->max_order; ++i) {
        s = BITS_TO_LONGS(1U << (unsigned int)(buddy->max_order - i));
        buddy->bits[i] = kcalloc(s, sizeof(long), GFP_KERNEL | __GFP_NOWARN);
        if (buddy->bits[i] == NULL) {
            buddy->bits[i] = vzalloc(array_size(s, sizeof(long)));
            if (buddy->bits[i] == NULL) {
                pr_err("hns3: alloc hns_roce buddy's member bits[%d] failed\n" ,i);
                goto err_out_free;
            }
        }
    }

    set_bit(0, buddy->bits[buddy->max_order]);
    buddy->num_free[buddy->max_order] = 1;

    return 0;

err_out_free:
    for (i--; i >= 0; --i) {
        kvfree(buddy->bits[i]);
        buddy->bits[i] = NULL;
    }
    kfree(buddy->num_free);
    buddy->num_free = NULL;

err_num_free:
    kfree(buddy->bits);
    buddy->bits = NULL;
    return -ENOMEM;
}

static void hns_roce_buddy_cleanup(struct hns_roce_buddy *buddy)
{
    int i;

    for (i = 0; i <= buddy->max_order; ++i) {
        kvfree(buddy->bits[i]);
        buddy->bits[i] = NULL;
    }

    kfree(buddy->bits);
    buddy->bits = NULL;
    kfree(buddy->num_free);
    buddy->num_free = NULL;
}

static int hns_roce_alloc_mtt_range(struct hns_roce_dev *hr_dev, int order,
    unsigned long *seg, u32 mtt_type)
{
    struct hns_roce_mr_table *mr_table = &hr_dev->mr_table;
    struct hns_roce_hem_table *table = NULL;
    struct hns_roce_buddy *buddy = NULL;
    int ret;

    switch (mtt_type) {
        case MTT_TYPE_WQE:
            buddy = &mr_table->mtt_buddy;
            table = &mr_table->mtt_table;
            break;
        case MTT_TYPE_CQE:
            buddy = &mr_table->mtt_cqe_buddy;
            table = &mr_table->mtt_cqe_table;
            break;
        case MTT_TYPE_SRQWQE:
            buddy = &mr_table->mtt_srqwqe_buddy;
            table = &mr_table->mtt_srqwqe_table;
            break;
        case MTT_TYPE_IDX:
            buddy = &mr_table->mtt_idx_buddy;
            table = &mr_table->mtt_idx_table;
            break;
        default:
            dev_err(hr_dev->dev, "Unsupport MTT table type: %d\n",
                    mtt_type);
            return -EINVAL;
    }

    ret = hns_roce_buddy_alloc(buddy, order, seg);
    if (ret == -1) {
        dev_err(hr_dev->dev, "hns_roce alloc buddy failed\n");
        return -1;
    }

    if (hns_roce_table_get_range(hr_dev, table, *seg,
                                 *seg + (1U << (unsigned int)order) - 1)) {
        hns_roce_buddy_free(buddy, *seg, order);
        dev_err(hr_dev->dev, "hns_roce_table get range failed\n");
        return -1;
    }

    return 0;
}

int hns_roce_mtt_init(struct hns_roce_dev *hr_dev, u32 npages, int page_shift,
    struct hns_roce_mtt *mtt)
{
    unsigned int i;
    int ret;

    /* Page num is zero, correspond to DMA memory register */
    if (!npages) {
        mtt->order = -1;
        mtt->page_shift = HNS_ROCE_HEM_PAGE_SHIFT;
        return 0;
    }

    /* Note: if page_shift is zero, FAST memory register */
    mtt->page_shift = page_shift;

    /* Compute MTT entry necessary */
    for (mtt->order = 0, i = HNS_ROCE_MTT_ENTRY_PER_SEG; i < npages;
            i <<= 1) {
        ++mtt->order;
    }

    /* Allocate MTT entry */
    ret = hns_roce_alloc_mtt_range(hr_dev, mtt->order, &mtt->first_seg,
                                   mtt->mtt_type);
    if (ret != 0) {
        dev_err(hr_dev->dev, "hns_roce alloc mtt range failed\n");
        return -ENOMEM;
    }

    return 0;
}

void hns_roce_mtt_cleanup(struct hns_roce_dev *hr_dev, struct hns_roce_mtt *mtt)
{
    struct hns_roce_mr_table *mr_table = &hr_dev->mr_table;

    if (mtt->order < 0) {
        return;
    }

    switch (mtt->mtt_type) {
        case MTT_TYPE_WQE:
            hns_roce_buddy_free(&mr_table->mtt_buddy, mtt->first_seg,
                                mtt->order);
            hns_roce_table_put_range(hr_dev, &mr_table->mtt_table,
                                     mtt->first_seg,
                                     mtt->first_seg + (1UL << (unsigned int)mtt->order) - 1);
            break;
        case MTT_TYPE_CQE:
            hns_roce_buddy_free(&mr_table->mtt_cqe_buddy, mtt->first_seg,
                                mtt->order);
            hns_roce_table_put_range(hr_dev, &mr_table->mtt_cqe_table,
                                     mtt->first_seg,
                                     mtt->first_seg + (1UL << (unsigned int)mtt->order) - 1);
            break;
        case MTT_TYPE_SRQWQE:
            hns_roce_buddy_free(&mr_table->mtt_srqwqe_buddy, mtt->first_seg,
                                mtt->order);
            hns_roce_table_put_range(hr_dev, &mr_table->mtt_srqwqe_table,
                                     mtt->first_seg,
                                     mtt->first_seg + (1UL << (unsigned int)mtt->order) - 1);
            break;
        case MTT_TYPE_IDX:
            hns_roce_buddy_free(&mr_table->mtt_idx_buddy, mtt->first_seg,
                                mtt->order);
            hns_roce_table_put_range(hr_dev, &mr_table->mtt_idx_table,
                                     mtt->first_seg,
                                     mtt->first_seg + (1UL << (unsigned int)mtt->order) - 1);
            break;
        default:
            dev_err(hr_dev->dev, "Unsupport mtt type %d, clean mtt failed\n",
                    mtt->mtt_type);
    }
}

STATIC void hns_roce_pbl_mhop_free(struct hns_roce_dev *hr_dev,
    struct hns_roce_mr *mr, int err_loop_index, u32 loop_i, u32 loop_j)
{
    struct device *dev = hr_dev->dev;
    u32 mhop_num;
    u32 pbl_bt_sz;
    u64 bt_idx;
    int i;
    u32 j;

    pbl_bt_sz = 1U << (hr_dev->caps.pbl_ba_pg_sz + PAGE_SHIFT);
    mhop_num = hr_dev->caps.pbl_hop_num;

    i = (int)loop_i - 1;
    if (mhop_num == MHOP_NUM_3LEVELS && err_loop_index == ERRLOP_INDEX2) {
        for (; i >= 0; i--) {
            dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l1[i], mr->pbl_l1_dma_addr[i]);
            mr->pbl_bt_l1[i] = NULL;
            for (j = 0; (j < pbl_bt_sz / PBLSZ_BASE) && (i != (int)loop_i || j < loop_j); j++) {
                bt_idx = i * pbl_bt_sz / PBLSZ_BASE + j;
                dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l2[bt_idx], mr->pbl_l2_dma_addr[bt_idx]);
                mr->pbl_bt_l2[bt_idx] = NULL;
            }
        }
    } else if (mhop_num == MHOP_NUM_3LEVELS && err_loop_index == ERRLOP_INDEX1) {
        for (i -= 1; i >= 0; i--) {
            dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l1[i], mr->pbl_l1_dma_addr[i]);
            mr->pbl_bt_l1[i] = NULL;

            for (j = 0; j < pbl_bt_sz / PBLSZ_BASE; j++) {
                bt_idx = i * pbl_bt_sz / PBLSZ_BASE + j;
                dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l2[bt_idx], mr->pbl_l2_dma_addr[bt_idx]);
                mr->pbl_bt_l2[bt_idx] = NULL;
            }
        }
    } else if (mhop_num == MHOP_NUM_2LEVELS && err_loop_index == ERRLOP_INDEX1) {
        for (i -= 1; i >= 0; i--) {
            dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l1[i], mr->pbl_l1_dma_addr[i]);
            mr->pbl_bt_l1[i] = NULL;
        }
    } else {
        dev_warn(dev, "not support: mhop_num=%d, err_loop_index=%d.",
                 mhop_num, err_loop_index);
        return;
    }

    dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l0, mr->pbl_l0_dma_addr);
    mr->pbl_bt_l0 = NULL;
    mr->pbl_l0_dma_addr = 0;
}

static int pbl_1hop_alloc(struct hns_roce_dev *hr_dev, u32 npages,
    struct hns_roce_mr *mr, u32 pbl_bt_sz)
{
    struct device *dev = hr_dev->dev;

    if (npages > pbl_bt_sz / BA_BYTE_LEN) {
        dev_err(dev, "npages %u is larger than buf_pg_sz!", npages);
        return -EINVAL;
    }
    mr->pbl_buf = dma_alloc_coherent(dev, npages * BA_BYTE_LEN,
                                     &(mr->pbl_dma_addr),
                                     GFP_KERNEL | __GFP_NOWARN);
    if (mr->pbl_buf == NULL) {
        dev_err(hr_dev->dev, "dma_alloc_boherent MR's PBL space failed\n");
        return -ENOMEM;
    }

    mr->pbl_size = npages;
    mr->pbl_ba = mr->pbl_dma_addr;
    mr->pbl_hop_num = 1;
    mr->pbl_ba_pg_sz = hr_dev->caps.pbl_ba_pg_sz;
    mr->pbl_buf_pg_sz = hr_dev->caps.pbl_buf_pg_sz;
    return 0;
}

static int pbl_2hop_alloc(struct hns_roce_dev *hr_dev, u32 npages,
    struct hns_roce_mr *mr, u32 pbl_bt_sz)
{
    struct device *dev = hr_dev->dev;
    int npages_alloced;
    int ret = 0;
    u64 pbl_last_bt_num;
    u64 pbl_bt_cnt = 0;
    u64 size;
    u32 i;

    pbl_last_bt_num = DIV_ROUND_UP(npages, pbl_bt_sz / BA_BYTE_LEN);

    /* alloc L1 BT */
    for (i = 0; i < pbl_bt_sz / BA_BYTE_LEN; i++) {
        if (pbl_bt_cnt + 1 < pbl_last_bt_num) {
            size = pbl_bt_sz;
        } else {
#ifndef DEFINE_HNS_LLT
            npages_alloced = i * (pbl_bt_sz / BA_BYTE_LEN);
            size = (npages - npages_alloced) * BA_BYTE_LEN;
#endif
        }
        mr->pbl_bt_l1[i] = dma_alloc_coherent(dev, size,
                                              &(mr->pbl_l1_dma_addr[i]),
                                              GFP_KERNEL | __GFP_NOWARN);
        if (mr->pbl_bt_l1[i] == NULL) {
            hns_roce_pbl_mhop_free(hr_dev, mr, 1, i, 0);
            dev_err(dev, "dma_alloc_coherent PBL BT L1 space failed\n");
            ret = -ENOMEM;
            goto err_fail;
        }

        *(mr->pbl_bt_l0 + i) = mr->pbl_l1_dma_addr[i];

        pbl_bt_cnt++;
        if (pbl_bt_cnt >= pbl_last_bt_num) {
            break;
        }
    }

#ifndef DEFINE_HNS_LLT
    mr->l0_chunk_last_num = i + 1;
#endif
err_fail:
    return ret;
}

static int pbl_3hop_l1l2_bt_dma_alloc(struct hns_roce_dev *hr_dev, u32 pbl_bt_sz,
    struct hns_roce_mr *mr, u32 npages)
{
    u32 i;
    u32 j = 0;
    u64 size;
    u64 pbl_bt_cnt = 0;
    u64 bt_idx;
    int npages_alloced;
    int mr_alloc_done = 0;
    struct device *dev = hr_dev->dev;
    u64 pbl_last_bt_num = DIV_ROUND_UP(npages, pbl_bt_sz / BA_BYTE_LEN);

    for (i = 0; i < pbl_bt_sz / BA_BYTE_LEN; i++) {
        mr->pbl_bt_l1[i] = dma_alloc_coherent(dev, pbl_bt_sz, &(mr->pbl_l1_dma_addr[i]), GFP_KERNEL | __GFP_NOWARN);
        if (mr->pbl_bt_l1[i] == NULL) {
            hns_roce_pbl_mhop_free(hr_dev, mr, 1, i, 0);
            dev_err(dev, "dma_alloc_coherent PBL BT L1[%u] dma space failed\n", i);
            return -ENOMEM;
        }

        *(mr->pbl_bt_l0 + i) = mr->pbl_l1_dma_addr[i];

        for (j = 0; j < pbl_bt_sz / BA_BYTE_LEN; j++) {
            bt_idx = i * pbl_bt_sz / BA_BYTE_LEN + j;

            if (pbl_bt_cnt + 1 < pbl_last_bt_num) {
                size = pbl_bt_sz;
            } else {
                npages_alloced = bt_idx * (pbl_bt_sz / BA_BYTE_LEN);
                size = (npages - npages_alloced) * BA_BYTE_LEN;
            }
            mr->pbl_bt_l2[bt_idx] = dma_alloc_coherent(dev, size, &(mr->pbl_l2_dma_addr[bt_idx]),
                GFP_KERNEL | __GFP_NOWARN);
            if (mr->pbl_bt_l2[bt_idx] == NULL) {
                hns_roce_pbl_mhop_free(hr_dev, mr, ERRLOP_INDEX2, i, j);
                dev_err(dev, "dma_alloc_coherent PBL BT L2[%llu] dma space failed\n", bt_idx);
                return -ENOMEM;
            }

            *(mr->pbl_bt_l1[i] + j) = mr->pbl_l2_dma_addr[bt_idx];

            pbl_bt_cnt++;
            if (pbl_bt_cnt >= pbl_last_bt_num) {
                mr_alloc_done = 1;
                break;
            }
        }

        if (mr_alloc_done) {
            dev_info(dev, "alloc MR completed\n");
            break;
        }
    }

    mr->l0_chunk_last_num = i + 1;
    mr->l1_chunk_last_num = j + 1;

    return 0;
}

static int pbl_3hop_alloc(struct hns_roce_dev *hr_dev, u32 npages, struct hns_roce_mr *mr, u32 pbl_bt_sz)
{
    int ret;
    struct device *dev = hr_dev->dev;
    u64 pbl_last_bt_num;

    pbl_last_bt_num = DIV_ROUND_UP(npages, pbl_bt_sz / BA_BYTE_LEN);

    mr->pbl_l2_dma_addr = kcalloc(pbl_last_bt_num, sizeof(*mr->pbl_l2_dma_addr), GFP_KERNEL);
    if (mr->pbl_l2_dma_addr == NULL) {
        dev_err(dev, "%s(%d) alloc PBL BT L2 dma space failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    mr->pbl_bt_l2 = kcalloc(pbl_last_bt_num, sizeof(*mr->pbl_bt_l2), GFP_KERNEL);
    if (mr->pbl_bt_l2 == NULL) {
        dev_err(dev, "%s(%d) alloc PBL BT L2 space failed\n", __func__, __LINE__);
        goto err_kcalloc_bt_l2;
    }

    ret = pbl_3hop_l1l2_bt_dma_alloc(hr_dev, pbl_bt_sz, mr, npages);
    if (ret) {
        dev_err(dev, "%s(%d) l1l2_bt_dma_alloc failed\n", __func__, __LINE__);
        goto err_dma_alloc_l0;
    }

    return 0;

err_dma_alloc_l0:
    kfree(mr->pbl_bt_l2);
    mr->pbl_bt_l2 = NULL;

err_kcalloc_bt_l2:
    kfree(mr->pbl_l2_dma_addr);
    mr->pbl_l2_dma_addr = NULL;

    return -ENOMEM;
}

static int hns_rcoe_mhop_2and3_alloc(struct hns_roce_dev *hr_dev, u32 npages, struct hns_roce_mr *mr,
    u32 mhop_num, u32 pbl_bt_sz)
{
    if (mhop_num == MHOP_NUM_2LEVELS) {
        if (pbl_2hop_alloc(hr_dev, npages, mr, pbl_bt_sz)) {
            dev_err(hr_dev->dev, "PBL 2 hop alloc failed, PBL BT size[%u]\n", pbl_bt_sz);
            goto out;
        }
    }

    if (mhop_num == MHOP_NUM_3LEVELS) {
        if (pbl_3hop_alloc(hr_dev, npages, mr, pbl_bt_sz)) {
            dev_err(hr_dev->dev, "PBL 3 hop alloc failed, PBL BT size[%u]\n", pbl_bt_sz);
            goto out;
        }
    }

    mr->pbl_size = npages;
    mr->pbl_ba = mr->pbl_l0_dma_addr;
    mr->pbl_hop_num = hr_dev->caps.pbl_hop_num;
    mr->pbl_ba_pg_sz = hr_dev->caps.pbl_ba_pg_sz;
    mr->pbl_buf_pg_sz = hr_dev->caps.pbl_buf_pg_sz;

    return 0;
out:
    return -EINVAL;
}

/* PBL multi hop addressing */
static int hns_roce_mhop_alloc(struct hns_roce_dev *hr_dev, u32 npages, struct hns_roce_mr *mr)
{
    struct device *dev = hr_dev->dev;
    u32 pbl_bt_sz, mhop_num;
    int ret;

    mhop_num = (mr->type == MR_TYPE_FRMR ? 1 : hr_dev->caps.pbl_hop_num);
    pbl_bt_sz = 1U << (hr_dev->caps.pbl_ba_pg_sz + PAGE_SHIFT);

    if (mhop_num == HNS_ROCE_HOP_NUM_0) {
        return 0;
    }

    if (mhop_num == 1) {
        return pbl_1hop_alloc(hr_dev, npages, mr, pbl_bt_sz);
    }

    mr->pbl_l1_dma_addr = kcalloc(pbl_bt_sz / BA_BYTE_LEN, sizeof(*mr->pbl_l1_dma_addr), GFP_KERNEL);
    if (mr->pbl_l1_dma_addr == NULL) {
        dev_err(dev, "%s(%d) alloc PBL BT L1 dma space failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    mr->pbl_bt_l1 = kcalloc(pbl_bt_sz / BA_BYTE_LEN, sizeof(*mr->pbl_bt_l1), GFP_KERNEL);
    if (mr->pbl_bt_l1 == NULL) {
        dev_err(dev, "%s(%d) alloc PBL BT L1 space failed\n", __func__, __LINE__);
        goto err_kcalloc_bt_l1;
    }

    /* alloc L0 BT */
    mr->pbl_bt_l0 = dma_alloc_coherent(dev, pbl_bt_sz, &(mr->pbl_l0_dma_addr), GFP_KERNEL | __GFP_NOWARN);
    if (mr->pbl_bt_l0 == NULL) {
        dev_err(dev, "%s(%d) alloc PBL BT L0 space failed\n", __func__, __LINE__);
        goto err_pbl_bt_l0;
    }

    ret = hns_rcoe_mhop_2and3_alloc(hr_dev, npages, mr, mhop_num, pbl_bt_sz);
    if (ret) {
        dev_err(dev, "%s(%d) alloc PBL BT L1 or L2 space failed\n", __func__, __LINE__);
        goto err_mhop2_3_alloc;
    }

    return 0;

err_mhop2_3_alloc:
    dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l0, mr->pbl_l0_dma_addr);
    mr->pbl_bt_l0 = NULL;
err_pbl_bt_l0:
    kfree(mr->pbl_bt_l1);
    mr->pbl_bt_l1 = NULL;

err_kcalloc_bt_l1:
    kfree(mr->pbl_l1_dma_addr);
    mr->pbl_l1_dma_addr = NULL;

    return -ENOMEM;
}

static int hns_roce_mr_alloc(struct hns_roce_dev *hr_dev,
    struct hns_roce_mr_alloc_para *mr_para, u32 npages, struct hns_roce_mr *mr)
{
    struct device *dev = hr_dev->dev;
    unsigned long index = 0;
    int ret;

    /* Allocate a key for mr from mr_table */
    ret = hns_roce_bitmap_alloc(&hr_dev->mr_table.mtpt_bitmap, &index);
    if (ret == -1) {
        dev_err(dev, "alloc a key for mr from mr_table failed\n");
        return -ENOMEM;
    }

    mr->iova = mr_para->iova;            /* MR va starting addr */
    mr->size = mr_para->size;            /* MR addr range */
    mr->pd = mr_para->pd;                /* MR num */
    mr->access = mr_para->access;            /* MR access permit */
    mr->enabled = 0;            /* MR active status */
    mr->key = hw_index_to_key(index);   /* MR key */

    if (mr_para->size == ~0ull) {
        mr->pbl_buf = NULL;
        mr->pbl_dma_addr = 0;
        /* PBL multi-hop addressing parameters */
        mr->pbl_bt_l2 = NULL;
        mr->pbl_bt_l1 = NULL;
        mr->pbl_bt_l0 = NULL;
        mr->pbl_l2_dma_addr = NULL;
        mr->pbl_l1_dma_addr = NULL;
        mr->pbl_l0_dma_addr = 0;
    } else {
        if (!npages) {
            dev_err(dev, "invalid npages: %u\n", npages);
            hns_roce_bitmap_free(&hr_dev->mr_table.mtpt_bitmap,
                                 key_to_hw_index(mr->key), BITMAP_NO_RR);
            return -EINVAL;
        }

        if (!hr_dev->caps.pbl_hop_num) {
            mr->pbl_buf = dma_alloc_coherent(dev,
                                             npages * BA_BYTE_LEN,
                                             &(mr->pbl_dma_addr),
                                             GFP_KERNEL | __GFP_NOWARN);
            if (mr->pbl_buf == NULL) {
                dev_err(dev, "%s(%d) dma_alloc_coherent PBL buf space failed\n", __func__, __LINE__);
                hns_roce_bitmap_free(&hr_dev->mr_table.mtpt_bitmap,
                                     key_to_hw_index(mr->key), BITMAP_NO_RR);
                return -ENOMEM;
            }
        } else {
            ret = hns_roce_mhop_alloc(hr_dev, npages, mr);
            if (ret) {
                dev_err(dev, "%s(%d) hns_roce_mhop_alloc failed\n", __func__, __LINE__);
                hns_roce_bitmap_free(&hr_dev->mr_table.mtpt_bitmap,
                                     key_to_hw_index(mr->key), BITMAP_NO_RR);
            }
        }
    }

    return ret;
}

STATIC void hns_roce_mhop_level_three_free(struct device *dev, struct hns_roce_mr *mr,
    u32 pbl_bt_sz, u32 npages)
{
    int npages_allocated;
    u64 bt_idx;
    u32 i, j;

    for (i = 0; i < mr->l0_chunk_last_num; i++) {
        dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l1[i], mr->pbl_l1_dma_addr[i]);
        mr->pbl_bt_l1[i] = NULL;
        for (j = 0; j < pbl_bt_sz / PBLSZ_BASE; j++) {
            bt_idx = i * (pbl_bt_sz / PBLSZ_BASE) + j;
            if ((i == mr->l0_chunk_last_num - 1) && j == mr->l1_chunk_last_num - 1) {
                npages_allocated = bt_idx * (pbl_bt_sz / PBLSZ_BASE);
                dma_free_coherent(dev, (npages - npages_allocated) * PBLSZ_BASE,
                                  mr->pbl_bt_l2[bt_idx], mr->pbl_l2_dma_addr[bt_idx]);
                mr->pbl_bt_l2[bt_idx] = NULL;

                break;
            }

            dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l2[bt_idx], mr->pbl_l2_dma_addr[bt_idx]);
            mr->pbl_bt_l2[bt_idx] = NULL;
        }
    }
}

static void hns_roce_mhop_free(struct hns_roce_dev *hr_dev, struct hns_roce_mr *mr)
{
    struct device *dev = hr_dev->dev;
    u32 npages_allocated, npages, i;
    u32 pbl_bt_sz, mhop_num;

    npages = mr->pbl_size;
    pbl_bt_sz = 1U << (hr_dev->caps.pbl_ba_pg_sz + PAGE_SHIFT);
    mhop_num = (mr->type == MR_TYPE_FRMR) ? 1 : hr_dev->caps.pbl_hop_num;

    if (mhop_num == HNS_ROCE_HOP_NUM_0) {
        dev_info(dev, "no hns_roce mdop need to free\n");
        return;
    }

    if (mhop_num == 1) {
        dma_free_coherent(dev, npages * PBLSZ_BASE, mr->pbl_buf, mr->pbl_dma_addr);
        mr->pbl_buf = NULL;
        return;
    }

    dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l0, mr->pbl_l0_dma_addr);
    mr->pbl_bt_l0 = NULL;

    if (mhop_num == MHOP_NUM_2LEVELS) {
        for (i = 0; i < mr->l0_chunk_last_num; i++) {
            if (i == mr->l0_chunk_last_num - 1) {
                npages_allocated = i * (pbl_bt_sz / PBLSZ_BASE);

                dma_free_coherent(dev, (npages - npages_allocated) * PBLSZ_BASE,
                    mr->pbl_bt_l1[i], mr->pbl_l1_dma_addr[i]);
                mr->pbl_bt_l1[i] = NULL;
                break;
            }
            dma_free_coherent(dev, pbl_bt_sz, mr->pbl_bt_l1[i], mr->pbl_l1_dma_addr[i]);
            mr->pbl_bt_l1[i] = NULL;
        }
    } else if (mhop_num == MHOP_NUM_3LEVELS) {
        hns_roce_mhop_level_three_free(dev, mr, pbl_bt_sz, npages);
    }

    kfree(mr->pbl_bt_l1);
    mr->pbl_bt_l1 = NULL;
    kfree(mr->pbl_l1_dma_addr);
    mr->pbl_l1_dma_addr = NULL;
    if (mhop_num == MHOP_NUM_3LEVELS) {
        kfree(mr->pbl_bt_l2);
        mr->pbl_bt_l2 = NULL;
        kfree(mr->pbl_l2_dma_addr);
        mr->pbl_l2_dma_addr = NULL;
    }
}

static void hns_roce_mr_free(struct hns_roce_dev *hr_dev,
    struct hns_roce_mr *mr)
{
    struct device *dev = hr_dev->dev;
    int npages = 0;
    int ret;

    if (mr->enabled) {
        ret = hns_roce_hw2sw_mpt(hr_dev, NULL, key_to_hw_index(mr->key)
                                 & ((unsigned int)hr_dev->caps.num_mtpts - 1));
        if (ret) {
            dev_warn(dev, "HW2SW_MPT failed (%d)\n", ret);
        }
    }

    if (mr->size != ~0ULL) {
        if (mr->type == MR_TYPE_MR) {
            npages = mr->hr_umem.npages;
        }

        if (!hr_dev->caps.pbl_hop_num) {
            dma_free_coherent(dev, (unsigned int)(npages * BA_BYTE_LEN), mr->pbl_buf, mr->pbl_dma_addr);
            mr->pbl_buf = NULL;
        } else {
            hns_roce_mhop_free(hr_dev, mr);
        }
    }

    if (mr->enabled) {
        hns_roce_table_put(hr_dev, &hr_dev->mr_table.mtpt_table,
                           key_to_hw_index(mr->key));
    }

    hns_roce_bitmap_free(&hr_dev->mr_table.mtpt_bitmap,
                         key_to_hw_index(mr->key), BITMAP_NO_RR);
}

static int hns_roce_mr_enable(struct hns_roce_dev *hr_dev,
    struct hns_roce_mr *mr)
{
    int ret;
    unsigned long mtpt_idx = key_to_hw_index(mr->key);
    struct device *dev = hr_dev->dev;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mr_table *mr_table = &hr_dev->mr_table;

    /* Prepare HEM entry memory */
    ret = hns_roce_table_get(hr_dev, &mr_table->mtpt_table, mtpt_idx);
    if (ret) {
        dev_err(dev, "get mtpt table(0x%lx) failed, ret = %d", mtpt_idx, ret);
        return ret;
    }
    /* Allocate mailbox memory */
    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        ret = PTR_ERR(mailbox);
        dev_err(dev, "hns_roce alloc cmd mailbox failed\n");
        goto err_table;
    }

    if (mr->type != MR_TYPE_FRMR) {
        ret = hr_dev->hw->write_mtpt(mailbox->buf, mr, mtpt_idx);
    } else {
        ret = hr_dev->hw->frmr_write_mtpt(mailbox->buf, mr);
    }
    if (ret) {
        dev_err(dev, "Write mtpt fail!\n");
        goto err_page;
    }

    ret = hns_roce_sw2hw_mpt(hr_dev, mailbox,
                             mtpt_idx & (unsigned int)(hr_dev->caps.num_mtpts - 1));
    if (ret) {
        dev_err(dev, "SW2HW_MPT(0x%lx) failed (%d)\n", mtpt_idx, ret);
        goto err_page;
    }

    mr->enabled = 1;
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return 0;

err_page:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

err_table:
    hns_roce_table_put(hr_dev, &mr_table->mtpt_table, mtpt_idx);
    return ret;
}

static int hns_roce_write_mtt_chunk(struct hns_roce_dev *hr_dev,
                                    struct hns_roce_mtt *mtt, u32 start_index, u32 npages, u64 *page_list)
{
    struct hns_roce_hem_table *table = NULL;
    dma_addr_t dma_handle;
    __le64 *mtts = NULL;
    size_t s = start_index * sizeof(u64);
    u32 bt_page_size;
    u32 i;

    switch (mtt->mtt_type) {
        case MTT_TYPE_WQE:
            table = &hr_dev->mr_table.mtt_table;
            bt_page_size = 1U << (hr_dev->caps.mtt_ba_pg_sz + PAGE_SHIFT);
            break;
        case MTT_TYPE_CQE:
            table = &hr_dev->mr_table.mtt_cqe_table;
            bt_page_size = 1U << (hr_dev->caps.cqe_ba_pg_sz + PAGE_SHIFT);
            break;
        case MTT_TYPE_SRQWQE:
            table = &hr_dev->mr_table.mtt_srqwqe_table;
            bt_page_size = 1U << (hr_dev->caps.srqwqe_ba_pg_sz + PAGE_SHIFT);
            break;
        case MTT_TYPE_IDX:
            table = &hr_dev->mr_table.mtt_idx_table;
            bt_page_size = 1U << (hr_dev->caps.idx_ba_pg_sz + PAGE_SHIFT);
            break;
        default:
            dev_err(hr_dev->dev, "Unsupport mtt type %d, write mtt chunk failed\n", mtt->mtt_type);
            return -EINVAL;
    }

    /* All MTTs must fit in the same page */
    if (start_index / (bt_page_size / sizeof(u64)) != (start_index + npages - 1) / (bt_page_size / sizeof(u64))) {
        dev_err(hr_dev->dev, "MTTS don't fit in the same page\n");
        return -EINVAL;
    }

    if (start_index & (HNS_ROCE_MTT_ENTRY_PER_SEG - 1)) {
        dev_err(hr_dev->dev, "invalid start_index[%d]\n", start_index);
        return -EINVAL;
    }

    mtts = hns_roce_table_find(hr_dev, table,
                               mtt->first_seg + s / hr_dev->caps.mtt_entry_sz, &dma_handle);
    if (mtts == NULL) {
        dev_err(hr_dev->dev, "hns_roce failed to find table[%lu]\n",
            mtt->first_seg + s / hr_dev->caps.mtt_entry_sz);
        return -ENOMEM;
    }

    /* Save page addr, low 12 bits : 0 */
    for (i = 0; i < npages; ++i) {
        mtts[i] = !hr_dev->caps.mtt_hop_num ? cpu_to_le64(page_list[i] >> PAGE_ADDR_SHIFT) : cpu_to_le64(page_list[i]);
    }

    return 0;
}

static int hns_roce_write_mtt(struct hns_roce_dev *hr_dev,
                              struct hns_roce_mtt *mtt, u32 start_index,
                              u32 npages, u64 *page_list)
{
    int chunk;
    int ret;
    u32 bt_page_size;

    if (mtt->order < 0) {
        dev_err(hr_dev->dev, "hns_roce_write_mtt order(%d) < 0\n", mtt->order);
        return -EINVAL;
    }

    switch (mtt->mtt_type) {
        case MTT_TYPE_WQE:
            bt_page_size = 1U << (hr_dev->caps.mtt_ba_pg_sz + PAGE_SHIFT);
            break;
        case MTT_TYPE_CQE:
            bt_page_size = 1U << (hr_dev->caps.cqe_ba_pg_sz + PAGE_SHIFT);
            break;
        case MTT_TYPE_SRQWQE:
            bt_page_size = 1U << (hr_dev->caps.srqwqe_ba_pg_sz + PAGE_SHIFT);
            break;
        case MTT_TYPE_IDX:
            bt_page_size = 1U << (hr_dev->caps.idx_ba_pg_sz + PAGE_SHIFT);
            break;
        default:
            dev_err(hr_dev->dev, "%s(%d) Unsupport mtt type %d, write mtt failed\n",
                    __func__, __LINE__, mtt->mtt_type);
            return -EINVAL;
    }

    while (npages > 0) {
        chunk = min_t(int, bt_page_size / sizeof(u64), npages);

        ret = hns_roce_write_mtt_chunk(hr_dev, mtt, start_index, chunk,
                                       page_list);
        if (ret) {
            dev_err(hr_dev->dev, "hns_roce_write_mtt_chunk err, ret:%d\n", ret);
            return ret;
        }

        npages -= chunk;
        start_index += chunk;
        page_list += chunk;
    }

    return 0;
}

int hns_roce_buf_write_mtt(struct hns_roce_dev *hr_dev,
    struct hns_roce_mtt *mtt, struct hns_roce_buf *buf)
{
    u64 *page_list;
    int ret;
    u32 i;

    page_list = kmalloc_array(buf->npages, sizeof(*page_list), GFP_KERNEL);
    if (page_list == NULL) {
        dev_err(hr_dev->dev, "alloc page_list failed\n");
        return -ENOMEM;
    }

    ret = memset_s(page_list, sizeof(*page_list) * buf->npages, 0, sizeof(*page_list) * buf->npages);
    if (ret) {
        kfree(page_list);
        page_list = NULL;
        dev_err(hr_dev->dev, "[buf_write_mtt]memset_s failed, err = %d\n", ret);
        return -ENOMEM;
    }

    for (i = 0; i < buf->npages; ++i) {
        if (buf->nbufs == 1) {
            page_list[i] = buf->direct.map + (i << (unsigned int)buf->page_shift);
        } else {
            page_list[i] = buf->page_list[i].map;
        }
    }
    ret = hns_roce_write_mtt(hr_dev, mtt, 0, buf->npages, page_list);
    if (ret) {
        dev_err(hr_dev->dev, "write mtt failed, err = %d\n", ret);
    }

    kfree(page_list);
    page_list = NULL;

    return ret;
}

int hns_roce_init_mr_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_mr_table *mr_table = &hr_dev->mr_table;
    struct device *dev = hr_dev->dev;
    int ret;

    ret = hns_roce_bitmap_init(&mr_table->mtpt_bitmap, hr_dev->caps.num_mtpts,
                               hr_dev->caps.num_mtpts - 1, hr_dev->caps.reserved_mrws, 0);
    if (ret) {
        dev_err(hr_dev->dev,
                "mtpt bitmap init failed, ret = %d\n", ret);
        return ret;
    }
    ret = hns_roce_buddy_init(&mr_table->mtt_buddy, ilog2(hr_dev->caps.num_mtt_segs));
    if (ret) {
        dev_err(dev, "init hns_roce mtt_buddy failed\n");
        goto err_buddy;
    }

    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        ret = hns_roce_buddy_init(&mr_table->mtt_cqe_buddy, ilog2(hr_dev->caps.num_cqe_segs));
        if (ret) {
            dev_err(dev, "init hns_roce mtt_cqe_buddy failed\n");
            goto err_buddy_cqe;
        }
    }

    ret = hns_roce_buddy_init(&mr_table->mtt_srqwqe_buddy, ilog2(hr_dev->caps.num_srqwqe_segs));
    if (ret) {
        dev_err(dev, "init hns_roce mtt_srqwqe_buddy failed\n");
        goto err_buddy_srqwqe;
    }

    ret = hns_roce_buddy_init(&mr_table->mtt_idx_buddy, ilog2(hr_dev->caps.num_idx_segs));
    if (ret) {
        dev_err(dev, "init hns_roce mtt_idx_buddy failed\n");
        goto err_buddy_idx;
    }

    return 0;

err_buddy_idx:
    hns_roce_buddy_cleanup(&mr_table->mtt_srqwqe_buddy);

err_buddy_srqwqe:
    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        hns_roce_buddy_cleanup(&mr_table->mtt_cqe_buddy);
    }

err_buddy_cqe:
    hns_roce_buddy_cleanup(&mr_table->mtt_buddy);

err_buddy:
    hns_roce_bitmap_cleanup(&mr_table->mtpt_bitmap);
    return ret;
}

void hns_roce_cleanup_mr_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_mr_table *mr_table = &hr_dev->mr_table;

    hns_roce_buddy_cleanup(&mr_table->mtt_idx_buddy);
    hns_roce_buddy_cleanup(&mr_table->mtt_srqwqe_buddy);
    hns_roce_buddy_cleanup(&mr_table->mtt_buddy);
    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        hns_roce_buddy_cleanup(&mr_table->mtt_cqe_buddy);
    }
    hns_roce_bitmap_cleanup(&mr_table->mtpt_bitmap);
}

struct ib_mr *hns_roce_get_dma_mr(struct ib_pd *pd, int acc)
{
    struct hns_roce_mr *mr = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    struct device *dev = NULL;
    struct hns_roce_mr_alloc_para mr_para;
    int ret;

    if (pd == NULL) {
        pr_err("hns3: get dma mr param err, pd is NULL\n");
        return ERR_PTR(-EINVAL);
    }

    hr_dev = to_hr_dev(pd->device);
    dev = hr_dev->dev;
    mr = kzalloc(sizeof(*mr), GFP_KERNEL);
    if (mr == NULL) {
        dev_err(dev, "alloc hns_roce MR failed\n");
        return ERR_PTR(-ENOMEM);
    }

    mr->type = MR_TYPE_DMA;

    /* Allocate memory region key */
    mr_para.pd = to_hr_pd(pd)->pdn;
    mr_para.iova = 0;
    mr_para.size = ~0ULL;
    mr_para.access = acc;
    ret = hns_roce_mr_alloc(hr_dev, &mr_para, 0, mr);
    if (ret) {
        dev_err(dev, "alloc mr failed(%d), pd =0x%lx\n", ret, to_hr_pd(pd)->pdn);
        goto err_free;
    }

#ifdef CONFIG_INFINIBAND_HNS_TEST
    test_set_mr_access(mr);
#endif

    ret = hns_roce_mr_enable(hr_dev, mr);
    if (ret) {
        dev_err(dev, "enable hns_roce MR failed\n");
        goto err_mr;
    }

    mr->ibmr.lkey = mr->key;
    mr->ibmr.rkey = mr->key;
    mr->umem = NULL;

    return &mr->ibmr;

err_mr:
    hns_roce_mr_free(to_hr_dev(pd->device), mr);

err_free:
    kfree(mr);
    mr = NULL;
    return ERR_PTR(ret);
}

STATIC int _hns_roce_ib_umem_get_mtt_type(struct hns_roce_dev *hr_dev,
    struct hns_roce_mtt *mtt, unsigned int *order)
{
    switch (mtt->mtt_type) {
        case MTT_TYPE_WQE:
            *order = hr_dev->caps.mtt_ba_pg_sz;
            break;
        case MTT_TYPE_CQE:
            *order = hr_dev->caps.cqe_ba_pg_sz;
            break;
        case MTT_TYPE_SRQWQE:
            *order = hr_dev->caps.srqwqe_ba_pg_sz;
            break;
        case MTT_TYPE_IDX:
            *order = hr_dev->caps.idx_ba_pg_sz;
            break;
        default:
            dev_err(hr_dev->dev, "%s(%d) Unsupport mtt type %d, write mtt failed\n",
                    __func__, __LINE__, mtt->mtt_type);
            return -EINVAL;
    }

    return 0;
}

STATIC int _hns_roce_ib_umem_get_pages(struct hns_roce_dev *hr_dev, struct hns_roce_share_sq *share_sq,
    struct hns_roce_mtt *mtt, struct ib_umem *umem, struct hns_roce_ib_umem_page_info *page_info)
{
    unsigned int len;
    unsigned int k;
    u64 page_addr;

    if (share_sq != NULL) {
        len = share_sq->sq_buf_len >> PAGE_SHIFT;

        // npage is num of 4K page, i is num of storage page, n is num of addr page
        for (k = 0; k < len; ++k) {
            page_addr = share_sq->sq_addr + (k << (unsigned int)PAGE_SHIFT);
            if ((page_info->npage) % (1U << (unsigned int)(mtt->page_shift - PAGE_SHIFT))) {
                (page_info->npage)++;
                continue;
            }

            if (page_addr & ((1UL << (unsigned int)mtt->page_shift) - 1)) {
                dev_err(hr_dev->dev, "page_addr 0x%llx is not page_shift %d alignment!\n", page_addr, mtt->page_shift);
                return -EINVAL;
            }
            (page_info->pages)[(page_info->i)++] = page_addr;
            (page_info->npage)++;
        }
    }

    return 0;
}

STATIC int _hns_roce_ib_umem_write_mtt_npage(struct hns_roce_dev *hr_dev, struct hns_roce_mtt *mtt,
    struct ib_umem *umem, int *n, struct hns_roce_ib_umem_page_info *page_info)
{
    unsigned int k;
    int ret, entry;
    unsigned int len;
    u64 page_addr;
    struct scatterlist *sg = NULL;

    for_each_sg(umem->sg_head.sgl, sg, umem->nmap, entry) {
        len = sg_dma_len(sg) >> PAGE_SHIFT;
        for (k = 0; k < len; ++k) {
            page_addr = sg_dma_address(sg) + (k << (unsigned int)PAGE_SHIFT);
            if (!((page_info->npage) % (1U << ((unsigned int)mtt->page_shift - PAGE_SHIFT)))) {
                if (page_addr & (unsigned int)((1UL << (unsigned int)mtt->page_shift) - 1)) {
                    dev_err(hr_dev->dev, "page_addr 0x%llx is not page_shift %d alignment!\n",
                        page_addr, mtt->page_shift);
                    return -EINVAL;
                }
                (page_info->pages)[(page_info->i)++] = page_addr;
            }
            (page_info->npage)++;
            if ((page_info->i) == (1U << (page_info->order + PAGE_SHIFT)) / sizeof(u64)) {
                ret = hns_roce_write_mtt(hr_dev, mtt, *n, page_info->i, page_info->pages);
                if (ret) {
                    dev_err(hr_dev->dev, "write mtt ret %d", ret);
                    return ret;
                }
                *n += page_info->i;
                page_info->i = 0;
            }
        }
    }

    return 0;
}

STATIC int _hns_roce_ib_umem_write_mtt(struct hns_roce_dev *hr_dev, struct hns_roce_mtt *mtt,
                                       struct ib_umem *umem, struct hns_roce_share_sq *share_sq)
{
    struct device *dev = hr_dev->dev;
    unsigned int order = 0;
    int i = 0;
    int npage = 0;
    int ret;
    int n = 0;
    u64 *pages = NULL;
    struct hns_roce_ib_umem_page_info page_info;

    ret = _hns_roce_ib_umem_get_mtt_type(hr_dev, mtt, &order);
    if (ret) {
        dev_err(dev, "get order[%u] failed\n", order);
        return ret;
    }

    pages = (u64 *)(uintptr_t)__get_free_pages(GFP_KERNEL, order);
    if (pages == NULL) {
        dev_err(dev, "get free pages using order[%u] failed\n", order);
        return -ENOMEM;
    }

    page_info.i = i;
    page_info.npage = npage;
    page_info.order = order;
    page_info.pages = pages;

    ret = _hns_roce_ib_umem_get_pages(hr_dev, share_sq, mtt, umem, &page_info);
    if (ret) {
        dev_err(dev, "get pages num failed ret %d\n", ret);
        goto out;
    }

    ret = _hns_roce_ib_umem_write_mtt_npage(hr_dev, mtt, umem, &n, &page_info);
    if (ret) {
        dev_err(dev, "write pages failed ret %d\n", ret);
        goto out;
    }

    if (page_info.i) {
        ret = hns_roce_write_mtt(hr_dev, mtt, n, page_info.i, page_info.pages);
    }

out:
    free_pages((uintptr_t)pages, order);
    return ret;
}

int hns_roce_gdr_write_mtt(struct hns_roce_dev *hr_dev,
                           struct hns_roce_mtt *mtt, struct ib_umem *umem,
                           struct hns_roce_share_sq *share_sq)
{
    return _hns_roce_ib_umem_write_mtt(hr_dev, mtt, umem, share_sq);
}

int hns_roce_ib_umem_write_mtt(struct hns_roce_dev *hr_dev,
    struct hns_roce_mtt *mtt, struct ib_umem *umem)
{
    return _hns_roce_ib_umem_write_mtt(hr_dev, mtt, umem, NULL);
}

static int hns_roce_ib_umem_write_mr(struct hns_roce_dev *hr_dev,
                                     struct hns_roce_mr *mr,
                                     struct ib_umem *umem, unsigned int page_shift)
{
    unsigned int k, len;
    u32 i = 0;
    u32 j = 0;
    int entry;
    u64 page_addr;
    u32 pbl_bt_sz;
    struct scatterlist *sg = NULL;

    if (hr_dev->caps.pbl_hop_num == HNS_ROCE_HOP_NUM_0) {
        return 0;
    }

    pbl_bt_sz = 1U << (hr_dev->caps.pbl_ba_pg_sz + PAGE_SHIFT);
    for_each_sg(umem->sg_head.sgl, sg, umem->nmap, entry) {
        len = sg_dma_len(sg) >> (page_shift);

        for (k = 0; k < len; ++k) {
            page_addr = sg_dma_address(sg) +
                        (k << page_shift);

            if (!hr_dev->caps.pbl_hop_num) {
                /* for hip06, page addr is aligned to 4K */
                mr->pbl_buf[i++] = page_addr >> PG4K_OFFSET;
            } else if (hr_dev->caps.pbl_hop_num == 1) {
                mr->pbl_buf[i++] = page_addr;
            } else {
                if (hr_dev->caps.pbl_hop_num == MHOP_NUM_2LEVELS) {
                    mr->pbl_bt_l1[i][j] = page_addr;
                } else if (hr_dev->caps.pbl_hop_num == MHOP_NUM_3LEVELS) {
                    mr->pbl_bt_l2[i][j] = page_addr;
                }

                j++;
                if (j >= (pbl_bt_sz / BA_BYTE_LEN)) {
                    i++;
                    j = 0;
                }
            }
        }
    }

    /* Memory barrier */
    mb();

    return 0;
}

STATIC int hns_roce_reg_user_mr_check(struct ib_pd *pd,
    struct ib_udata *udata, struct hns_roce_ib_reg_mr *ucmd)
{
    struct hns_roce_dev *hr_dev = NULL;
    if (pd == NULL || udata == NULL) {
        pr_err("hns3: invalid param, pd[%pK], udata[%pK] \n", pd, udata);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(pd->device);
    if (pd->uobject) {
        if (udata->inlen >= sizeof(struct hns_roce_ib_reg_mr)) {
            if (ib_copy_from_udata(ucmd, udata, sizeof(struct hns_roce_ib_reg_mr))) {
                dev_err(hr_dev->dev, "ib_copy_from_udata error for create qp\n");
                return -EFAULT;
            }
        }
    }

    return 0;
}

STATIC int hns_roce_umem_get_mr_sign(struct device *dev, struct hns_roce_mr *mr,
    struct hns_roce_ib_reg_mr ucmd, struct ib_pd *pd, struct hns_roce_user_mr_info user_mr_info)
{
    int ret;

    mr->hr_umem.psign.tgid = ucmd.tgid;
    mr->hr_umem.psign.devid = ucmd.devid;
    mr->hr_umem.psign.vfid = ucmd.vfid;

    if (strlen(ucmd.sign) >= PROCESS_SIGN_LENGTH) {
        dev_err(dev, "sign len is invliad\n");
        return -EINVAL;
    }

    ret = strncpy_s(mr->hr_umem.psign.sign, PROCESS_SIGN_LENGTH, ucmd.sign, strlen(ucmd.sign));
    if (ret) {
        dev_err(dev, "copy sign failed ret %d\n", ret);
        return ret;
    }

    mr->umem = hns_roce_umem_get(pd->uobject->context, user_mr_info, 0, IB_PEER_MEM_ALLOW, &mr->hr_umem);
    if (IS_ERR(mr->umem)) {
        ret = PTR_ERR(mr->umem);
        dev_err(dev, "ib_umem_get failed, ret = %d\n", ret);
        return -ENOMEM;
    }

    return 0;
}

STATIC int hns_roce_mr_alloc_page_get(struct hns_roce_dev *hr_dev, struct hns_roce_mr *mr,
    struct ib_pd *pd, struct hns_roce_user_mr_info user_mr_info)
{
    struct hns_roce_mr_alloc_para mr_para;
    u64 pbl_size;
    u32 bt_size;
    int ret;
    u32 i;
    u32 n;

    n = (u32)mr->hr_umem.npages;
    if (!hr_dev->caps.pbl_hop_num) {
        if (n > HNS_ROCE_MAX_MTPT_PBL_NUM) {
            dev_err(hr_dev->dev, " MR len %lld err. MR is limited to 4G at most!\n", user_mr_info.length);
            return -EINVAL;
        }
    } else {
        pbl_size = 1;
        bt_size = (1U << (hr_dev->caps.pbl_ba_pg_sz + PAGE_SHIFT)) / BA_BYTE_LEN;
        for (i = 0; i < hr_dev->caps.pbl_hop_num; i++) {
            pbl_size *= bt_size;
        }
        if (n > pbl_size) {
            dev_err(hr_dev->dev, " MR len %lld err. MR page num is limited to %lld!\n",
                user_mr_info.length, pbl_size);
            return -EINVAL;
        }
    }

    mr->type = MR_TYPE_MR;
    mr_para.pd = to_hr_pd(pd)->pdn;
    mr_para.iova = user_mr_info.virt_addr;
    mr_para.size = user_mr_info.length;
    mr_para.access = user_mr_info.access_flags;
    ret = hns_roce_mr_alloc(hr_dev, &mr_para, n, mr);
    if (ret) {
        dev_err(hr_dev->dev, "alloc hns_roce MR failed, size[%llu], access flasg[%d], napages[%d]",
            user_mr_info.length, user_mr_info.access_flags, n);
        return ret;
    }

    return 0;
}

STATIC int hns_roce_mr_write_enable(struct hns_roce_dev *hr_dev, struct hns_roce_mr *mr)
{
    int ret;

    ret = hns_roce_ib_umem_write_mr(hr_dev, mr, mr->umem, mr->hr_umem.page_shift);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) hns_roce ib_umem write MR failed\n", __func__, __LINE__);
        return ret;
    }

    ret = hns_roce_mr_enable(hr_dev, mr);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) enable hns_roce MR MR failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    mr->ibmr.lkey = mr->key;
    mr->ibmr.rkey = mr->ibmr.lkey;

    return 0;
}

struct ib_mr *hns_roce_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
                                   u64 virt_addr, int access_flags, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct device *dev = NULL;
    struct hns_roce_mr *mr = NULL;
    struct hns_roce_ib_reg_mr ucmd = {0};
    int ret;
    struct hns_roce_user_mr_info user_mr_info = {0};
    user_mr_info.start = start;
    user_mr_info.length = length;
    user_mr_info.virt_addr = virt_addr;
    user_mr_info.access_flags = access_flags;

    ret = hns_roce_reg_user_mr_check(pd, udata, &ucmd);
    if (ret) {
        return ERR_PTR(ret);
    }

    mr = kzalloc(sizeof(*mr), GFP_KERNEL);
    if (mr == NULL) {
        return ERR_PTR(-ENOMEM);
    }

    hr_dev = to_hr_dev(pd->device);
    dev = hr_dev->dev;
    ret = hns_roce_umem_get_mr_sign(dev, mr, ucmd, pd, user_mr_info);
    if (ret) {
        dev_err(dev, "umem get MR failed\n");
        goto err_free;
    }

    ret = hns_roce_mr_alloc_page_get(hr_dev, mr, pd, user_mr_info);
    if (ret) {
        dev_err(dev, "mr page get failed\n");
        goto err_umem;
    }

    ret = hns_roce_mr_write_enable(hr_dev, mr);
    if (ret) {
        dev_err(dev, "mr write and enbale\n");
        goto err_mr;
    }

    hns_roce_inc_rdma_hw_stats(pd->device, HW_STATS_MR_ALLOC);

    return &mr->ibmr;

err_mr:
    hns_roce_mr_free(hr_dev, mr);

err_umem:
    hns_roce_umem_release(mr->umem, &mr->hr_umem);

err_free:
    kfree(mr);
    mr = NULL;
    return ERR_PTR(ret);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
struct ib_mr *hns_roce_rereg_user_mr(struct ib_mr *ibmr, int flags, u64 start, u64 length, u64 virt_addr,
    int mr_access_flags, struct ib_pd *pd, struct ib_udata *udata)
{
    pr_err("hns3: hns_roce_rereg_user_mr is not supported\n");
    return NULL;
}
#else
static int rereg_mr_trans_write(struct hns_roce_dev *hr_dev, struct hns_roce_mr *mr,
    struct hns_roce_user_mr_info *user_mr, struct hns_roce_cmd_mailbox *mailbox)
{
    int ret;
    struct device *dev = hr_dev->dev;
    struct rreg_wrt_mtpt_info rereg_info = {0};
    int npages;

    npages = mr->hr_umem.npages;
    if (!npages) {
        dev_err(hr_dev->dev, "invalid npages: %d\n", npages);
        return -EINVAL;
    }

    if (hr_dev->caps.pbl_hop_num) {
        ret = hns_roce_mhop_alloc(hr_dev, npages, mr);
        if (ret) {
            dev_err(hr_dev->dev, "alloc hns_roce_mhop failed, npages[%d]\n", npages);
            return ret;
        }
    } else {
        mr->pbl_buf = dma_alloc_coherent(dev, npages * BA_BYTE_LEN, &(mr->pbl_dma_addr), GFP_KERNEL | __GFP_NOWARN);
        if (mr->pbl_buf == NULL) {
            dev_err(hr_dev->dev, "%s(%d) dma_alloc_coherent PBL buf failed\n", __func__, __LINE__);
            return -ENOMEM;
        }
    }

    rereg_info.flags = user_mr->flags;
    rereg_info.pdn = user_mr->pdn;
    rereg_info.mr_access_flags = user_mr->access_flags;
    rereg_info.iova = user_mr->virt_addr;
    rereg_info.size = user_mr->length;
    ret = hr_dev->hw->rereg_write_mtpt(hr_dev, mr, rereg_info, mailbox->buf);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) hns_roce_hw rereg write mtpt failed\n", __func__, __LINE__);
        goto out;
    }

    ret = hns_roce_ib_umem_write_mr(hr_dev, mr, mr->umem, mr->hr_umem.page_shift);
    if (ret) {
        dev_err(hr_dev->dev, "roce ib umem write mr failed., ret:%d\n", ret);
        goto out;
    }

    return 0;

out:
    npages = mr->hr_umem.npages;
    if (hr_dev->caps.pbl_hop_num) {
        hns_roce_mhop_free(hr_dev, mr);
    } else {
        dma_free_coherent(dev, npages * BA_BYTE_LEN, mr->pbl_buf, mr->pbl_dma_addr);
    }

    return ret;
}

static int rereg_mr_trans(struct ib_mr *ibmr, int flags, u64 start, u64 length, u64 virt_addr,
    int mr_access_flags, struct hns_roce_cmd_mailbox *mailbox, u32 pdn)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibmr->device);
    struct hns_roce_mr *mr = to_hr_mr(ibmr);
    struct device *dev = hr_dev->dev;
    int npages;
    int ret;
    struct hns_roce_user_mr_info user_mr = {0};
    user_mr.start = start;
    user_mr.length = length;
    user_mr.virt_addr = virt_addr;
    user_mr.access_flags = mr_access_flags;
    user_mr.flags = flags;
    user_mr.pdn = pdn;

    if (mr->size != ~0ULL) {
        npages = mr->hr_umem.npages;
        user_mr.npages = npages;
        if (hr_dev->caps.pbl_hop_num) {
            hns_roce_mhop_free(hr_dev, mr);
        } else {
            dma_free_coherent(dev, npages * BA_BYTE_LEN, mr->pbl_buf, mr->pbl_dma_addr);
        }
    }
    hns_roce_umem_release(mr->umem, &mr->hr_umem);
    mr->umem = hns_roce_umem_get(ibmr->uobject->context, user_mr, 0, IB_PEER_MEM_ALLOW, &mr->hr_umem);
    if (IS_ERR(mr->umem)) {
        mr->umem = NULL;
        dev_err(hr_dev->dev, "hns_roce uemm get operation failed, addr[%llu], size[%llu]," \
            "access flag[%d] and peer mem flag[%d]\n", start, length, mr_access_flags, IB_PEER_MEM_ALLOW);
        return -ENOMEM;
    }

    ret = rereg_mr_trans_write(hr_dev, mr, &user_mr, mailbox);
    if (ret) {
        dev_err(hr_dev->dev, "reg mr trans write failed");
        goto release_umem;
    }

    return 0;
release_umem:
    hns_roce_umem_release(mr->umem, &mr->hr_umem);
    return ret;
}

STATIC int hns_roce_write_mpt_trans(struct hns_roce_dev *hr_dev,
    struct hns_roce_cmd_mailbox *mailbox, struct hns_roce_user_reregmr_info rereg_mr_info)
{
    int ret;
    unsigned long mtpt_idx;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    struct rreg_wrt_mtpt_info rereg_info = {0};
    struct hns_roce_mr *mr = to_hr_mr(rereg_mr_info.ibmr);

    mtpt_idx = key_to_hw_index(mr->key) & (unsigned int)(hr_dev->caps.num_mtpts - 1);
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = mtpt_idx;
    mbox_pst_params.op = HNS_ROCE_CMD_QUERY_MPT;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mailbox preparation failed (%d)\n", ret);
        return ret;
    }

    ret = hns_roce_hw2sw_mpt(hr_dev, NULL, mtpt_idx);
    if (ret) {
        dev_warn(hr_dev->dev, "HW2SW_MPT failed (%d)\n", ret);
    }

    mr->enabled = 0;
    if ((unsigned int)rereg_mr_info.flags & IB_MR_REREG_PD) {
        rereg_info.flags = rereg_mr_info.flags;
        rereg_info.pdn = to_hr_pd(rereg_mr_info.pd)->pdn;
        rereg_info.mr_access_flags = rereg_mr_info.access_flags;
        rereg_info.iova = rereg_mr_info.virt_addr;
        rereg_info.size = rereg_mr_info.length;
        ret = hr_dev->hw->rereg_write_mtpt(hr_dev, mr, rereg_info, mailbox->buf);
        if (ret) {
            dev_err(hr_dev->dev, "%s(%d) hns_roce_hw rereg write mtpt failed\n", __func__, __LINE__);
            return ret;
        }
    }

    if ((unsigned int)rereg_mr_info.flags & IB_MR_REREG_TRANS) {
        ret = rereg_mr_trans(rereg_mr_info.ibmr, rereg_mr_info.flags, rereg_mr_info.start,
            rereg_mr_info.length, rereg_mr_info.virt_addr, rereg_mr_info.access_flags, mailbox, rereg_info.pdn);
        if (ret) {
            dev_err(hr_dev->dev, "rereg MR trans failed, addr[%llu], size[%llu], virt_aadr[%llu], pdn[%u]\n",
                rereg_mr_info.start, rereg_mr_info.length, rereg_mr_info.virt_addr, rereg_info.pdn);
            return ret;
        }
    }

    ret = hns_roce_sw2hw_mpt(hr_dev, mailbox, mtpt_idx);
    if (ret) {
        dev_err(hr_dev->dev, "SW2HW_MPT failed (%d)\n", ret);
        hns_roce_umem_release(mr->umem, &mr->hr_umem);
    }

    return ret;
}

int hns_roce_rereg_user_mr(struct ib_mr *ibmr, int flags, u64 start, u64 length, u64 virt_addr,
    int mr_access_flags, struct ib_pd *pd, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_mr *mr = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_user_reregmr_info rereg_mr_info;
    int ret;

    if (ibmr == NULL || pd == NULL) {
        pr_err("hns3: rereg user mr param err, ibmr %pK, pd %pK\n", ibmr, pd);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibmr->device);
    mr = to_hr_mr(ibmr);
    if (!mr->enabled) {
        dev_err(hr_dev->dev, "current hns_roce MR isn't enabled\n");
        return -EINVAL;
    }

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        dev_err(hr_dev->dev, "%s(%d) alloc cmd_mailbox failed\n", __func__, __LINE__);
        return PTR_ERR(mailbox);
    }

    rereg_mr_info.access_flags = mr_access_flags;
    rereg_mr_info.pd = pd;
    rereg_mr_info.flags = flags;
    rereg_mr_info.ibmr = ibmr;
    rereg_mr_info.length = length;
    rereg_mr_info.start = start;
    rereg_mr_info.virt_addr = virt_addr;
    ret = hns_roce_write_mpt_trans(hr_dev, mailbox, rereg_mr_info);
    if (ret) {
        dev_err(hr_dev->dev, "write mpt and trans failed\n");
        goto free_cmd_mbox;
    }

    mr->enabled = 1;
    if (((unsigned int)flags) & IB_MR_REREG_ACCESS) {
        mr->access = mr_access_flags;
    }

    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return 0;

free_cmd_mbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}
#endif

int hns_roce_dereg_mr(struct ib_mr *ibmr, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_mr *mr = NULL;
    int ret = 0;

    if (ibmr == NULL) {
        pr_err("hns3: dereg mr param err, ibmr is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibmr->device);
    mr = to_hr_mr(ibmr);

    hns_roce_inc_rdma_hw_stats(&hr_dev->ib_dev, HW_STATS_MR_DEALLOC);

    if (hr_dev->hw->dereg_mr) {
        ret = hr_dev->hw->dereg_mr(hr_dev, mr);
    } else {
        hns_roce_mr_free(hr_dev, mr);

        if (mr->umem != NULL) {
            hns_roce_umem_release(mr->umem, &mr->hr_umem);
        }

        kfree(mr);
        mr = NULL;
    }

    return ret;
}

struct ib_mr *hns_roce_alloc_mr(struct ib_pd *pd, enum ib_mr_type mr_type,
    u32 max_num_sg)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_mr *mr = NULL;
    struct hns_roce_mr_alloc_para mr_para;
    u32 page_size;
    int ret;

    if (mr_type != IB_MR_TYPE_MEM_REG || pd == NULL) {
        pr_err("hns3: alloc mr param err, mr_type %d, pd %pK\n", mr_type, pd);
        return ERR_PTR(-EINVAL);
    }

    hr_dev = to_hr_dev(pd->device);
    page_size = 1U << (hr_dev->caps.pbl_buf_pg_sz + PAGE_SHIFT);

    if (max_num_sg > HNS_ROCE_FRMR_MAX_PA) {
        dev_err(hr_dev->dev, "max_num_sg larger than %d(HNS_ROCE_FRMR_MAX_PA)\n", HNS_ROCE_FRMR_MAX_PA);
        return ERR_PTR(-EINVAL);
    }

    mr = kzalloc(sizeof(*mr), GFP_KERNEL);
    if (mr == NULL) {
        dev_err(hr_dev->dev, "kzalloc MR mem failed\n");
        return ERR_PTR(-ENOMEM);
    }

    mr->type = MR_TYPE_FRMR;

    /* Allocate memory region key */
    mr_para.pd = to_hr_pd(pd)->pdn;
    mr_para.iova = 0;
    mr_para.size = max_num_sg * page_size;
    mr_para.access = 0;
    ret = hns_roce_mr_alloc(hr_dev, &mr_para, max_num_sg, mr);
    if (ret) {
        dev_err(hr_dev->dev, "alloc memory region key failed\n");
        goto err_free;
    }

    ret = hns_roce_mr_enable(hr_dev, mr);
    if (ret) {
        dev_err(hr_dev->dev, "enable hns_roce failed\n");
        goto err_free_mr;
    }

    mr->ibmr.rkey = mr->ibmr.lkey = mr->key;
    mr->umem = NULL;

    hns_roce_inc_rdma_hw_stats(pd->device, HW_STATS_MR_ALLOC);

    return &mr->ibmr;

err_free_mr:
    hns_roce_mr_free(to_hr_dev(pd->device), mr);

err_free:
    kfree(mr);
    mr = NULL;
    return ERR_PTR(ret);
}

static int hns_roce_set_page(struct ib_mr *ibmr, u64 addr)
{
    struct hns_roce_mr *mr = to_hr_mr(ibmr);

    mr->pbl_buf[mr->npages++] = cpu_to_le64(addr);

    return 0;
}

int hns_roce_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg, int sg_nents,
    unsigned int *sg_offset)
{
    struct hns_roce_mr *mr = NULL;

    if (ibmr == NULL || sg == NULL || sg_offset == NULL) {
        pr_err("hns3: map_mr_sg param err, ibmr %pK, sg %pK, sg_offset %pK\n", ibmr, sg, sg_offset);
        return -EINVAL;
    }

    mr = to_hr_mr(ibmr);
    mr->npages = 0;

    return ib_sg_to_pages(ibmr, sg, sg_nents, sg_offset, hns_roce_set_page);
}

static void hns_roce_mw_free(struct hns_roce_dev *hr_dev,
    struct hns_roce_mw *mw)
{
    struct device *dev = hr_dev->dev;
    int ret;

    if (mw->enabled) {
        ret = hns_roce_hw2sw_mpt(hr_dev, NULL, key_to_hw_index(mw->rkey)
                                 & ((unsigned int)hr_dev->caps.num_mtpts - 1));
        if (ret) {
            dev_warn(dev, "MW HW2SW_MPT failed (%d)\n", ret);
        }

        hns_roce_table_put(hr_dev, &hr_dev->mr_table.mtpt_table,
                           key_to_hw_index(mw->rkey));
    }

    hns_roce_bitmap_free(&hr_dev->mr_table.mtpt_bitmap,
                         key_to_hw_index(mw->rkey), BITMAP_NO_RR);
}

static int hns_roce_mw_enable(struct hns_roce_dev *hr_dev,
    struct hns_roce_mw *mw)
{
    unsigned long mtpt_idx = key_to_hw_index(mw->rkey);
    struct device *dev = hr_dev->dev;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mr_table *mr_table = &hr_dev->mr_table;
    int ret;

    /* prepare HEM entry memory */
    ret = hns_roce_table_get(hr_dev, &mr_table->mtpt_table, mtpt_idx);
    if (ret) {
        dev_err(dev, "prepare HEM entry memory failed\n");
        return ret;
    }

    /* allocate mailbox memory */
    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        ret = PTR_ERR(mailbox);
        dev_err(dev, "%s(%d) alloc cmd mailbox failed\n", __func__, __LINE__);
        goto err_table;
    }

    ret = hr_dev->hw->mw_write_mtpt(mailbox->buf, mw);
    if (ret) {
        dev_err(dev, "MW write mtpt fail!\n");
        goto err_page;
    }

    ret = hns_roce_sw2hw_mpt(hr_dev, mailbox,
                             mtpt_idx & ((unsigned int)hr_dev->caps.num_mtpts - 1));
    if (ret) {
        dev_err(dev, "MW sw2hw_mpt failed (%d)\n", ret);
        goto err_page;
    }

    mw->enabled = 1;

    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return 0;

err_page:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

err_table:
    hns_roce_table_put(hr_dev, &mr_table->mtpt_table, mtpt_idx);

    return ret;
}

int hns_roce_alloc_mw(struct ib_mw *ibmw, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_mw *mw = NULL;
    unsigned long index = 0;
    int ret;

    if (ibmw == NULL) {
        pr_err("hns3: alloc mw param err, ibmw is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibmw->device);
    mw = to_hr_mw(ibmw);

    /* Allocate a key for mw from bitmap */
    ret = hns_roce_bitmap_alloc(&hr_dev->mr_table.mtpt_bitmap, &index);
    if (ret) {
        dev_err(hr_dev->dev, "allocate a key for mw from bitmap failed, ret[%d]\n", ret);
        return ret;
    }

    mw->rkey = hw_index_to_key(index);

    mw->ibmw.rkey = mw->rkey;
    mw->pdn = to_hr_pd(ibmw->pd)->pdn;
    mw->pbl_hop_num = hr_dev->caps.pbl_hop_num;
    mw->pbl_ba_pg_sz = hr_dev->caps.pbl_ba_pg_sz;
    mw->pbl_buf_pg_sz = hr_dev->caps.pbl_buf_pg_sz;

    ret = hns_roce_mw_enable(hr_dev, mw);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce enable mw failed\n");
        goto err_mw;
    }

    return 0;

err_mw:
    hns_roce_mw_free(hr_dev, mw);

    return ret;
}

int hns_roce_dealloc_mw(struct ib_mw *ibmw)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_mw *mw = NULL;

    if (ibmw == NULL) {
        pr_err("hns3: dealloc ibmw param err, ibmw is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibmw->device);
    mw = to_hr_mw(ibmw);

    hns_roce_mw_free(hr_dev, mw);

    return 0;
}
