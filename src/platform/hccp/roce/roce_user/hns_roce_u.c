/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "hns_roce_u_abi.h"
#include "ascend_hal.h"
#include "hns_roce_u.h"

static void hns_roce_free_context(struct ibv_context *ibctx);

#define HID_LEN         15
#define DEV_MATCH_LEN       128

#ifndef PCI_VENDOR_ID_HUAWEI
#define PCI_VENDOR_ID_HUAWEI            0x19E5
#endif

static const struct verbs_match_ent g_hca_table[] = {
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA222, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA223, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA224, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA225, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA226, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA227, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA125, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xD800, &g_hns_roce_u_hw_v2),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA220, &g_hns_roce_u_hw_v3),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA228, &g_hns_roce_u_hw_v3),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA229, &g_hns_roce_u_hw_v3),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA22E, &g_hns_roce_u_hw_v3),
    VERBS_PCI_MATCH(PCI_VENDOR_ID_HUAWEI, 0xA22F, &g_hns_roce_u_hw_v3),
    {}
};

STATIC const struct verbs_context_ops hns_common_ops = {
    .query_device_ex  = hns_roce_u_query_device,
    .query_port       = hns_roce_u_query_port,
    .alloc_pd         = hns_roce_u_alloc_pd,
    .dealloc_pd       = hns_roce_u_free_pd,
    .reg_mr           = hns_roce_u_reg_mr,
    .rereg_mr         = hns_roce_u_rereg_mr,
    .dereg_mr         = hns_roce_u_dereg_mr,
    .alloc_mw         = hns_roce_u_alloc_mw,
    .dealloc_mw       = hns_roce_u_dealloc_mw,
    .bind_mw          = hns_roce_u_bind_mw,
    .create_cq        = hns_roce_u_create_cq,
    .cq_event         = hns_roce_u_cq_event,
    .modify_cq        = hns_roce_u_modify_cq,
    .destroy_cq       = hns_roce_u_destroy_cq,
    .create_srq       = hns_roce_u_create_srq,
    .modify_srq       = hns_roce_u_modify_srq,
    .query_srq        = hns_roce_u_query_srq,
    .destroy_srq      = hns_roce_u_destroy_srq,
    .create_qp        = hns_roce_u_create_qp,
    .query_qp         = hns_roce_u_query_qp,
    .create_ah        = hns_roce_u_create_ah,
    .destroy_ah       = hns_roce_u_destroy_ah,
    .free_context     = hns_roce_free_context,
};

static void hns_roce_init_exp_ops(struct hns_roce_device *hr_dev, struct hns_roce_context *context)
{
    context->ibv_ctx_exp.drv_exp_ibv_poll_cq   = hr_dev->u_hw->hw_ops.poll_cq;
    context->ibv_ctx_exp.exp_peer_commit_qp    = hr_dev->u_hw->exp_peer_commit_qp;
    context->ibv_ctx_exp.drv_exp_post_send     = hr_dev->u_hw->exp_post_send;
    context->ibv_ctx_exp.drv_exp_create_qp     = hns_roce_u_exp_create_qp;
    context->ibv_ctx_exp.drv_exp_create_cq     = hns_roce_u_exp_create_cq;
    context->ibv_ctx_exp.drv_exp_query_device  = hns_roce_u_exp_query_device;

    context->ibv_ctx_exp.drv_exp_query_notify  = hns_roce_u_exp_query_notify;
    context->ibv_ctx_exp.drv_exp_reg_mr    = hns_roce_u_exp_reg_mr;
    context->ibv_ctx_exp.sz = sizeof(struct verbs_context_exp);
}

static int hns_roce_alloc_hw_db(struct hns_roce_device *hr_dev, struct hns_roce_context *context, int cmd_fd)
{
#ifndef HNS_ROCE_LLT
    context->uar = mmap(NULL, hr_dev->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, cmd_fd, 0);
    if (context->uar == MAP_FAILED) {
        roce_err("failed to mmap() uar page, cmd_fd[%d]", cmd_fd);
        return -ENOMEM;
    }
#else
    context->uar = malloc(hr_dev->page_size);
#endif

    return 0;
}

