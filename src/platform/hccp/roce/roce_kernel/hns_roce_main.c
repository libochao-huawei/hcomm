/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hns_roce_main.h"
#include "hns-abi.h"
#include <linux/acpi.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/version.h>
#ifdef HAVE_LINUX_MM_H
#include <linux/mm.h>
#else
#include <linux/sched/mm.h>
#endif
#ifdef HAVE_LINUX_SCHED_H
#include <linux/sched.h>
#else
#include <linux/sched/task.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <linux/sysfs.h>
#endif
#include <rdma/ib_addr.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_cache.h>
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include "hns_roce_hem.h"
#include "hns_roce_peer.h"
#include "hns_roce_sm.h"
#include "hns_roce_notify.h"
#include "hns_roce_sec.h"
#include "hns_roce_cdev.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

/**
 * hns_get_gid_index - Get gid index.
 * @hr_dev: pointer to structure hns_roce_dev.
 * @port:  port, value range: 0 ~ MAX
 * @gid_index:  gid_index, value range: 0 ~ MAX
 * Description:
 *    N ports shared gids, allocation method as follow:
 *      GID[0][0], GID[1][0],.....GID[N - 1][0],
 *      GID[0][0], GID[1][0],.....GID[N - 1][0],
 *      And so on
 */
int hns_get_gid_index(struct hns_roce_dev *hr_dev, u8 port, int gid_index)
{
    return gid_index * hr_dev->caps.num_ports + port;
}

static int hns_roce_set_mac(struct hns_roce_dev *hr_dev, u8 port, u8 *addr)
{
    u8 phy_port;
    u32 i = 0;

    if (!memcmp(hr_dev->dev_addr[port], addr, ETH_ALEN)) {
        return 0;
    }

    for (i = 0; i < ETH_ALEN; i++) {
        hr_dev->dev_addr[port][i] = addr[i];
    }

    phy_port = hr_dev->iboe.phy_port[port];
    return hr_dev->hw->set_mac(hr_dev, phy_port, addr);
}

#ifdef CONFIG_NEW_KERNEL
static int hns_roce_add_gid(const struct ib_gid_attr *attr, void **context)
{
    struct hns_roce_dev *hr_dev = NULL;
    u8 port;
    int ret;

    if (attr == NULL) {
        pr_err("hns3: add gid attr is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(attr->device);
    port = attr->port_num - 1;

    if (port >= hr_dev->caps.num_ports ||
            attr->index > hr_dev->caps.gid_table_len[port]) {
        dev_err(hr_dev->dev, "add gid failed. port - %d, index - %d\n",
                port, attr->index);
        return -EINVAL;
    }

    ret = hr_dev->hw->set_gid(hr_dev, port, attr->index, &attr->gid, attr);
    if (ret) {
        dev_err(hr_dev->dev, "set gid failed(%d), index = %d", ret,
                attr->index);
    }

    return ret;
}

STATIC int hns_roce_del_gid(const struct ib_gid_attr *attr, void **context)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct ib_gid_attr zattr = { };
    u8 port;
    int ret;

    if (attr == NULL) {
        pr_err("hns3: del gid param err, attr is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(attr->device);
    port = attr->port_num - 1;

    if (port >= hr_dev->caps.num_ports) {
        dev_err(hr_dev->dev,
                "Port num %d id large than max port num %d.\n",
                port, hr_dev->caps.num_ports);
        return -EINVAL;
    }

    ret = hr_dev->hw->set_gid(hr_dev, port, attr->index, &zgid, &zattr);
    if (ret) {
        dev_warn(hr_dev->dev, "del gid failed(%d), index = %d", ret,
                 attr->index);
    }

    return ret;
}
#else
static int hns_roce_add_gid(struct ib_device *device, u8 port_num,
                            unsigned int index, const union ib_gid *gid,
                            const struct ib_gid_attr *attr, void **context)
{
    struct hns_roce_dev *hr_dev = NULL;
    u8 port = port_num - 1;
    int ret;

    if (device == NULL || gid == NULL || attr == NULL || context == NULL) {
        pr_err("hns3: add gid param err, device %pK, gid %pK, attr %pK, context %pK\n",
            device, gid, attr, context);
        return -EINVAL;
    }
    hr_dev = to_hr_dev(device);
    if (port >= hr_dev->caps.num_ports ||
            index > hr_dev->caps.gid_table_len[port]) {
        dev_err(hr_dev->dev, "add gid failed. port - %d, index - %d\n",
                port, index);
        return -EINVAL;
    }

    ret = hr_dev->hw->set_gid(hr_dev, port, index, (union ib_gid *)gid,
                              attr);
    if (ret) {
        dev_err(hr_dev->dev, "set gid failed(%d), index = %d",
                ret, index);
    }

    return ret;
}

static int hns_roce_del_gid(struct ib_device *device, u8 port_num,
                            unsigned int index, void **context)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct ib_gid_attr zattr = { };
    union ib_gid zero_gid = { {0} };
    u8 port = port_num - 1;
    int ret;

    if (device == NULL) {
        pr_err("hns3: del gid device is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(device);
    if (port >= hr_dev->caps.num_ports) {
        dev_err(hr_dev->dev,
                "Port num %d id large than max port num %d.\n",
                port, hr_dev->caps.num_ports);
        return -EINVAL;
    }

    ret = hr_dev->hw->set_gid(hr_dev, port, index, &zero_gid, &zattr);
    if (ret) {
        dev_warn(hr_dev->dev, "del gid failed(%d), index = %d", ret,
                 index);
    }

    return ret;
}
#endif

static int handle_en_event(struct hns_roce_dev *hr_dev, u8 port,
                           unsigned long event)
{
    struct device *dev = hr_dev->dev;
    struct net_device *netdev = NULL;
    int ret = 0;

    netdev = hr_dev->iboe.netdevs[port];
    if (netdev == NULL) {
        dev_err(dev, "port(%d) can't find netdev\n", port);
        return -ENODEV;
    }

    switch (event) {
        case NETDEV_UP:
        case NETDEV_CHANGE:
        case NETDEV_REGISTER:
        case NETDEV_CHANGEADDR:
            ret = hns_roce_set_mac(hr_dev, port, (u8 *)netdev->dev_addr);
            if (ret)
                dev_err(dev, "set mac failed(%d), event = %lu\n",
                        ret, event);
            break;
        case NETDEV_DOWN:
            /*
             * In v1 engine, only support all ports closed together.
             */
            break;
        default:
            dev_dbg(dev, "NETDEV event = 0x%x!\n", (u32)(event));
            break;
    }

    return ret;
}

static int hns_roce_netdev_event(struct notifier_block *self,
                                 unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);
    struct hns_roce_ib_iboe *ib_iboe = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    u8 port;
    int ret;

    hr_dev = container_of(self, struct hns_roce_dev, iboe.nb);
    ib_iboe = &hr_dev->iboe;

    for (port = 0; port < hr_dev->caps.num_ports; port++) {
        if (dev == ib_iboe->netdevs[port]) {
            ret = handle_en_event(hr_dev, port, event);
            if (ret) {
                dev_err(hr_dev->dev, "handle en event[%lu] failed\n", event);
                return NOTIFY_DONE;
            }
            break;
        }
    }

    return NOTIFY_DONE;
}

static int hns_roce_setup_mtu_mac(struct hns_roce_dev *hr_dev)
{
    int ret;
    u8 i;

    for (i = 0; i < hr_dev->caps.num_ports; i++) {
        if (hr_dev->hw->set_mtu) {
            hr_dev->hw->set_mtu(hr_dev, hr_dev->iboe.phy_port[i], hr_dev->caps.max_mtu);
        }

        ret = hns_roce_set_mac(hr_dev, i,
                               (u8 *)hr_dev->iboe.netdevs[i]->dev_addr);
        if (ret) {
            dev_err(hr_dev->dev, "Port %d set mac failed(%d)\n",
                    i, ret);
            return ret;
        }
    }

    return 0;
}

STATIC void hns_roce_query_device_caps(struct hns_roce_dev *hr_dev, struct ib_device_attr *props)
{
    props->max_sge_rd = 1;
    props->max_cq = hr_dev->caps.num_cqs;
    props->max_cqe = hr_dev->caps.max_cqes;
    props->max_mr = hr_dev->caps.num_mtpts;
    props->max_pd = hr_dev->caps.num_pds;
    props->max_qp_rd_atom = hr_dev->caps.max_qp_dest_rdma;
    props->max_qp_init_rd_atom = hr_dev->caps.max_qp_init_rdma;
    props->atomic_cap = hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_ATOMIC ? IB_ATOMIC_HCA : IB_ATOMIC_NONE;
    props->max_pkeys = 1;
    props->local_ca_ack_delay = hr_dev->caps.local_ca_ack_delay;

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SRQ) {
        props->max_srq = hr_dev->caps.max_srqs;
        props->max_srq_wr = hr_dev->caps.max_srq_wrs;
        props->max_srq_sge = hr_dev->caps.max_srq_sges;
    }

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_MW) {
        props->max_mw = hr_dev->caps.num_mtpts;
        props->device_cap_flags |= IB_DEVICE_MEM_WINDOW | IB_DEVICE_MEM_WINDOW_TYPE_2B;
    }

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_FRMR) {
        props->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
        props->max_fast_reg_page_list_len = HNS_ROCE_FRMR_MAX_PA;
    }
}

