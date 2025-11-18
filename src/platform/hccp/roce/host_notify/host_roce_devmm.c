/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/kallsyms.h>

#include "host_roce_devmm.h"

invalidate_peer_memory g_devmm_invalidate_callback;
#define DEVMM_NAME         "HOST RoCE DEVMM"
#define DEVMM_VERSION      "1.0"
#define DEVMM_INITED_FLAG      0xF1234567UL

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

int svm_peer_mem_acquire(unsigned long addr, size_t size, void *memory_data,
    char *peer_mem_name, void **client_context);
int svm_peer_mem_get_pages(unsigned long addr, size_t size, int write, int force, struct sg_table *sg_head,
    void *mm_context, u64 core_context);
void svm_peer_mem_put_pages(struct sg_table *sg_head, void *mm_context);
void svm_peer_mem_release(void *mm_context);
unsigned long svm_peer_mem_get_page_size(void *mm_context);
int svm_peer_mem_dma_map(struct sg_table *sg_head, void *context,
    struct device *dma_device, int dmasync, int *nmap);
int svm_peer_mem_dma_unmap(struct sg_table *sg_head, void *context, struct device *dma_device);

#define RANDOM_SIZE          24

STATIC int svm_creat_random_sign(char *random_sign, u32 len)
{
    char random[RANDOM_SIZE] = {0};
    int offset = 0;
    int ret;
    int i;

    for (i = 0; i < RANDOM_SIZE; i++) {
        ret = snprintf_s(random_sign + offset, len - offset, len - 1 - offset, "%02x", (unsigned char)random[i]);
        if (ret < 0) {
            pr_err("snprintf failed, ret(%d).\n", ret);
            return -EINVAL;
        }
        offset += ret;
    }
    random_sign[len - 1] = '\0';

    return 0;
}

/* host_peer_mem_acquire: The function returns 0 for failure and 1 for success. */
STATIC int host_peer_mem_acquire(unsigned long addr, size_t size, void *memory_data,
    char *peer_mem_name, void **client_context)
{
    struct peer_memory_data peer_memory_data;
    int ret;

    peer_memory_data.host_pid = current->tgid;
    peer_memory_data.devid = 0;
    peer_memory_data.vfid = 0;
    ret = memset_s(peer_memory_data.pid_sign, PROCESS_SIGN_LENGTH, 0, PROCESS_SIGN_LENGTH);
    if (ret) {
        pr_err("host_peer_mem_acquire memset failed, ret[%d]\n", ret);
        return 0;
    }
    ret = svm_creat_random_sign(peer_memory_data.pid_sign, PROCESS_SIGN_LENGTH);
    if (ret) {
        pr_err("svm_creat_random_sign failed, ret[%d]\n", ret);
        return 0;
    }

    return svm_peer_mem_acquire(addr, size, &peer_memory_data, peer_mem_name, client_context);
}

static struct mlx_peer_memory_client g_devmm_client = {
    .acquire    = host_peer_mem_acquire,
    .get_pages  = svm_peer_mem_get_pages,
    .dma_map    = svm_peer_mem_dma_map,
    .dma_unmap  = svm_peer_mem_dma_unmap,
    .put_pages  = svm_peer_mem_put_pages,
    .get_page_size  = svm_peer_mem_get_page_size,
    .release    = svm_peer_mem_release,
};

void *host_devmm_client_init(ib_register_peer_memory_client_fun func_ib_register_peer_memory_client)
{
    size_t lenth;
    int ret;

    lenth = strlen(DEVMM_NAME);
    lenth = lenth > IB_PEER_MEMORY_NAME_MAX ? IB_PEER_MEMORY_NAME_MAX : lenth;
    ret = memcpy_s(g_devmm_client.name, sizeof(g_devmm_client.name),
                   DEVMM_NAME, lenth);
    if (ret) {
        pr_err("host_devmm_client_init name memcpy_s err, ret[%d].\n", ret);
        return NULL;
    }

    lenth = strlen(DEVMM_VERSION);
    lenth = lenth > IB_PEER_MEMORY_VER_MAX ? IB_PEER_MEMORY_VER_MAX : lenth;
    ret = memcpy_s(g_devmm_client.version, sizeof(g_devmm_client.version),
                   DEVMM_VERSION, lenth);
    if (ret) {
        pr_err("host_devmm_client_init, version memcpy_s err, ret[%d].\n", ret);
        return NULL;
    }

    return func_ib_register_peer_memory_client(&g_devmm_client,
                                               &g_devmm_invalidate_callback);
}

void host_devmm_client_uninit(char *priv)
{
#ifndef DEFINE_HNS_LLT
    ib_unregister_peer_memory_client_fun func_ib_unregister_peer_memory_client =
        (ib_unregister_peer_memory_client_fun)(uintptr_t)symbol_get(ib_unregister_peer_memory_client);
    if (func_ib_unregister_peer_memory_client == NULL) {
        pr_err("ib_unregister_peer_memory_client syms not found.\n");
        return;
    }
#else
    ib_unregister_peer_memory_client_fun func_ib_unregister_peer_memory_client =
        ib_unregister_peer_memory_client;
#endif

    func_ib_unregister_peer_memory_client(priv);

#ifndef DEFINE_HNS_LLT
    if (func_ib_unregister_peer_memory_client != NULL) {
        symbol_put(ib_unregister_peer_memory_client);
    }
#endif
}
