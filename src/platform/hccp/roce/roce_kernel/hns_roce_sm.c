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
#include "hns_roce_sm.h"
#include "hns_roce_sec.h"
#include "hnae3.h"
#include "hns_roce_hw_v2_wqe.h"
#include "hns_roce_common.h"
#if defined(HNS_ROCE_DEVICE) && !defined(CONFIG_PLATFORM_ESL)
#include "devdrv_mailbox.h"
#else
STATIC int devdrv_send_rdmainfo_to_ts(u32 dev_id, const char *buf, u32 len, u32 *result)
{
    return 0;
}
#endif

int hns_roce_get_devid(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    int ret;
    u32 devid;

    ret = handle->ae_algo->ops->get_device_info(handle, NULL, NULL, &devid);
    if (ret) {
        dev_err(hr_dev->dev, "roce get device info err, ret:%d\n.", ret);
        return -1;
    }

    return devid;
}

STATIC int hns_roce_mailbox_to_ts(struct hns_roce_dev *hr_dev, const char *buf, u32 len)
{
    int ret;
    int result = 0;
    int dev_id;
    int pfn = hr_dev->pci_dev->devfn & 0x7;

    if (pfn != 0) {
        dev_notice(hr_dev->dev, "hns_roce_mailbox_to_ts pfn:%u not support!\n", pfn);
        return 0;
    }

    dev_id = hns_roce_get_devid(hr_dev);
    if (dev_id < 0) {
        dev_err(hr_dev->dev, "Invalid device id: %d", dev_id);
        return -EINVAL;
    }

    ret = devdrv_send_rdmainfo_to_ts(dev_id, buf, len, &result);
    if (ret) {
        dev_err(hr_dev->dev, "devdrv_send_rdmainfo_to_ts fail! dev_id:%d ret:0x%x result:%d\n", dev_id, ret, result);
        return ret;
    }

    return 0;
}

STATIC void hns_roce_magic_words_err_print(struct device *dev, unsigned char *tmp, int len)
{
    dev_err(dev, "magic words front part 0x%x 0x%x 0x%x 0x%x\n",
            *(tmp),  *(tmp + HNS_ROCE_MAGIC_OFF1),
            *(tmp + HNS_ROCE_MAGIC_OFF2), *(tmp + HNS_ROCE_MAGIC_OFF3));
    dev_err(dev, "magic words back part 0x%x 0x%x 0x%x 0x%x\n",
            *(tmp + HNS_ROCE_MAGIC_OFF4), *(tmp + HNS_ROCE_MAGIC_OFF5),
            *(tmp + HNS_ROCE_MAGIC_OFF6), *(tmp + HNS_ROCE_MAGIC_OFF7));
}