STATIC int hns_roce_query_device(struct ib_device *ib_dev, struct ib_device_attr *props, struct ib_udata *uhw)
{
    int ret;
    struct hns_roce_dev *hr_dev = NULL;

    if (ib_dev == NULL || props == NULL) {
        pr_err("hns3: query device param err, ib_dev %pK, props %pK\n", ib_dev, props);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ib_dev);

    ret = memset_s(props, sizeof(*props), 0, sizeof(*props));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    props->fw_ver = hr_dev->caps.fw_ver;
    props->sys_image_guid = cpu_to_be64(hr_dev->sys_image_guid);
    props->max_mr_size = (u64)(~(0ULL));
    props->page_size_cap = hr_dev->caps.page_size_cap;
    props->vendor_id = hr_dev->vendor_id;
    props->vendor_part_id = hr_dev->vendor_part_id;
    props->hw_ver = hr_dev->hw_rev;
    props->max_qp = hr_dev->caps.num_qps;
    props->max_qp_wr = hr_dev->caps.max_wqes;
    props->device_cap_flags = IB_DEVICE_PORT_ACTIVE_EVENT | IB_DEVICE_RC_RNR_NAK_GEN;
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC) {
        props->device_cap_flags |= IB_DEVICE_XRC;
    }
    props->max_send_sge = hr_dev->caps.max_sq_sg;
    props->max_recv_sge = hr_dev->caps.max_rq_sg;
    hns_roce_query_device_caps(hr_dev, props);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC struct net_device *hns_roce_get_netdev(struct ib_device *ib_dev,
                                              u32 port_num)
#else
STATIC struct net_device *hns_roce_get_netdev(struct ib_device *ib_dev,
                                              u8 port_num)
#endif
{
    struct hns_roce_dev *hr_dev = NULL;
    struct net_device *ndev = NULL;

    if (ib_dev == NULL) {
        pr_err("hns3: get netdev param err, ib_dev is NULL\n");
        return NULL;
    }

    hr_dev = to_hr_dev(ib_dev);
    if (port_num < 1 || port_num > hr_dev->caps.num_ports) {
        dev_err(hr_dev->dev, "invalid port_num[%d], valid value should be in range[1, %d]\n",
            port_num, hr_dev->caps.num_ports);
        return NULL;
    }

    rcu_read_lock();

    ndev = hr_dev->iboe.netdevs[port_num - 1];
    if (ndev != NULL) {
        dev_hold(ndev);
    }

    rcu_read_unlock();
    return ndev;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int hns_roce_query_port(struct ib_device *ib_dev, u32 port_num,
                               struct ib_port_attr *props)
#else
static int hns_roce_query_port(struct ib_device *ib_dev, u8 port_num,
                               struct ib_port_attr *props)
#endif
{
    struct hns_roce_dev *hr_dev = NULL;
    struct device *dev = NULL;
    struct net_device *net_dev = NULL;
    unsigned long flags;
    enum ib_mtu mtu;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    u32 port;
#else
    u8 port;
#endif

    if (ib_dev == NULL || props == NULL) {
        pr_err("hns3: query port param err, ib_dev %pK, props %pK\n", ib_dev, props);
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ib_dev);
    dev = hr_dev->dev;

    if (port_num > HNS_ROCE_MAX_PORTS || port_num < 1) {
        pr_err("hns3: query port gets invalid port num %u, invalid range[1, %d]\n", port_num, HNS_ROCE_MAX_PORTS);
        return -EINVAL;
    }
    port = port_num - 1;

    /* props being zeroed by the caller, avoid zeroing it here */
    props->max_mtu = hr_dev->caps.max_mtu;
    props->gid_tbl_len = hr_dev->caps.gid_table_len[port];
    props->port_cap_flags = IB_PORT_CM_SUP | IB_PORT_REINIT_SUP |
                            IB_PORT_VENDOR_CLASS_SUP |
                            IB_PORT_BOOT_MGMT_SUP;
    props->max_msg_sz = HNS_ROCE_MAX_MSG_LEN;
    props->pkey_tbl_len = 1;
    props->active_width = IB_WIDTH_4X;
    props->active_speed = 1;

    spin_lock_irqsave(&hr_dev->iboe.lock, flags);

    net_dev = hr_dev->iboe.netdevs[port];
    if (net_dev == NULL) {
        spin_unlock_irqrestore(&hr_dev->iboe.lock, flags);
        dev_err(dev, "find netdev %d failed!\r\n", port);
        return -EINVAL;
    }

    mtu = iboe_get_mtu(net_dev->mtu);
    props->active_mtu = mtu ? min(props->max_mtu, mtu) : IB_MTU_256;
    props->state = (netif_running(net_dev) && netif_carrier_ok(net_dev)) ?
                   IB_PORT_ACTIVE : IB_PORT_DOWN;
    props->phys_state = (props->state == IB_PORT_ACTIVE) ?
                        HNS_ROCE_PHY_LINKUP : HNS_ROCE_PHY_DISABLED;

    spin_unlock_irqrestore(&hr_dev->iboe.lock, flags);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC enum rdma_link_layer hns_roce_get_link_layer(struct ib_device *device,
                                                    u32 port_num)
#else
STATIC enum rdma_link_layer hns_roce_get_link_layer(struct ib_device *device,
                                                    u8 port_num)
#endif
{
    if (device == NULL) {
        pr_warn("hns3: get link layer param err, device is NULL\n");
    }

    return IB_LINK_LAYER_ETHERNET;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC int hns_roce_query_pkey(struct ib_device *ib_dev, u32 port, u16 index,
                               u16 *pkey)
#else
STATIC int hns_roce_query_pkey(struct ib_device *ib_dev, u8 port, u16 index,
                               u16 *pkey)
#endif
{
    if (ib_dev == NULL || pkey == NULL) {
        pr_warn("hns3: query pkey param err, ib_dev %pK, pkey %pK\n", ib_dev, pkey);
        return -EINVAL;
    }

    *pkey = PKEY_ID;

    return 0;
}

STATIC int hns_roce_modify_device(struct ib_device *ib_dev, int mask, struct ib_device_modify *props)
{
    unsigned long flags;
    struct hns_roce_dev *hr_dev = NULL;
    struct device *dev = NULL;
    int ret;

    if (ib_dev == NULL || props == NULL) {
        pr_err("hns3: modify device param err, ib_dev %pK, props %pK\n", ib_dev, props);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ib_dev);
    dev = hr_dev->dev;
    if ((unsigned int)mask & ~IB_DEVICE_MODIFY_NODE_DESC) {
        dev_err(dev, "mask check failedi!\n");
        return -EOPNOTSUPP;
    }

    if ((unsigned int)mask & IB_DEVICE_MODIFY_NODE_DESC) {
        spin_lock_irqsave(&to_hr_dev(ib_dev)->sm_lock, flags);
#ifndef DEFINE_HNS_LLT
        ret = memcpy_s(ib_dev->node_desc, NODE_DESC_SIZE,
                       props->node_desc, NODE_DESC_SIZE);
        if (ret) {
            dev_err(dev, "copy [%d] bytes from props node_desc to ib_dev node_desc failed\n", NODE_DESC_SIZE);
            spin_unlock_irqrestore(&to_hr_dev(ib_dev)->sm_lock, flags);
            return ret;
        }
#endif
        spin_unlock_irqrestore(&to_hr_dev(ib_dev)->sm_lock, flags);
    }

    return 0;
}

#if defined(HNS_ROCE_DEVICE) && !defined(CONFIG_PLATFORM_ESL)
extern int devdrv_get_notify_base_addr(u32 devid, u64 *dev_phy_addr);
#else
STATIC int devdrv_get_notify_base_addr(u32 devid, u64 *dev_phy_addr)
{
    *dev_phy_addr = HNS_ROCE_NOTIFY_BASE_ADDR;
    return 0;
}
#endif

int hns_roce_get_notify_base_addr(struct hns_roce_dev *hr_dev, u64 *phy_addr, u64 *size)
{
    int ret;
    int dev_id;
    u64 dev_phy_addr = 0;

    dev_notice(hr_dev->dev, "devdrv_get_notify_base_addr enter! \n");

    dev_id = hns_roce_get_devid(hr_dev);
    if (dev_id < 0) {
        dev_err(hr_dev->dev, "get err dev_id[%d] \n", dev_id);
        return -EINVAL;
    }
    ret = devdrv_get_notify_base_addr(dev_id, &dev_phy_addr);
    if (ret) {
        dev_err(hr_dev->dev, "devdrv_get_notify_base_addr fail! dev_id:%u ret:0x%x \n",
                dev_id, ret);
        return ret;
    }

    *phy_addr = dev_phy_addr;
    *size = PGSZ_BASE * PAGE_SIZE;
    return 0;
}

STATIC void hns_roce_ucontext_list_mutex_init(struct hns_roce_dev *hr_dev, struct hns_roce_ucontext *context)
{
    INIT_LIST_HEAD(&context->vma_list);
    mutex_init(&context->vma_list_mutex);
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
        INIT_LIST_HEAD(&context->page_list);
        mutex_init(&context->page_mutex);
    }
}

STATIC void hns_roce_ucontext_list_mutex_deinit(struct hns_roce_dev *hr_dev, struct hns_roce_ucontext *context)
{
    mutex_destroy(&context->vma_list_mutex);
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
        mutex_destroy(&context->page_mutex);
    }
}

static int hns_roce_init_ib_alloc_ucontext_resp(struct hns_roce_dev *hr_dev,
    struct hns_roce_ib_alloc_ucontext_resp *resp)
{
    u64 phy_addr, size;
    int chip_id;
    int ret;

    resp->qp_tab_size = hr_dev->caps.num_qps;
    resp->port_num = hr_dev->port_num;
    resp->port_id = hr_dev->port_id;
    chip_id = hns_roce_get_devid(hr_dev);
    if (chip_id == -1) {
        dev_err(hr_dev->dev, "hns_roce_get_devid failed.\n");
        return -EINVAL;
    }
    resp->chip_id = chip_id;

    // notify
    ret = hns_roce_get_notify_base_addr(hr_dev, &phy_addr, &size);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce get notify base addr failed, ret：%d\n", ret);
        return ret;
    }
    resp->notify_pa = phy_addr;
    resp->notify_size = size;

    return ret;
}