static void hns_roce_free_hw_db(struct hns_roce_device *hr_dev, struct hns_roce_context *context)
{
#ifndef HNS_ROCE_LLT
    if (munmap(context->uar, hr_dev->page_size)) {
        roce_err("Warning: Munmap uar failed.");
    }
#else
    free(context->uar);
#endif
    context->uar = NULL;
}

static int hns_roce_alloc_hw_v1_buf(struct hns_roce_context *context, int cmd_fd)
{
    /*
     * when vma->vm_pgoff is 1, the cq_tptr_base includes 64K CQ,
     * a pointer of CQ need 2B size
     */
#ifndef HNS_ROCE_LLT
    context->cq_tptr_base = mmap(NULL, HNS_ROCE_CQ_DB_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
        cmd_fd, HNS_ROCE_TPTR_OFFSET);
    if (context->cq_tptr_base == MAP_FAILED) {
        roce_err("Failed to mmap cq_tptr page, cmd_fd[%d]", cmd_fd);
        return -ENOMEM;
    }
#else
    context->cq_tptr_base = malloc(HNS_ROCE_CQ_DB_BUF_SIZE);
#endif
    return 0;
}

static void hns_roce_free_hw_v1_buf(struct hns_roce_context *context)
{
#ifndef HNS_ROCE_LLT
    if (munmap(context->cq_tptr_base, HNS_ROCE_CQ_DB_BUF_SIZE)) {
        roce_err("Warning: Munmap tptr failed.");
    }
#else
    free(context->cq_tptr_base);
#endif
    context->cq_tptr_base = NULL;
}

#ifdef HNS_ROCE_DEVICE
static int hns_roce_alloc_hw_v2_buf(struct hns_roce_context *context, int cmd_fd)
{
    /*
     * when vma->vm_pgoff is 1, the notify includes page
     */
#ifndef HNS_ROCE_LLT

    /*
     * when vma->vm_pgoff is 3, the share buffer includes 8M
     */
    context->share_buffer_base = mmap(NULL, HNS_ROCE_SHARE_BUF_SIZE / context->port_num,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      cmd_fd, HNS_ROCE_SHARE_OFFSET);
    if (context->share_buffer_base == MAP_FAILED) {
        roce_err("Warning: Failed to mmap share memory page, cmd_fd[%d], errno[%d]", cmd_fd, errno);
        return -ENOMEM;
    }

#else
    context->share_buffer_base = malloc(HNS_ROCE_SHARE_BUF_SIZE);
#endif

    return 0;
}

static void hns_roce_free_hw_v2_buf(struct hns_roce_context *context)
{
#ifndef HNS_ROCE_LLT
    if (munmap(context->share_buffer_base, HNS_ROCE_SHARE_BUF_SIZE / context->port_num)) {
        roce_err("Warning: Munmap share memory failed.");
    }

    if (munmap((void *)context->notify_va_base, context->notify_size)) {
        roce_err("Warning: Munmap notify memory failed.");
    }
#else
    free(context->share_buffer_base);
    free(context->notify_va_base);
#endif
    context->share_buffer_base = NULL;
    context->notify_va_base = NULL;
}
#endif

static int hns_roce_alloc_hw_buf(struct hns_roce_device *hr_dev,
    struct hns_roce_context *context, int cmd_fd)
{
    int ret = hns_roce_alloc_hw_db(hr_dev, context, cmd_fd);
    if (ret) {
        roce_err("hns_roce_alloc_hw_db failed");
        return ret;
    }

    if (hr_dev->hw_version == HNS_ROCE_HW_VER1) {
        ret = hns_roce_alloc_hw_v1_buf(context, cmd_fd);
        if (ret) {
            roce_err("hns_roce_alloc_hw_v1_buf failed");
            hns_roce_free_hw_db(hr_dev, context);
            return ret;
        }
    }

#ifdef HNS_ROCE_DEVICE
    if (hr_dev->hw_version == HNS_ROCE_HW_VER2 || hr_dev->hw_version == HNS_ROCE_HW_VER3) {
        ret = hns_roce_alloc_hw_v2_buf(context, cmd_fd);
        if (ret) {
            roce_err("hns_roce_alloc_hw_v2_buf failed");
            hns_roce_free_hw_db(hr_dev, context);
            return ret;
        }

        roce_info("user mmap share memory.\n");
    }
#endif