STATIC int hns_roce_magic_words_fill(struct device *dev, char *addr_magic)
{
    int ret;
    u64 *num = (u64 *)addr_magic;
    ret = memset_s(addr_magic, HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN,
                   0, HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    if (ret) {
        dev_err(dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }
    *num = HNS_ROCE_MAGIC_WORDS;
    return 0;
}

STATIC int hns_roce_magic_words_init(struct hns_roce_dev *hr_dev, struct hns_roce_buf_list *share_buf,
    unsigned long index)
{
    int ret;
    char *addr_magic = NULL;
    struct device *dev = hr_dev->dev;

    addr_magic = share_buf->buf + index * (hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH) +
                 (index + 1) * HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN -
                 HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN;
    ret = hns_roce_magic_words_fill(dev, addr_magic);
    if (ret) {
        dev_err(dev, "sq %lu head magic_words_fill failed ret %d, normal ret 0\n", index, ret);
        return ret;
    }
    addr_magic += ((hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH) + HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    ret = hns_roce_magic_words_fill(dev, addr_magic);
    if (ret) {
        dev_err(dev, "sq %ld tail magic_words_fill failed ret %d, normal ret 0\n", index, ret);
        return ret;
    }

    return 0;
}

STATIC void hns_roce_init_shared_buffer_set_priv(struct hns_roce_sm_priv *priv,
    struct hns_roce_buf_list *share_buf, struct hns_roce_dev *hr_dev)
{
    int addr_shift = 0;
    long shared_sq_buf_len;
    long shared_temp_buf_len;
    long shared_db_buf_len;

    if (hr_dev->port_num > 1 && hr_dev->port_id == 1) {
        addr_shift = HNS_ROCE_SHARED_BUF_SIZE / hr_dev->port_num;
    }
    shared_sq_buf_len = (hr_dev->max_sq_num + 1) * HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN +
        hr_dev->max_sq_num * hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH;

    priv->sq_buf.map = (void *)(uintptr_t)(share_buf->map + addr_shift);
    priv->sq_buf.length = shared_sq_buf_len;

    shared_temp_buf_len = hr_dev->max_sq_num * hr_dev->temp_depth * HNS_ROCE_SHARED_TEMP_LENGTH;
    priv->temp_buf.map = (void *)(uintptr_t)(share_buf->map + shared_sq_buf_len + addr_shift);
    priv->temp_buf.length = shared_temp_buf_len;

    shared_db_buf_len = hr_dev->max_sq_num * HNS_ROCE_SHARED_DB_BUF_ENTRY_LENGTH;
    priv->db_buf.map = (void *)(uintptr_t)(share_buf->map + shared_sq_buf_len + shared_temp_buf_len + addr_shift);
    priv->db_buf.length = shared_db_buf_len;

    priv->dfx_buf.map = (void *)(uintptr_t)(share_buf->map + shared_sq_buf_len + shared_temp_buf_len +
        shared_db_buf_len + addr_shift);
    priv->dfx_buf.length = hr_dev->max_sq_num * HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH;
}

STATIC void hns_roce_init_shared_buffer_common(struct hns_roce_sm_priv *priv, struct hns_roce_buf_list *share_buf,
    struct hns_roce_dev *hr_dev)
{
    int ret;
    unsigned int i;

    for (i = 0; i < hr_dev->max_sq_num; i++) {
        ret = hns_roce_magic_words_init(hr_dev, share_buf, i);
        if (ret) {
            dev_err(hr_dev->dev, "magic words fill failed, ret %d, expected ret 0.\n", ret);
        }
    }

    hns_roce_init_shared_buffer_set_priv(priv, share_buf, hr_dev);

    return;
}

STATIC int hns_roce_init_shared_buffer_info(struct hns_roce_sm_priv *priv, struct hns_roce_buf_list *share_buf,
    struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_share_send_buf send_buf_cmd;

#ifdef HNS_ROCE_DEVICE
    ret = memset_s(share_buf->buf, priv->share_mem_size, 0, priv->share_mem_size);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }
#endif
    /*
    * show buffer info
    */
    dev_notice(hr_dev->dev, "share_buf va:%pK pa:0x%llx size:%u\n", share_buf->buf,
        (u64)share_buf->map, priv->share_mem_size);

    /*
    * send buffer info to ts
    */
    send_buf_cmd.opcode = MAILBOX_SEND_BUF;
    send_buf_cmd.length = HNS_ROCE_SHARED_BUF_SIZE;
    send_buf_cmd.addr = (u64)priv->share_buf.map;

    if (hr_dev->port_id == 0) {
        ret = hns_roce_mailbox_to_ts(hr_dev, (char *)&send_buf_cmd, sizeof(send_buf_cmd));
        if (ret) {
            dev_err(hr_dev->dev, "hns_roce_mailbox_to_ts fail! share_buf opcode:%u addr:%llu size:%u ret:%d\n", \
                    send_buf_cmd.opcode, send_buf_cmd.addr, HNS_ROCE_SHARED_BUF_SIZE, ret);
            return ret;
        }
    }

    /*
     * init share buffer
     */
    hr_dev->max_sq_num = HNS_ROCE_SHARED_MAX_SQ_NUM / hr_dev->port_num;
    hr_dev->temp_depth = HNS_ROCE_SHARED_TEMP_DEPTH;
    hr_dev->sq_depth = HNS_ROCE_SHARED_SQ_DEPTH;
    hr_dev->qp_cnt = 0;
    hns_roce_init_shared_buffer_common(priv, share_buf, hr_dev);

    ret = hns_roce_bitmap_init(&priv->sq_bitmap, ROCE_MAX_BITMAP_SIZE, ROCE_MAX_BITMAP_SIZE - 1, 0, 0);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_bitmap_init fail! ret:%d\n", ret);
        return ret;
    }

    return 0;
}

STATIC int hns_roce_get_max_sq_num(struct hns_roce_dev *hr_dev, struct roce_set_tsqp_depth_data *tsqp_depth_data)
{
    unsigned int share_buf_len = HNS_ROCE_SHARED_BUF_SIZE;
    unsigned int sq_entry_buf_len, temp_entry_buf_len;
    unsigned int toal_entry_len;
    unsigned int sq_depth;
    unsigned int qp_num;
    unsigned int i = 0;

    if (tsqp_depth_data->rdev_index >= hr_dev->port_num) {
        dev_err(hr_dev->dev, "[hns_roce] rdev_index[%u] >= port_num[%u], invalid.\n",
            tsqp_depth_data->rdev_index, hr_dev->port_num);
        return -EINVAL;
    }

    if (tsqp_depth_data->temp_depth < ROCE_MIN_TEMPTH_DEPTH || tsqp_depth_data->temp_depth > ROCE_MAX_TEMPTH_DEPTH) {
        dev_err(hr_dev->dev, "[hns_roce]param error! temp_depth[%u] can not smaller than [%d] or bigerr than [%d].\n",
            tsqp_depth_data->temp_depth, ROCE_MIN_TEMPTH_DEPTH, ROCE_MAX_TEMPTH_DEPTH);
        return -EINVAL;
    }

    if (hr_dev->port_num > 1) {
        share_buf_len = HNS_ROCE_SHARED_BUF_SIZE / hr_dev->port_num;
    }

    /* the sq_depth must be power of two to ensure 4k alignment */
    while ((1 << i) < (HNS_ROCE_SQ_DEPTH_PROPORT_TEMP_DEPTH * tsqp_depth_data->temp_depth)) {
        i++;
    }
    sq_depth = 1 << i;

    hr_dev->sq_depth = sq_depth;
    sq_entry_buf_len = sq_depth * HNS_ROCE_SHARED_WQE_LENGTH;
    temp_entry_buf_len = tsqp_depth_data->temp_depth * HNS_ROCE_SHARED_TEMP_LENGTH;

    toal_entry_len = sq_entry_buf_len + HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN + temp_entry_buf_len +
        HNS_ROCE_SHARED_DB_BUF_ENTRY_LENGTH + HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH;
    qp_num = (share_buf_len - HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN) / toal_entry_len;
    if (qp_num == 0) {
        dev_err(hr_dev->dev, "[hns_roce] qp_num is 0, the temp_depth[%u] is too big.\n", tsqp_depth_data->temp_depth);
        return -EINVAL;
    }

    tsqp_depth_data->qp_num = qp_num;
    dev_info(hr_dev->dev, "hr_dev->sq_depth:%u, qp_num:%u", hr_dev->sq_depth, tsqp_depth_data->qp_num);
    return 0;
}

int hns_roce_set_tsqp_depth(struct hns_roce_dev *hr_dev, struct roce_set_tsqp_depth_data *tsqp_depth_data)
{
    int ret;
    struct hns_roce_sm_priv *priv = NULL;
    struct hns_roce_buf_list *share_buf = NULL;

    if (hr_dev == NULL || tsqp_depth_data == NULL) {
        pr_err("hns3: [hns_roce] hr_dev[%pk] is NULL or tsqp_depth_data[%pk] is NULL.\n", hr_dev, tsqp_depth_data);
        return -EINVAL;
    }

    if (hr_dev->port_num == 0) {
        dev_err(hr_dev->dev, "[hns_roce] port_num is 0, invalid.\n");
        return -EINVAL;
    }

    ret = hns_roce_get_max_sq_num(hr_dev, tsqp_depth_data);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_get_max_sq_num fail! ret:%d, rdev_index:%u\n",
            ret, tsqp_depth_data->rdev_index);
        return ret;
    }

    hr_dev->max_sq_num = tsqp_depth_data->qp_num / hr_dev->port_num;
    hr_dev->temp_depth = tsqp_depth_data->temp_depth;
    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;

    share_buf = &priv->share_buf;
    hns_roce_init_shared_buffer_common(priv, share_buf, hr_dev);

    tsqp_depth_data->sq_depth = hr_dev->sq_depth;
    return 0;
}

