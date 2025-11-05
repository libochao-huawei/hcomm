/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/mman.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/mm.h>
#include "host_roce_notify.h"
#include "host_roce_devmm.h"
#include "host_roce_cdev.h"

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

typedef int (*host_ioctl_handle_func)(char *arg_ptr);
struct host_cdev_cmd_ops {
    unsigned int host_cmd;
    host_ioctl_handle_func cmd_op;
};

#define HOST_DEVICE_NAME     "host_rdma"

#define PGSZ_BASE        2
#define MAX_LOGIC_ID 8

struct mmap_node {
    unsigned int mmap_sign;
    pid_t tgid;
};

struct host_cdev_info {
    int major;
    struct cdev cdev;
    struct class *cdev_class;
    struct device *cdev_device;
    void *notify_priv;
    void *devmm_priv;
    struct mmap_node mmap_node[MAX_LOGIC_ID];
};

struct host_cdev_info g_host_cdev;
static atomic_t cdev_valid = ATOMIC_INIT(1);
static DEFINE_MUTEX(g_host_cdev_lock);

extern int devdrv_get_notify_base_addr(u32 devid, u64 *dev_phy_addr);
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

#ifndef PCI_VENDOR_ID_MELLANOX
#define PCI_VENDOR_ID_MELLANOX                  0x15b3
#endif
static const struct pci_device_id g_host_notify_tbl[] = {
    { PCI_VDEVICE(MELLANOX, 0x1011), 0 },  /* MT4113 Connect-IB */
    { PCI_VDEVICE(MELLANOX, 0x1012), 0 },  /* Connect-IB Virtual Function */
    { PCI_VDEVICE(MELLANOX, 0x1013), 0 },  /* ConnectX-4 */
    { PCI_VDEVICE(MELLANOX, 0x1014), 0 },  /* ConnectX-4 Virtual Function */
    { PCI_VDEVICE(MELLANOX, 0x1015), 0 },  /* ConnectX-4LX */
    { PCI_VDEVICE(MELLANOX, 0x1016), 0 },  /* ConnectX-4LX Virtual Function */
    { PCI_VDEVICE(MELLANOX, 0x1017), 0 },  /* ConnectX-5, PCIe 3.0 */
    { PCI_VDEVICE(MELLANOX, 0x1018), 0 },  /* ConnectX-5 Virtual Function */
    { PCI_VDEVICE(MELLANOX, 0x1019), 0 },    /* ConnectX-5 Ex */
    { PCI_VDEVICE(MELLANOX, 0x101a), 0 },  /* ConnectX-5 Ex VF */
    { PCI_VDEVICE(MELLANOX, 0x101b), 0 },    /* ConnectX-6 */
    { PCI_VDEVICE(MELLANOX, 0x101c), 0 },  /* ConnectX-6 VF */
    { PCI_VDEVICE(MELLANOX, 0x101d), 0 },  /* ConnectX-6 DX */
    { PCI_VDEVICE(MELLANOX, 0x101e), 0 },  /* ConnectX family mlx5Gen Virtual Function */
    { PCI_VDEVICE(MELLANOX, 0x1021), 0 },  /* ConnectX-7 */
    { PCI_VDEVICE(MELLANOX, 0xa2d2), 0 },  /* BlueField integrated ConnectX-5 network controller */
    { PCI_VDEVICE(MELLANOX, 0xa2d3), 0 },  /* BlueField integrated ConnectX-5 network controller VF */
    { PCI_VDEVICE(MELLANOX, 0xa2d6), 0 },  /* BlueField-2 integrated ConnectX-6 Dx network controller */
    {}
};

MODULE_DEVICE_TABLE(pci, g_host_notify_tbl); //lint !e14 !e508

STATIC int host_roce_get_notify_base_addr(u32 phy_id, u64 *phy_addr, u64 *size)
{
    int ret;
    u64 dev_phy_addr = 0;

    ret = devdrv_get_notify_base_addr(phy_id, &dev_phy_addr);
    if (ret) {
        pr_err("devdrv_get_notify_base_addr fail! phy_id:%u ret:0x%x \n", phy_id, ret);
        return ret;
    }

    *phy_addr = dev_phy_addr;
    *size = PGSZ_BASE * PAGE_SIZE;

    return 0;
}

