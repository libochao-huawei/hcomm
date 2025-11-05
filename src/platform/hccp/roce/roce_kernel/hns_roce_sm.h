/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */


#ifndef _HNS_ROCE_SM_H
#define _HNS_ROCE_SM_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/scatterlist.h>

#include "hns_roce_device.h"
#include "hns_roce_cdev.h"

#ifdef HNS_ROCE_DEVICE
#ifdef HNS_ROCE_V3
#define HNS_ROCE_SHARED_MEMORY_ADDR       (0x2FE00000)
#else
#define HNS_ROCE_SHARED_MEMORY_ADDR       (0x6D400000)
#endif

#define HNS_ROCE_SHARED_BUF_SIZE          (0x4000000)
#define HNS_ROCE_SHARED_MAX_SQ_NUM        (4806)
#define HNS_ROCE_SHARED_SQ_DEPTH          (128)
#define HNS_ROCE_SHARED_WQE_LENGTH        (64)
#define HNS_ROCE_SHARED_TEMP_DEPTH        (12)
#define HNS_ROCE_SHARED_TEMP_LENGTH       (2 * 64)
#define HNS_ROCE_SHARED_DB_DEPTH          (1)
#define HNS_ROCE_SHARED_DB_LENGTH         (8)
#define HNS_ROCE_SHARED_DFX_DEPTH          (1)
#define HNS_ROCE_SHARED_DFX_LENGTH         (128)
#define HNS_ROCE_MAGIC_WORDS               (0xa5a5a5a55a5a5a5a)
#define HNS_ROCE_HIGH_SHIFT                (0X2000)
#define HNS_ROCE_SHARED_MEMORY_BITWIDTH    (32)
#define HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN (8)  // MAGIC WORDS LEN
#define HNS_RCOE_SHARED_SQ_MAGIC_WORDS_NUM (2)  // MAGIC WORDS NUM
#define HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN (4*1024)  // RSVD FOR MAGIC WORDS
#define HNS_ROCE_SQ_DEPTH_PROPORT_TEMP_DEPTH 8
#define ROCE_MIN_TEMPTH_DEPTH 8
#define ROCE_MAX_TEMPTH_DEPTH 4096
#define ROCE_MAX_BITMAP_SIZE 8192
#define HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN_EXINCLUDE1MAGICWORDS \
    (HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN - HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN)
#define HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN_EXINCLUDE2MAGICWORDS \
    (HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN_EXINCLUDE1MAGICWORDS - HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN)
#else
#define HNS_ROCE_SHARED_BUF_SIZE          (0x200000)
#define HNS_ROCE_SHARED_SQ_DEPTH          (1024)
#define HNS_ROCE_SHARED_WQE_LENGTH        (64)
#define HNS_ROCE_SHARED_TEMP_DEPTH        (64)
#define HNS_ROCE_SHARED_TEMP_LENGTH       (2 * 64)
#define HNS_ROCE_SHARED_DB_DEPTH          (1)
#define HNS_ROCE_SHARED_DB_LENGTH         (8)
#define HNS_ROCE_SHARED_MAX_SQ_NUM        (16)
#define HNS_ROCE_SHARED_DFX_DEPTH          (1)
#define HNS_ROCE_SHARED_DFX_LENGTH         (128)
#define HNS_ROCE_HIGH_SHIFT                (0X2000)
#define HNS_ROCE_MAGIC_WORDS               (0xa5a5a5a55a5a5a5a)
#define HNS_ROCE_SHARED_MEMORY_ADDR        (0x6D480000)
#define HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN (8)  // MAGIC WORDS LEN
#define HNS_RCOE_SHARED_SQ_MAGIC_WORDS_NUM (2)  // MAGIC WORDS NUM
#define HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN (4*1024) // RSVD FOR MAGIC WORDS
#define HNS_ROCE_SHARED_MEMORY_BITWIDTH    (32)
#define HNS_ROCE_SQ_DEPTH_PROPORT_TEMP_DEPTH 8
#define ROCE_MIN_TEMPTH_DEPTH 8
#define ROCE_MAX_TEMPTH_DEPTH 4096
#define ROCE_MAX_BITMAP_SIZE 8192
#define HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN_EXINCLUDE1MAGICWORDS \
    (HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN - HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN)
#define HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN_EXINCLUDE2MAGICWORDS \
    (HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN_EXINCLUDE1MAGICWORDS - HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN)
#endif