int hns_roce_get_tsqp_depth(struct hns_roce_dev *hr_dev, struct roce_get_tsqp_depth_data *tsqp_depth_data)
{
    if (hr_dev == NULL || tsqp_depth_data == NULL) {
        pr_err("hns3: [hns_roce] hr_dev[%pk] is NULL or tsqp_depth_data[%pk] is NULL.\n", hr_dev, tsqp_depth_data);
        return -EINVAL;
    }

    if (tsqp_depth_data->rdev_index >= hr_dev->port_num) {
        dev_err(hr_dev->dev, "[hns_roce] rdev_index[%u] >= port_num[%u], invalid.\n",
            tsqp_depth_data->rdev_index, hr_dev->port_num);
        return -EINVAL;
    }

    tsqp_depth_data->qp_num = hr_dev->max_sq_num;
    tsqp_depth_data->temp_depth = hr_dev->temp_depth;
    tsqp_depth_data->sq_depth = hr_dev->sq_depth;

    return 0;
}

#ifdef HNS_ROCE_DEVICE
STATIC int hns_roce_init_device_shared_map(struct hns_roce_dev *hr_dev, struct hns_roce_buf_list *share_buf)
{
    u64 addr;
    int dev_id = hns_roce_get_devid(hr_dev);
    if (dev_id < 0) {
        dev_err(hr_dev->dev, "get err dev_id[%d] \n", dev_id);
        return -EINVAL;
    }

    addr = dev_id * HNS_ROCE_HIGH_SHIFT;
    addr <<= HNS_ROCE_SHARED_MEMORY_BITWIDTH;
    addr += HNS_ROCE_SHARED_MEMORY_ADDR;
    if (hr_dev->port_num > 1 && hr_dev->port_id == 1) {
        addr += HNS_ROCE_SHARED_BUF_SIZE / hr_dev->port_num;
    }
    share_buf->map = addr;

    return 0;
}
#endif