STATIC int hns_roce_alloc_ucontext(struct ib_ucontext *uctx, struct ib_udata *udata)
{
    int ret;
    struct hns_roce_ucontext *context = NULL;
    struct hns_roce_ib_alloc_ucontext_resp resp = {};
    struct hns_roce_dev *hr_dev = NULL;

    if (uctx == NULL || udata == NULL) {
        pr_err("hns3: alloc ucontext param err, uctx %pK, udata %pK\n", uctx, udata);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(uctx->device);
    if (!hr_dev->active) {
        dev_err(hr_dev->dev, "current device is not active\n");
        return -EAGAIN;
    }

    ret = hns_roce_init_ib_alloc_ucontext_resp(hr_dev, &resp);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_init_ib_alloc_ucontext_resp failed, ret:%d\n", ret);
        return -EAGAIN;
    }

    context = to_hr_ucontext(uctx);
    ret = hns_roce_uar_alloc(hr_dev, &context->uar);
    if (ret) {
        dev_err(hr_dev->dev, "alloc hns_roce uar failed, %d\n", ret);
        return ret;
    }

    hns_roce_ucontext_list_mutex_init(hr_dev,  context);
    ret = ib_copy_to_udata(udata, &resp, sizeof(resp));
    if (ret) {
        dev_err(hr_dev->dev, "ib copy [%lu] bytes to udata failed\n", sizeof(resp));
        hns_roce_ucontext_list_mutex_deinit(hr_dev, context);
        hns_roce_uar_free(hr_dev, &context->uar);
        return ret;
    }

    return 0;
}

STATIC void hns_roce_dealloc_ucontext(struct ib_ucontext *ibcontext)
{
    struct hns_roce_ucontext *context = NULL;

    if (ibcontext == NULL) {
        pr_err("hns3: dealloc ucontext is NULL\n");
        return;
    }

    context = to_hr_ucontext(ibcontext);

    hns_roce_ucontext_list_mutex_deinit(to_hr_dev(ibcontext->device), context);
    hns_roce_uar_free(to_hr_dev(ibcontext->device), &context->uar);
    hns_roce_notify_del(&context->notify_node);
}

static void hns_roce_vma_open(struct vm_area_struct *vma)
{
    vma->vm_ops = NULL;
}

static void hns_roce_vma_close(struct vm_area_struct *vma)
{
    struct hns_roce_vma_data *vma_data;

    vma_data = (struct hns_roce_vma_data *)vma->vm_private_data;
    vma_data->vma = NULL;
    mutex_lock(vma_data->vma_list_mutex);
    list_del(&vma_data->list);
    mutex_unlock(vma_data->vma_list_mutex);
    kfree(vma_data);
    vma_data = NULL;
}

// prevent PUAF issues
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
static int hns_roce_vma_mremap(struct vm_area_struct *vma)
{
    pr_err("mremap not support\n");
	return -EOPNOTSUPP;
}
#endif

static const struct vm_operations_struct g_hns_roce_vm_ops = {
    .open = hns_roce_vma_open,
    .close = hns_roce_vma_close,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
    .mremap = hns_roce_vma_mremap,
#endif
};

static int hns_roce_set_vma_data(struct vm_area_struct *vma,
                                 struct hns_roce_ucontext *context)
{
    struct list_head *vma_head = &context->vma_list;
    struct hns_roce_vma_data *vma_data = NULL;

    vma_data = kzalloc(sizeof(*vma_data), GFP_KERNEL);
    if (vma_data == NULL) {
        pr_err("hns3: kzalloc [%lu] bytes for hns_roce vma_data failed\n", sizeof(*vma_data));
        return -ENOMEM;
    }

    vma_data->vma = vma;
    vma_data->vma_list_mutex = &context->vma_list_mutex;
    vma->vm_private_data = vma_data;
    vma->vm_ops = &g_hns_roce_vm_ops;

    mutex_lock(&context->vma_list_mutex);
    list_add(&vma_data->list, vma_head);
    mutex_unlock(&context->vma_list_mutex);

    return 0;
}

STATIC int hns_roce_mmap_check(struct ib_ucontext *context,
    struct vm_area_struct *vma)
{
    if ((context == NULL) || (vma == NULL)) {
        pr_err("hns3: null point:context %pK, vma %pK\n", context, vma);
        return -EINVAL;
    }

    if (((vma->vm_end - vma->vm_start) % PAGE_SIZE) != 0) {
        pr_err("hns3: vma->vm_end[%lu] - vma->vm_start[%lu]) %% PAGE_SIZE[%lu]) != 0, expect equal to 0\n",
            vma->vm_end, vma->vm_start, PAGE_SIZE);
        return -EINVAL;
    }

    return 0;
}

STATIC int hns_roce_mmap_notify_priv(struct hns_roce_dev *hr_dev,
    struct vm_area_struct *vma, struct ib_ucontext *context)
{
    int ret;
    u64 phy_addr;
    u64 size;
    struct hns_roce_ucontext *hr_context = to_hr_ucontext(context);

    // notify 2 page
    ret = hns_roce_get_notify_base_addr(hr_dev, &phy_addr, &size);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce get notify base addr failed\n");
        return -EAGAIN;
    }

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    /* vm_pgoff: 1 -- gdr notify buffer */
    if (io_remap_pfn_range(vma, vma->vm_start, phy_addr >> PAGE_SHIFT, size, vma->vm_page_prot)) {
        dev_err(hr_dev->dev, "remap pfn range failed, vm_start[%lu], page size[%llu\n]", vma->vm_start, size);
        return -EAGAIN;
    }

    hr_context->notify_node.va = vma->vm_start;
    hr_context->notify_node.pa = phy_addr;
    hr_context->notify_node.sz = size;
    if (hns_roce_notify_add(&hr_context->notify_node)) {
        dev_err(hr_dev->dev, "hns_roce_notify_add fail");
        return -EINVAL;
    }

    return 0;
}

STATIC int hns_roce_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
    int ret;
    struct hns_roce_dev *hr_dev = NULL;

    if (hns_roce_mmap_check(context, vma)) {
        pr_err("hns3: roce mmap fail, invalid para\n");
        return -EINVAL;
    }
    hr_dev = to_hr_dev(context->device);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    vm_flags_set(vma, VM_DONTCOPY | VM_DONTEXPAND | VM_WIPEONFORK | VM_IO);
#else
    vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_WIPEONFORK | VM_IO;
