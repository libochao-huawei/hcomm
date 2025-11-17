/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <dlfcn.h>
#include <pthread.h>
#include "errno.h"
#include "ra.h"
#include "ra_rs_err.h"
#include "ra_rdma_lite.h"

static pthread_mutex_t g_rdma_lite_api_lock = PTHREAD_MUTEX_INITIALIZER;
void *g_rdma_lite_api_handle = NULL;
int g_rdma_lite_api_refcnt = 0;
#ifndef HNS_ROCE_LLT
struct ra_rdma_lite_ops g_rdma_lite_ops;
#else
struct ra_rdma_lite_ops g_rdma_lite_ops = {
    .ra_rdma_lite_alloc_ctx = rdma_lite_alloc_context,
    .ra_rdma_lite_free_ctx = rdma_lite_free_context,
    .ra_rdma_lite_init_mem_pool = rdma_lite_init_mem_pool,
    .ra_rdma_lite_deinit_mem_pool = rdma_lite_deinit_mem_pool,
    .ra_rdma_lite_create_cq = rdma_lite_create_cq,
    .ra_rdma_lite_destroy_cq = rdma_lite_destroy_cq,
    .ra_rdma_lite_poll_cq = rdma_lite_poll_cq,
    .ra_rdma_lite_poll_cq_v2 = rdma_lite_poll_cq_v2,
    .ra_rdma_lite_create_qp = rdma_lite_create_qp,
    .ra_rdma_lite_destroy_qp = rdma_lite_destroy_qp,
    .ra_rdma_lite_post_send = rdma_lite_post_send,
    .ra_rdma_lite_post_recv = rdma_lite_post_recv,
    .ra_rdma_lite_set_qp_sl = rdma_lite_set_qp_sl,
    .ra_rdma_lite_clean_qp = rdma_lite_clean_qp,
    .ra_rdma_lite_restore_snapshot = rdma_lite_restore_snapshot,
};
#endif

STATIC int ra_hdc_open_rdma_lite_so(void)
{
#ifndef HNS_ROCE_LLT
    if (g_rdma_lite_api_handle == NULL) {
        g_rdma_lite_api_handle = dlopen("libascend_rdma_lite.so", RTLD_NOW);
        if (g_rdma_lite_api_handle != NULL) {
            return 0;
        }
        return -EINVAL;
    } else {
            hccp_run_info("rdma lite api dlopen again!");
    }
    return 0;
#endif
    return 0;
}

#ifndef HNS_ROCE_LLT
static int ra_rdma_lite_control_plane_api_init(void)
{
    g_rdma_lite_ops.ra_rdma_lite_alloc_ctx = (struct rdma_lite_context* (*)(u8 phy_id, struct dev_cap_info *cap))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_alloc_context");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_alloc_ctx, "rdma_lite_alloc_context", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_free_ctx = (void (*)(struct rdma_lite_context *lite_ctx))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_free_context");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_free_ctx, "rdma_lite_free_context", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_init_mem_pool = (int (*)(struct rdma_lite_context *lite_ctx,
        struct rdma_lite_mem_attr * lite_mem_attr)) dlsym(g_rdma_lite_api_handle, "rdma_lite_init_mem_pool");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_init_mem_pool, "rdma_lite_init_mem_pool");

    g_rdma_lite_ops.ra_rdma_lite_deinit_mem_pool = (int (*)(struct rdma_lite_context *lite_ctx, u32 mem_idx))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_deinit_mem_pool");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_deinit_mem_pool, "rdma_lite_deinit_mem_pool");

    g_rdma_lite_ops.ra_rdma_lite_create_cq = (struct rdma_lite_cq* (*)(struct rdma_lite_context * lite_ctx,
        struct rdma_lite_cq_attr * lite_cq_attr)) dlsym(g_rdma_lite_api_handle, "rdma_lite_create_cq");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_create_cq, "rdma_lite_create_cq", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_destroy_cq = (int (*)(struct rdma_lite_cq * lite_cq))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_destroy_cq");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_destroy_cq, "rdma_lite_destroy_cq", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_create_qp = (struct rdma_lite_qp* (*)(struct rdma_lite_context * lite_ctx,
        struct rdma_lite_qp_attr * lite_qp_attr)) dlsym(g_rdma_lite_api_handle, "rdma_lite_create_qp");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_create_qp, "rdma_lite_create_qp", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_destroy_qp = (int (*)(struct rdma_lite_qp * lite_qp))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_destroy_qp");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_destroy_qp, "rdma_lite_destroy_qp", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_set_qp_sl = (int (*)(struct rdma_lite_qp * lite_qp, int sl))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_set_qp_sl");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_set_qp_sl, "rdma_lite_set_qp_sl", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_clean_qp = (int (*)(struct rdma_lite_qp *lite_qp))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_clean_qp");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_clean_qp, "rdma_lite_clean_qp");

    g_rdma_lite_ops.ra_rdma_lite_restore_snapshot = (int (*)(struct rdma_lite_context *lite_ctx))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_restore_snapshot");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_restore_snapshot, "rdma_lite_restore_snapshot");

    g_rdma_lite_ops.ra_rdma_lite_get_api_version = (unsigned int (*)(void))
        dlsym(g_rdma_lite_api_handle, "rdma_lite_get_api_version");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_get_api_version, "rdma_lite_get_api_version");

    return 0;
}

