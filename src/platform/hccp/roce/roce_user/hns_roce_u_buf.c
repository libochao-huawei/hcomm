/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>

#include "hns_roce_u.h"
#include "hns_roce_u_hw_v2.h"
#include "securec.h"
#include "ascend_hal.h"
#include "ascend_hal_define.h"
#include "hns_roce_u_buf.h"

static unsigned int g_bitmap_list[BITMAP_WORD_NUM] = { 0 };
static struct hns_roce_mem_pool *g_mem_pool_list[BITMAP_WORD_SIZE] = { NULL };

static int bitmap_alloc(unsigned int *bitmap_list, unsigned int list_size, unsigned int *index)
{
    unsigned int i;
    unsigned int j;

    for (i = 0; i < list_size; i++) {
        if (bitmap_list[i] != 0xFFFFFFFF) {
            break;
        }
    }

    if (i == list_size) {
        roce_err("alloc bitmap failed, has no empty bit");
        return -EINVAL;
    }

    for (j = 0; j < BITMAP_WORD_LEN; j++) {
        if (((bitmap_list[i] >> j) & 0x1) == 0) {
            break;
        }
    }

    bitmap_list[i] = bitmap_list[i] | (0x1 << j);

    *index = i * BITMAP_WORD_LEN + j;
    return 0;
}

static inline void bitmap_free(unsigned int *bitmap_list, unsigned int list_size, unsigned int index)
{
    unsigned int word_index = index / BITMAP_WORD_LEN;
    unsigned int bit_index = index % BITMAP_WORD_LEN;

    if (word_index >= list_size) {
        roce_err("free bitmap failed, word_index[%u] >= list_size[%u]", word_index, list_size);
        return;
    }

    bitmap_list[word_index] = bitmap_list[word_index] & (~(1 << bit_index));
}