    return 0;
}

static void hns_roce_free_hw_buf(struct hns_roce_device *hr_dev, struct hns_roce_context *context)
{
#ifdef HNS_ROCE_DEVICE
    if (hr_dev->hw_version == HNS_ROCE_HW_VER2 || hr_dev->hw_version == HNS_ROCE_HW_VER3) {
        hns_roce_free_hw_v2_buf(context);
    }
#endif

    if (hr_dev->hw_version == HNS_ROCE_HW_VER1) {
        hns_roce_free_hw_v1_buf(context);
    }

    hns_roce_free_hw_db(hr_dev, context);
}

STATIC int hns_roce_get_dev_id(unsigned int chip_id, unsigned int *dev_id)
{
    unsigned int *device_list;
    unsigned int dev_num = 0;
    long int val = 0;
    unsigned int i;
    int ret;

    ret = drvGetDevNum(&dev_num);
    if (ret) {
        roce_err("hns_roce_get_dev_id failed!, ret is %d", ret);
        return -EINVAL;
    }

    if (dev_num == 0) {
        roce_err("hns_roce_get_dev_id dev_num is 0, failed!");
        return -EINVAL;
    }

    device_list = (unsigned int *)calloc(dev_num, sizeof(unsigned int));
    if (device_list == NULL) {
        roce_err("hns_roce_get_dev_id failed, calloc device list failed");
        return -ENOMEM;
    }

    ret = drvGetDeviceLocalIDs(device_list, dev_num);
    if (ret) {
        roce_err("hns_roce_get_dev_id failed, drvGetDeviceLocalIDs failed, ret is %d", ret);
        goto out;
    }

    for (i = 0; i < dev_num; i++) {
        ret = halGetDeviceInfo(device_list[i], MODULE_TYPE_SYSTEM, INFO_TYPE_PHY_CHIP_ID, &val);
        if (ret) {
            roce_err("hns_roce_get_dev_id failed, halGetDeviceInfo failed, ret is %d", ret);
            goto out;
        }

        if ((unsigned int)val == chip_id) {
            *dev_id = device_list[i];
            goto out;
        }
    }

    *dev_id = 0;
    ret = EINVAL;
    roce_err("hns_roce_get_dev_id failed, dev_id set to 0!");

out:
    free(device_list);
    device_list = NULL;
    return ret;
}

static struct hns_roce_context* hns_roce_context_init(struct ibv_device *ibdev, int cmd_fd)
{
    struct hns_roce_alloc_ucontext_resp resp = {};
    struct hns_roce_context *context;
    struct ibv_get_context cmd;
    int ret;

    context = verbs_init_and_alloc_context(ibdev, cmd_fd, context, ibv_ctx, RDMA_DRIVER_HNS);
    if (context == NULL) {
        roce_err("verbs_init_and_alloc_context failed, cmd_fd[%d].", cmd_fd);
        return NULL;
    }

    if (ibv_cmd_get_context(&context->ibv_ctx, &cmd, sizeof(cmd),
        (struct ib_uverbs_get_context_resp *)&resp, sizeof(resp))) {
        roce_err("ibv_cmd_get_context failed.");
        verbs_uninit_context(&context->ibv_ctx);
        free(context);
        return NULL;
    }

    context->num_qps = resp.qp_tab_size;
    context->qp_table_shift = ffs(context->num_qps) - 1 - HNS_ROCE_QP_TABLE_BITS;
    context->qp_table_mask = (1 << (unsigned int)(context->qp_table_shift)) - 1;
    context->notify_pa_base = (void *)(uintptr_t)resp.notify_pa;
    context->notify_size    = resp.notify_size;
    context->port_num = resp.port_num;
    context->port_id = resp.port_id;

    ret = hns_roce_get_dev_id(resp.chip_id, &context->dev_id);
    if (ret) {
        roce_err("hns_roce_get_dev_id failed.");
        verbs_uninit_context(&context->ibv_ctx);
        free(context);
        return NULL;
    }

    return context;
}

static void hns_roce_context_uninit(struct hns_roce_context *context)
{
    verbs_uninit_context(&context->ibv_ctx);
    free(context);
    context = NULL;
}