static int ra_rdma_lite_data_plane_api_init(void)
{
    g_rdma_lite_ops.ra_rdma_lite_post_send = (int (*)(struct rdma_lite_qp * lite_qp, struct rdma_lite_send_wr * wr,
        struct rdma_lite_send_wr * *bad_wr, struct rdma_lite_post_send_attr * attr,
        struct rdma_lite_post_send_resp * resp)) dlsym(g_rdma_lite_api_handle, "rdma_lite_post_send");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_post_send, "rdma_lite_post_send", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_post_recv = (int (*)(struct rdma_lite_qp * lite_qp, struct rdma_lite_recv_wr * wr,
        struct rdma_lite_recv_wr * *bad_wr)) dlsym(g_rdma_lite_api_handle, "rdma_lite_post_recv");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_post_recv, "rdma_lite_post_recv");

    g_rdma_lite_ops.ra_rdma_lite_poll_cq = (int (*)(struct rdma_lite_cq * lite_cq, int num_entries,
        struct rdma_lite_wc *lite_wc)) dlsym(g_rdma_lite_api_handle, "rdma_lite_poll_cq");
    DL_API_RET_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_poll_cq, "rdma_lite_poll_cq", g_rdma_lite_api_lock);

    g_rdma_lite_ops.ra_rdma_lite_poll_cq_v2 = (int (*)(struct rdma_lite_cq * lite_cq, int num_entries,
        struct rdma_lite_wc_v2 *lite_wc)) dlsym(g_rdma_lite_api_handle, "rdma_lite_poll_cq_v2");
    DL_API_PTR_IS_NULL_CHECK(g_rdma_lite_ops.ra_rdma_lite_poll_cq_v2, "rdma_lite_poll_cq_v2");
    return 0;
}
#endif

DL_ATTRI_VISI_DEF int ra_hdc_rdma_lite_api_init(void)
{
#ifndef HNS_ROCE_LLT
    int ret;

    pthread_mutex_lock(&g_rdma_lite_api_lock);
    if (g_rdma_lite_api_handle != NULL) {
        g_rdma_lite_api_refcnt++;
        pthread_mutex_unlock(&g_rdma_lite_api_lock);
        return 0;
    }

    ret = ra_hdc_open_rdma_lite_so();
    if (ret) {
        pthread_mutex_unlock(&g_rdma_lite_api_lock);
        hccp_err("hccp_dlopen[libascend_rdma_lite.so]"\
            "fail! ret=[%d][%s]. Please check rdma lite driver has been installed.", ret, dlerror());
        return ret;
    }

    ret = ra_rdma_lite_control_plane_api_init();
    if (ret != 0) {
        return ret;
    }

    ret = ra_rdma_lite_data_plane_api_init();
    if (ret != 0) {
        return ret;
    }

    g_rdma_lite_api_refcnt++;
    pthread_mutex_unlock(&g_rdma_lite_api_lock);
#endif
    return 0;
}

DL_ATTRI_VISI_DEF void ra_hdc_rdma_lite_api_deinit(void)
{
    pthread_mutex_lock(&g_rdma_lite_api_lock);
    if (g_rdma_lite_api_handle != NULL) {
        g_rdma_lite_api_refcnt--;
        if (g_rdma_lite_api_refcnt > 0) {
            pthread_mutex_unlock(&g_rdma_lite_api_lock);
            return;
        }
        (void)dlclose(g_rdma_lite_api_handle);
        g_rdma_lite_api_handle = NULL;
    }
    pthread_mutex_unlock(&g_rdma_lite_api_lock);

    return;
}