#define HNS_ROCE_SHARED_SQ_BUF_ENTRY_LENGTH       (HNS_ROCE_SHARED_SQ_DEPTH * HNS_ROCE_SHARED_WQE_LENGTH)
#define HNS_ROCE_SHARED_TEMP_BUF_ENTRY_LENGTH     (HNS_ROCE_SHARED_TEMP_DEPTH * HNS_ROCE_SHARED_TEMP_LENGTH)
#define HNS_ROCE_SHARED_DB_BUF_ENTRY_LENGTH       (HNS_ROCE_SHARED_DB_DEPTH * HNS_ROCE_SHARED_DB_LENGTH)
#define HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH (HNS_ROCE_SHARED_DFX_DEPTH * HNS_ROCE_SHARED_DFX_LENGTH)
#define HNS_ROCE_SHARED_TEMP_BUF_LENGTH           (HNS_ROCE_SHARED_MAX_SQ_NUM * HNS_ROCE_SHARED_TEMP_BUF_ENTRY_LENGTH)
#define HNS_ROCE_SHARED_DB_BUF_LENGTH             (HNS_ROCE_SHARED_MAX_SQ_NUM * HNS_ROCE_SHARED_DB_BUF_ENTRY_LENGTH)
#define HNS_ROCE_SHARED_DFX_BUF_LENGTH    (HNS_ROCE_SHARED_MAX_SQ_NUM * HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH)
#define INDEX_SHIFT_OFFSET     16
#define HNS_ROCE_MAX_PORT_NUM  2
#define HNS_ROCE_MAGIC_OFF1    1
#define HNS_ROCE_MAGIC_OFF2    2
#define HNS_ROCE_MAGIC_OFF3    3
#define HNS_ROCE_MAGIC_OFF4    4
#define HNS_ROCE_MAGIC_OFF5    5
#define HNS_ROCE_MAGIC_OFF6    6
#define HNS_ROCE_MAGIC_OFF7    7

enum {
    HNS_ROCE_NOR_QP_MODE    = 0,
    HNS_ROCE_GDR_QP_MODE    = 1,
    HNS_ROCE_SOP_QP_MODE    = 2,
    HNS_ROCE_ERR_QP_MODE    = 3,
};

enum hns_roce_mailbox_ts {
    MAILBOX_SEND_BUF = 0x1,
    MAILBOX_RESET_SQ,
};

struct hns_roce_share_send_buf {
    u32 opcode;
    u32 length;
    u64 addr;
};

struct hns_roce_share_reset_sq {
    u32 opcode;
    u32 index;
    u64 sq_addr;
    u64 temp_addr;
    u64 db_addr;
    u64 dfx_addr;
    u32 sq_depth;
    u32 temp_depth;
};

struct hns_roce_share_mem_piece {
    void                *map;
    unsigned int    length;
};

struct hns_roce_sm_priv {
    u32    share_mem_size;
    struct hns_roce_buf_list share_buf;
    struct hns_roce_share_mem_piece sq_buf;
    struct hns_roce_share_mem_piece temp_buf;
    struct hns_roce_share_mem_piece db_buf;
    struct hns_roce_share_mem_piece dfx_buf;
    struct hns_roce_bitmap    sq_bitmap;
};

int hns_roce_get_devid(struct hns_roce_dev *hr_dev);
int hns_roce_init_shared_buffer(struct hns_roce_dev *hr_dev);
void hns_roce_free_shared_buffer(struct hns_roce_dev *hr_dev);
int hns_roce_alloc_shared_sq(struct hns_roce_dev *hr_dev, struct hns_roce_share_sq *share_sq);
void hns_roce_free_shared_sq(struct hns_roce_dev *hr_dev, int index);
int hns_roce_mailbox_shared_sq(struct hns_roce_dev *hr_dev,
    struct hns_roce_share_sq share_sq, u32 gdr_enable);
dma_addr_t hns_roce_get_shared_mem_base(struct hns_roce_dev *hr_dev);
int hns_roce_get_shared_mem_size(struct hns_roce_dev *hr_dev);

int hns_roce_set_tsqp_depth(struct hns_roce_dev *hr_dev, struct roce_set_tsqp_depth_data *tsqp_depth_data);
int hns_roce_get_tsqp_depth(struct hns_roce_dev *hr_dev, struct roce_get_tsqp_depth_data *tsqp_depth_data);
#endif /* _HNS_ROCE_SM_H */