static struct verbs_context *hns_roce_alloc_context(struct ibv_device *ibdev, int cmd_fd, void *private_data)
{
    int ret;
    int i;
    struct ibv_device_attr dev_attrs;
    struct hns_roce_context *context = NULL;
    struct hns_roce_device *hr_dev = to_hr_dev(ibdev);

    context = hns_roce_context_init(ibdev, cmd_fd);
    if (context == NULL) {
        roce_err("hns_roce_context_init failed");
        return NULL;
    }

    if (context->port_num == 0) {
        roce_err("context port_num is 0, invalid.\n");
        goto context_uninit;
    }

    pthread_mutex_init(&context->qp_table_mutex, NULL);
    pthread_mutex_init(&context->db_list_mutex, NULL);
    for (i = 0; i < HNS_ROCE_QP_TABLE_SIZE; ++i) {
        context->qp_table[i].refcnt = 0;
    }

    ret = hns_roce_alloc_hw_buf(hr_dev, context, cmd_fd);
    if (ret) {
        roce_err("hns_roce_alloc_hw_v2_buf failed");
        goto lock_free;
    }

    verbs_set_ops(&context->ibv_ctx, &hns_common_ops);
    verbs_set_ops(&context->ibv_ctx, &hr_dev->u_hw->hw_ops);
    hns_roce_init_exp_ops(hr_dev, context);

    if (hns_roce_u_query_device(&context->ibv_ctx.context, NULL,
                                container_of(&dev_attrs, struct ibv_device_attr_ex, orig_attr), sizeof(dev_attrs))) {
        roce_err("Warning: Failed to query device");
        goto hw_buf_free;
    }

    context->max_qp_wr = dev_attrs.max_qp_wr;
    context->max_sge = dev_attrs.max_sge;
    context->max_cqe = dev_attrs.max_cqe;

    return &context->ibv_ctx;

hw_buf_free:
    hns_roce_free_hw_buf(hr_dev, context);
lock_free:
    pthread_mutex_destroy(&context->db_list_mutex);
    pthread_mutex_destroy(&context->qp_table_mutex);
context_uninit:
    hns_roce_context_uninit(context);
    context = NULL;
    return NULL;
}

static void hns_roce_free_context(struct ibv_context *ibctx)
{
    struct hns_roce_device *hr_dev = to_hr_dev(ibctx->device);
    struct hns_roce_context *context = to_hr_ctx(ibctx);

    hns_roce_free_hw_buf(hr_dev, context);
    pthread_mutex_destroy(&context->db_list_mutex);
    pthread_mutex_destroy(&context->qp_table_mutex);
    verbs_uninit_context(&context->ibv_ctx);
    free(context);
    context = NULL;
}

static void hns_uninit_device(struct verbs_device *verbs_device)
{
    struct hns_roce_device *dev = to_hr_dev(&verbs_device->device);

    free(dev);
    dev = NULL;
}

STATIC int hns_roce_calloc_dev(int num, struct hns_roce_device **dev)
{
    if (num <= 0) {
        roce_err("para err num is %d", num);
        return -EINVAL;
    }

    *dev = calloc(num, sizeof(struct hns_roce_device));
    if ((*dev) == NULL) {
        roce_err("calloc dev failed");
        return -ENOMEM;
    }
    return 0;
}

static struct verbs_device *hns_device_alloc(struct verbs_sysfs_dev *sysfs_dev)
{
    int ret;
    struct hns_roce_device *dev = NULL;

    ret = hns_roce_calloc_dev(1, &dev);
    if (ret) {
        roce_err("calloc dev failed, ret [%d], expect 0", ret);
        return NULL;
    }

    dev->u_hw = sysfs_dev->match->driver_data;
    dev->hw_version = dev->u_hw->hw_version;
    dev->page_size   = sysconf(_SC_PAGESIZE);
    return &(dev->ibv_dev);
}

static const struct verbs_device_ops g_hns_roce_dev_ops = {
    .name = "hns",
    .match_min_abi_version = 0,
    .match_max_abi_version = INT_MAX,
    .match_table = g_hca_table,
    .alloc_device = hns_device_alloc,
    .uninit_device = hns_uninit_device,
    .alloc_context = hns_roce_alloc_context,
};
PROVIDER_DRIVER(hns, g_hns_roce_dev_ops);