#endif
    if (vma->vm_pgoff == 0) {
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        if (io_remap_pfn_range(vma, vma->vm_start, to_hr_ucontext(context)->uar.pfn, PAGE_SIZE, vma->vm_page_prot)) {
            dev_err(hr_dev->dev, "remap pfn range failed, vm_start[%lu], page size[%lu\n]", vma->vm_start, PAGE_SIZE);
            return -EAGAIN;
        }
    } else if (vma->vm_pgoff == 1 && hr_dev->notify_priv) {
        // notify 2 page
        ret = hns_roce_mmap_notify_priv(hr_dev, vma, context);
        if (ret) {
            dev_err(hr_dev->dev, "remap notify priv failed, %d", ret);
            return ret;
        }
#ifndef DEFINE_HNS_LLT
    } else if (vma->vm_pgoff == 1 && hr_dev->uar2_dma_addr && hr_dev->uar2_size) {
        if (io_remap_pfn_range(vma, vma->vm_start, hr_dev->uar2_dma_addr >> PAGE_SHIFT,
                               hr_dev->uar2_size, vma->vm_page_prot)) {
            dev_err(hr_dev->dev, "remap pfn range failed, vm_start[%lu], page size[%u\n]",
                vma->vm_start, hr_dev->uar2_size);
            return -EAGAIN;
        }
#endif
    } else if (vma->vm_pgoff == VM_PGOFF_VAL && hns_roce_get_shared_mem_base(hr_dev)) {
        dev_notice(hr_dev->dev, "mmap share memory enter!\n");
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

        /* vm_pgoff: 1 -- gdr share buffer */
        if (io_remap_pfn_range(vma, vma->vm_start,
                               hns_roce_get_shared_mem_base(hr_dev) >> PAGE_SHIFT,
                               hns_roce_get_shared_mem_size(hr_dev), vma->vm_page_prot)) {
            dev_err(hr_dev->dev, "remap pfn range failed, vm_start[%lu], page size[%d\n]",
                vma->vm_start, hns_roce_get_shared_mem_size(hr_dev));
            return -EAGAIN;
        }
    } else {
        dev_err(hr_dev->dev, "invalid vm_pgoff[%lu]\n", vma->vm_pgoff);
        return -EINVAL;
    }

    return hns_roce_set_vma_data(vma, to_hr_ucontext(context));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC int hns_roce_port_immutable(struct ib_device *ib_dev, u32 port_num,
                                   struct ib_port_immutable *immutable)
#else
STATIC int hns_roce_port_immutable(struct ib_device *ib_dev, u8 port_num,
                                   struct ib_port_immutable *immutable)
#endif
{
    struct ib_port_attr attr;
    int ret;

    if (ib_dev == NULL || immutable == NULL) {
        pr_err("hns3: port_immutable param err, ib_dev %pK, immutable %pK\n", ib_dev, immutable);
        return -EINVAL;
    }

    ret = ib_query_port(ib_dev, port_num, &attr);
    if (ret) {
        pr_err("hns3: query port err,[%d]", ret);
        return ret;
    }

    immutable->pkey_tbl_len = attr.pkey_tbl_len;
    immutable->gid_tbl_len = attr.gid_tbl_len;

    immutable->max_mad_size = IB_MGMT_MAD_SIZE;
    immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE;
    if (to_hr_dev(ib_dev)->caps.flags & HNS_ROCE_CAP_FLAG_ROCE_V1_V2) {
        immutable->core_cap_flags |= RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;
    }

    return 0;
}