struct rdma_lite_context *ra_rdma_lite_alloc_ctx(u8 phy_id, struct dev_cap_info *cap)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_alloc_ctx == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_alloc_ctx is null");
        return NULL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_alloc_ctx(phy_id, cap);
}

void ra_rdma_lite_free_ctx(struct rdma_lite_context *lite_ctx)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_free_ctx == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_free_ctx is null");
        return;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_free_ctx(lite_ctx);
}

struct rdma_lite_cq *ra_rdma_lite_create_cq(struct rdma_lite_context *lite_ctx, struct rdma_lite_cq_attr *lite_cq_attr)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_create_cq == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_create_cq is null");
        return NULL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_create_cq(lite_ctx, lite_cq_attr);
}

int ra_rdma_lite_destroy_cq(struct rdma_lite_cq *lite_cq)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_destroy_cq == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_destroy_cq is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_destroy_cq(lite_cq);
}

int ra_rdma_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_poll_cq == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_poll_cq is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_poll_cq(lite_cq, num_entries, lite_wc);
}

int ra_rdma_lite_poll_cq_v2(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc_v2 *lite_wc)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_poll_cq_v2 == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_poll_cq_v2 is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_poll_cq_v2(lite_cq, num_entries, lite_wc);
}

struct rdma_lite_qp *ra_rdma_lite_create_qp(struct rdma_lite_context *lite_ctx, struct rdma_lite_qp_attr *lite_qp_attr)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_create_qp == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_create_qp is null");
        return NULL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_create_qp(lite_ctx, lite_qp_attr);
}

int ra_rdma_lite_destroy_qp(struct rdma_lite_qp *lite_qp)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_destroy_qp == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_destroy_qp is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_destroy_qp(lite_qp);
}

int ra_rdma_lite_post_send(struct rdma_lite_qp *lite_qp, struct rdma_lite_send_wr *wr,
    struct rdma_lite_send_wr **bad_wr, struct rdma_lite_post_send_attr *attr, struct rdma_lite_post_send_resp *resp)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_post_send == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_post_send is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_post_send(lite_qp, wr, bad_wr, attr, resp);
}

int ra_rdma_lite_post_recv(struct rdma_lite_qp *lite_qp, struct rdma_lite_recv_wr *wr,
    struct rdma_lite_recv_wr **bad_wr)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_post_recv == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_post_recv is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_post_recv(lite_qp, wr, bad_wr);
}

int ra_rdma_lite_set_qp_sl(struct rdma_lite_qp *lite_qp, unsigned char sl)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_set_qp_sl == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_set_qp_sl is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_set_qp_sl(lite_qp, sl);
}

int ra_rdma_lite_clean_qp(struct rdma_lite_qp *lite_qp)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_clean_qp == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_clean_qp is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_clean_qp(lite_qp);
}

int ra_rdma_lite_init_mem_pool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_init_mem_pool == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_init_mem_pool is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_init_mem_pool(lite_ctx, lite_mem_attr);
}

int ra_rdma_lite_deinit_mem_pool(struct rdma_lite_context *lite_ctx, u32 mem_idx)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_deinit_mem_pool == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("ra_rdma_lite_deinit_mem_pool is null");
        return -EINVAL;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_deinit_mem_pool(lite_ctx, mem_idx);
}

int ra_rdma_lite_restore_snapshot(struct rdma_lite_context *lite_ctx)
{
    if (g_rdma_lite_api_handle == NULL || g_rdma_lite_ops.ra_rdma_lite_restore_snapshot == NULL) {
#ifndef HNS_ROCE_LLT
        hccp_err("driver package may not support ra_rdma_lite_restore_snapshot interface, please change new one");
        return -ENOTSUPP;
#endif
    }
    return g_rdma_lite_ops.ra_rdma_lite_restore_snapshot(lite_ctx);
}

unsigned int ra_rdma_lite_get_api_version(void)
{
    if (g_rdma_lite_api_handle != NULL && g_rdma_lite_ops.ra_rdma_lite_get_api_version != NULL) {
        return g_rdma_lite_ops.ra_rdma_lite_get_api_version();
    }

    return 0;
}