// prevent PUAF issues
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
STATIC int host_roce_vma_mremap(struct vm_area_struct *vma)
{
    pr_err("mremap not support\n");
	return -EOPNOTSUPP;
}
#endif

static const struct vm_operations_struct g_host_roce_vm_ops = {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
    .mremap = host_roce_vma_mremap,
#endif
};

STATIC int host_roce_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    u64 phy_addr, size;
    u32 phy_id = 0;
    u32 vfid = 0;
    u32 vma_size;
    u32 logic_id;
    struct host_roce_notify_node *notify_node = NULL;

    if ((filp == NULL) || (vma == NULL)) {
        pr_err("hns3: null point:filp %pK, vma %pK\n", filp, vma);
        return -EINVAL;
    }

    vma->vm_ops = &g_host_roce_vm_ops;
    logic_id = (u32)vma->vm_pgoff;

    ret = devdrv_manager_container_logical_id_to_physical_id(logic_id, &phy_id, &vfid);
    if (ret || vfid != 0) {
        pr_err("devdrv_manager_container_logical_id_to_physical_id failed, ret[%d], vfid[%u]\n", ret, vfid);
        return -EINVAL;
    }

    if (phy_id >= MAX_LOGIC_ID) {
        pr_err("phy_id[%u], MAX_LOGIC_ID[%d]\n", phy_id, MAX_LOGIC_ID);
        return -EINVAL;
    }

    if (g_host_cdev.mmap_node[phy_id].mmap_sign) {
        pr_err("g_host_cdev.mmap_node[phy_id].mmap_sign:%u\n", g_host_cdev.mmap_node[phy_id].mmap_sign);
        return -EINVAL;
    }

    // 调用driver的接口获取对应的物理地址
    ret = host_roce_get_notify_base_addr(phy_id, &phy_addr, &size);
    if (ret) {
        pr_err("hns_roce get notify base addr failed\n");
        return ret;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    vm_flags_set(vma, VM_DONTCOPY | VM_DONTEXPAND | VM_IO);
#else
    vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_IO;
#endif

    vma_size = vma->vm_end - vma->vm_start;
    if ((vma_size % PAGE_SIZE) != 0 || vma_size > size) {
        pr_err("hns3: vma_size[%u] %% PAGE_SIZE[%lu]) != 0, \
            or vma_size[%u] is larger than the phy_addr size[%llu].\n",
            vma_size, PAGE_SIZE, vma_size, size);
        return -EINVAL;
    }

    mutex_lock(&g_host_cdev_lock);
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    if (io_remap_pfn_range(vma, vma->vm_start, phy_addr >> PAGE_SHIFT, size, vma->vm_page_prot)) {
        pr_err("remap pfn range failed, vm_start[%lu], page size[%llu\n]", vma->vm_start, size);
        mutex_unlock(&g_host_cdev_lock);
        return -EAGAIN;
    }

    notify_node = kzalloc(sizeof(*notify_node), GFP_ATOMIC | __GFP_ACCOUNT);
    if (notify_node == NULL) {
        pr_err("kzalloc notify_node err.\n");
        mutex_unlock(&g_host_cdev_lock);
        return -ENOMEM;
    }

    notify_node->va = vma->vm_start;
    notify_node->pa = phy_addr;
    notify_node->sz = size;

    ret = host_roce_notify_add(notify_node);
    if (ret) {
        kfree(notify_node);
        notify_node = NULL;
    }

    g_host_cdev.mmap_node[phy_id].mmap_sign = 1;
    g_host_cdev.mmap_node[phy_id].tgid = current->tgid;
    mutex_unlock(&g_host_cdev_lock);

    return 0; /* node exists return success */
}