STATIC void hns_roce_disassociate_ucontext(struct ib_ucontext *ibcontext)
{
    struct hns_roce_ucontext *context = NULL;
#ifndef DEFINE_HNS_LLT
    struct hns_roce_vma_data *vma_data = NULL;
    struct hns_roce_vma_data *n = NULL;
    struct vm_area_struct *vma = NULL;

    if (ibcontext == NULL) {
        pr_err("hns3: disassociate_ucontext param err, ibcontext is NULL\n");
        return ;
    }
    context = to_hr_ucontext(ibcontext);

    mutex_lock(&context->vma_list_mutex);
    list_for_each_entry_safe(vma_data, n, &context->vma_list, list) {
        vma = vma_data->vma;
        zap_vma_ptes(vma, vma->vm_start, PAGE_SIZE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
        vma->__vm_flags &= ~(VM_SHARED | VM_MAYSHARE);
#else
        vma->vm_flags &= ~(VM_SHARED | VM_MAYSHARE);
#endif
        vma->vm_ops = NULL;
        list_del(&vma_data->list);
        kfree(vma_data);
        vma_data = NULL;
    }
    mutex_unlock(&context->vma_list_mutex);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
enum hns_roce_hw_stats_index {
    PD_ALLOC_STATS_INDEX = 0,
    PD_DEALLOC_STATS_INDEX,
    MR_ALLOC_STATS_INDEX,
    MR_DEALLOC_STATS_INDEX,
    CQ_ALLOC_STATS_INDEX,
    CQ_DEALLOC_STATS_INDEX,
    QP_ALLOC_STATS_INDEX,
    QP_DEALLOC_STATS_INDEX,
    PD_ACTIVE_STATS_INDEX,
    MR_ACTIVE_STATS_INDEX,
    CQ_ACTIVE_STATS_INDEX,
    QP_ACTIVE_STATS_INDEX,
    AEQE_STATS_INDEX,
    CEQE_STATS_INDEX,
};

static const struct rdma_stat_desc hns_roce_hw_stats_name[] = {
    [PD_ALLOC_STATS_INDEX].name = "pd_alloc",
    [PD_DEALLOC_STATS_INDEX].name = "pd_dealloc",
    [MR_ALLOC_STATS_INDEX].name = "mr_alloc",
    [MR_DEALLOC_STATS_INDEX].name = "mr_dealloc",
    [CQ_ALLOC_STATS_INDEX].name = "cq_alloc",
    [CQ_DEALLOC_STATS_INDEX].name = "cq_dealloc",
    [QP_ALLOC_STATS_INDEX].name = "qp_alloc",
    [QP_DEALLOC_STATS_INDEX].name = "qp_dealloc",
    [PD_ACTIVE_STATS_INDEX].name = "pd_active",
    [MR_ACTIVE_STATS_INDEX].name = "mr_active",
    [CQ_ACTIVE_STATS_INDEX].name = "cq_active",
    [QP_ACTIVE_STATS_INDEX].name = "qp_active",
    [AEQE_STATS_INDEX].name = "aeqe",
    [CEQE_STATS_INDEX].name = "ceqe",
};

/* In order to solve the kernel upgrade compilation problem, the following definition is added.
 * The following structure definition comes from kernel source file: drivers/infiniband/core/sysfs.c
 */
struct hw_stats_device_attribute {
    struct device_attribute attr;
    ssize_t (*show)(struct ib_device *ibdev, struct rdma_hw_stats *stats,
            unsigned int index, unsigned int port_num, char *buf);
    ssize_t (*store)(struct ib_device *ibdev, struct rdma_hw_stats *stats,
            unsigned int index, unsigned int port_num,
            const char *buf, size_t count);
};

struct hw_stats_device_data {
	struct attribute_group group;
	struct rdma_hw_stats *stats;
	struct hw_stats_device_attribute attrs[];
};
/* end */

void hns_roce_inc_rdma_hw_stats(struct ib_device *dev, int stats)
{
    if (dev->hw_stats_data && dev->hw_stats_data->stats)
        dev->hw_stats_data->stats->value[stats]++;
}

STATIC int hns_roce_get_rdma_hw_stats_value(struct ib_device *dev, int index)
{
    if (dev->hw_stats_data && dev->hw_stats_data->stats)
        return dev->hw_stats_data->stats->value[index];
 
    return 0;
}

int hns_roce_get_hw_stats_num_counters(struct ib_device *dev)
{
    if (dev->hw_stats_data && dev->hw_stats_data->stats)
        return dev->hw_stats_data->stats->num_counters;
 
    return 0;
}

struct rdma_hw_stats *hns_roce_get_net_hw_stats(struct ib_device *dev)
{
    if (dev->hw_stats_data)
    return dev->hw_stats_data->stats;
 
    return NULL;
}
#else
static const char * const hns_roce_hw_stats_name[] = {
    "pd_alloc",
    "pd_dealloc",
    "mr_alloc",
    "mr_dealloc",
    "cq_alloc",
    "cq_dealloc",
    "qp_alloc",
    "qp_dealloc",
    "pd_active",
    "mr_active",
    "cq_active",
    "qp_active",
    "aeqe",
    "ceqe",
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC struct rdma_hw_stats *hns_roce_alloc_hw_stats(struct ib_device *device, u32 port_num)
#else
STATIC struct rdma_hw_stats *hns_roce_alloc_hw_stats(struct ib_device *device, u8 port_num)
#endif
{
    BUILD_BUG_ON(ARRAY_SIZE(hns_roce_hw_stats_name) != HW_STATS_TOTAL);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    struct hns_roce_dev *hr_dev = to_hr_dev(device);

    if (port_num > hr_dev->caps.num_ports) {
        ibdev_err(device, "invalid port num[%u], caps.num_ports[%u]\n", port_num, hr_dev->caps.num_ports);
        return NULL;
    }
#else
    if (port_num != 0)
        return NULL; /* nothing to do for port */
#endif

    return rdma_alloc_hw_stats_struct(hns_roce_hw_stats_name, ARRAY_SIZE(hns_roce_hw_stats_name),
                                      RDMA_HW_STATS_DEFAULT_LIFESPAN);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC int hns_roce_get_hw_stats(struct ib_device *device, struct rdma_hw_stats *stats,
                                 u32 port, int index)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(device);

     /* the port value starts from 1, return 0 will cause WARN_ON */
    if (port == 0)
        return 0;

    if (port > hr_dev->caps.num_ports)
        return -EINVAL;

    switch (index) {
        case HW_STATS_PD_ALLOC:
        case HW_STATS_PD_DEALLOC:
        case HW_STATS_MR_ALLOC:
        case HW_STATS_MR_DEALLOC:
        case HW_STATS_CQ_ALLOC:
        case HW_STATS_CQ_DEALLOC:
        case HW_STATS_QP_ALLOC:
            /* fallthrough; */
        case HW_STATS_QP_DEALLOC:
            stats->value[index] = hns_roce_get_rdma_hw_stats_value(device, index);
            break;
        case HW_STATS_PD_ACTIVE:
            stats->value[index] = hns_roce_get_rdma_hw_stats_value(device, HW_STATS_PD_ALLOC) -
                hns_roce_get_rdma_hw_stats_value(device, HW_STATS_PD_DEALLOC);
            break;
        case HW_STATS_MR_ACTIVE:
            stats->value[index] = hns_roce_get_rdma_hw_stats_value(device, HW_STATS_MR_ALLOC) -
                hns_roce_get_rdma_hw_stats_value(device, HW_STATS_MR_DEALLOC);
            break;
        case HW_STATS_CQ_ACTIVE:
            stats->value[index] = hns_roce_get_rdma_hw_stats_value(device, HW_STATS_CQ_ALLOC) -
                        hns_roce_get_rdma_hw_stats_value(device, HW_STATS_CQ_DEALLOC);
            break;
        case HW_STATS_QP_ACTIVE:
            stats->value[index] = hns_roce_get_rdma_hw_stats_value(device, HW_STATS_QP_ALLOC) -
                hns_roce_get_rdma_hw_stats_value(device, HW_STATS_QP_DEALLOC);
            break;
        case HW_STATS_AEQE:
            stats->value[index] = hr_dev->dfx_cnt[HNS_ROCE_DFX_AEQE];
            break;
        case HW_STATS_CEQE:
            stats->value[index] = hr_dev->dfx_cnt[HNS_ROCE_DFX_CEQE];
            break;
        default:
            break;
    }

    return index;
}
#else
STATIC int hns_roce_get_hw_stats(struct ib_device *device, struct rdma_hw_stats *stats,
                                 u8 port, int index)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(device);

    if (port != 0)
        return 0; /* nothing to do for port */

    switch (index) {
        case HW_STATS_PD_ALLOC:
        case HW_STATS_PD_DEALLOC:
        case HW_STATS_MR_ALLOC:
        case HW_STATS_MR_DEALLOC:
        case HW_STATS_CQ_ALLOC:
        case HW_STATS_CQ_DEALLOC:
        case HW_STATS_QP_ALLOC:
            /* fallthrough; */
        case HW_STATS_QP_DEALLOC:
            stats->value[index] = device->hw_stats->value[index];
            break;
        case HW_STATS_PD_ACTIVE:
            stats->value[index] = device->hw_stats->value[HW_STATS_PD_ALLOC] -
                device->hw_stats->value[HW_STATS_PD_DEALLOC];
            break;
        case HW_STATS_MR_ACTIVE:
            stats->value[index] = device->hw_stats->value[HW_STATS_MR_ALLOC] -
                device->hw_stats->value[HW_STATS_MR_DEALLOC];
            break;
        case HW_STATS_CQ_ACTIVE:
            stats->value[index] = device->hw_stats->value[HW_STATS_CQ_ALLOC] -
                        device->hw_stats->value[HW_STATS_CQ_DEALLOC];
            break;
        case HW_STATS_QP_ACTIVE:
            stats->value[index] = device->hw_stats->value[HW_STATS_QP_ALLOC] -
                device->hw_stats->value[HW_STATS_QP_DEALLOC];
            break;
        case HW_STATS_AEQE:
            stats->value[index] = hr_dev->dfx_cnt[HNS_ROCE_DFX_AEQE];
            break;
        case HW_STATS_CEQE:
            stats->value[index] = hr_dev->dfx_cnt[HNS_ROCE_DFX_CEQE];
            break;
        default:
            break;
    }

    return index;
}
#endif

static void hns_roce_unregister_device(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_ib_iboe *iboe = &hr_dev->iboe;
    int ret;

    hr_dev->active = false;
    ret = unregister_netdevice_notifier(&iboe->nb);
    if (ret) {
        pr_err("unregister netdevice notifier failed, %d\n", ret);
    }

    roce_remove_cdev(hr_dev);
    ib_unregister_device(&hr_dev->ib_dev);
}

STATIC const struct ib_device_ops hns_roce_dev_ops = {
    .owner = THIS_MODULE,
    .driver_id = RDMA_DRIVER_HNS,
    .uverbs_abi_ver = 1,
    .uverbs_no_driver_id_binding = 1,

    .add_gid = hns_roce_add_gid,
    .alloc_pd = hns_roce_alloc_pd,
    .alloc_ucontext = hns_roce_alloc_ucontext,
    .create_ah = hns_roce_create_ah,
    .create_cq = hns_roce_ib_create_cq,
    .create_qp = hns_roce_create_qp,
    .dealloc_pd = hns_roce_dealloc_pd,
    .dealloc_ucontext = hns_roce_dealloc_ucontext,
    .del_gid = hns_roce_del_gid,
    .dereg_mr = hns_roce_dereg_mr,
    .destroy_ah = hns_roce_destroy_ah,
    .destroy_cq = hns_roce_ib_destroy_cq,
    .disassociate_ucontext = hns_roce_disassociate_ucontext,
    // .fill_res_cq_entry = hns_roce_fill_res_cq_entry,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    .alloc_hw_port_stats = hns_roce_alloc_hw_stats,
#else
    .alloc_hw_stats = hns_roce_alloc_hw_stats,
#endif
    .get_hw_stats = hns_roce_get_hw_stats,
    .get_dma_mr = hns_roce_get_dma_mr,
    .get_link_layer = hns_roce_get_link_layer,
    .get_port_immutable = hns_roce_port_immutable,
    .mmap = hns_roce_mmap,
    // .mmap_free = hns_roce_free_mmap,
    .modify_device = hns_roce_modify_device,
    .modify_qp = hns_roce_modify_qp,
    .query_ah = hns_roce_query_ah,
    .query_device = hns_roce_query_device,
    .query_pkey = hns_roce_query_pkey,
    .query_port = hns_roce_query_port,
    .get_netdev = hns_roce_get_netdev,
    .reg_user_mr = hns_roce_reg_user_mr,

    INIT_RDMA_OBJ_SIZE(ib_ah, hns_roce_ah, ibah),
    INIT_RDMA_OBJ_SIZE(ib_cq, hns_roce_cq, ib_cq),
    INIT_RDMA_OBJ_SIZE(ib_pd, hns_roce_pd, ibpd),
    INIT_RDMA_OBJ_SIZE(ib_ucontext, hns_roce_ucontext, ibucontext),
};

static const struct ib_device_ops hns_roce_dev_mr_ops = {
    .rereg_user_mr = hns_roce_rereg_user_mr,
};

static const struct ib_device_ops hns_roce_dev_mw_ops = {
    .alloc_mw = hns_roce_alloc_mw,
    .dealloc_mw = hns_roce_dealloc_mw,

    INIT_RDMA_OBJ_SIZE(ib_mw, hns_roce_mw, ibmw),
};

static const struct ib_device_ops hns_roce_dev_frmr_ops = {
    .alloc_mr = hns_roce_alloc_mr,
    .map_mr_sg = hns_roce_map_mr_sg,
};

static const struct ib_device_ops hns_roce_dev_srq_ops = {
    .create_srq = hns_roce_create_srq,
    .destroy_srq = hns_roce_destroy_srq,

    INIT_RDMA_OBJ_SIZE(ib_srq, hns_roce_srq, ibsrq),
};

static const struct ib_device_ops hns_roce_dev_xrcd_ops = {
    .alloc_xrcd = hns_roce_ib_alloc_xrcd,
    .dealloc_xrcd = hns_roce_ib_dealloc_xrcd,

    INIT_RDMA_OBJ_SIZE(ib_xrcd, hns_roce_xrcd, ibxrcd),
};

static void hns_roce_register_device_attr(struct ib_device *ib_dev, struct hns_roce_dev *hr_dev)
{
    struct device *dev = hr_dev->dev;
    ib_dev->node_type       = RDMA_NODE_IB_CA;
    ib_dev->dev.parent      = dev;

    ib_dev->phys_port_cnt       = hr_dev->caps.num_ports;
    ib_dev->local_dma_lkey      = hr_dev->caps.reserved_lkey;
    ib_dev->num_comp_vectors    = hr_dev->caps.num_comp_vectors;
    ib_dev->uverbs_cmd_mask     =
        (1ULL << IB_USER_VERBS_CMD_GET_CONTEXT) | (1ULL << IB_USER_VERBS_CMD_QUERY_DEVICE) |
        (1ULL << IB_USER_VERBS_CMD_QUERY_PORT) | (1ULL << IB_USER_VERBS_CMD_ALLOC_PD) |
        (1ULL << IB_USER_VERBS_CMD_DEALLOC_PD) | (1ULL << IB_USER_VERBS_CMD_REG_MR) |
        (1ULL << IB_USER_VERBS_CMD_DEREG_MR) | (1ULL << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
        (1ULL << IB_USER_VERBS_CMD_CREATE_CQ) | (1ULL << IB_USER_VERBS_CMD_DESTROY_CQ) |
        (1ULL << IB_USER_VERBS_CMD_CREATE_QP) | (1ULL << IB_USER_VERBS_CMD_MODIFY_QP) |
        (1ULL << IB_USER_VERBS_CMD_QUERY_QP) | (1ULL << IB_USER_VERBS_CMD_DESTROY_QP) |
        (1ULL << IB_USER_VERBS_CMD_CREATE_SRQ) | (1ULL << IB_USER_VERBS_CMD_MODIFY_SRQ) |
        (1ULL << IB_USER_VERBS_CMD_QUERY_SRQ) | (1ULL << IB_USER_VERBS_CMD_DESTROY_SRQ) |
        (1ULL << IB_USER_VERBS_CMD_POST_SRQ_RECV) | (1ULL << IB_USER_VERBS_CMD_CREATE_XSRQ);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
#ifdef MODIFY_CQ_MASK
    ib_dev->uverbs_ex_cmd_mask |= (1ULL << IB_USER_VERBS_EX_CMD_MODIFY_CQ);
#endif
#endif
}

static void hns_roce_register_device_srq(struct ib_device *ib_dev, struct hns_roce_dev *hr_dev)
{
    ib_set_device_ops(ib_dev, &hns_roce_dev_srq_ops);
    ib_set_device_ops(ib_dev, hr_dev->hw->hns_roce_dev_srq_ops);
}

static void hns_roce_register_device_mr(struct ib_device *ib_dev, struct hns_roce_dev *hr_dev)
{
    /* MR */
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_REREG_MR) {
        ib_dev->uverbs_cmd_mask |= (1ULL << IB_USER_VERBS_CMD_REREG_MR);
        ib_set_device_ops(ib_dev, &hns_roce_dev_mr_ops);
    }
}

static void hns_roce_register_device_mw(struct ib_device *ib_dev, struct hns_roce_dev *hr_dev)
{
    /* MW */
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_MW) {
        ib_dev->uverbs_cmd_mask |=
            (1ULL << IB_USER_VERBS_CMD_ALLOC_MW) | (1ULL << IB_USER_VERBS_CMD_DEALLOC_MW);
        ib_set_device_ops(ib_dev, &hns_roce_dev_mw_ops);
    }
}

static void hns_roce_register_device_frmr_others(struct ib_device *ib_dev, struct hns_roce_dev *hr_dev)
{
    /* FRMR */
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_FRMR) {
        ib_set_device_ops(ib_dev, &hns_roce_dev_frmr_ops);
    }

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC) {
        ib_dev->uverbs_cmd_mask |=
            (1ULL << IB_USER_VERBS_CMD_OPEN_XRCD) | (1ULL << IB_USER_VERBS_CMD_CLOSE_XRCD);
        ib_set_device_ops(ib_dev, &hns_roce_dev_xrcd_ops);
    }
}

