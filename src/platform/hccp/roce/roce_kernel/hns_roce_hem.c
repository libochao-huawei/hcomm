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
#include "roce_k_compat.h"
#include "hns_roce_device.h"
#include "hns_roce_common.h"
#include "hns_roce_sec.h"
#include "hns_roce_hem.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#define DMA_ADDR_T_SHIFT        12
#define BT_BA_SHIFT         32

bool hns_roce_check_whether_mhop(struct hns_roce_dev *hr_dev, u32 type)
{
    int hop_num = 0;

    switch (type) {
        case HEM_TYPE_QPC:
            hop_num = hr_dev->caps.qpc_hop_num;
            break;
        case HEM_TYPE_MTPT:
            hop_num = hr_dev->caps.mpt_hop_num;
            break;
        case HEM_TYPE_CQC:
            hop_num = hr_dev->caps.cqc_hop_num;
            break;
        case HEM_TYPE_SRQC:
            hop_num = hr_dev->caps.srqc_hop_num;
            break;
        case HEM_TYPE_SCC_CTX:
            hop_num = hr_dev->caps.scc_ctx_hop_num;
            break;
        case HEM_TYPE_QPC_TIMER:
            hop_num = hr_dev->caps.qpc_timer_hop_num;
            break;
        case HEM_TYPE_CQC_TIMER:
            hop_num = hr_dev->caps.cqc_timer_hop_num;
            break;
        case HEM_TYPE_CQE:
            hop_num = hr_dev->caps.cqe_hop_num;
            break;
        case HEM_TYPE_MTT:
            hop_num = hr_dev->caps.mtt_hop_num;
            break;
        case HEM_TYPE_SRQWQE:
            hop_num = hr_dev->caps.srqwqe_hop_num;
            break;
        case HEM_TYPE_IDX:
            hop_num = hr_dev->caps.idx_hop_num;
            break;
        default:
            return false;
    }

    return hop_num ? true : false;
}

static bool hns_roce_check_hem_null(struct hns_roce_hem **hem, u64 start_idx,
                                    u32 bt_chunk_num)
{
    u32 i;

    for (i = 0; i < bt_chunk_num; i++) {
        if (hem[start_idx + i]) {
            return false;
        }
    }

    return true;
}

static bool hns_roce_check_bt_null(u64 **bt, u64 start_idx, u32 bt_chunk_num)
{
    u32 i;

    for (i = 0; i < bt_chunk_num; i++) {
        if (bt[start_idx + i]) {
            return false;
        }
    }

    return true;
}

static int hns_roce_get_bt_num(u32 table_type, u32 hop_num)
{
    if (check_whether_bt_num_3(table_type, hop_num)) {
        return BT_NUM_3;
    } else if (check_whether_bt_num_2(table_type, hop_num)) {
        return BT_NUM_2;
    } else if (check_whether_bt_num_1(table_type, hop_num)) {
        return BT_NUM_1;
    } else {
        return 0;
    }
}

static void get_qpc_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.qpc_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.qpc_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.qpc_bt_num;
    mhop->hop_num = hr_dev->caps.qpc_hop_num;
}

static void get_mpt_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.mpt_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.mpt_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.mpt_bt_num;
    mhop->hop_num = hr_dev->caps.mpt_hop_num;
}

static void get_cqc_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.cqc_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.cqc_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.cqc_bt_num;
    mhop->hop_num = hr_dev->caps.cqc_hop_num;
}

static void get_scc_ctx_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.scc_ctx_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.scc_ctx_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.scc_ctx_bt_num;
    mhop->hop_num = hr_dev->caps.scc_ctx_hop_num;
}


static void get_qpc_timer_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.qpc_timer_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.qpc_timer_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.qpc_timer_bt_num;
    mhop->hop_num = hr_dev->caps.qpc_timer_hop_num;
}

static void get_cqc_timer_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.cqc_timer_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.cqc_timer_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.cqc_timer_bt_num;
    mhop->hop_num = hr_dev->caps.cqc_timer_hop_num;
}

static void get_srqc_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.srqc_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.srqc_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = hr_dev->caps.srqc_bt_num;
    mhop->hop_num = hr_dev->caps.srqc_hop_num;
}

static void get_mtt_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.mtt_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.mtt_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = mhop->bt_chunk_size / BA_BYTE_LEN;
    mhop->hop_num = hr_dev->caps.mtt_hop_num;
}

static void get_cqe_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.cqe_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.cqe_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = mhop->bt_chunk_size / BA_BYTE_LEN;
    mhop->hop_num = hr_dev->caps.cqe_hop_num;
}

static void get_srq_wqe_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.srqwqe_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.srqwqe_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = mhop->bt_chunk_size / BA_BYTE_LEN;
    mhop->hop_num = hr_dev->caps.srqwqe_hop_num;
}


static void get_idx_table_config(struct hns_roce_dev *hr_dev, struct hns_roce_hem_mhop *mhop)
{
    mhop->buf_chunk_size = 1U << (hr_dev->caps.idx_buf_pg_sz
                                 + PAGE_SHIFT);
    mhop->bt_chunk_size = 1U << (hr_dev->caps.idx_ba_pg_sz
                                + PAGE_SHIFT);
    mhop->ba_l0_num = mhop->bt_chunk_size / BA_BYTE_LEN;
    mhop->hop_num = hr_dev->caps.idx_hop_num;
}

static int get_hem_table_config(struct hns_roce_dev *hr_dev,
                                struct hns_roce_hem_mhop *mhop,
                                u32 type)
{
    struct device *dev = hr_dev->dev;

    switch (type) {
        case HEM_TYPE_QPC:
            get_qpc_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_MTPT:
            get_mpt_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_CQC:
            get_cqc_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_SCC_CTX:
            get_scc_ctx_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_QPC_TIMER:
            get_qpc_timer_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_CQC_TIMER:
            get_cqc_timer_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_SRQC:
            get_srqc_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_MTT:
            get_mtt_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_CQE:
            get_cqe_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_SRQWQE:
            get_srq_wqe_table_config(hr_dev, mhop);
            break;
        case HEM_TYPE_IDX:
            get_idx_table_config(hr_dev, mhop);
            break;
        default:
            dev_err(dev, "Table %d not support multi-hop addressing!\n", type);
            return -EINVAL;
    }

    return 0;
}