int hns_roce_hal_alloc_buf(struct hns_roce_buf *buf, unsigned int size, unsigned int page_size, unsigned int dev_id)
{
#define NODE_MASK_LIST_NUM 2
#define MAX_NODE_BIT_NUM 128
#define NODE_LONG_BIT_NUM 64
    unsigned long long node_mask[NODE_MASK_LIST_NUM] = {0};
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    unsigned long long node_index = 0;
    unsigned long long node_cnt = 0;
    unsigned long long max_node = 0;
    struct MemInfo info = {0};
    unsigned long long i;
    int ret = 0;

    if (size == 0 || page_size == 0) {
        roce_err("hns_roce_hal_alloc_buf size or page_size is zero! size [0x%x], page_size [0x%x]", size, page_size);
        return -EINVAL;
    }

    buf->length = (unsigned int)hns_roce_align(size, page_size);

    ret = (int)halMemGetInfo(dev_id, MEM_INFO_TYPE_BAR_NUMA_INFO, &info);
    if (ret) {
        roce_err("hns_roce_hal_alloc_buf: halMemGetInfo failed, dev_id [%d], ret [%d]", dev_id, ret);
        return ret;
    }

    node_cnt = (unsigned long long)info.numa_info.node_cnt;
    if (node_cnt > sizeof(info.numa_info.node_id) / sizeof(int)) {
        roce_err("hns_roce_hal_alloc_buf: halMemGetInfom node_cnt 0x%llx invalid", node_cnt);
        return -EINVAL;
    }

    for (i = 0; i < node_cnt; i++) {
        node_index = (unsigned long long)info.numa_info.node_id[i];
        if (node_index >= MAX_NODE_BIT_NUM) {
            roce_err("hns_roce_hal_alloc_buf: halMemGetInfom node_index 0x%llx >= %u, failed!",
                node_index, MAX_NODE_BIT_NUM);
            return -EINVAL;
        }
        max_node = (node_index > max_node) ? node_index : max_node;

        if (node_index >= NODE_LONG_BIT_NUM) {
            node_index -= NODE_LONG_BIT_NUM;
            node_mask[1] |= ((unsigned long long)1 << node_index);
        } else {
            node_mask[0] |= ((unsigned long long)1 << node_index);
        }
    }

    // plus 2 to avoid kernel get_nodes maxnode param high-order truncation bug
    max_node += 2;

    if (page_size == PAGE_ALIGN_2MB) {
        flags |= MAP_HUGETLB;
    }
    buf->buf = mmap(0, buf->length, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (buf->buf == MAP_FAILED) {
        roce_err("hns_roce_hal_alloc_buf mmap fail. length 0x%x", buf->length);
        return -ENOMEM;
    }

    ret = syscall(__NR_mbind, buf->buf, buf->length, MPOL_BIND, node_mask, max_node, MPOL_MF_MOVE);
    if (ret != 0) {
        roce_err("hns_roce_hal_alloc_buf mbind fail. length 0x%x, ret %d", buf->length, ret);
        goto err_unmamp;
    }

    ret = memset_s(buf->buf, buf->length, 0, buf->length);
    if (ret != 0) {
        roce_err("hns_roce_hal_alloc_buf memset_s fail. length 0x%x, ret %d", buf->length, ret);
        goto err_unmamp;
    }

    ret = ibv_dontfork_range(buf->buf, buf->length);
    if (ret != 0) {
        roce_err("hns_roce_hal_alloc_buf ibv_dontfork_range error, length 0x%x, ret %d", buf->length, ret);
        goto err_unmamp;
    }

    // set default buf offset 0
    buf->offset = 0;
    return 0;

err_unmamp:
    munmap(buf->buf, buf->length);
    buf->buf = NULL;

    return -ENOMEM;
}

int hns_roce_alloc_buf(struct hns_roce_buf *buf, unsigned int size,
                       int page_size)
{
    int ret;

    buf->length = hns_roce_align(size, page_size);
    buf->buf = mmap(NULL, buf->length, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // set default buf offset 0
    buf->offset = 0;
    if (buf->buf == MAP_FAILED) {
        roce_err("buf is MAP_FAILED");
        return errno;
    }

    ret = ibv_dontfork_range(buf->buf, buf->length);
    if (ret) {
        if (munmap(buf->buf, buf->length)) {
            roce_err("alloc buf munmap error, buf->length:%d", buf->length);
        }
        buf->buf = NULL;
    }

    return ret;
}

void hns_roce_free_buf(struct hns_roce_buf *buf)
{
    int ret;

    if (buf == NULL) {
        roce_err("hns roce buf is null");
        return;
    }

    if (buf->mem_align == LITE_ALIGN_2MB) {
        ret = hns_roce_hal_free_mem(buf);
        if (ret < 0) {
            roce_warn("free buf mem error, ret:%d", ret);
        }
        buf->buf = NULL;
        return;
    }

    ibv_dofork_range(buf->buf, buf->length);

    if (munmap(buf->buf, buf->length)) {
        roce_err("free buf munmap error, buf->length:%d", buf->length);
    }
    buf->buf = NULL;
}

static void hns_roce_hal_calc_mem_size(const struct roce_mem_cq_qp_attr *mem_attr, unsigned int *size)
{
#define SEND_RECV_DB_PAGE_NUM 2
    unsigned int cq_buf_db_size = 0;
    unsigned int qp_buf_db_size = 0;
    unsigned int sq_sge_shift = 0;
    unsigned int sq_wqe_shift = 0;
    unsigned int rq_wqe_shift = 0;
    unsigned int sq_wqe_size = 0;

    // calc qp buf, send, recv db size
    sq_wqe_size = sizeof(struct hns_roce_rc_send_wqe);
    for (sq_wqe_shift = WQE_SHIFT_START; (1U << sq_wqe_shift) < sq_wqe_size;) {
        sq_wqe_shift++;
    }
    if (mem_attr->send_sge_num > HNS_ROCE_SGE_IN_WQE) {
        sq_sge_shift = HNS_ROCE_SGE_SHIFT;
    }
    for (rq_wqe_shift = HNS_ROCE_SGE_SHIFT;
            (1U << rq_wqe_shift) < (HNS_ROCE_SGE_SIZE * mem_attr->recv_sge_num);) {
        rq_wqe_shift++;
    }
    qp_buf_db_size = hns_roce_align((unsigned int)(mem_attr->send_qp_depth << sq_wqe_shift), PAGE_ALIGN_4KB) +
                     hns_roce_align((unsigned int)(mem_attr->send_sge_num << sq_sge_shift), PAGE_ALIGN_4KB) +
                     hns_roce_align((unsigned int)(mem_attr->recv_qp_depth << rq_wqe_shift), PAGE_ALIGN_4KB) +
                     SEND_RECV_DB_PAGE_NUM * PAGE_ALIGN_4KB;

    // calc send, recv cq buf and db size
    cq_buf_db_size = hns_roce_align((unsigned int)(mem_attr->send_cq_depth * HNS_ROCE_CQE_ENTRY_SIZE), PAGE_ALIGN_4KB) +
                     hns_roce_align((unsigned int)(mem_attr->recv_cq_depth * HNS_ROCE_CQE_ENTRY_SIZE), PAGE_ALIGN_4KB) +
                     SEND_RECV_DB_PAGE_NUM * PAGE_ALIGN_4KB;

    // total size
    *size = hns_roce_align((unsigned int)(qp_buf_db_size + cq_buf_db_size), PAGE_ALIGN_2MB);
    return;
}

int roce_init_mem_pool(const struct roce_mem_cq_qp_attr *mem_attr, struct rdma_lite_device_mem_attr *mem_data,
    unsigned int dev_id)
{
    struct hns_roce_mem_pool *mem_pool = NULL;
    int ret = 0;

    if (mem_attr == NULL || mem_data == NULL) {
        roce_err("hns init pool param error, mem_attr is NULL or mem_data is NULL");
        return -EINVAL;
    }
    if (mem_attr->mem_align != LITE_ALIGN_2MB) {
        roce_err("hns init pool param error, mem_align[0x%x] is not 0x%x", mem_attr->mem_align, LITE_ALIGN_2MB);
        return -EINVAL;
    }

    hns_roce_hal_calc_mem_size(mem_attr, &mem_data->mem_size);
    if (mem_data->mem_size == 0) {
        roce_err("hns init pool failed, calc mem size is 0");
        return -EINVAL;
    }

    ret = bitmap_alloc(g_bitmap_list, BITMAP_WORD_NUM, &mem_data->mem_idx);
    if (ret != 0) {
        roce_err("hns init pool failed, bitmap_alloc failed ret = %d", ret);
        return ret;
    }

    mem_pool = (struct hns_roce_mem_pool *)calloc(1, sizeof(struct hns_roce_mem_pool));
    if (mem_pool == NULL) {
        roce_err("hns init pool failed, alloc mem_pool failed");
        ret = -ENOMEM;
        goto err_calloc;
    }
    mem_pool->roce_buf.mem_align = mem_attr->mem_align;
    mem_pool->roce_buf.mem_idx = mem_data->mem_idx;
    ret = hns_roce_hal_alloc_buf(&mem_pool->roce_buf, mem_data->mem_size, PAGE_ALIGN_2MB, dev_id);
    if (ret != 0) {
        roce_err("hns init pool failed, hal_alloc_buf failed ret = %d", ret);
        goto err_alloc_buf;
    }

    mem_pool->used_size = 0;
    g_mem_pool_list[mem_data->mem_idx] = mem_pool;
    mem_data->va = (unsigned long long)mem_pool->roce_buf.buf;

    return 0;

err_alloc_buf:
    free(mem_pool);
err_calloc:
    bitmap_free(g_bitmap_list, BITMAP_WORD_NUM, mem_data->mem_idx);

    return ret;
}

int hns_roce_hal_alloc_mem(struct hns_roce_buf *buf, unsigned int size, unsigned int page_size)
{
    struct hns_roce_mem_pool *mem_pool = NULL;
    unsigned int alloc_size = 0;

    if (buf->mem_align != LITE_ALIGN_2MB || buf->mem_idx >= BITMAP_WORD_SIZE) {
        roce_err("hns alloc mem param error, mem_align[0x%x] is not 0x2 or mem_idx[%u] >= %u",
            buf->mem_align, buf->mem_idx, BITMAP_WORD_SIZE);
        return -EINVAL;
    }

    alloc_size = (unsigned int)hns_roce_align(size, page_size);
    mem_pool = g_mem_pool_list[buf->mem_idx];
    if (mem_pool == NULL) {
        roce_err("hns alloc pool mem err, mem_idx:%u, mem_pool is NULL", buf->mem_idx);
        return -EINVAL;
    }
    if (mem_pool->roce_buf.mem_idx != buf->mem_idx) {
        roce_err("hns alloc pool mem err, mem_idx:%u inconsistent with pool mem_idx:0x%x",
            buf->mem_idx, mem_pool->roce_buf.mem_idx);
        return -EINVAL;
    }

    if (mem_pool->roce_buf.offset + alloc_size > mem_pool->roce_buf.length) {
        roce_err("hns alloc mem pool mem_idx:%u length:0x%x exhaust, pool offset:0x%x, alloc_size:0x%x",
            buf->mem_idx, mem_pool->roce_buf.length, mem_pool->roce_buf.offset, alloc_size);
        return -ENOMEM;
    }

    // prepare out buf
    buf->buf = mem_pool->roce_buf.buf;
    buf->length = alloc_size;
    buf->offset = mem_pool->roce_buf.offset;

    // update mem pool usage
    mem_pool->roce_buf.offset += alloc_size;
    mem_pool->used_size += alloc_size;

    return 0;
}

int hns_roce_hal_free_mem(struct hns_roce_buf *buf)
{
    struct hns_roce_mem_pool *mem_pool = NULL;

    if (buf->mem_align != LITE_ALIGN_2MB || buf->mem_idx >= BITMAP_WORD_SIZE) {
        roce_err("hns free pool param error, mem_align[0x%x] is not 0x2 or mem_idx[%u] >= %u",
            buf->mem_align, buf->mem_idx, BITMAP_WORD_SIZE);
        return -EINVAL;
    }

    mem_pool = g_mem_pool_list[buf->mem_idx];
    if (mem_pool == NULL) {
        roce_err("hns free pool mem err, mem_idx:%u, mem_pool is NULL", buf->mem_idx);
        return -EINVAL;
    }
    if (mem_pool->roce_buf.buf != buf->buf) {
        roce_err("hns free pool mem addr is err, mem_idx:%u", buf->mem_idx);
        return -EINVAL;
    }

    if (mem_pool->used_size < buf->length) {
        roce_err("hns free pool len is err, mem_pool->used_size:%u, buf->length:%u",
            mem_pool->used_size, buf->length);
        return -EINVAL;
    }

    mem_pool->used_size -= buf->length;
    return (int)(mem_pool->used_size);
}

int roce_deinit_mem_pool(unsigned int mem_idx)
{
    struct hns_roce_mem_pool *mem_pool = NULL;
    int ret = 0;

    if (mem_idx >= BITMAP_WORD_SIZE) {
        roce_err("hns deinit pool param error, mem_idx[%u] >= %u",
            mem_idx, BITMAP_WORD_SIZE);
        return -EINVAL;
    }

    mem_pool = g_mem_pool_list[mem_idx];
    if (mem_pool == NULL) {
        roce_err("hns deinit pool mem err, mem_idx:%u, mem_pool is NULL", mem_idx);
        return -EINVAL;
    }

    if (mem_pool->used_size > 0) {
        roce_err("hns deinit pool failed, still has used_size:%u", mem_pool->used_size);
        return -EINVAL;
    }

    // should free & munmap buf
    ibv_dofork_range(mem_pool->roce_buf.buf, mem_pool->roce_buf.length);

    if (munmap(mem_pool->roce_buf.buf, mem_pool->roce_buf.length)) {
        roce_err("free buf munmap error, mem_pool->roce_buf.length:%d", mem_pool->roce_buf.length);
        ret = -ENOMEM;
    }
    mem_pool->roce_buf.buf = NULL;

    free(mem_pool);
    mem_pool = NULL;
    g_mem_pool_list[mem_idx] = NULL;
    bitmap_free(g_bitmap_list, BITMAP_WORD_NUM, mem_idx);

    return ret;
}
