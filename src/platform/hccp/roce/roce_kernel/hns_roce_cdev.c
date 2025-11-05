/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hns_roce_cdev.h"
#include "linux/kernel.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/version.h>
#include "hns_roce_sm.h"
#include "hns_roce_hw_v2.h"

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

#define ROCE_CDEV_IOC_MAXNR     4
#define ROCE_CDEV_DEV_MAJOR     0

STATIC int roce_cdev_set_tsqp_depth(struct hns_roce_dev *hr_dev, char *buf)
{
    int ret;
    struct roce_set_tsqp_depth_data tsqp_depth_data = {0};

    if (copy_from_user((char *)&tsqp_depth_data, buf, sizeof(struct roce_set_tsqp_depth_data))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_from_user fail.\n");
        return -EINVAL;
    }

    mutex_lock(&hr_dev->sm_mutex);
    if (hr_dev->qp_cnt != 0) {
        dev_err(hr_dev->dev, "hr_dev->qp_cnt:%u, not 0, can not set tsqp_depth", hr_dev->qp_cnt);
        mutex_unlock(&hr_dev->sm_mutex);
        return -EINVAL;
    }

    ret = hns_roce_set_tsqp_depth(hr_dev, &tsqp_depth_data);
    if (ret) {
        dev_err(hr_dev->dev, "[roce_cdev] hns_roce_set_tsqp_depth fail, ret[%d] rdev_index[%u] temp_depth[%u].\n",
            ret, tsqp_depth_data.rdev_index, tsqp_depth_data.temp_depth);
        mutex_unlock(&hr_dev->sm_mutex);
        return ret;
    }

    mutex_unlock(&hr_dev->sm_mutex);
    if (copy_to_user(buf, (char *)&tsqp_depth_data, sizeof(struct roce_set_tsqp_depth_data))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_to_user fail.\n");
        return -EINVAL;
    }

    return 0;
}

STATIC int roce_cdev_get_tsqp_depth(struct hns_roce_dev *hr_dev, char *buf)
{
    int ret;
    struct roce_get_tsqp_depth_data tsqp_depth_data = {0};

    if (copy_from_user((char *)&tsqp_depth_data, buf, sizeof(struct roce_get_tsqp_depth_data))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_from_user fail.\n");
        return -EINVAL;
    }

    mutex_lock(&hr_dev->sm_mutex);
    ret = hns_roce_get_tsqp_depth(hr_dev, &tsqp_depth_data);
    if (ret) {
        dev_err(hr_dev->dev, "[roce_cdev] hns_roce_get_tsqp_depth fail, ret[%d] rdev_index[%u]\n",
            ret, tsqp_depth_data.rdev_index);
        mutex_unlock(&hr_dev->sm_mutex);
        return ret;
    }

    mutex_unlock(&hr_dev->sm_mutex);
    if (copy_to_user(buf, (char *)&tsqp_depth_data, sizeof(struct roce_get_tsqp_depth_data))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_to_user fail.\n");
        return -EINVAL;
    }

    return 0;
}

STATIC int roce_cdev_get_roce_dev_info(struct hns_roce_dev *hr_dev, char *buf)
{
    struct roce_dev_data rdev_data = {0};

    if (copy_from_user((char *)&rdev_data, buf, sizeof(struct roce_dev_data))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_from_user fail.\n");
        return -EINVAL;
    }

    rdev_data.rdev_index = hr_dev->port_id;

    if (copy_to_user(buf, (char *)&rdev_data, sizeof(struct roce_dev_data))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_to_user fail.\n");
        return -EINVAL;
    }

    return 0;
}

STATIC int roce_cdev_get_roce_qpc_info(struct hns_roce_dev *hr_dev, char *buf)
{
    struct hns_roce_qpc_stat qpc_stat = {0};
    int ret;

    if (copy_from_user((char *)&qpc_stat, buf, sizeof(struct hns_roce_qpc_stat))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = hns_roce_v2_get_roce_qpc_stat(hr_dev, (char *)qpc_stat.index, (char *)qpc_stat.info, INFO_PAYLOAD_LEN);
    if (ret) {
        dev_err(hr_dev->dev, "[roce_cdev] hns_roce_v2_get_roce_qpc_stat fail, retval[%d].\n", ret);
        return ret;
    }

    if (copy_to_user(buf, (char *)&qpc_stat, sizeof(struct hns_roce_qpc_stat))) {
        dev_err(hr_dev->dev, "[roce_cdev] copy_to_user fail.\n");
        return -EINVAL;
    }

    return 0;
}

STATIC int roce_cdev_open(struct inode *inode, struct file *filp)
{
    struct hns_roce_dev *hr_dev = NULL;

    if ((inode == NULL) || (filp == NULL)) {
        pr_err("[roce_cdev_open] Inode[%pk] or filp[%pk] is null\n", inode, filp);
        return -EINVAL;
    }

    hr_dev = container_of(inode->i_cdev, struct hns_roce_dev, cdev.cdev);
    filp->private_data = hr_dev;

    return 0;
}

STATIC int roce_cdev_close(struct inode *inode, struct file *filp)
{
    if ((inode == NULL) || (filp == NULL)) {
        pr_err("[roce_cdev_close] Inode[%pk] or filp[%pk] is null\n", inode, filp);
        return -EINVAL;
    }

    filp->private_data = NULL;

    return 0;
}

typedef int (*roce_ioctl_handle_func)(struct hns_roce_dev *hr_dev, char *buf);

struct roce_cdev_cmd_ops {
    unsigned int roce_cdev_cmd;
    roce_ioctl_handle_func cmd_op;
};

STATIC struct roce_cdev_cmd_ops g_roce_cdev_cmd_handler[] = {
    { ROCE_CMD_SET_TSQP_DEPTH, roce_cdev_set_tsqp_depth },
    { ROCE_CMD_GET_TSQP_DEPTH, roce_cdev_get_tsqp_depth },
    { ROCE_CMD_GET_ROCE_DEV_INFO, roce_cdev_get_roce_dev_info },
    { ROCE_CMD_GET_ROCE_QPC_STAT, roce_cdev_get_roce_qpc_info },
};

STATIC long roce_cdev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    char *buf = (char *)(uintptr_t)arg;
    struct hns_roce_dev *hr_dev = NULL;
    int ret = -EINVAL;
    size_t i;

    if (_IOC_TYPE(cmd) != ROCE_IOCTL_MAGIC) {
        pr_err("hns3: [roce_cdev] cmd[%u] type incorrect\n", cmd);
        return -ENOTTY;
    }

    if (_IOC_NR(cmd) > ROCE_CDEV_IOC_MAXNR) {
        pr_err("hns3: [roce_cdev] cmd[%u] incorrect\n", cmd);
        return -ENOTTY;
    }

    if (buf == NULL || fp == NULL) {
        pr_err("hns3: [roce_cdev] arg err buf[%pk] is NULL or fp[%pk] is NULL\n", buf, fp);
        return -EINVAL;
    }

    hr_dev = fp->private_data;
    if (hr_dev == NULL) {
        pr_err("hns3: [roce_cdev] hr_dev is NULL!\n");
        return -EINVAL;
    }

    for (i = 0; i < (sizeof(g_roce_cdev_cmd_handler) / sizeof(struct roce_cdev_cmd_ops)); i++) {
        if (cmd == g_roce_cdev_cmd_handler[i].roce_cdev_cmd) {
            ret = g_roce_cdev_cmd_handler[i].cmd_op(hr_dev, buf);
            break;
        }
    }

    if (ret) {
        dev_err(hr_dev->dev, "[roce_cdev] cmd[%u] execute fail, ret[%d]", cmd, ret);
        return ret;
    }

    return 0;
}