static int hns_roce_alloc_shared_buffer(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_sm_priv *priv = NULL;
    struct hns_roce_buf_list *share_buf = NULL;
    int ret;
    int share_buf_size;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;
    share_buf = &priv->share_buf;
    share_buf_size = HNS_ROCE_SHARED_BUF_SIZE / hr_dev->port_num;
    /* set cpu core affinity */
    /*
     * This buffer will be used for asynchronous send queue, template wqe and doorbell
     */
#ifdef HNS_ROCE_DEVICE
    ret = hns_roce_init_device_shared_map(hr_dev, share_buf);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_init_device_shared_map failed, ret:%d\n", ret);
        return -ENOMEM;
    }
    share_buf->buf = ioremap_wc(share_buf->map, share_buf_size);
#else
    share_buf->buf = dma_alloc_coherent(hr_dev->dev, HNS_ROCE_SHARED_BUF_SIZE, &(share_buf->map),
        GFP_KERNEL | __GFP_NOWARN);
#endif

    if (share_buf->buf == NULL) {
        dev_err(hr_dev->dev, "dma_alloc_coherent or ioremap_wc for share buff failed\n");
        return -ENOMEM;
    }
    priv->share_mem_size = share_buf_size;
    ret = hns_roce_init_shared_buffer_info(priv, share_buf, hr_dev);
    if (ret) {
        goto init_shared_buffer_fail;
    }

    return 0;

init_shared_buffer_fail:
#ifdef HNS_ROCE_DEVICE
    iounmap((void __iomem *)share_buf->buf);
    share_buf->buf = NULL;
#else
    dma_free_coherent(hr_dev->dev, HNS_ROCE_SHARED_BUF_SIZE, share_buf->buf, share_buf->map);
    share_buf->buf = NULL;
#endif
    priv->share_mem_size = 0;

    return ret;
}

int hns_roce_init_shared_buffer(struct hns_roce_dev *hr_dev)
{
    int ret;

    if (hr_dev->port_num > HNS_ROCE_MAX_PORT_NUM || hr_dev->port_id > 1 || hr_dev->port_num == 0) {
        dev_err(hr_dev->dev, "hns_roce_init_device_shared_map failed, port_num:%u, port_id:%u\n",
            hr_dev->port_num, hr_dev->port_id);
        return -EINVAL;
    }

    mutex_init(&hr_dev->sm_mutex);

    hr_dev->share_mem_priv = kzalloc(sizeof(struct hns_roce_sm_priv), GFP_KERNEL);
    if (hr_dev->share_mem_priv == NULL) {
        mutex_destroy(&hr_dev->sm_mutex);
        dev_err(hr_dev->dev, "kzalloc memory for share_mem_priv failed\n");
        return -ENOMEM;
    }

    ret = hns_roce_alloc_shared_buffer(hr_dev);
    if (ret) {
        kfree(hr_dev->share_mem_priv);
        hr_dev->share_mem_priv = NULL;
        mutex_destroy(&hr_dev->sm_mutex);
        dev_err(hr_dev->dev, "hns_roce_alloc_shared_buffer failed, ret:%d\n", ret);
        return ret;
    }

    return 0;
}