int hns_roce_calc_hem_mhop(struct hns_roce_dev *hr_dev,
                           struct hns_roce_hem_table *table, unsigned long *obj,
                           struct hns_roce_hem_mhop *mhop)
{
    u32 chunk_ba_num;
    u32 table_idx;
    u32 bt_num;
    u32 chunk_size;

    if (get_hem_table_config(hr_dev, mhop, table->type)) {
        dev_err(hr_dev->dev, "hns_roce_calc_hem_mhop get hem table config failed!\n");
        return -EINVAL;
    }

    if (obj == NULL) {
        return 0;
    }

    /*
     * QPC/MTPT/CQC/SRQC/SCC_CTX alloc hem for buffer pages.
     * MTT/CQE alloc hem for bt pages.
     */
    bt_num = hns_roce_get_bt_num(table->type, mhop->hop_num);
    chunk_ba_num = mhop->bt_chunk_size / BA_BYTE_LEN;
    chunk_size = table->type < HEM_TYPE_MTT ? mhop->buf_chunk_size :
                 mhop->bt_chunk_size;
    table_idx = (*obj & (table->num_obj - 1)) /
                (chunk_size / table->obj_size);
    switch (bt_num) {
        case BT_NUM_3:
            mhop->l2_idx = table_idx & (chunk_ba_num - 1);
            mhop->l1_idx = (table_idx / chunk_ba_num) & (chunk_ba_num - 1);
            mhop->l0_idx = (table_idx / chunk_ba_num) / chunk_ba_num;
            break;
        case BT_NUM_2:
            mhop->l1_idx = table_idx & (chunk_ba_num - 1);
            mhop->l0_idx = table_idx / chunk_ba_num;
            break;
        case BT_NUM_1:
            mhop->l0_idx = table_idx;
            break;
        default:
            dev_err(hr_dev->dev, "Table %u not support hop_num = %u!\n", table->type, mhop->hop_num);
            return -EINVAL;
    }
    if (mhop->l0_idx >= mhop->ba_l0_num) {
        mhop->l0_idx %= mhop->ba_l0_num;
    }

    return 0;
}

STATIC void hns_roce_free_hem(struct hns_roce_dev *hr_dev, struct hns_roce_hem **hem)
{
    struct hns_roce_hem_chunk *chunk = NULL;
    struct hns_roce_hem_chunk *tmp = NULL;
    int i;

    if (hem == NULL || *hem == NULL) {
        return;
    }
    list_for_each_entry_safe(chunk, tmp, &((*hem)->chunk_list), list) {
        for (i = 0; i < chunk->npages; ++i) {
            dma_free_coherent(hr_dev->dev,
                              sg_dma_len(&chunk->mem[i]),
                              chunk->buf[i],
                              sg_dma_address(&chunk->mem[i]));
            chunk->buf[i] = NULL;
        }
        list_del(&chunk->list);
        kfree(chunk);
        chunk = NULL;
    }

    kfree(*hem);
    *hem = NULL;
}

static struct hns_roce_hem_chunk *hns_roce_alloc_hem_chunk(struct hns_roce_dev *hr_dev, gfp_t gfp_mask)
{
    int ret;
    struct hns_roce_hem_chunk *chunk = NULL;

    chunk = kzalloc(sizeof(*chunk), gfp_mask & ~(__GFP_HIGHMEM | __GFP_NOWARN));
    if (chunk == NULL) {
        dev_err(hr_dev->dev, "kzalloc hns_roce_hem_chunk failed\n");
        return NULL;
    }

    sg_init_table(chunk->mem, HNS_ROCE_HEM_CHUNK_LEN);
    chunk->npages = 0;
    chunk->nsg = 0;
    ret = memset_s(chunk->buf, sizeof(chunk->buf), 0, sizeof(chunk->buf));
    if (ret) {
        dev_err(hr_dev->dev, "memset_s func %s line %d\n", __func__, __LINE__);
        kfree(chunk);
        chunk = NULL;
        return NULL;
    }

    return chunk;
}

static struct hns_roce_hem *hns_roce_alloc_hem(struct hns_roce_dev *hr_dev,
                                               u32 npages,
                                               unsigned long hem_alloc_size,
                                               gfp_t gfp_mask)
{
    struct hns_roce_hem_chunk *chunk = NULL;
    struct hns_roce_hem *hem = NULL;
    struct scatterlist *mem = NULL;
    unsigned int order;
    void *buf = NULL;
    u32 left;

    WARN_ON(gfp_mask & __GFP_HIGHMEM);

    hem = kzalloc(sizeof(*hem), gfp_mask & ~(__GFP_HIGHMEM | __GFP_NOWARN));
    if (hem == NULL) {
        dev_err(hr_dev->dev, "kzalloc hns_roce_hem failed\n");
        return NULL;
    }

    hem->refcount = 0;
    INIT_LIST_HEAD(&hem->chunk_list);

    order = (unsigned int)get_order(hem_alloc_size);
    left = npages;
    while (left > 0) {
        if (chunk == NULL) {
            chunk = hns_roce_alloc_hem_chunk(hr_dev, gfp_mask);
            if (chunk == NULL) {
                dev_err(hr_dev->dev, "hns_roce_alloc_hem_chunk failed\n");
                goto fail;
            }

            list_add_tail(&chunk->list, &hem->chunk_list);
        }

        while ((1U << order) > left) {
            --order;
        }

        /*
         * Alloc memory one time. If failed, don't alloc small block
         * memory, directly return fail.
         */
        mem = &chunk->mem[chunk->npages];
        buf = dma_alloc_coherent(hr_dev->dev, PAGE_SIZE << order, &sg_dma_address(mem), gfp_mask);
        if (buf == NULL) {
            dev_err(hr_dev->dev, "dma_alloc_coherent failed\n");
            goto fail;
        }

        chunk->buf[chunk->npages] = buf;
        sg_dma_len(mem) = PAGE_SIZE << order;

        ++chunk->npages;
        ++chunk->nsg;
        left -= 1U << order;
    }

    return hem;

fail:
    hns_roce_free_hem(hr_dev, &hem);
    return NULL;
}

