/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/acpi.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <net/addrconf.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>

#include "hnae3.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

#define DECIMAL        10

STATIC ssize_t cqc_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int cqn;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: cqc_store get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = kstrtou32(buf, DECIMAL, &cqn);
    if (ret) {
        dev_err(dev, "Input params format unmatch\n");
        return -EINVAL;
    }

    if ((cqn < 0) || (cqn >= hr_dev->caps.num_cqs)) {
        dev_err(dev, "cqn[%d], hr_dev->caps.num_cqs[%d]\n",
            cqn, hr_dev->caps.num_cqs);
        return -EINVAL;
    }

    hr_dev->hr_stat.cqn = cqn;
    return strnlen(buf, count);
}

STATIC ssize_t cqc_show(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: cqc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_cqc_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "pkt query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t cmd_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: cqc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_cmd_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "cmd query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t pkt_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: cqc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_pkt_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "cmd query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t ceqc_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int ceqn;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: cqc_show get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = kstrtou32(buf, DECIMAL, &ceqn);
    if (ret) {
        dev_err(dev, "Input params format unmatch\n");
        return -EINVAL;
    }

    /* hardware handle eqn =  ceqn + 1 */
    if ((ceqn < 0) || (ceqn >= hr_dev->caps.num_comp_vectors)) {
        dev_err(hr_dev->dev, "ceqn[%d], hr_dev->caps.num_comp_vectors[%d]\n",
            ceqn, hr_dev->caps.num_comp_vectors);
        return -EINVAL;
    }

    hr_dev->hr_stat.ceqn = ceqn;
    return strnlen(buf, count);
}

STATIC ssize_t ceqc_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: cqc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_ceqc_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "ceqc query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t aeqc_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int aeqn;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: aeqc_store get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = kstrtou32(buf, DECIMAL, &aeqn);
    if (ret) {
        dev_err(dev, "Input params format unmatch\n");
        return -EINVAL;
    }

    /* hardware ignore aeqn, eqn = 0 */
    if ((aeqn < 0) || (aeqn >= hr_dev->caps.num_aeq_vectors)) {
        dev_err(hr_dev->dev, "aeqn[%d], num_aeq_vectors[%d]\n",
            aeqn, hr_dev->caps.num_aeq_vectors);
        return -EINVAL;
    }

    hr_dev->hr_stat.aeqn = aeqn;
    return strnlen(buf, count);
}

STATIC ssize_t aeqc_show(struct device *dev, struct device_attribute *attr,
                         char *buf)

{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: aeqc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_aeqc_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "aeqc query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t qpc_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int qpn;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: qpc_store get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev =
        container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = kstrtou32(buf, DECIMAL, &qpn);
    if (ret) {
        dev_err(dev, "Input params format unmatch, ret[%d]\n", ret);
        return -EINVAL;
    }

    if ((qpn < 0) || (qpn >= hr_dev->caps.num_qps)) {
        dev_err(dev, "qpn[%d] > hr_dev->caps.num_qps[%d]\n",
            qpn, hr_dev->caps.num_qps);
        return -EINVAL;
    }

    hr_dev->hr_stat.qpn = qpn;
    return strnlen(buf, count);
}