void hns_roce_free_shared_buffer(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_sm_priv *priv = NULL;
    struct hns_roce_buf_list *share_buf = NULL;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;
    share_buf = &priv->share_buf;

    if (share_buf->buf != NULL) {
#ifdef HNS_ROCE_DEVICE
        iounmap((void __iomem *)share_buf->buf);
        share_buf->buf = NULL;
#else
        dma_free_coherent(hr_dev->dev, HNS_ROCE_SHARED_BUF_SIZE, share_buf->buf, share_buf->map);
        share_buf->buf = NULL;
#endif
        priv->share_mem_size = 0;
        priv->sq_buf.map = NULL;
        priv->temp_buf.map = NULL;
        priv->db_buf.map = NULL;
        priv->dfx_buf.map = NULL;
        hns_roce_bitmap_cleanup(&priv->sq_bitmap);
    }

    kfree(hr_dev->share_mem_priv);
    hr_dev->share_mem_priv = NULL;
    mutex_destroy(&hr_dev->sm_mutex);
}

STATIC void hns_roce_clean_shared_dfx(struct hns_roce_dev *hr_dev, int index)
{
    struct hns_roce_sm_priv *priv = NULL;
    struct hns_roce_buf_list *share_buf = NULL;
    void *share_dfx = NULL;
    int ret;
    long shared_sq_buf_len;
    long shared_temp_buf_len;
    long shared_db_buf_len;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;
    share_buf = &priv->share_buf;

    shared_sq_buf_len = (hr_dev->max_sq_num + 1) * HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN +
        hr_dev->max_sq_num * hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH;
    shared_temp_buf_len = hr_dev->max_sq_num * hr_dev->temp_depth * HNS_ROCE_SHARED_TEMP_LENGTH;
    shared_db_buf_len = hr_dev->max_sq_num * HNS_ROCE_SHARED_DB_BUF_ENTRY_LENGTH;

    share_dfx = share_buf->buf + (shared_sq_buf_len + shared_temp_buf_len + shared_db_buf_len +
        index * HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH);
    ret = memset_s(share_dfx, HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH, 0,
                   HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
    }
    return;
}

int hns_roce_alloc_shared_sq(struct hns_roce_dev *hr_dev, struct hns_roce_share_sq *share_sq)
{
    int ret;
    unsigned long index;
    struct hns_roce_sm_priv *priv = NULL;
    struct hns_roce_buf_list *share_buf = NULL;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;
    share_buf = &priv->share_buf;

    index = 0;
    ret = hns_roce_bitmap_alloc(&priv->sq_bitmap, &index);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_bitmap_alloc fail! ret:%u\n", ret);
        return ret;
    }

    ret = hns_roce_magic_words_init(hr_dev, share_buf, index);
    if (ret) {
        dev_err(hr_dev->dev, "magic words fill failed, ret %d, expected ret 0.\n", ret);
        hns_roce_bitmap_free(&priv->sq_bitmap, index, BITMAP_NO_RR);
        return ret;
    }

    share_sq->index = index;
    share_sq->sq_addr = (u64)(uintptr_t)(priv->sq_buf.map + (share_sq->index *
                        (hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH))) +
                        (share_sq->index + 1) * HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN;
    share_sq->temp_addr = (u64)(uintptr_t)(priv->temp_buf.map +
        (share_sq->index * (hr_dev->temp_depth * HNS_ROCE_SHARED_TEMP_LENGTH)));
    share_sq->db_addr = (u64)(uintptr_t)(priv->db_buf.map + (share_sq->index * HNS_ROCE_SHARED_DB_BUF_ENTRY_LENGTH));
    share_sq->dfx_addr = (u64)(uintptr_t)(priv->dfx_buf.map + (share_sq->index * HNS_ROCE_SHARED_DFX_BUF_ENTRY_LENGTH));
    share_sq->max_sq_depth = hr_dev->sq_depth;
    share_sq->max_temp_depth = hr_dev->temp_depth;
    share_sq->max_db_depth = HNS_ROCE_SHARED_DB_DEPTH;
    share_sq->max_sq_num = hr_dev->max_sq_num;
    hns_roce_clean_shared_dfx(hr_dev, index);

    dev_notice(hr_dev->dev, "alloc share buffer success! index:%u sq_addr:0x%llx temp_addr:0x%llx db_addr:0x%llx \
        gdrdfx_addr:0x%llx sq_depth:%u temp_depth:%u db_depth:%u!\n", share_sq->index,
        share_sq->sq_addr, share_sq->temp_addr, share_sq->db_addr, share_sq->dfx_addr,
        share_sq->max_sq_depth, share_sq->max_temp_depth, share_sq->max_db_depth);

    return 0;
}

