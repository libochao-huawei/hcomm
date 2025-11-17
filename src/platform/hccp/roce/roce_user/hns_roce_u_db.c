/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include "hns_roce_u.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_db.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

static struct hns_roce_db_page *hns_roce_add_db_page(struct hns_roce_context *ctx, enum hns_roce_db_type type,
    enum hns_roce_buf_type buf_type, uint32_t mem_idx)
{
    struct hns_roce_db_page *page = NULL;
    uint32_t bitmap_num, page_size, i;

    /* alloc db page */
    page_size = (uint32_t)to_hr_dev(ctx->ibv_ctx.context.device)->page_size;
    page = (struct hns_roce_db_page *)calloc(1, sizeof(struct hns_roce_db_page));
    if (page == NULL) {
        roce_err("calloc page failed!");
        goto err_page;
    }

    /* allocate bitmap space for sw db */
    page->num_db = page_size / g_db_size[type];
    page->use_cnt = 0;
    bitmap_num = (uint32_t)hns_roce_align(page->num_db,
                                 BIT_CNT_PER_BYTE * sizeof(uint64_t));
    HNS_ROCE_U_PARA_CHECK_GOTO_ERR(bitmap_num);

    page->bitmap = calloc(1, bitmap_num / BIT_CNT_PER_BYTE);
    if (page->bitmap == NULL) {
        roce_err("calloc bitmap failed!");
        goto err_map;
    }

    /* bitmap_num indicate the mount of u64 */
    bitmap_num = bitmap_num / (BIT_CNT_PER_BYTE * sizeof(uint64_t));

    page->buf_type = buf_type;
    if (buf_type == HNS_ROCE_BUF_TYPE_LITE_ALIGN_4KB) {
        if (hns_roce_hal_alloc_buf(&(page->buf), page_size, page_size, ctx->dev_id)) {
            roce_err("hns_roce_hal_alloc_buf failed, page_size [%u] dev_id [%u]", page_size, ctx->dev_id);
            goto err;
        }
    } else if (buf_type == HNS_ROCE_BUF_TYPE_LITE_ALIGN_2MB) {
        page->buf.mem_idx = mem_idx;
        page->buf.mem_align = LITE_ALIGN_2MB;
        if (hns_roce_hal_alloc_mem(&(page->buf), page_size, page_size)) {
            roce_err("hns_roce_hal_alloc_mem failed, page_size [%u] mem_idx [%u]", page_size, mem_idx);
            goto err;
        }
    } else {
        if (hns_roce_alloc_buf(&(page->buf), page_size, (int)page_size)) {
            roce_err("hns_roce_alloc_buf failed, page_size is %u", page_size);
            goto err;
        }
    }

    /* init the sw db bitmap */
    for (i = 0; i < bitmap_num; ++i) {
        page->bitmap[i] = ~(0UL);
    }

    /* add the set ctx->db_list */
    page->prev = NULL;
    page->next = ctx->db_list[type];
    ctx->db_list[type] = page;
    if (page->next != NULL) {
        page->next->prev = page;
    }

    return page;
err:
    free(page->bitmap);
    page->bitmap = NULL;

err_map:
    free(page);
    page = NULL;

err_page:
    return NULL;
}

static void hns_roce_clear_db_page(struct hns_roce_db_page *page)
{
    if (page == NULL) {
        roce_err("error clear: db page is NULL!");
        return;
    }

    if (page->bitmap != NULL) {
        free(page->bitmap);
        page->bitmap = NULL;
    }

    hns_roce_free_buf(&(page->buf));
}

void *hns_roce_alloc_db(struct hns_roce_context *ctx, enum hns_roce_db_type type,
                        enum hns_roce_buf_type buf_type, uint32_t mem_idx)
{
    struct hns_roce_db_page *page = NULL;
    uint32_t bit_num, i;
    void *db = NULL;

    (void)pthread_mutex_lock((pthread_mutex_t *)&ctx->db_list_mutex);

    for (page = ctx->db_list[type]; page != NULL && buf_type != HNS_ROCE_BUF_TYPE_LITE_ALIGN_2MB; page = page->next) {
        if ((page->buf_type == buf_type) && page->use_cnt < page->num_db) {
            goto found;
        }
    }
    page = hns_roce_add_db_page(ctx, type, buf_type, mem_idx);
    if (page == NULL) {
        roce_err("hns_roce_add_db_page failed!");
        goto out;
    }

found:
    ++page->use_cnt;
    /* if the bitmap is allocated, the bitmap[i] is set zero */
    for (i = 0; page->bitmap[i] == 0; ++i) {
        ;
    }

    bit_num = (uint32_t)ffsl(page->bitmap[i]);
    page->bitmap[i] &= ~(1ULL << (bit_num - 1));

    db = (void *)((uint8_t *)((char *)page->buf.buf + page->buf.offset) +
                  (size_t)((i * sizeof(uint64_t) * BIT_CNT_PER_BYTE +
                            (bit_num - 1)) * g_db_size[type]));

out:
    (void)pthread_mutex_unlock((pthread_mutex_t *)&ctx->db_list_mutex);

    return db;
}

void hns_roce_free_db(struct hns_roce_context *ctx, const unsigned int *db,
                      enum hns_roce_db_type type)
{
    struct hns_roce_db_page *page = NULL;
    uint32_t bit_num, page_size;

    (void)pthread_mutex_lock((pthread_mutex_t *)&ctx->db_list_mutex);

    page_size = (uint32_t)to_hr_dev(ctx->ibv_ctx.context.device)->page_size;
    for (page = ctx->db_list[type]; page != NULL; page = page->next) {
        if (((uintptr_t)db & (~((uintptr_t)page_size - 1))) ==
                (uintptr_t)((char *)page->buf.buf + page->buf.offset)) {
            goto found;
        }
    }
    roce_err("db page can't be found!");
    goto out;

found:
    --page->use_cnt;
    if (!page->use_cnt) {
        if (page->prev != NULL) {
            page->prev->next = page->next;
        } else {
            ctx->db_list[type] = page->next;
        }

        if (page->next != NULL) {
            page->next->prev = page->prev;
        }

        hns_roce_clear_db_page(page);
        free(page);
        page = NULL;
        goto out;
    }

    bit_num = (uint32_t)(((uintptr_t)db - (uintptr_t)page->buf.buf) /
                         g_db_size[type]);
    page->bitmap[bit_num / (sizeof(uint64_t) * BIT_CNT_PER_BYTE)] |=
        ((uint64_t)1ULL) <<
        (bit_num % (sizeof(uint64_t) * BIT_CNT_PER_BYTE));

out:
    (void)pthread_mutex_unlock((pthread_mutex_t *)&ctx->db_list_mutex);
}