STATIC ssize_t qpc_show(struct device *dev, struct device_attribute *attr,
                        char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: qpc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev, ib_dev.dev);

    mutex_lock(&hr_dev->hr_stat_mutex);
    ret = hr_dev->dfx->query_qpc_stat(hr_dev, buf, &count);
    mutex_unlock(&hr_dev->hr_stat_mutex);

    if (ret) {
        dev_err(dev, "qpc query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t srqc_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int srqn;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: srqc_store get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev =
        container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = kstrtou32(buf, DECIMAL, &srqn);
    if (ret) {
        dev_err(dev, "Input params format unmatch, ret[%d]\n", ret);
        return -EINVAL;
    }

    if ((srqn < 0) || (srqn >= hr_dev->caps.num_srqs)) {
        dev_err(dev, "srqn[%d], hr_dev->caps.num_srqs[%d]\n",
            srqn, hr_dev->caps.num_srqs);
        return -EINVAL;
    }

    hr_dev->hr_stat.srqn = srqn;
    return strnlen(buf, count);
}

STATIC ssize_t srqc_show(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: srqc_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev =
        container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_srqc_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "srqc query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t mpt_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int key;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: mpt_store get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev =
        container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = kstrtou32(buf, DECIMAL, &key);
    if (ret) {
        dev_err(dev, "Input params format unmatch, ret[%d]\n", ret);
        return -EINVAL;
    }

    if ((key < 0) || (key >= hr_dev->caps.num_mtpts)) {
        dev_err(dev, "key[%d], hr_dev->caps.num_mtpts[%d]\n",
            key, hr_dev->caps.num_mtpts);
        return -EINVAL;
    }

    hr_dev->hr_stat.key = key;
    return strnlen(buf, count);
}

STATIC ssize_t mpt_show(struct device *dev, struct device_attribute *attr,
                        char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;
    int count = 0;

    if (dev == NULL) {
        pr_err("hns3: mpt_show get null ptr dev\n");
        return -EINVAL;
    }

    if (buf == NULL) {
        dev_err(dev, "get null ptr buf\n");
        return -EINVAL;
    }

    hr_dev =
        container_of(dev, struct hns_roce_dev, ib_dev.dev);
    ret = hr_dev->dfx->query_mpt_stat(hr_dev, buf, &count);
    if (ret) {
        dev_err(dev, "mpt query failed, ret[%d]\n", ret);
        return -EBUSY;
    }

    return count;
}

STATIC ssize_t coalesce_maxcnt_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_eq *eq = NULL;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: coalesce_maxcnt_show get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev,
                                               ib_dev.dev);
    eq = hr_dev->eq_table.eq;

    return scnprintf(buf, PAGE_SIZE, "%d\n", eq->eq_max_cnt);
}

STATIC ssize_t coalesce_period_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_eq *eq = NULL;

    if (dev == NULL || buf == NULL) {
        pr_err("hns3: coalesce_period_show get null ptr, dev %pK, buf %pK\n", dev, buf);
        return -EINVAL;
    }

    hr_dev = container_of(dev, struct hns_roce_dev,
                                               ib_dev.dev);
    eq = hr_dev->eq_table.eq;

    return scnprintf(buf, PAGE_SIZE, "%u\n", eq->eq_period);
}

static DEVICE_ATTR(aeqc, 0600, aeqc_show, aeqc_store);
static DEVICE_ATTR(qpc, 0600, qpc_show, qpc_store);
static DEVICE_ATTR(srqc, 0600, srqc_show, srqc_store);
static DEVICE_ATTR(mpt, 0600, mpt_show, mpt_store);
static DEVICE_ATTR(ceqc, 0600, ceqc_show, ceqc_store);
static DEVICE_ATTR(pkt, 0400, pkt_show, NULL);
static DEVICE_ATTR(cmd, 0400, cmd_show, NULL);
static DEVICE_ATTR(cqc, 0600, cqc_show, cqc_store);
static DEVICE_ATTR(coalesce_maxcnt, 0400, coalesce_maxcnt_show, NULL);
static DEVICE_ATTR(coalesce_period, 0400, coalesce_period_show, NULL);

static struct device_attribute *g_hns_roce_hw_attrs_list[] = {
    &dev_attr_cmd,
    &dev_attr_cqc,
    &dev_attr_aeqc,
    &dev_attr_qpc,
    &dev_attr_mpt,
    &dev_attr_pkt,
    &dev_attr_ceqc,
    &dev_attr_srqc,
    &dev_attr_coalesce_maxcnt,
    &dev_attr_coalesce_period,
};

int hns_roce_register_sysfs(struct hns_roce_dev *hr_dev)
{
    int ret;
    int i;

    for (i = 0; i < (int)ARRAY_SIZE(g_hns_roce_hw_attrs_list); i++) {
        ret = device_create_file(&hr_dev->ib_dev.dev,
                                 g_hns_roce_hw_attrs_list[i]);
        if (ret) {
            dev_err(hr_dev->dev, "register_sysfs failed!\n");
            goto err;
        }
    }

    return 0;

err:
    for (i = i - 1; i >= 0; i--) {
        device_remove_file(&hr_dev->ib_dev.dev,
                           g_hns_roce_hw_attrs_list[i]);
    }

    return ret;
}

void hns_roce_unregister_sysfs(struct hns_roce_dev *hr_dev)
{
    u32 i;

    for (i = 0; i < (u32)ARRAY_SIZE(g_hns_roce_hw_attrs_list); i++)
        device_remove_file(&hr_dev->ib_dev.dev,
                           g_hns_roce_hw_attrs_list[i]);
}