STATIC void hns_roce_magic_words_comp(struct hns_roce_dev *hr_dev, int index)
{
    int ret;
    u64 num;
    unsigned char* magic_num = NULL;
    struct hns_roce_sm_priv *priv = NULL;
    struct hns_roce_buf_list *share_buf = NULL;
    unsigned char *magic_addr = NULL;
    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;
    share_buf = &priv->share_buf;
    num = HNS_ROCE_MAGIC_WORDS;
    magic_num = (unsigned char *)&num;
    magic_addr = share_buf->buf + (index * (hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH) + (index + 1) *
                 HNS_ROCE_SHARED_SQ_MAGIC_WORDS_RSV_LEN - HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    ret = strncmp(magic_addr, magic_num, HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    if (ret) {
        dev_err(hr_dev->dev, "index %d, head maigc words not equal ret %d\n", index, ret);
        hns_roce_magic_words_err_print(hr_dev->dev, magic_addr, HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    }
    magic_addr += ((hr_dev->sq_depth * HNS_ROCE_SHARED_WQE_LENGTH) + HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    ret = strncmp(magic_addr, magic_num, HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    if (ret) {
        dev_err(hr_dev->dev, "index %d, tail maigc words not equal ret %d\n", index, ret);
        hns_roce_magic_words_err_print(hr_dev->dev, magic_addr, HNS_RCOE_SHARED_SQ_MAGIC_WORDS_LEN);
    }
    return;
}

void hns_roce_free_shared_sq(struct hns_roce_dev *hr_dev, int index)
{
    struct hns_roce_sm_priv *priv = NULL;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;

    hns_roce_magic_words_comp(hr_dev, index);
    hns_roce_bitmap_free(&priv->sq_bitmap, index, BITMAP_NO_RR);

    return;
}

int hns_roce_mailbox_shared_sq(struct hns_roce_dev *hr_dev,
    struct hns_roce_share_sq share_sq, u32 gdr_enable)
{
    int ret;
    int index;
    struct hns_roce_share_reset_sq share_reset_cmd;

    if (gdr_enable != HNS_ROCE_GDR_QP_MODE) {
        return 0;
    }
    /*
    * share send sq info to ts
    */
    index = share_sq.index + (hr_dev->max_sq_num * hr_dev->port_id);
    index = (unsigned int)index | (hr_dev->port_id << INDEX_SHIFT_OFFSET);
    share_reset_cmd.opcode = MAILBOX_RESET_SQ;
    share_reset_cmd.index = index;
    share_reset_cmd.sq_addr = share_sq.sq_addr;
    share_reset_cmd.temp_addr = share_sq.temp_addr;
    share_reset_cmd.db_addr = share_sq.db_addr;
    share_reset_cmd.dfx_addr = share_sq.dfx_addr;
    share_reset_cmd.sq_depth = share_sq.max_sq_depth;
    share_reset_cmd.temp_depth = share_sq.max_temp_depth;

    ret = hns_roce_mailbox_to_ts(hr_dev, (char *)&share_reset_cmd, sizeof(share_reset_cmd));
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_mailbox_to_ts fail! ret:%u index:%u sq_addr:0x%llx temp_addr:0x%llx \
                db_addr:0x%llx gdrdfx_addr:0x%llx sq_depth:%u temp_depth:%u\n", ret, share_sq.index, share_sq.sq_addr, \
                share_sq.temp_addr, share_sq.db_addr, share_sq.dfx_addr,
                share_sq.max_sq_depth, share_sq.max_temp_depth);
        return ret;
    }

    return ret;
}

dma_addr_t hns_roce_get_shared_mem_base(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_sm_priv *priv = NULL;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;

    return priv->share_buf.map;
}

int hns_roce_get_shared_mem_size(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_sm_priv *priv = NULL;

    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;

    return priv->share_mem_size;
}