static int set_hem_type_info(struct hns_roce_hem_table *table, unsigned int *bt_cmd_h_val)
{
    switch (table->type) {
        case HEM_TYPE_QPC:
            roce_set_field(*bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_M,
                           ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_S, HEM_TYPE_QPC);
            break;
        case HEM_TYPE_MTPT:
            roce_set_field(*bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_M,
                           ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_S, HEM_TYPE_MTPT);
            break;
        case HEM_TYPE_CQC:
            roce_set_field(*bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_M,
                           ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_S, HEM_TYPE_CQC);
            break;
        case HEM_TYPE_SRQC:
            roce_set_field(*bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_M,
                           ROCEE_BT_CMD_H_ROCEE_BT_CMD_MDF_S, HEM_TYPE_SRQC);
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static int hns_roce_set_hem(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table, unsigned long obj)
{
    spinlock_t *lock = &hr_dev->bt_cmd_lock;
    struct device *dev = hr_dev->dev;
    unsigned long flags;
    struct hns_roce_hem_iter iter;
    void __iomem *bt_cmd = NULL;
    u32 bt_cmd_h_val = 0;
    u32 bt_cmd_val[BT_CMD_VAL_LEN];
    u32 bt_cmd_l = 0;
    u64 bt_ba = 0;
    int ret;
    int timeout_count;

    /* Find the HEM(Hardware Entry Memory) entry */
    unsigned long i = (obj & (table->num_obj - 1)) / (table->table_chunk_size / table->obj_size);
    ret = set_hem_type_info(table, &bt_cmd_h_val);
    if (ret) {
        return 0;
    }

    roce_set_field(bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_IN_MDF_M, ROCEE_BT_CMD_H_ROCEE_BT_CMD_IN_MDF_S, obj);
    roce_set_bit(bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_S, 0);
    roce_set_bit(bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_HW_SYNS_S, 1);

    /* Currently iter only a chunk */
    for (hns_roce_hem_first(table->hem[i], &iter); !hns_roce_hem_last(&iter); hns_roce_hem_next(&iter)) {
        bt_ba = hns_roce_hem_addr(&iter) >> DMA_ADDR_T_SHIFT;
        timeout_count = HW_SYNC_TIMEOUT_COUNT;
        spin_lock_irqsave(lock, flags);

        bt_cmd = hr_dev->reg_base + ROCEE_BT_CMD_H_REG;
        while (1) {
            if (!(readl(bt_cmd) >> BT_CMD_SYNC_SHIFT)) {
                break;
            }

            if (timeout_count <= 0) {
                dev_err(dev, "Write bt_cmd err,hw_sync is not zero.\n");
                spin_unlock_irqrestore(lock, flags);
                return -EBUSY;
            }

            mdelay(HW_SYNC_SLEEP_TIME_INTERVAL);
            timeout_count--;
        }

        bt_cmd_l = (u32)bt_ba;
        roce_set_field(bt_cmd_h_val, ROCEE_BT_CMD_H_ROCEE_BT_CMD_BA_H_M,
            ROCEE_BT_CMD_H_ROCEE_BT_CMD_BA_H_S, bt_ba >> BT_BA_SHIFT);

        bt_cmd_val[0] = bt_cmd_l;
        bt_cmd_val[1] = bt_cmd_h_val;
        hns_roce_write64_k(bt_cmd_val, sizeof(bt_cmd_val) / sizeof(u32), hr_dev->reg_base + ROCEE_BT_CMD_L_REG);
        spin_unlock_irqrestore(lock, flags);
    }

    return ret;
}

static int hns_roce_get_bt_idx(struct hns_roce_hem_table *table,
    struct hns_roce_hem_mhop mhop, u64 *hem_idx, u64 *bt_l1_idx, u64 *bt_l0_idx)
{
    u32 buf_chunk_size;
    u32 bt_chunk_size;
    u32 chunk_ba_num;
    u32 hop_num;
    u32 bt_num;

    buf_chunk_size = mhop.buf_chunk_size;
    bt_chunk_size = mhop.bt_chunk_size;
    hop_num = mhop.hop_num;
    chunk_ba_num = bt_chunk_size / BA_BYTE_LEN;

    bt_num = hns_roce_get_bt_num(table->type, hop_num);
    switch (bt_num) {
        case BT_NUM_3:
            *hem_idx = mhop.l0_idx * chunk_ba_num * chunk_ba_num +
                      mhop.l1_idx * chunk_ba_num + mhop.l2_idx;
            *bt_l1_idx = mhop.l0_idx * chunk_ba_num + mhop.l1_idx;
            *bt_l0_idx = mhop.l0_idx;
            break;
        case BT_NUM_2:
            *hem_idx = mhop.l0_idx * chunk_ba_num + mhop.l1_idx;
            *bt_l0_idx = mhop.l0_idx;
            break;
        case BT_NUM_1:
            *hem_idx = mhop.l0_idx;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static int hns_roce_set_bt_ba(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    struct hns_roce_hem_mhop mhop, const struct hns_roce_bt_info bt_info, unsigned long obj)
{
    struct device *dev = hr_dev->dev;
    u32 hop_num = mhop.hop_num;
    u64 bt_ba = bt_info.bt_ba;
    u64 bt_l0_idx = bt_info.bt_l0_idx;
    u64 bt_l1_idx = bt_info.bt_l1_idx;
    int step_idx;
    if (table->type < HEM_TYPE_MTT) {
        if (hop_num == HOP_NUM_2) {
            *(table->bt_l1[bt_l1_idx] + mhop.l2_idx) = bt_ba;
            step_idx = HOP_NUM_2_STEP_INDX;
        } else if (hop_num == HOP_NUM_1) {
            *(table->bt_l0[bt_l0_idx] + mhop.l1_idx) = bt_ba;
            step_idx = HOP_NUM_1_STEP_INDX;
        } else if (hop_num == HNS_ROCE_HOP_NUM_0) {
            step_idx = 0;
        } else {
            dev_err(dev, "Invalid hop num : %u\n", hop_num);
            return -EINVAL;
        }

        /* set HEM base address to hardware */
        if (hr_dev->hw->set_hem(hr_dev, table, obj, step_idx)) {
            dev_err(dev, "set HEM base address to HW failed!\n");
            return -ENODEV;
        }
    } else if (hop_num == HOP_NUM_2) {
        *(table->bt_l0[bt_l0_idx] + mhop.l1_idx) = bt_ba;
    }

    return 0;
}

void hns_roce_table_var_init(u64 *hem_idx, u64 *bt_l0_idx, u64 *bt_l1_idx, struct hns_roce_hem_mhop mhop,
    struct hns_roce_alloc_param *alloc_params)
{
    *hem_idx = 0;
    *bt_l0_idx = 0;
    *bt_l1_idx = 0;
    alloc_params->buf_chunk_size = mhop.buf_chunk_size;
    alloc_params->bt_chunk_size = mhop.bt_chunk_size;
}

void hns_roce_table_mhop_idx_init(struct hns_roce_alloc_param *alloc_params, u64 hem_idx, u64 bt_l1_idx,
    u64 bt_l0_idx, unsigned long obj)
{
    alloc_params->hem_idx = hem_idx;
    alloc_params->bt_l0_idx = bt_l0_idx;
    alloc_params->bt_l1_idx = bt_l1_idx;
    alloc_params->obj = obj;
}

STATIC void hns_roce_free_bt_l0(struct device *dev, struct hns_roce_alloc_param *alloc_params,
    struct hns_roce_hem_table *table)
{
    if (alloc_params->bt_l0_allocated) {
        dma_free_coherent(dev, alloc_params->bt_chunk_size, table->bt_l0[alloc_params->bt_l0_idx],
            table->bt_l0_dma_addr[alloc_params->bt_l0_idx]);
        table->bt_l0[alloc_params->bt_l0_idx] = NULL;
    }
}

STATIC void hns_roce_free_bt_l1(struct device *dev, struct hns_roce_alloc_param *alloc_params,
    struct hns_roce_hem_table *table)
{
    if (alloc_params->bt_l1_allocated) {
        dma_free_coherent(dev, alloc_params->bt_chunk_size, table->bt_l1[alloc_params->bt_l1_idx],
            table->bt_l1_dma_addr[alloc_params->bt_l1_idx]);
        table->bt_l1[alloc_params->bt_l1_idx] = NULL;
    }
}

int hns_roce_alloc_bt_l0(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    struct hns_roce_hem_mhop mhop, struct hns_roce_alloc_param *alloc_params)
{
    int ret = 0;
    int step_idx;
    bool alloc_bt_l0;
    struct device *dev = hr_dev->dev;

    alloc_bt_l0 = (check_whether_bt_num_3(table->type, mhop.hop_num) ||
        check_whether_bt_num_2(table->type, mhop.hop_num)) && (table->bt_l0[alloc_params->bt_l0_idx] == NULL);
    if (alloc_bt_l0) {
        table->bt_l0[alloc_params->bt_l0_idx] = dma_alloc_coherent(dev, alloc_params->bt_chunk_size,
            &(table->bt_l0_dma_addr[alloc_params->bt_l0_idx]), GFP_KERNEL | __GFP_NOWARN);
        if (table->bt_l0[alloc_params->bt_l0_idx] == NULL) {
            dev_err(dev, "bt_l0[%llu] is NULL\n", alloc_params->bt_l0_idx);
            return -ENOMEM;
        }
        alloc_params->bt_l0_allocated = 1;

        if (table->type < HEM_TYPE_MTT) {
            step_idx = 0;
            ret = hr_dev->hw->set_hem(hr_dev, table, alloc_params->obj, step_idx);
            if (ret) {
                dev_err(dev, "bt_l0 set HEM base address to HW failed(%d), type = %d\n", ret, table->type);
                hns_roce_free_bt_l0(dev, alloc_params, table);
                return -ENODEV;
            }
        }
    }

    return ret;
}

int hns_roce_alloc_bt_l1(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    struct hns_roce_alloc_param *alloc_params, struct hns_roce_hem_mhop mhop)
{
    int ret = 0;
    int step_idx;
    bool alloc_bt_l1;
    struct device *dev = hr_dev->dev;

    alloc_bt_l1 = check_whether_bt_num_3(table->type, mhop.hop_num) && (table->bt_l1[alloc_params->bt_l1_idx] == NULL);
    if (alloc_bt_l1) {
        table->bt_l1[alloc_params->bt_l1_idx] = dma_alloc_coherent(dev, alloc_params->bt_chunk_size,
            &(table->bt_l1_dma_addr[alloc_params->bt_l1_idx]), GFP_KERNEL | __GFP_NOWARN);
        if (table->bt_l1[alloc_params->bt_l1_idx] == NULL) {
            dev_err(dev, "bt_l1[%llu] is NULL\n", alloc_params->bt_l1_idx);
            return -ENOMEM;
        }
        alloc_params->bt_l1_allocated = 1;
        *(table->bt_l0[alloc_params->bt_l0_idx] + mhop.l1_idx) = table->bt_l1_dma_addr[alloc_params->bt_l1_idx];

        /* set base address to hardware */
        step_idx = 1;
        ret = hr_dev->hw->set_hem(hr_dev, table, alloc_params->obj, step_idx);
        if (ret) {
            dev_err(dev, "bt_l1 set HEM base address to HW failed(%d), type = %u\n", ret, table->type);
            hns_roce_free_bt_l1(dev, alloc_params, table);
            return -ENODEV;
        }
    }
    return ret;
}

int hns_roce_alloc_bt_space(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    struct hns_roce_alloc_param alloc_params, struct hns_roce_hem_mhop mhop)
{
    u64 bt_ba;
    int ret;
    u32 size;
    struct hns_roce_hem_iter iter;
    struct hns_roce_bt_info roce_bt_info;
    struct device *dev = hr_dev->dev;

    /*
     * alloc buffer space chunk for QPC/MTPT/CQC/SRQC/SCC_CTX.
     * alloc bt space chunk for MTT/CQE.
     */
    size = table->type < HEM_TYPE_MTT ? alloc_params.buf_chunk_size : alloc_params.bt_chunk_size;
    table->hem[alloc_params.hem_idx] = hns_roce_alloc_hem(hr_dev, size >> PAGE_SHIFT, size,
        (table->lowmem ? GFP_KERNEL : GFP_HIGHUSER) | __GFP_NOWARN);
    if (table->hem[alloc_params.hem_idx] == NULL) {
        dev_err(dev, "hem[%llu] is NULL\n", alloc_params.hem_idx);
        return -ENOMEM;
    }

    hns_roce_hem_first(table->hem[alloc_params.hem_idx], &iter);
    bt_ba = hns_roce_hem_addr(&iter);
    HNS_ROCE_BT_ASSIGN(roce_bt_info, bt_ba, alloc_params.bt_l0_idx, alloc_params.bt_l1_idx);
    ret = hns_roce_set_bt_ba(hr_dev, table, mhop, roce_bt_info, alloc_params.obj);
    if (ret) {
        dev_err(dev, "set HEM base address ret %d!\n", ret);
        hns_roce_free_hem(hr_dev, &table->hem[alloc_params.hem_idx]);
        return ret;
    }

    return ret;
}

static int hns_roce_table_mhop_get(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table, unsigned long obj)
{
    int ret;
    struct hns_roce_hem_mhop mhop;
    u64 hem_idx, bt_l1_idx, bt_l0_idx;
    struct device *dev = hr_dev->dev;
    struct hns_roce_alloc_param alloc_params = {0};

    ret = hns_roce_calc_hem_mhop(hr_dev, table, &obj, &mhop);
    if (ret) {
        return ret;
    }

    hns_roce_table_var_init(&hem_idx, &bt_l0_idx, &bt_l1_idx, mhop, &alloc_params);
    ret = hns_roce_get_bt_idx(table, mhop, &hem_idx, &bt_l1_idx, &bt_l0_idx);
    if (ret) {
        dev_err(dev, "Table %u fail to hop_num = %u get bt idx!\n", table->type, mhop.hop_num);
        return ret;
    }

    hns_roce_table_mhop_idx_init(&alloc_params, hem_idx, bt_l1_idx, bt_l0_idx, obj);

    mutex_lock(&table->mutex);

    if (table->hem[hem_idx]) {
        ++table->hem[hem_idx]->refcount;
        goto out;
    }

    /* alloc L1 BA's chunk */
    ret = hns_roce_alloc_bt_l0(hr_dev, table, mhop, &alloc_params);
    if (ret) {
        goto err_alloc_bt_l0;
    }

    /* alloc L2 BA's chunk */
    ret = hns_roce_alloc_bt_l1(hr_dev, table, &alloc_params, mhop);
    if (ret) {
        goto err_alloc_bt_l1;
    }

    ret = hns_roce_alloc_bt_space(hr_dev, table, alloc_params, mhop);
    if (ret) {
        goto err_alloc_bt_space;
    }

    ++table->hem[hem_idx]->refcount;
    mutex_unlock(&table->mutex);
    return 0;

err_alloc_bt_space:
    hns_roce_free_bt_l1(hr_dev->dev, &alloc_params, table);
err_alloc_bt_l1:
    hns_roce_free_bt_l0(hr_dev->dev, &alloc_params, table);
err_alloc_bt_l0:
out:
    mutex_unlock(&table->mutex);
    return ret;
}

int hns_roce_table_get(struct hns_roce_dev *hr_dev,
                       struct hns_roce_hem_table *table, unsigned long obj)
{
    struct device *dev = hr_dev->dev;
    int ret = 0;
    unsigned long i;

    if (hns_roce_check_whether_mhop(hr_dev, table->type)) {
        return hns_roce_table_mhop_get(hr_dev, table, obj);
    }

    i = (obj & (table->num_obj - 1)) / (table->table_chunk_size /
                                        table->obj_size);

    mutex_lock(&table->mutex);

    if (table->hem[i]) {
        ++table->hem[i]->refcount;
        goto out;
    }

    table->hem[i] = hns_roce_alloc_hem(hr_dev,
                                       table->table_chunk_size >> PAGE_SHIFT,
                                       table->table_chunk_size,
                                       (table->lowmem ? GFP_KERNEL :
                                        GFP_HIGHUSER) | __GFP_NOWARN);
    if (table->hem[i] == NULL) {
        ret = -ENOMEM;
        dev_err(dev, "hem[%lu] is NULL\n", i);
        goto out;
    }

    /* Set HEM base address(128K/page, pa) to Hardware */
    if (hns_roce_set_hem(hr_dev, table, obj)) {
        hns_roce_free_hem(hr_dev, &table->hem[i]);
        table->hem[i] = NULL;
        ret = -ENODEV;
        dev_err(dev, "set HEM base address to HW failed.\n");
        goto out;
    }

    ++table->hem[i]->refcount;
out:
    mutex_unlock(&table->mutex);
    return ret;
}


void hns_roce_table_clear_hem(struct hns_roce_dev *hr_dev,
    struct hns_roce_hem_table *table, unsigned long obj, u32 hop_num)
{
    if (table->type < HEM_TYPE_MTT && hop_num == HOP_NUM_1) {
        hr_dev->hw->clear_hem(hr_dev, table, obj, HOP_NUM_1_STEP_INDX);
    } else if (table->type < HEM_TYPE_MTT && hop_num == HOP_NUM_2) {
        hr_dev->hw->clear_hem(hr_dev, table, obj, HOP_NUM_2_STEP_INDX);
    } else if (table->type < HEM_TYPE_MTT && hop_num == HNS_ROCE_HOP_NUM_0) {
        hr_dev->hw->clear_hem(hr_dev, table, obj, 0);
    }
}

void hns_roce_table_clean_hem(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    unsigned long obj, u32 hop_num, u64 hem_idx)
{
    hns_roce_table_clear_hem(hr_dev, table, obj, hop_num);

    /*
     * free buffer space chunk for QPC/MTPT/CQC/SRQC/SCC_CTX.
     * free bt space chunk for MTT/CQE.
     */
    hns_roce_free_hem(hr_dev, &table->hem[hem_idx]);
    table->hem[hem_idx] = NULL;
}

void hns_roce_idx_init(u64 *hem_idx, u64 *bt_l1_idx, u64 *bt_l0_idx)
{
    *hem_idx = 0;
    *bt_l0_idx = 0;
    *bt_l1_idx = 0;
}

void hns_roce_bt_chunk_hopnum_init(struct hns_roce_hem_mhop mhop, u32 *bt_chunk_size,
    u32 *chunk_ba_num, u32 *hop_num)
{
    *bt_chunk_size = mhop.bt_chunk_size;
    *hop_num = mhop.hop_num;
    *chunk_ba_num = *bt_chunk_size / BA_BYTE_LEN;
}

static void hns_roce_table_mhop_put(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    unsigned long obj, int check_refcount)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_hem_mhop mhop;
    unsigned long mhop_obj = obj;
    u32 bt_chunk_size, chunk_ba_num, hop_num, start_idx;
    u64 hem_idx, bt_l1_idx, bt_l0_idx;

    hns_roce_idx_init(&hem_idx, &bt_l1_idx, &bt_l0_idx);
    if (hns_roce_calc_hem_mhop(hr_dev, table, &mhop_obj, &mhop)) {
        dev_err(dev, "hns_roce_table_mhop_put calc_hem_mhop err\n");
        return;
    }

    hns_roce_bt_chunk_hopnum_init(mhop, &bt_chunk_size, &chunk_ba_num, &hop_num);

    if (hns_roce_get_bt_idx(table, mhop, &hem_idx, &bt_l1_idx, &bt_l0_idx)) {
        dev_err(dev, "Table %u fail to hop_num = %u get bt idx\n", table->type, hop_num);
        return;
    }

    mutex_lock(&table->mutex);
    if (check_refcount) {
        --table->hem[hem_idx]->refcount;
        if (table->hem[hem_idx]->refcount > 0) {
            mutex_unlock(&table->mutex);
            return;
        }
    }

    hns_roce_table_clean_hem(hr_dev, table, obj, hop_num, hem_idx);

    if (check_whether_bt_num_2(table->type, hop_num)) {
        start_idx = mhop.l0_idx * chunk_ba_num;
        if (hns_roce_check_hem_null(table->hem, start_idx, chunk_ba_num)) {
#ifndef DEFINE_HNS_LLT
            table->type < HEM_TYPE_MTT ? hr_dev->hw->clear_hem(hr_dev, table, obj, 0) : "" ;
#endif
            dma_free_coherent(dev, bt_chunk_size, table->bt_l0[mhop.l0_idx], table->bt_l0_dma_addr[mhop.l0_idx]);
            table->bt_l0[mhop.l0_idx] = NULL;
        }
    } else if (check_whether_bt_num_3(table->type, hop_num)) {
        start_idx = mhop.l0_idx * chunk_ba_num * chunk_ba_num + mhop.l1_idx * chunk_ba_num;
        if (hns_roce_check_hem_null(table->hem, start_idx, chunk_ba_num)) {
            hr_dev->hw->clear_hem(hr_dev, table, obj, 1);
            dma_free_coherent(dev, bt_chunk_size, table->bt_l1[bt_l1_idx], table->bt_l1_dma_addr[bt_l1_idx]);
            table->bt_l1[bt_l1_idx] = NULL;
            start_idx = mhop.l0_idx * chunk_ba_num;
            if (hns_roce_check_bt_null(table->bt_l1, start_idx, chunk_ba_num)) {
                hr_dev->hw->clear_hem(hr_dev, table, obj, 0);
                dma_free_coherent(dev, bt_chunk_size, table->bt_l0[mhop.l0_idx], table->bt_l0_dma_addr[mhop.l0_idx]);
                table->bt_l0[mhop.l0_idx] = NULL;
            }
        }
    }

    mutex_unlock(&table->mutex);
}

void hns_roce_table_put(struct hns_roce_dev *hr_dev,
                        struct hns_roce_hem_table *table, unsigned long obj)
{
    unsigned long i;

    if (hns_roce_check_whether_mhop(hr_dev, table->type)) {
        hns_roce_table_mhop_put(hr_dev, table, obj, 1);
        return;
    }

    i = (obj & (table->num_obj - 1)) /
        (table->table_chunk_size / table->obj_size);

    mutex_lock(&table->mutex);

    if (--table->hem[i]->refcount == 0) {
        /* Clear HEM base address */
        hr_dev->hw->clear_hem(hr_dev, table, obj, 0);

        hns_roce_free_hem(hr_dev, &table->hem[i]);
        table->hem[i] = NULL;
    }

    mutex_unlock(&table->mutex);
}

struct hns_roce_hem *hns_roce_table_find_hem(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    unsigned long obj, int *offset)
{
    struct hns_roce_hem *hem = NULL;
    unsigned long obj_per_chunk;
    struct hns_roce_hem_mhop mhop;
    unsigned long mhop_obj = obj;
    unsigned long idx_offset;
    u32 hem_idx = 0;

    if (!hns_roce_check_whether_mhop(hr_dev, table->type)) {
        obj_per_chunk = table->table_chunk_size / table->obj_size;
        hem = table->hem[(obj & (table->num_obj - 1)) / obj_per_chunk];
        idx_offset = (obj & (table->num_obj - 1)) % obj_per_chunk;
        *offset = idx_offset * table->obj_size;
    } else {
        if (hns_roce_calc_hem_mhop(hr_dev, table, &mhop_obj, &mhop)) {
            dev_err(hr_dev->dev, "hns_roce_calc_hem_mhop err.\n");
            return NULL;
        }

        if (mhop.hop_num == HNS_ROCE_TABLE_NUM_TWO) {
            hem_idx = mhop.l0_idx * (mhop.bt_chunk_size / BA_BYTE_LEN) + mhop.l1_idx;
        } else if (mhop.hop_num == 1 || mhop.hop_num == HNS_ROCE_HOP_NUM_0) {
            hem_idx = mhop.l0_idx;
        }

        hem = table->hem[hem_idx];
        *offset = (obj & (table->num_obj - 1)) * table->obj_size % mhop.bt_chunk_size;
        if (mhop.hop_num == HNS_ROCE_TABLE_NUM_TWO) {
            *offset = 0;
        }
    }

    return hem;
}

void *hns_roce_table_find(struct hns_roce_dev *hr_dev,
                          struct hns_roce_hem_table *table,
                          unsigned long obj, dma_addr_t *dma_handle)
{
    struct hns_roce_hem_chunk *chunk = NULL;
    struct hns_roce_hem *hem = NULL;
    void *addr = NULL;
    int offset, dma_offset;
    int length;
    int i;

    if (!table->lowmem) {
        dev_err(hr_dev->dev, "Invalid lowmem value of hns_roce_hem_table : %d\n", table->lowmem);
        return NULL;
    }

    mutex_lock(&table->mutex);

    hem = hns_roce_table_find_hem(hr_dev, table, obj, &offset);
    if (hem == NULL) {
        dev_err(hr_dev->dev, "hns_roce_hem hem is NULL\n");
        goto out;
    }

    dma_offset = offset;
    list_for_each_entry(chunk, &hem->chunk_list, list) {
        for (i = 0; i < chunk->npages; ++i) {
            length = sg_dma_len(&chunk->mem[i]);
            if (dma_handle != NULL && dma_offset >= 0) {
                if (length > dma_offset) {
                    *dma_handle = sg_dma_address(
                                      &chunk->mem[i]) + dma_offset;
                }
                dma_offset -= length;
            }

            if (length > offset) {
                addr = chunk->buf[i] + offset;
                goto out;
            }
            offset -= length;
        }
    }

out:
    mutex_unlock(&table->mutex);
    return addr;
}

int hns_roce_table_get_range(struct hns_roce_dev *hr_dev,
                             struct hns_roce_hem_table *table,
                             unsigned long start, unsigned long end)
{
    struct hns_roce_hem_mhop mhop;
    unsigned long inc = table->table_chunk_size / table->obj_size;
    unsigned long i;
    int ret;

    if (hns_roce_check_whether_mhop(hr_dev, table->type)) {
        ret = get_hem_table_config(hr_dev, &mhop, table->type);
        if (ret) {
            return ret;
        }
        inc = mhop.bt_chunk_size / table->obj_size;
    }

    /* Allocate MTT entry memory according to chunk(128K) */
    for (i = start; i <= end; i += inc) {
        ret = hns_roce_table_get(hr_dev, table, i);
        if (ret) {
            dev_err(hr_dev->dev, "hns_roce_table_get err.\n");
            goto fail;
        }
    }

    return 0;

fail:
    while (i > start) {
        i -= inc;
        hns_roce_table_put(hr_dev, table, i);
    }
    return ret;
}

void hns_roce_table_put_range(struct hns_roce_dev *hr_dev,
                              struct hns_roce_hem_table *table,
                              unsigned long start, unsigned long end)
{
    struct hns_roce_hem_mhop mhop;
    unsigned long inc = table->table_chunk_size / table->obj_size;
    unsigned long i;

    if (hns_roce_check_whether_mhop(hr_dev, table->type)) {
        if (get_hem_table_config(hr_dev, &mhop, table->type)) {
            return;
        }
        inc = mhop.bt_chunk_size / table->obj_size;
    }

    for (i = start; i <= end; i += inc) {
        hns_roce_table_put(hr_dev, table, i);
    }
}

static int hns_roce_init_hem_table_0hop(const struct hns_roce_dev *hr_dev,
    struct hns_roce_hem_table *table, const struct hns_roce_caps_info caps_info,
    unsigned long *num_hem)
{
    unsigned long obj_size = caps_info.obj_size;
    unsigned long nobj = caps_info.nobj;
    unsigned long obj_per_chunk;

    table->table_chunk_size = hr_dev->caps.chunk_sz;
    if (obj_size == 0) {
        dev_err(hr_dev->dev, "obj_size is zero\n");
        return -EINVAL;
    }

    obj_per_chunk = table->table_chunk_size / obj_size;
    *num_hem = (nobj + obj_per_chunk - 1) / obj_per_chunk;
    if (*num_hem == 0) {
        dev_err(hr_dev->dev, "num_hem is zero\n");
        return -EINVAL;
    }

    table->hem = kcalloc(*num_hem, sizeof(*table->hem), GFP_KERNEL);
    if (table->hem == NULL) {
        dev_err(hr_dev->dev, "kcalloc hns_roce_hem_table's hem failed\n");
        return -ENOMEM;
    }

    return 0;
}

static void hns_roce_free_hem_table_l1(struct hns_roce_hem_table *table)
{
    if (table->bt_l1_dma_addr != NULL) {
        kfree(table->bt_l1_dma_addr);
        table->bt_l1_dma_addr = NULL;
    }

    if (table->bt_l1 != NULL) {
        kfree(table->bt_l1);
        table->bt_l1 = NULL;
    }
}

static int hns_roce_init_hem_table_l1(const struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    unsigned long num_bt_l1)
{
    table->bt_l1 = kcalloc(num_bt_l1, sizeof(*table->bt_l1), GFP_KERNEL);
    if (table->bt_l1 == NULL) {
        dev_err(hr_dev->dev, "kcalloc table bt_l1 failed\n");
        return -ENOMEM;
    }

    table->bt_l1_dma_addr = kcalloc(num_bt_l1, sizeof(*table->bt_l1_dma_addr), GFP_KERNEL);
    if (table->bt_l1_dma_addr == NULL) {
        dev_err(hr_dev->dev, "kcalloc table bt_l1_dma_addr failed\n");
        kfree(table->bt_l1);
        table->bt_l1 = NULL;
        return -ENOMEM;
    }

    return 0;
}

static int hns_roce_init_hem_table_l0(const struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table,
    unsigned long num_bt_l0)
{
    table->bt_l0 = kcalloc(num_bt_l0, sizeof(*table->bt_l0), GFP_KERNEL);
    if (table->bt_l0 == NULL) {
        dev_err(hr_dev->dev, "kcalloc table bt_l0 failed\n");
        return -ENOMEM;
    }

    table->bt_l0_dma_addr = kcalloc(num_bt_l0, sizeof(*table->bt_l0_dma_addr), GFP_KERNEL);
    if (table->bt_l0_dma_addr == NULL) {
        dev_err(hr_dev->dev, "kcalloc table bt_l0_dma_addr failed\n");
        kfree(table->bt_l0);
        table->bt_l0 = NULL;
        return -ENOMEM;
    }

    return 0;
}

static int hns_roce_init_hem_table_mhop(const struct hns_roce_dev *hr_dev,
    struct hns_roce_hem_table *table, u32 type, const struct hns_roce_caps_info caps_info,
    unsigned long *num_hem)
{
    unsigned long obj_per_chunk = 0;
    struct hns_roce_hem_mhop mhop = {};
    unsigned long bt_chunk_num, num_bt_l0;
    unsigned long num_bt_l1 = 0;

    if (get_hem_table_config((struct hns_roce_dev *)hr_dev, &mhop, type)) {
        dev_err(hr_dev->dev, "get_hem_table_config failed\n");
        return -EINVAL;
    }

    if (caps_info.obj_size > 0) {
        obj_per_chunk = mhop.buf_chunk_size / caps_info.obj_size;
    }

    if (obj_per_chunk > 0) {
        *num_hem = (caps_info.nobj + obj_per_chunk - 1) / obj_per_chunk;
    }

    bt_chunk_num = mhop.bt_chunk_size / BA_BYTE_LEN;
    num_bt_l0 = (type >= HEM_TYPE_MTT) ? bt_chunk_num : mhop.ba_l0_num;

    table->hem = kcalloc(*num_hem, sizeof(*table->hem), GFP_KERNEL);
    if (table->hem == NULL) {
        dev_err(hr_dev->dev, "kcalloc table hem failed\n");
        return -ENOMEM;
    }

    if (check_whether_bt_num_3(type, mhop.hop_num)) {
        if (bt_chunk_num != 0) {
            num_bt_l1 = (*num_hem + bt_chunk_num - 1) / bt_chunk_num;
        }

        if (hns_roce_init_hem_table_l1(hr_dev, table, num_bt_l1)) {
            goto free_hem;
        }

        if (hns_roce_init_hem_table_l0(hr_dev, table, num_bt_l0)) {
            dev_err(hr_dev->dev, "kcalloc table bt_l0_dma_addr failed\n");
            goto free_table_l1;
        }
    } else if (check_whether_bt_num_2(type, mhop.hop_num)) {
        if (hns_roce_init_hem_table_l0(hr_dev, table, num_bt_l0)) {
            dev_err(hr_dev->dev, "kcalloc table bt_l0_dma_addr failed\n");
            goto free_hem;
        }
    }

    return 0;

free_table_l1:
    hns_roce_free_hem_table_l1(table);

free_hem:
    kfree(table->hem);
    table->hem = NULL;
    return -ENOMEM;
}

int hns_roce_init_hem_table(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table, u32 type,
    const struct hns_roce_caps_info caps_info, int use_lowmem)
{
    unsigned long num_hem = 0;
    int ret;

    if (!hns_roce_check_whether_mhop(hr_dev, type)) {
        ret = hns_roce_init_hem_table_0hop(hr_dev, table, caps_info, &num_hem);
        if (ret) {
            dev_err(hr_dev->dev, "init hem table 0hop failed, ret %d\n", ret);
            return ret;
        }
    } else {
        ret = hns_roce_init_hem_table_mhop(hr_dev, table, type, caps_info, &num_hem);
        if (ret) {
            dev_err(hr_dev->dev, "init hem table mhop failed, ret %d\n", ret);
            return ret;
        }
    }

    table->type = type;
    table->num_hem = num_hem;
    table->num_obj = caps_info.nobj;
    table->obj_size = caps_info.obj_size;
    table->lowmem = use_lowmem;
    mutex_init(&table->mutex);

    return 0;
}

static void hns_roce_cleanup_mhop_hem_table(struct hns_roce_dev *hr_dev,
                                            struct hns_roce_hem_table *table)
{
    struct hns_roce_hem_mhop mhop;
    u32 buf_chunk_size;
    int ret;
    u64 obj;
    u32 i;

    ret = get_hem_table_config(hr_dev, &mhop, table->type);
    if (!ret) {
        buf_chunk_size = table->type < HEM_TYPE_MTT ?
                         mhop.buf_chunk_size : mhop.bt_chunk_size;

        for (i = 0; i < table->num_hem; ++i) {
            obj = i * buf_chunk_size / table->obj_size;
            if (table->hem[i]) {
                hns_roce_table_mhop_put(hr_dev, table, obj, 0);
            }
        }
    }

    kfree(table->hem);
    table->hem = NULL;
    kfree(table->bt_l1);
    table->bt_l1 = NULL;
    kfree(table->bt_l1_dma_addr);
    table->bt_l1_dma_addr = NULL;
    kfree(table->bt_l0);
    table->bt_l0 = NULL;
    kfree(table->bt_l0_dma_addr);
    table->bt_l0_dma_addr = NULL;
}

void hns_roce_cleanup_hem_table(struct hns_roce_dev *hr_dev,
                                struct hns_roce_hem_table *table)
{
    unsigned long i;

    if (hns_roce_check_whether_mhop(hr_dev, table->type)) {
        hns_roce_cleanup_mhop_hem_table(hr_dev, table);
        return;
    }

    for (i = 0; i < table->num_hem; ++i) {
        if (table->hem[i]) {
            hr_dev->hw->clear_hem(hr_dev, table,
                                  i * table->table_chunk_size / table->obj_size, 0);

            hns_roce_free_hem(hr_dev, &table->hem[i]);
        }
    }
    kfree(table->hem);
    table->hem = NULL;
    mutex_destroy(&table->mutex);
}

void hns_roce_cleanup_hem(struct hns_roce_dev *hr_dev)
{
    if ((hr_dev->caps.num_idx_segs))
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->mr_table.mtt_idx_table);
    if (hr_dev->caps.num_srqwqe_segs)
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->mr_table.mtt_srqwqe_table);
    if (hr_dev->caps.srqc_entry_sz)
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->srq_table.table);
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->cq_table.table);
    if (hr_dev->caps.qpc_timer_entry_sz)
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->qpc_timer_table.table);
    if (hr_dev->caps.cqc_timer_entry_sz)
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->cqc_timer_table.table);
    if (hr_dev->caps.scc_ctx_entry_sz)
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->qp_table.scc_ctx_table);
    if (hr_dev->caps.trrl_entry_sz)
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->qp_table.trrl_table);
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qp_table.irrl_table);
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qp_table.qp_table);
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtpt_table);
    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE))
        hns_roce_cleanup_hem_table(hr_dev,
                                   &hr_dev->mr_table.mtt_cqe_table);
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtt_table);
}