STATIC int host_release_notify(char *arg_ptr)
{
    int ret;
    u32 phy_id = 0;
    u32 vfid = 0;

    struct host_roce_notify_info notify_info = {0};

    if (copy_from_user((char *)&notify_info, arg_ptr, sizeof(struct host_roce_notify_info))) {
        pr_err("host_release_notify copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(notify_info.logic_id, &phy_id, &vfid);
    if (ret || vfid != 0) {
        pr_err("devdrv_manager_container_logical_id_to_physical_id failed, ret[%d], vfid[%u]\n", ret, vfid);
        return -EINVAL;
    }

    if (phy_id >= MAX_LOGIC_ID) {
        pr_err("phy_id[%u], MAX_LOGIC_ID[%d]\n", phy_id, MAX_LOGIC_ID);
        return -EINVAL;
    }

    mutex_lock(&g_host_cdev_lock);
    host_roce_notify_del(&notify_info);
    g_host_cdev.mmap_node[phy_id].mmap_sign = 0;
    mutex_unlock(&g_host_cdev_lock);

    return 0;
}

STATIC long host_cdev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    int retval = -EINVAL;
    int i;
    char *arg_void_uintptr = (char *)(uintptr_t)arg;
    struct host_cdev_cmd_ops host_cmd_handler[] = {
        {HOST_CDEV_IOC_FREE_NOTIFY, host_release_notify},
    };

    if (_IOC_TYPE(cmd) != HOST_CDEV_IOC_MAGIC) {
        pr_err("[host_cdev] cmd[%u] type incorrect\n", cmd);
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > HOST_CDEV_IOC_MAXNR) {
        pr_err("[host_cdev] cmd[%u] incorrect\n", cmd);
        return -ENOTTY;
    }

    if (arg_void_uintptr == NULL) {
        pr_err("[host_cdev] arg err arg_void_uintptr is NULL\n");
        return -EINVAL;
    }

    for (i = 0; i < (sizeof(host_cmd_handler) / sizeof(struct host_cdev_cmd_ops)); i++) {
        if (cmd == host_cmd_handler[i].host_cmd) {
            retval = host_cmd_handler[i].cmd_op(arg_void_uintptr);
            break;
        }
    }

    if (retval) {
        pr_err("[host_cdev] host_data_set fail, cmd[%u] retval[%d] fail.", cmd, retval);
        return retval;
    }
    return 0;
}

STATIC int host_cdev_close(struct inode *inode, struct file *filp)
{
    int i;

    mutex_lock(&g_host_cdev_lock);
    host_roce_notify_clear();

    for (i = 0; i < MAX_LOGIC_ID; i++) {
        if (g_host_cdev.mmap_node[i].tgid == current->tgid) {
            g_host_cdev.mmap_node[i].mmap_sign = 0;
        }
    }
    mutex_unlock(&g_host_cdev_lock);

    return 0;
}

STATIC unsigned long host_roce_get_unmapped_area(struct file *filp, unsigned long addr, unsigned long len,
    unsigned long pgoff, unsigned long flags)
{
    if ((flags & MAP_FIXED) != 0) {
        pr_err("host roce not support MAP_FIXED\n");
        return -EINVAL;
    }

    if (filp == NULL) {
        pr_warn("filp is NULL in host_roce_get_unmapped_area\n");
        return -ENODEV;
    }

    return current->mm->get_unmapped_area(filp, addr, len, pgoff, flags);
}

static const struct file_operations g_host_cdev_fops = {
    .owner = THIS_MODULE,
    .release = host_cdev_close,
    .mmap = host_roce_mmap,
    .unlocked_ioctl = host_cdev_ioctl,
    .get_unmapped_area = host_roce_get_unmapped_area, // prevent PUAF issues
};

STATIC int host_cdev_init(void)
{
#ifndef DEFINE_HNS_LLT
    ib_register_peer_memory_client_fun func_ib_register_peer_memory_client =
        (ib_register_peer_memory_client_fun)(uintptr_t)symbol_get(ib_register_peer_memory_client);
#else
    ib_register_peer_memory_client_fun func_ib_register_peer_memory_client =
        ib_register_peer_memory_client;
#endif
    dev_t devno;
    int result;
    int i, ret;

#ifndef DEFINE_HNS_LLT
    if (func_ib_register_peer_memory_client == NULL) {
        pr_warn("ib_register_peer_memory_client syms not found.\n");
        atomic_set(&cdev_valid, 0);
        return 0;
    }
#endif

    result = alloc_chrdev_region(&devno, 0, 1, HOST_DEVICE_NAME);
    if (result < 0) {
        pr_err("[third_card] alloc_chrdev_region failed: %d!\n", result);
        goto err_alloc;
    }

    g_host_cdev.major = MAJOR(devno);
    cdev_init(&g_host_cdev.cdev, &g_host_cdev_fops);
    g_host_cdev.cdev.owner = THIS_MODULE;
    result = cdev_add(&g_host_cdev.cdev, devno, 1);
    if (result) {
        pr_err("[host_cdev] cdev_add failed: %d!\n", result);
        goto err_chrdev_unreg;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    g_host_cdev.cdev_class = class_create(HOST_DEVICE_NAME);
#else
    g_host_cdev.cdev_class = class_create(THIS_MODULE, HOST_DEVICE_NAME);
#endif
    if (IS_ERR(g_host_cdev.cdev_class)) {
        pr_err("[host_cdev] class_create failed.\n");
        result = -EAGAIN;
        goto err_cdev_del;
    }

    g_host_cdev.cdev_device = device_create(g_host_cdev.cdev_class, NULL,
        MKDEV((unsigned int)(g_host_cdev.major), 0), NULL, HOST_DEVICE_NAME);
    if (IS_ERR(g_host_cdev.cdev_device)) {
        pr_err("[host_cdev] failed to create device.\n");
        result = -EAGAIN;
        goto err_class_destr;
    }

    g_host_cdev.notify_priv = host_roce_notify_client_init(func_ib_register_peer_memory_client);
    if (g_host_cdev.notify_priv == NULL) {
        result = -ENOMEM;
        pr_err("RoCE host Notify init failed!\n");
        goto error_notify_client_init;
    }

    g_host_cdev.devmm_priv = host_devmm_client_init(func_ib_register_peer_memory_client);
    if (g_host_cdev.devmm_priv == NULL) {
        result = -ENOMEM;
        pr_err("RoCE host devmm init failed!\n");
        goto error_devmm_client_init;
    }

    for (i = 0; i < MAX_LOGIC_ID; i++) {
        g_host_cdev.mmap_node[i].mmap_sign = 0;
    }

    pr_info("host_cdev_init success.\n");

#ifndef DEFINE_HNS_LLT
    if (func_ib_register_peer_memory_client != NULL) {
        symbol_put(ib_register_peer_memory_client);
    }
#endif

    return 0;

error_devmm_client_init:
    host_roce_notify_client_uninit(g_host_cdev.notify_priv);
error_notify_client_init:
    device_destroy(g_host_cdev.cdev_class, MKDEV((unsigned int)(g_host_cdev.major), 0));
err_class_destr:
    class_destroy(g_host_cdev.cdev_class);
err_cdev_del:
    cdev_del(&g_host_cdev.cdev);
err_chrdev_unreg:
    unregister_chrdev_region(devno, 1);
err_alloc:
    ret = memset_s(&g_host_cdev, sizeof(g_host_cdev), 0, sizeof(g_host_cdev));
    if (ret) {
        pr_err("memset 0 error,ret[%d].\n", ret);
    }

#ifndef DEFINE_HNS_LLT
    if (func_ib_register_peer_memory_client != NULL) {
        symbol_put(ib_register_peer_memory_client);
    }
#endif

    return result;
}
#ifndef DEFINE_HNS_LLT
module_init(host_cdev_init);
#endif

STATIC void host_cdev_exit(void)
{
    int ret;

    if (atomic_read(&cdev_valid) == 0) {
        pr_warn("[host_cdev] cdev invalid.\n");
        goto cdev_invalid;
    }
    host_roce_notify_client_uninit(g_host_cdev.notify_priv);
    host_devmm_client_uninit(g_host_cdev.devmm_priv);
    device_destroy(g_host_cdev.cdev_class, MKDEV((unsigned int)(g_host_cdev.major), 0));
    class_destroy(g_host_cdev.cdev_class);
    cdev_del(&g_host_cdev.cdev);
    unregister_chrdev_region(MKDEV((unsigned int)(g_host_cdev.major), 0), 1);

cdev_invalid:
    ret = memset_s(&g_host_cdev, sizeof(g_host_cdev), 0, sizeof(g_host_cdev));
    if (ret) {
        pr_err("memset 0 error, ret[%d].\n", ret);
    }
    pr_info("host_cdev_exit exit.\n");
}
#ifndef DEFINE_HNS_LLT
module_exit(host_cdev_exit);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("Host Notify Driver");