static void hns_roce_register_device_info(struct hns_roce_dev *hr_dev)
{
    struct ib_device *ib_dev = NULL;

    ib_dev = &hr_dev->ib_dev;
    hns_roce_register_device_attr(ib_dev, hr_dev);

    ib_set_device_ops(ib_dev, hr_dev->hw->hns_roce_dev_ops);
    ib_set_device_ops(ib_dev, &hns_roce_dev_ops);

    hns_roce_register_device_srq(ib_dev, hr_dev);
    hns_roce_register_device_mr(ib_dev, hr_dev);
    hns_roce_register_device_mw(ib_dev, hr_dev);
    hns_roce_register_device_frmr_others(ib_dev, hr_dev);
}

static int hns_roce_get_device_name(struct hns_roce_dev *hr_dev, struct ib_device *ib_dev)
{
    int ret;
    int dev_id;

    dev_id = hns_roce_get_devid(hr_dev);
    if (dev_id < 0) {
        dev_err(hr_dev->dev, "hns roce get loc devid failed, loc dev:%d.\n", dev_id);
        return -EINVAL;
    }

    if (!strlen(ib_dev->name)) {
        ret = snprintf_s(ib_dev->name, IB_DEVICE_NAME_MAX, IB_DEVICE_NAME_MAX - 1, "hns_%d", dev_id);
        if (ret <= 0) {
            dev_err(hr_dev->dev, "snprintf_s ib dev name err, ret:%d.\n", ret);
            return -EINVAL;
        }

        dev_info(hr_dev->dev, "config roce dev name:%s\n", ib_dev->name);
    }

    return 0;
}

static int hns_roce_register_device(struct hns_roce_dev *hr_dev)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_ib_iboe *iboe = NULL;
    struct ib_device *ib_dev = &hr_dev->ib_dev;
    int ret;

    ret = hns_roce_get_device_name(hr_dev, ib_dev);
    if (ret) {
        dev_err(dev, "hns_roce_get_device_name err, ret:%d.\n", ret);
        return ret;
    }

    iboe = &hr_dev->iboe;
    spin_lock_init(&iboe->lock);

    hns_roce_register_device_info(hr_dev);

    ret = ib_register_device(ib_dev, "hns_%d", dev);
    if (ret) {
        dev_err(dev, "ib_register_device failed, %d!\n", ret);
        return ret;
    }

    ret = roce_init_cdev(hr_dev);
    if (ret) {
        dev_err(dev, "roce_init_cdev failed, ret = %d, dev name:%s\n", ret, ib_dev->name);
        goto error_failed_init_cdev;
    }

    ret = hns_roce_setup_mtu_mac(hr_dev);
    if (ret) {
        dev_err(dev, "setup_mtu_mac failed, ret = %d\n", ret);
        goto error_failed_setup_mtu_mac;
    }

    iboe->nb.notifier_call = hns_roce_netdev_event;
    ret = register_netdevice_notifier(&iboe->nb);
    if (ret) {
        dev_err(dev, "register_netdevice_notifier failed, %d!\n", ret);
        goto error_failed_setup_mtu_mac;
    }

    hr_dev->active = true;
    return 0;

error_failed_setup_mtu_mac:
    roce_remove_cdev(hr_dev);

error_failed_init_cdev:
    ib_unregister_device(ib_dev);

    return ret;
}

static int hns_roce_init_hem_mtt_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    roce_caps_info.obj_size = hr_dev->caps.mtt_entry_sz;
    roce_caps_info.nobj = hr_dev->caps.num_mtt_segs;
    ret = hns_roce_init_hem_table(hr_dev, &hr_dev->mr_table.mtt_table, HEM_TYPE_MTT, roce_caps_info, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init MTT context memory, aborting, %d.\n", ret);
        return ret;
    }

    return 0;
}

static void hns_roce_free_hem_mtt_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtt_table);
}

static int hns_roce_init_hem_cqe_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.mtt_entry_sz, hr_dev->caps.num_cqe_segs);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->mr_table.mtt_cqe_table, HEM_TYPE_CQE, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init MTT CQE context memory, aborting, %d.\n", ret);
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_mtt_cqe_table(struct hns_roce_dev *hr_dev)
{
    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtt_cqe_table);
    }
}

static int hns_roce_init_hem_mtpt_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    roce_caps_info.obj_size = hr_dev->caps.mtpt_entry_sz;
    roce_caps_info.nobj = hr_dev->caps.num_mtpts;
    ret = hns_roce_init_hem_table(hr_dev, &hr_dev->mr_table.mtpt_table, HEM_TYPE_MTPT, roce_caps_info, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init MTPT context memory, aborting, %d.\n", ret);
        return ret;
    }

    return 0;
}

