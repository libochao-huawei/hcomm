/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HEM_H
#define _HNS_ROCE_HEM_H

#include "hns_roce_device.h"

#define HW_SYNC_TIMEOUT_MSECS       500
#define HW_SYNC_TIMEOUT_COUNT       25
#define HW_SYNC_SLEEP_TIME_INTERVAL 20
#define BT_CMD_SYNC_SHIFT       31
#define BT_NUM_1    1
#define BT_NUM_2    2
#define BT_NUM_3    3
#define HOP_NUM_2    2
#define HOP_NUM_1    1
#define HOP_NUM_2_STEP_INDX    2
#define HOP_NUM_1_STEP_INDX    1

#define BT_CMD_VAL_LEN    2

#define HNS_ROCE_TABLE_NUM_TWO      2
enum {
    /* MAP HEM(Hardware Entry Memory) */
    HEM_TYPE_QPC = 0,
    HEM_TYPE_MTPT,
    HEM_TYPE_CQC,
    HEM_TYPE_SRQC,
    HEM_TYPE_SCC_CTX,
    HEM_TYPE_QPC_TIMER,
    HEM_TYPE_CQC_TIMER,

    /* UNMAP HEM */
    HEM_TYPE_MTT,
    HEM_TYPE_CQE,
    HEM_TYPE_SRQWQE,
    HEM_TYPE_IDX,
    HEM_TYPE_IRRL,
    HEM_TYPE_TRRL,
};

#define HNS_ROCE_HEM_CHUNK_LEN  \
    ((256 - sizeof(struct list_head) - 2 * sizeof(int)) /   \
    (sizeof(struct scatterlist)))

#define check_whether_bt_num_3(type, hop_num) \
    ((type) < HEM_TYPE_MTT && (hop_num) == 2)

#define check_whether_bt_num_2(type, hop_num) \
    (((type) < HEM_TYPE_MTT && (hop_num) == 1) || \
    ((type) >= HEM_TYPE_MTT && (hop_num) == 2))

#define check_whether_bt_num_1(type, hop_num) \
    (((type) < HEM_TYPE_MTT && (hop_num) == HNS_ROCE_HOP_NUM_0) || \
    ((type) >= HEM_TYPE_MTT && (hop_num) == 1) || \
    ((type) >= HEM_TYPE_MTT && (hop_num) == HNS_ROCE_HOP_NUM_0))

#define HNS_ROCE_BT_ASSIGN(bt_info, bt_b, bt_l0, bt_l1) do { \
    (bt_info).bt_ba = (bt_b); \
    (bt_info).bt_l0_idx = (bt_l0); \
    (bt_info).bt_l1_idx = (bt_l1); \
} while (0)

#define HNS_ROCE_CAPS_INFO_ASSIGN(caps_info, obj_size_, nobj_) do { \
    (caps_info).obj_size = (obj_size_); \
    (caps_info).nobj = (nobj_); \
} while (0)

enum {
    HNS_ROCE_HEM_PAGE_SHIFT = 12,
    HNS_ROCE_HEM_PAGE_SIZE  = 1 << HNS_ROCE_HEM_PAGE_SHIFT,
};

struct hns_roce_alloc_param {
    u64 hem_idx;
    u64 bt_l0_idx;
    u64 bt_l1_idx;
    unsigned long obj;
    u32 bt_chunk_size;
    u32 buf_chunk_size;
    int bt_l0_allocated;
    int bt_l1_allocated;
};

struct hns_roce_caps_info {
    unsigned long obj_size;
    unsigned long nobj;
};

struct hns_roce_bt_info {
    u64 bt_ba;
    u64 bt_l0_idx;
    u64 bt_l1_idx;
};

struct hns_roce_hem_chunk {
    struct list_head     list;
    int          npages;
    int          nsg;
    struct scatterlist   mem[HNS_ROCE_HEM_CHUNK_LEN];
    void             *buf[HNS_ROCE_HEM_CHUNK_LEN];
};

struct hns_roce_hem {
    struct list_head     chunk_list;
    int          refcount;
};

struct hns_roce_hem_iter {
    struct hns_roce_hem      *hem;
    struct hns_roce_hem_chunk    *chunk;
    int              page_idx;
};

struct hns_roce_hem_mhop {
    u32 hop_num;
    u32 buf_chunk_size;
    u32 bt_chunk_size;
    u32 ba_l0_num;
    u32 l0_idx; /* level 0 base address table index */
    u32 l1_idx; /* level 1 base address table index */
    u32 l2_idx; /* level 2 base address table index */
};

struct hns_roce_set_hem_para {
    int obj;
    int step_idx;
    u32 l0_idx;
    u64 l1_idx;
    u64 hem_idx;
    u32 hop_num;
    int op;
};

struct hns_roce_wqe_sge {
    unsigned int sge_ind;
    char *wqe;
};

struct hns_roce_post_send_para {
    unsigned int ind;
    unsigned long flags;
    int nreq;
    struct hns_roce_wqe_sge wqe_sge;
};

int hns_roce_table_get(struct hns_roce_dev *hr_dev,
                       struct hns_roce_hem_table *table, unsigned long obj);
void hns_roce_table_put(struct hns_roce_dev *hr_dev,
                        struct hns_roce_hem_table *table, unsigned long obj);
void *hns_roce_table_find(struct hns_roce_dev *hr_dev,
                          struct hns_roce_hem_table *table, unsigned long obj,
                          dma_addr_t *dma_handle);
int hns_roce_table_get_range(struct hns_roce_dev *hr_dev,
                             struct hns_roce_hem_table *table,
                             unsigned long start, unsigned long end);
void hns_roce_table_put_range(struct hns_roce_dev *hr_dev,
                              struct hns_roce_hem_table *table,
                              unsigned long start, unsigned long end);
int hns_roce_init_hem_table(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table *table, u32 type,
    struct hns_roce_caps_info caps_info, int use_lowmem);
void hns_roce_cleanup_hem_table(struct hns_roce_dev *hr_dev,
                                struct hns_roce_hem_table *table);
void hns_roce_cleanup_hem(struct hns_roce_dev *hr_dev);
int hns_roce_calc_hem_mhop(struct hns_roce_dev *hr_dev,
                           struct hns_roce_hem_table *table, unsigned long *obj,
                           struct hns_roce_hem_mhop *mhop);
bool hns_roce_check_whether_mhop(struct hns_roce_dev *hr_dev, u32 type);

static inline void hns_roce_hem_first(struct hns_roce_hem *hem,
                                      struct hns_roce_hem_iter *iter)
{
    iter->hem = hem;
    iter->chunk = list_empty(&hem->chunk_list) ? NULL :
                  list_entry(hem->chunk_list.next,
                             struct hns_roce_hem_chunk, list);
    iter->page_idx = 0;
}

static inline int hns_roce_hem_last(struct hns_roce_hem_iter *iter)
{
    return !iter->chunk;
}

static inline void hns_roce_hem_next(struct hns_roce_hem_iter *iter)
{
    if (++iter->page_idx >= iter->chunk->nsg) {
        if (iter->chunk->list.next == &iter->hem->chunk_list) {
            iter->chunk = NULL;
            return;
        }

        iter->chunk = list_entry(iter->chunk->list.next, struct hns_roce_hem_chunk, list);
        iter->page_idx = 0;
    }
}

static inline dma_addr_t hns_roce_hem_addr(struct hns_roce_hem_iter *iter)
{
    return sg_dma_address(&iter->chunk->mem[iter->page_idx]);
}

#endif /* _HNS_ROCE_HEM_H */