static const struct file_operations g_roce_cdev_fops = {
    .owner = THIS_MODULE,
    .open = roce_cdev_open,
    .release = roce_cdev_close,
    .unlocked_ioctl = roce_cdev_ioctl,
};

int roce_init_cdev(struct hns_roce_dev *hr_dev)
{
    int ret;
    dev_t devno;

    if (hr_dev == NULL) {
        pr_err("hns3: [roce_cdev] hr_dev is NULL, invalid.\n");
        return -EINVAL;
    }

    ret = alloc_chrdev_region(&devno, 0, 1, hr_dev->ib_dev.name);
    if (ret < 0) {
        dev_err(hr_dev->dev, "[roce_cdev] alloc_chrdev_region failed: %d!\n", ret);
        return ret;
    }

    hr_dev->cdev.dev_major = MAJOR(devno);

    cdev_init(&hr_dev->cdev.cdev, &g_roce_cdev_fops);
    hr_dev->cdev.cdev.owner = THIS_MODULE;
    hr_dev->cdev.cdev.ops = &g_roce_cdev_fops;

    ret = cdev_add(&hr_dev->cdev.cdev, devno, 1);
    if (ret) {
        dev_err(hr_dev->dev, "[roce_cdev] cdev_add failed: %d!\n", ret);
        goto err_chrdev_unreg;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    hr_dev->cdev.cdev_class = class_create(hr_dev->ib_dev.name);
#else
    hr_dev->cdev.cdev_class = class_create(THIS_MODULE, hr_dev->ib_dev.name);
#endif
    if (IS_ERR_OR_NULL(hr_dev->cdev.cdev_class)) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "[roce_cdev] create class\n");
        goto err_cdev_del;
    }

    hr_dev->cdev.cdev_device = device_create(hr_dev->cdev.cdev_class, NULL,
        MKDEV((unsigned int)(hr_dev->cdev.dev_major), 0), NULL, hr_dev->ib_dev.name);
    if (IS_ERR_OR_NULL(hr_dev->cdev.cdev_device)) {
        dev_err(hr_dev->dev, "[roce_cdev] failed to create device, dev_name[%s].\n", hr_dev->ib_dev.name);
        ret = -ENOMEM;
        goto err_class_destr;
    }

    dev_notice(hr_dev->dev, "dev_name[%s] roce_cdev_init success.\n", hr_dev->ib_dev.name);

    return 0;

err_class_destr:
    class_destroy(hr_dev->cdev.cdev_class);
err_cdev_del:
    cdev_del(&hr_dev->cdev.cdev);
err_chrdev_unreg:
    unregister_chrdev_region(devno, 1);

    return ret;
}

void roce_remove_cdev(struct hns_roce_dev *hr_dev)
{
    if (hr_dev == NULL) {
        pr_err("hns3: [roce_cdev] hr_dev is NULL, invalid.\n");
        return;
    }

    if (IS_ERR_OR_NULL(hr_dev->cdev.cdev_class)) {
        dev_err(hr_dev->dev, "[roce_cdev] cdev_class invalid.\n");
        return;
    }

    device_destroy(hr_dev->cdev.cdev_class, MKDEV((unsigned int)(hr_dev->cdev.dev_major), 0));

    class_destroy(hr_dev->cdev.cdev_class);

    cdev_del(&hr_dev->cdev.cdev);

    unregister_chrdev_region(MKDEV((unsigned int)(hr_dev->cdev.dev_major), 0), 1);

    dev_notice(hr_dev->dev, "dev_name[%s] roce_remove_cdev success.\n", hr_dev->ib_dev.name);
}