static void hns_roce_free_hem_mtpt_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtpt_table);
}

static int hns_roce_init_hem_qpc_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.qpc_entry_sz, hr_dev->caps.num_qps);
    ret = hns_roce_init_hem_table(hr_dev, &hr_dev->qp_table.qp_table, HEM_TYPE_QPC, roce_caps_info, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init QP context memory, aborting, %d.\n", ret);
        return ret;
    }

    return 0;
}

static void hns_roce_free_hem_qpc_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qp_table.qp_table);
}

static int hns_roce_init_hem_irrl_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.irrl_entry_sz * hr_dev->caps.max_qp_init_rdma,
                              hr_dev->caps.num_qps);
    ret = hns_roce_init_hem_table(hr_dev, &hr_dev->qp_table.irrl_table, HEM_TYPE_IRRL, roce_caps_info, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init irrl_table memory, aborting, %d.\n", ret);
        return ret;
    }

    return 0;
}

static void hns_roce_free_hem_irrl_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.irrl_entry_sz) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qp_table.irrl_table);
    }
}

static int hns_roce_init_hem_trrl_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.trrl_entry_sz) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.trrl_entry_sz * hr_dev->caps.max_qp_dest_rdma,
                                  hr_dev->caps.num_qps);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->qp_table.trrl_table, HEM_TYPE_TRRL, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init trrl_table memory, aborting, %d.\n", ret);
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_trrl_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.trrl_entry_sz) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qp_table.trrl_table);
    }
}

static int hns_roce_init_hem_cqc_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.cqc_entry_sz, hr_dev->caps.num_cqs);
    ret = hns_roce_init_hem_table(hr_dev, &hr_dev->cq_table.table, HEM_TYPE_CQC, roce_caps_info, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init CQ context memory, aborting.\n");
        return ret;
    }

    return 0;
}

static void hns_roce_free_hem_cqc_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_hem_table(hr_dev, &hr_dev->cq_table.table);
}

static int hns_roce_init_hem_scc_ctx_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.scc_ctx_entry_sz) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.scc_ctx_entry_sz, hr_dev->caps.num_qps);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->qp_table.scc_ctx_table, HEM_TYPE_SCC_CTX, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init SCC context memory, aborting.\n");
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_scc_ctx_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.scc_ctx_entry_sz) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qp_table.scc_ctx_table);
    }
}

static int hns_roce_init_hem_qpc_timer_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.qpc_timer_entry_sz) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.qpc_timer_entry_sz, hr_dev->caps.num_qpc_timer);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->qpc_timer_table.table, HEM_TYPE_QPC_TIMER, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init QPC timer memory, aborting.\n");
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_qpc_timer_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.qpc_timer_entry_sz) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->qpc_timer_table.table);
    }
}

static int hns_roce_init_hem_cpc_timer_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.cqc_timer_entry_sz) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.cqc_timer_entry_sz, hr_dev->caps.num_cqc_timer);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->cqc_timer_table.table, HEM_TYPE_CQC_TIMER, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init CQC timer memory, aborting.\n");
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_cqc_timer_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.cqc_timer_entry_sz) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->cqc_timer_table.table);
    }
}

static int hns_roce_init_hem_srqc_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.srqc_entry_sz) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.srqc_entry_sz, hr_dev->caps.num_srqs);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->srq_table.table, HEM_TYPE_SRQC, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init SRQ context memory, aborting.\n");
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_srqc_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.srqc_entry_sz) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->srq_table.table);
    }
}

static int hns_roce_init_hem_srqwqe_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.num_srqwqe_segs) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.mtt_entry_sz, hr_dev->caps.num_srqwqe_segs);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->mr_table.mtt_srqwqe_table, HEM_TYPE_SRQWQE, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init MTT srqwqe memory, aborting.\n");
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_srqwqe_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.num_srqwqe_segs) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtt_srqwqe_table);
    }
}

static int hns_roce_init_hem_idx_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct hns_roce_caps_info roce_caps_info;

    if (hr_dev->caps.num_idx_segs) {
        HNS_ROCE_CAPS_INFO_ASSIGN(roce_caps_info, hr_dev->caps.idx_entry_sz, hr_dev->caps.num_idx_segs);
        ret = hns_roce_init_hem_table(hr_dev, &hr_dev->mr_table.mtt_idx_table, HEM_TYPE_IDX, roce_caps_info, 1);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init MTT idx memory, aborting.\n");
            return ret;
        }
    }

    return 0;
}

static void hns_roce_free_hem_idx_table(struct hns_roce_dev *hr_dev)
{
    if ((hr_dev->caps.num_idx_segs)) {
        hns_roce_cleanup_hem_table(hr_dev, &hr_dev->mr_table.mtt_idx_table);
    }
}

static struct hns_roce_hem_init_ops g_hem_init_handler[] = {
    {hns_roce_init_hem_mtt_table, hns_roce_free_hem_mtt_table},
    {hns_roce_init_hem_cqe_table, hns_roce_free_hem_mtt_cqe_table},
    {hns_roce_init_hem_mtpt_table, hns_roce_free_hem_mtpt_table},
    {hns_roce_init_hem_qpc_table, hns_roce_free_hem_qpc_table},
    {hns_roce_init_hem_irrl_table, hns_roce_free_hem_irrl_table},
    {hns_roce_init_hem_trrl_table, hns_roce_free_hem_trrl_table},
    {hns_roce_init_hem_cqc_table, hns_roce_free_hem_cqc_table},
    {hns_roce_init_hem_scc_ctx_table, hns_roce_free_hem_scc_ctx_table},
    {hns_roce_init_hem_qpc_timer_table, hns_roce_free_hem_qpc_timer_table},
    {hns_roce_init_hem_cpc_timer_table, hns_roce_free_hem_cqc_timer_table},
    {hns_roce_init_hem_srqc_table, hns_roce_free_hem_srqc_table},
    {hns_roce_init_hem_srqwqe_table, hns_roce_free_hem_srqwqe_table},
    {hns_roce_init_hem_idx_table, hns_roce_free_hem_idx_table},
};

static int hns_roce_init_hem(struct hns_roce_dev *hr_dev)
{
    u32 init_hem_num = (u32)(sizeof(g_hem_init_handler) / sizeof(g_hem_init_handler[0]));
    int ret;
    int i;

    for (i = 0; i < (int)init_hem_num; i++) {
        ret = g_hem_init_handler[i].hns_roce_hem_init_op(hr_dev);
        if (ret) {
            for (i--; i >= 0; i--) {
                g_hem_init_handler[i].hns_roce_hem_free_op(hr_dev);
            }

            return ret;
        }
    }

    return 0;
}

static int hns_roce_hca_init_uar_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    ret = hns_roce_init_uar_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init uar table(%d). aborting\n", ret);
        return ret;
    }

    return 0;
}

static int hns_roce_hca_init_pd_table(struct hns_roce_dev *hr_dev)
{
    int ret;

    ret = hns_roce_init_pd_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init pd table(%d).\n", ret);
        return ret;
    }

    return 0;
}


static int hns_roce_hca_init_xrcd_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC) {
        ret = hns_roce_init_xrcd_table(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init xrcd table(%d).\n", ret);
            return ret;
        }
    }

    return 0;
}

static int hns_roce_hca_init_mr_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    ret = hns_roce_init_mr_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init mr table(%d).\n", ret);
        return ret;
    }

    return 0;
}

static int hns_roce_hca_init_cq_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    ret = hns_roce_init_cq_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init cq table(%d).\n", ret);
        return ret;
    }

    return 0;
}

static int hns_roce_hca_init_qp_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    ret = hns_roce_init_qp_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init qp table(%d).\n", ret);
        return ret;
    }

    return 0;
}

static int hns_roce_hca_init_atu_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    ret = hns_roce_init_atu_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to init ATU table(%d).\n", ret);
        return ret;
    }

    return 0;
}

static int hns_roce_hca_init_srq_table(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SRQ) {
        ret = hns_roce_init_srq_table(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Failed to init srq table(%d).\n", ret);
            return ret;
        }
    }

    return 0;
}

static int hns_roce_hca_init_uar(struct hns_roce_dev *hr_dev)
{
    int ret;
    ret = hns_roce_uar_alloc(hr_dev, &hr_dev->priv_uar);
    if (ret) {
        dev_err(hr_dev->dev, "Failed to allocate priv_uar(%d).\n", ret);
        return ret;
    }

    return 0;
}

static void hns_roce_free_uar_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_uar_table(hr_dev);
}

static void hns_roce_free_pd_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_pd_table(hr_dev);
}

static void hns_roce_free_atu_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_atu_table(hr_dev);
}

static void hns_roce_free_qp_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_qp_table(hr_dev);
}

static void hns_roce_free_cq_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_cq_table(hr_dev);
}

static void hns_roce_free_mr_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_cleanup_mr_table(hr_dev);
}

static void hns_roce_free_xrcd_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC) {
        hns_roce_cleanup_xrcd_table(hr_dev);
    }
}

static void hns_roce_free_srq_table(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SRQ) {
        hns_roce_cleanup_srq_table(hr_dev);
    }
}

static void hns_roce_free_uar(struct hns_roce_dev *hr_dev)
{
    hns_roce_uar_free(hr_dev, &hr_dev->priv_uar);
}

static struct hns_roce_hca_ops g_hca_init_handler[] = {
    {hns_roce_hca_init_uar_table, hns_roce_free_uar_table},
    {hns_roce_hca_init_pd_table, hns_roce_free_pd_table},
    {hns_roce_hca_init_xrcd_table, hns_roce_free_xrcd_table},
    {hns_roce_hca_init_mr_table, hns_roce_free_mr_table},
    {hns_roce_hca_init_cq_table, hns_roce_free_cq_table},
    {hns_roce_hca_init_qp_table, hns_roce_free_qp_table},
    {hns_roce_hca_init_atu_table, hns_roce_free_atu_table},
    {hns_roce_hca_init_srq_table, hns_roce_free_srq_table},
    {hns_roce_hca_init_uar, hns_roce_free_uar},
};

/**
 * hns_roce_setup_hca - setup host channel adapter
 * @hr_dev: pointer to hns roce device
 * Return : int
 */
static int hns_roce_setup_hca(struct hns_roce_dev *hr_dev)
{
    u32 hns_roce_hca_init_num = (u32)(sizeof(g_hca_init_handler) / sizeof(g_hca_init_handler[0]));
    int ret;
    int i;

    spin_lock_init(&hr_dev->sm_lock);
    spin_lock_init(&hr_dev->bt_cmd_lock);
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
        INIT_LIST_HEAD(&hr_dev->pgdir_list);
        mutex_init(&hr_dev->pgdir_mutex);
    }

    for (i = 0; i < (int)hns_roce_hca_init_num; i++) {
        ret = g_hca_init_handler[i].hns_roce_init_op(hr_dev);
        if (ret) {
            for (i--; i >= 0; i--) {
                g_hca_init_handler[i].hns_roce_free_op(hr_dev);
            }
            if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
                mutex_destroy(&hr_dev->pgdir_mutex);
            }
            return ret;
        }
    }

    return 0;
}

STATIC int hns_roce_reset(struct hns_roce_dev *hr_dev)
{
    int ret;

    if (hr_dev->hw->reset) {
        ret = hr_dev->hw->reset(hr_dev, true);
        if (ret) {
            dev_err(hr_dev->dev, "reset hr_dev hw failed(%d)!\n", ret);
            return ret;
        }
    }
    hr_dev->is_reset = false;

    return 0;
}

STATIC void hns_roce_clean_reset(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->hw->reset) {
        if (hr_dev->hw->reset(hr_dev, false)) {
            dev_err(hr_dev->dev, "Dereset RoCE engine failed!\n");
        }
    }
}

STATIC int hns_roce_cmq_init(struct hns_roce_dev *hr_dev)
{
    int ret;

    if (hr_dev->hw->cmq_init) {
        ret = hr_dev->hw->cmq_init(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Init RoCE cmq failed(%d)!\n", ret);
            return ret;
        }
    }

    return 0;
}

STATIC void hns_roce_clean_cmq(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->hw->cmq_exit) {
        hr_dev->hw->cmq_exit(hr_dev);
    }
}

STATIC int hns_roce_hw_rst_atu(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->hw->hw_rst_atu) {
        ret = hr_dev->hw->hw_rst_atu(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "ATU reset failed(%d)!\n", ret);
            return ret;
        }
    }

    return 0;
}

STATIC int hns_roce_hw_profile(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->hw->hw_profile) {
        ret = hr_dev->hw->hw_profile(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Get RoCE engine profile failed(%d)!\n", ret);
            return ret;
        }
    }

    return 0;
}

STATIC int hns_roce_init_eq(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->hw->init_eq) {
        ret = hr_dev->hw->init_eq(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Eq init failed(%d)!\n", ret);
            return ret;
        }
    }

    return 0;
}

STATIC void hns_roce_clean_eq(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->hw->cleanup_eq) {
        hr_dev->hw->cleanup_eq(hr_dev);
    }
}

STATIC int hns_roce_cmd_init_events(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->cmd_mod) {
        ret = hns_roce_cmd_use_events(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Switch to event-driven cmd failed(%d)!\n", ret);
            return ret;
        }
    }

    return 0;
}

STATIC void hns_roce_init_polling(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->cmd_mod) {
        hns_roce_cmd_use_polling(hr_dev);
    }
}

STATIC int hns_roce_hw_init(struct hns_roce_dev *hr_dev)
{
    int ret;
    if (hr_dev->hw->hw_init) {
        ret = hr_dev->hw->hw_init(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "Hw_init failed(%d)!\n", ret);
            return ret;
        }
    }

    return 0;
}

STATIC void hns_roce_hw_exit(struct hns_roce_dev *hr_dev)
{
    if (hr_dev->hw->hw_exit) {
        hr_dev->hw->hw_exit(hr_dev);
    }
}

STATIC int hns_roce_init_list_register_device(struct hns_roce_dev *hr_dev)
{
    int ret;
    INIT_LIST_HEAD(&hr_dev->qp_list);
    spin_lock_init(&hr_dev->reset_lock);

    ret = hns_roce_register_device(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "register hns_roce device failed, ret(%d)!\n", ret);
        return ret;
    }

    return 0;
}

static struct hns_roce_init_ops g_hns_roce_init_handler[] = {
    {hns_roce_reset, hns_roce_clean_reset},
    {hns_roce_cmq_init, hns_roce_clean_cmq},
    {hns_roce_hw_rst_atu, NULL},
    {hns_roce_hw_profile, NULL},
    {hns_roce_cmd_init, hns_roce_cmd_cleanup},
    {hns_roce_init_eq, hns_roce_clean_eq},
    {hns_roce_cmd_init_events, hns_roce_init_polling},
    {hns_roce_init_hem, hns_roce_cleanup_hem},
    {hns_roce_setup_hca, hns_roce_cleanup_bitmap},
    {hns_roce_hw_init, hns_roce_hw_exit},
    {hns_roce_init_list_register_device, hns_roce_unregister_device},
};

int hns_roce_init(struct hns_roce_dev *hr_dev)
{
    u32 hns_roce_init_num;
    int ret;
    int i;

    hns_roce_init_num = (u32)(sizeof(g_hns_roce_init_handler) / sizeof(g_hns_roce_init_handler[0]));

    for (i = 0; i < (int)hns_roce_init_num; i++) {
        ret = g_hns_roce_init_handler[i].hns_roce_init_op(hr_dev);
        if (ret) {
            goto init_fail;
        }
    }

    (void)hns_roce_register_sysfs(hr_dev);

    return 0;

init_fail:
    for (i--; i >= 0; i--) {
        if (g_hns_roce_init_handler[i].hns_roce_clean_op) {
            g_hns_roce_init_handler[i].hns_roce_clean_op(hr_dev);
        }
    }
    return ret;
}

void hns_roce_uninit(struct hns_roce_dev *hr_dev)
{
    u32 hns_roce_init_num;
    int i;

    hns_roce_unregister_sysfs(hr_dev);

    hns_roce_init_num = (u32)(sizeof(g_hns_roce_init_handler) / sizeof(g_hns_roce_init_handler[0]));

    for (i = (int)hns_roce_init_num - 1; i >= 0; i--) {
        if (g_hns_roce_init_handler[i].hns_roce_clean_op) {
            g_hns_roce_init_handler[i].hns_roce_clean_op(hr_dev);
        }
    }
}

void hns_roce_exit(struct hns_roce_dev *hr_dev)
{
    hns_roce_unregister_device(hr_dev);
    if (hr_dev->hw->hw_exit) {
        hr_dev->hw->hw_exit(hr_dev);
    }
    hns_roce_cleanup_bitmap(hr_dev);
    hns_roce_cleanup_hem(hr_dev);

    if (hr_dev->cmd_mod) {
        hns_roce_cmd_use_polling(hr_dev);
    }

    if (hr_dev->hw->cleanup_eq) {
        hr_dev->hw->cleanup_eq(hr_dev);
    }

    hns_roce_cmd_cleanup(hr_dev);
    if (hr_dev->hw->cmq_exit) {
        hr_dev->hw->cmq_exit(hr_dev);
    }
    if (hr_dev->hw->reset) {
        hr_dev->hw->reset(hr_dev, false);
    }
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("Hisilicon Hip08 Family RoCE Driver");
