/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include "errno.h"
#include "dl_ibverbs_function.h"

static pthread_mutex_t g_roce_user_api_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_roce_user_api_refcnt = 0;
void *g_ibverbs_api_handle = NULL;
void *g_roce_user_api_handle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_ibverbs_ops g_ibverbs_ops;
struct rs_roce_user_ops g_roce_user_ops;
#else
struct rs_ibverbs_ops g_ibverbs_ops = {
    .rs_ibv_free_device_list = ibv_free_device_list,
    .rs_ibv_ack_cq_events = ibv_ack_cq_events,
    .rs_ibv_get_device_name = ibv_get_device_name,
    .rs_ibv_wc_status_str = ibv_wc_status_str,
    .rs_ibv_query_gid_type = ibv_query_gid_type,
    .rs_ibv_dereg_mr = ibv_dereg_mr,
    .rs_ibv_query_qp = ibv_query_qp,
    .rs_ibv_destroy_qp = ibv_destroy_qp,
    .rs_ibv_get_cq_event = ibv_get_cq_event,
    .rs_ibv_destroy_cq = ibv_destroy_cq,
    .rs_ibv_modify_qp = ibv_modify_qp,
    .rs_ibv_query_port = ibv_query_port,
    .rs_ibv_query_gid = ibv_query_gid,
    .rs_ibv_close_device = ibv_close_device,
    .rs_ibv_dealloc_pd = ibv_dealloc_pd,
    .rs_ibv_destroy_comp_channel = ibv_destroy_comp_channel,
    .rs_ibv_open_device = ibv_open_device,
    .rs_ibv_alloc_pd = ibv_alloc_pd,
    .rs_ibv_get_device_list = ibv_get_device_list,
    .rs_ibv_create_cq = ibv_create_cq,
    .rs_ibv_reg_mr = ibv_reg_mr,
    .rs_ibv_create_qp = ibv_create_qp,
    .rs_ibv_create_comp_channel = ibv_create_comp_channel,
    .rs_ibv_create_srq = ibv_create_srq,
    .rs_ibv_destroy_srq = ibv_destroy_srq,
    .rs_ibv_create_ah = ibv_create_ah,
    .rs_ibv_destroy_ah = ibv_destroy_ah,
};

struct rs_roce_user_ops g_roce_user_ops = {
    .rs_roce_set_tsqp_depth = roce_set_tsqp_depth,
    .rs_roce_get_tsqp_depth = roce_get_tsqp_depth,
    .rs_roce_get_roce_dev_data = roce_get_roce_dev_data,
    .rs_ibv_exp_query_notify = ibv_exp_query_notify,
    .rs_ibv_exp_post_send = ibv_exp_post_send,
    .rs_ibv_exp_reg_mr = ibv_exp_reg_mr,
    .rs_ibv_exp_create_qp = ibv_exp_create_qp,
    .rs_ibv_ext_post_send = ibv_ext_post_send,
    .rs_ibv_exp_create_cq = ibv_exp_create_cq,
    .rs_ibv_exp_query_device = ibv_exp_query_device,
    .rs_roce_init_mem_pool = roce_init_mem_pool,
    .rs_roce_deinit_mem_pool = roce_deinit_mem_pool,
    .rs_roce_query_qpc = roce_query_qpc,
    .rs_ibv_exp_create_ah = ibv_exp_create_ah,
    .rs_roce_mmap_ai_db_reg = roce_mmap_ai_db_reg,
    .rs_roce_unmmap_ai_db_reg = roce_unmmap_ai_db_reg,
    .rs_roce_get_cq_data_plane_info = roce_get_cq_data_plane_info,
    .rs_roce_get_qp_data_plane_info = roce_get_qp_data_plane_info,
    .rs_roce_remap_mr = roce_remap_mr,
    .rs_roce_get_api_version = roce_get_api_version,
};
#endif

STATIC int rs_context_ops_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_ibverbs_ops.rs_ibv_query_port = (int (*)(struct ibv_context*, uint8_t, struct ibv_port_attr*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_query_port");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_query_port, "ibv_query_port");

    g_ibverbs_ops.rs_ibv_query_gid = (int (*)(struct ibv_context*, uint8_t, int, union ibv_gid*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_query_gid");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_query_gid, "ibv_query_gid");

    g_ibverbs_ops.rs_ibv_alloc_pd = (struct ibv_pd* (*)(struct ibv_context*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_alloc_pd");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_alloc_pd, "ibv_alloc_pd");

    g_ibverbs_ops.rs_ibv_dealloc_pd = (int (*)(struct ibv_pd*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_dealloc_pd");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_dealloc_pd, "ibv_dealloc_pd");

    g_ibverbs_ops.rs_ibv_create_cq = (struct ibv_cq* (*)(struct ibv_context*, int, void*,
        struct ibv_comp_channel*, int))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_create_cq");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_create_cq, "ibv_create_cq");

    g_ibverbs_ops.rs_ibv_destroy_cq = (int (*)(struct ibv_cq*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_destroy_cq");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_destroy_cq, "ibv_destroy_cq");

    g_ibverbs_ops.rs_ibv_create_comp_channel = (struct ibv_comp_channel* (*)(struct ibv_context *))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_create_comp_channel");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_create_comp_channel, "ibv_create_comp_channel");

    g_ibverbs_ops.rs_ibv_destroy_comp_channel = (int (*)(struct ibv_comp_channel *))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_destroy_comp_channel");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_destroy_comp_channel, "ibv_destroy_comp_channel");

    g_ibverbs_ops.rs_ibv_create_srq = (struct ibv_srq* (*)(struct ibv_pd *pd, struct ibv_srq_init_attr *srq_init_attr))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_create_srq");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_create_srq, "ibv_create_srq");

    g_ibverbs_ops.rs_ibv_destroy_srq = (int (*)(struct ibv_srq*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_destroy_srq");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_destroy_srq, "ibv_destroy_srq");

    g_ibverbs_ops.rs_ibv_query_gid_type = (int (*)(struct ibv_context*, uint8_t, unsigned int,
        enum ibv_gid_type_sysfs*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_query_gid_type");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_query_gid_type, "ibv_query_gid_type");

    g_ibverbs_ops.rs_ibv_create_ah = (struct ibv_ah* (*)(struct ibv_pd *, struct ibv_ah_attr *))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_create_ah");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_create_ah, "ibv_create_ah");

    g_ibverbs_ops.rs_ibv_destroy_ah = (int (*)(struct ibv_ah *))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_destroy_ah");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_destroy_ah, "ibv_destroy_ah");
#endif
    return 0;
}

STATIC int rs_qp_ops_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_ibverbs_ops.rs_ibv_query_qp = (int (*)(struct ibv_qp*, struct ibv_qp_attr*, int, struct ibv_qp_init_attr*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_query_qp");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_query_qp, "ibv_query_qp");

    g_ibverbs_ops.rs_ibv_get_cq_event = (int (*)(struct ibv_comp_channel*, struct ibv_cq**, void**))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_get_cq_event");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_get_cq_event, "ibv_get_cq_event");

    g_ibverbs_ops.rs_ibv_ack_cq_events = (void (*)(struct ibv_cq*, unsigned int))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_ack_cq_events");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_ack_cq_events, "ibv_ack_cq_events");

    g_ibverbs_ops.rs_ibv_modify_qp = (int (*)(struct ibv_qp*, struct ibv_qp_attr*, int))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_modify_qp");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_modify_qp, "ibv_modify_qp");
#endif
    return 0;
}

STATIC int rs_pd_ops_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_ibverbs_ops.rs_ibv_reg_mr = (struct ibv_mr* (*)(struct ibv_pd*, void*, size_t, int))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_reg_mr");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_reg_mr, "ibv_reg_mr");

    g_ibverbs_ops.rs_ibv_dereg_mr = (int (*)(struct ibv_mr*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_dereg_mr");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_dereg_mr, "ibv_dereg_mr");

    g_ibverbs_ops.rs_ibv_create_qp = (struct ibv_qp* (*)(struct ibv_pd*, struct ibv_qp_init_attr*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_create_qp");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_create_qp, "ibv_create_qp");

    g_ibverbs_ops.rs_ibv_destroy_qp = (int (*)(struct ibv_qp*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_destroy_qp");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_destroy_qp, "ibv_destroy_qp");
#endif
    return 0;
}

STATIC int rs_device_ops_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_ibverbs_ops.rs_ibv_get_device_list = (struct ibv_device** (*)(int *))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_get_device_list");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_get_device_list, "ibv_get_device_list");

    g_ibverbs_ops.rs_ibv_free_device_list = (void (*)(struct ibv_device**))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_free_device_list");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_free_device_list, "ibv_free_device_list");

    g_ibverbs_ops.rs_ibv_get_device_name = (const char* (*)(struct ibv_device*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_get_device_name");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_get_device_name, "ibv_get_device_name");

    g_ibverbs_ops.rs_ibv_close_device = (int (*)(struct ibv_context*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_close_device");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_close_device, "ibv_close_device");

    g_ibverbs_ops.rs_ibv_open_device = (struct ibv_context* (*)(struct ibv_device*))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_open_device");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_open_device, "ibv_open_device");

    g_ibverbs_ops.rs_ibv_wc_status_str = (const char* (*)(enum ibv_wc_status))
        hccp_dlsym(g_ibverbs_api_handle, "ibv_wc_status_str");
    DL_API_RET_IS_NULL_CHECK(g_ibverbs_ops.rs_ibv_wc_status_str, "ibv_wc_status_str");
#endif
    return 0;
}

STATIC int rs_open_ibverbs_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_ibverbs_api_handle == NULL) {
        g_ibverbs_api_handle = hccp_dlopen("libibverbs.so", RTLD_NOW);
        if (g_ibverbs_api_handle != NULL) {
            return 0;
        }

        g_ibverbs_api_handle = hccp_dlopen("libibverbs.so.1", RTLD_NOW);
        if (g_ibverbs_api_handle != 0) {
            return 0;
        }
        return -EINVAL;
    } else {
            hccp_run_info("ibverbs_api dlopen again!");
        }
#endif
    return 0;
}

STATIC void rs_close_ibverbs_so(void)
{
    if (g_ibverbs_api_handle != NULL) {
        (void)hccp_dlclose(g_ibverbs_api_handle);
        g_ibverbs_api_handle = NULL;
    }
    return;
}

int rs_ibverbs_api_init(void)
{
#ifndef CA_CONFIG_LLT
    int ret;

    ret = rs_open_ibverbs_so();
    CHK_PRT_RETURN(ret, hccp_err("hccp_dlopen[libibverbs.so or libibverbs.so.1] fail! ret=[%d],"\
    "Please check network adapter driver has been installed", ret), ret);

    ret = rs_context_ops_api_init();
    if (ret) {
        hccp_err("[rs_context_ops_api_init]hccp_dlopen fail! ret=[%d]", ret);
        rs_close_ibverbs_so();
        return ret;
    }

    ret = rs_qp_ops_api_init();
    if (ret) {
        hccp_err("[rs_qp_ops_api_init]hccp_dlopen fail! ret=[%d]", ret);
        rs_close_ibverbs_so();
        return ret;
    }

    ret = rs_pd_ops_api_init();
    if (ret) {
        hccp_err("[rs_pd_ops_api_init]hccp_dlopen fail! ret=[%d]", ret);
        rs_close_ibverbs_so();
        return ret;
    }

    ret = rs_device_ops_api_init();
    if (ret) {
        hccp_err("[rs_device_ops_api_init]hccp_dlopen fail! ret=[%d]", ret);
        rs_close_ibverbs_so();
        return ret;
    }
#endif
    return 0;
}

STATIC int rs_open_roce_user_so(enum so_type *type)
{
    pthread_mutex_lock(&g_roce_user_api_lock);
#ifndef CA_CONFIG_LLT
    if (g_roce_user_api_handle == NULL) {
        g_roce_user_api_handle = hccp_dlopen("libhns-rdmav17.so", RTLD_NOW);
        if (g_roce_user_api_handle != NULL) {
            *type = SO_TYPE_EXP;
            goto out;
        }

        g_roce_user_api_handle = hccp_dlopen("libhns-rdmav25.so", RTLD_NOW);
        if (g_roce_user_api_handle != NULL) {
            *type = SO_TYPE_EXT;
            goto out;
        }

        g_roce_user_api_handle = hccp_dlopen("libhrn0-rdmav17.so", RTLD_NOW);
        if (g_roce_user_api_handle != NULL) {
            *type = SO_TYPE_EXP;
            goto out;
        }

        pthread_mutex_unlock(&g_roce_user_api_lock);
        return -EINVAL;
    } else {
        hccp_run_info("dlopen roce_user_api again, g_roce_user_api_refcnt:%d", g_roce_user_api_refcnt + 1);
    }

out:
#endif
    g_roce_user_api_refcnt++;
    pthread_mutex_unlock(&g_roce_user_api_lock);
    return 0;
}

STATIC void rs_close_roce_user_so(void)
{
    pthread_mutex_lock(&g_roce_user_api_lock);
#ifndef CA_CONFIG_LLT
    if (g_roce_user_api_handle != NULL) {
        g_roce_user_api_refcnt--;
        if (g_roce_user_api_refcnt > 0) {
            goto out;
        }

        hccp_run_info("dlclose roce_user_api, g_roce_user_api_refcnt:%d", g_roce_user_api_refcnt);
        (void)hccp_dlclose(g_roce_user_api_handle);
        g_roce_user_api_handle = NULL;
        g_roce_user_api_refcnt = 0;
    }
out:
#endif
    pthread_mutex_unlock(&g_roce_user_api_lock);
    return;
}

STATIC int rs_roce_user_ibv_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_roce_user_ops.rs_ibv_exp_create_qp = (struct ibv_qp* (*)(struct ibv_pd *pd,
        struct ibv_exp_qp_init_attr *qp_init_attr, struct rdma_lite_device_qp_attr *qp_resp))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_create_qp");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_create_qp, "ibv_exp_create_qp");
    g_roce_user_ops.rs_ibv_exp_reg_mr = (struct ibv_mr* (*)(struct ibv_pd *pd, void *addr, size_t length,
        int access, struct roce_process_sign roce_sign))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_reg_mr");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_reg_mr, "ibv_exp_reg_mr");
    g_roce_user_ops.rs_ibv_exp_query_notify = (int (*)(struct ibv_context *context,
        unsigned long long *notify_va, unsigned long long *size))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_query_notify");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_query_notify, "ibv_exp_query_notify");
    g_roce_user_ops.rs_ibv_exp_post_send = (int (*)(struct ibv_qp *qp, struct ibv_send_wr *wr,
        struct ibv_send_wr **bad_wr, struct wr_exp_rsp *exp_rsp))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_post_send");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_post_send, "ibv_exp_post_send");
    g_roce_user_ops.rs_ibv_exp_create_cq = (struct ibv_cq* (*)(struct ibv_context*, int, void*,
        struct ibv_comp_channel*, int, struct rdma_lite_device_cq_init_attr*, struct rdma_lite_device_cq_attr*))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_create_cq");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_create_cq, "ibv_exp_create_cq");
    g_roce_user_ops.rs_ibv_exp_query_device = (int (*)(struct ibv_context*, struct dev_cap_info*))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_query_device");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_query_device, "ibv_exp_query_device");
    g_roce_user_ops.rs_ibv_exp_create_ah = (struct ibv_ah* (*)(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx))
        hccp_dlsym(g_roce_user_api_handle, "ibv_exp_create_ah");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_ibv_exp_create_ah, "ibv_exp_create_ah");
#endif
    return 0;
}

STATIC int rs_roce_user_drv_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_roce_user_ops.rs_roce_get_roce_dev_data = (int (*)(const char *dev_name, struct roce_dev_data *rdev_data))
        hccp_dlsym(g_roce_user_api_handle, "roce_get_roce_dev_data");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_get_roce_dev_data, "roce_get_roce_dev_data");
    g_roce_user_ops.rs_roce_get_tsqp_depth = (int (*)(const char *dev_name, unsigned int rdev_index,
        unsigned int *temp_depth, unsigned int *qp_num, unsigned int *sq_depth))
        hccp_dlsym(g_roce_user_api_handle, "roce_get_tsqp_depth");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_get_tsqp_depth, "roce_get_tsqp_depth");
    g_roce_user_ops.rs_roce_set_tsqp_depth = (int (*)(const char *dev_name, unsigned int rdev_index,
        unsigned int temp_depth, unsigned int *qp_num, unsigned int *sq_depth))
        hccp_dlsym(g_roce_user_api_handle, "roce_set_tsqp_depth");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_set_tsqp_depth, "roce_set_tsqp_depth");
    g_roce_user_ops.rs_roce_init_mem_pool = (int (*)(const struct roce_mem_cq_qp_attr *,
        struct rdma_lite_device_mem_attr *, unsigned int)) hccp_dlsym(g_roce_user_api_handle, "roce_init_mem_pool");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_init_mem_pool, "roce_init_mem_pool");
    g_roce_user_ops.rs_roce_deinit_mem_pool = (int (*)(unsigned int))
        hccp_dlsym(g_roce_user_api_handle, "roce_deinit_mem_pool");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_deinit_mem_pool, "roce_deinit_mem_pool");
    g_roce_user_ops.rs_roce_query_qpc = (int (*)(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attr_val,
        unsigned int attr_mask)) hccp_dlsym(g_roce_user_api_handle, "roce_query_qpc");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_query_qpc, "roce_query_qpc");
    g_roce_user_ops.rs_roce_mmap_ai_db_reg = (int (*)(struct ibv_context *context, unsigned int tgid))
        hccp_dlsym(g_roce_user_api_handle, "roce_mmap_ai_db_reg");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_mmap_ai_db_reg, "roce_mmap_ai_db_reg");
    g_roce_user_ops.rs_roce_unmmap_ai_db_reg = (int (*)(struct ibv_context *context))
        hccp_dlsym(g_roce_user_api_handle, "roce_unmmap_ai_db_reg");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_unmmap_ai_db_reg, "roce_unmmap_ai_db_reg");
    g_roce_user_ops.rs_roce_get_cq_data_plane_info = (int (*)(struct ibv_cq *cq,
        struct hns_roce_cq_data_plane_info *info)) hccp_dlsym(g_roce_user_api_handle, "roce_get_cq_data_plane_info");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_get_cq_data_plane_info, "roce_get_cq_data_plane_info");
    g_roce_user_ops.rs_roce_get_qp_data_plane_info = (int (*)(struct ibv_qp *qp,
        struct hns_roce_qp_data_plane_info *info)) hccp_dlsym(g_roce_user_api_handle, "roce_get_qp_data_plane_info");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_get_qp_data_plane_info, "roce_get_qp_data_plane_info");
    g_roce_user_ops.rs_roce_remap_mr = (int (*)(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[],
        unsigned int num)) hccp_dlsym(g_roce_user_api_handle, "roce_remap_mr");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_remap_mr, "roce_remap_mr");
    g_roce_user_ops.rs_roce_get_api_version = (unsigned int (*)(void))
        hccp_dlsym(g_roce_user_api_handle, "roce_get_api_version");
    DL_API_RET_IS_NULL_CHECK(g_roce_user_ops.rs_roce_get_api_version, "roce_get_api_version");
#endif
    return 0;
}

int rs_roce_user_api_init(void)
{
    enum so_type type = SO_TYPE_INVALID;
    int ret = 0;

    ret = rs_open_roce_user_so(&type);
    CHK_PRT_RETURN(ret != 0, hccp_err("hccp_dlopen[libhns-rdmav17.so or libhns-rdmav25.so or libhrn0-rdmav17.so]"
        "fail! ret=[%d]. Please check network adapter driver has been installed.", ret), ret);

#ifndef CA_CONFIG_LLT
    ret = rs_roce_user_ibv_api_init();
    if (ret != 0) {
        hccp_err("rs_roce_user_ibv_api_init fail! ret=[%d]", ret);
        goto close_roce_user_so;
    }

    ret = rs_roce_user_drv_api_init();
    if (ret != 0) {
        hccp_err("rs_roce_user_drv_api_init fail! ret=[%d]", ret);
        goto close_roce_user_so;
    }

    if (type == SO_TYPE_EXT) {
        g_roce_user_ops.rs_ibv_ext_post_send = (int (*)(struct ibv_qp *qp, struct ibv_send_wr *wr,
            struct ibv_send_wr **bad_wr, struct ibv_post_send_ext_attr *ext_attr,
            struct ibv_post_send_ext_resp *ext_resp))
            hccp_dlsym(g_roce_user_api_handle, "ibv_ext_post_send");
        if (g_roce_user_ops.rs_ibv_ext_post_send == NULL) {
            ret = -EINVAL;
            hccp_err("ibv_ext_post_send is null");
            goto close_roce_user_so;
        }
    }
    return 0;

close_roce_user_so:
    rs_close_roce_user_so();
#endif
    return ret;
}

DL_ATTRI_VISI_DEF int rs_api_init(void)
{
#ifndef NOT_INIT_IBVERBS
#ifndef CA_CONFIG_LLT
    int ret;
    ret = rs_ibverbs_api_init();
    CHK_PRT_RETURN(ret, hccp_err("rs_ibverbs_api_init fail! ret=[%d]", ret), ret);
#ifdef CUSTOM_INTERFACE
    ret = rs_roce_user_api_init();
    if (ret != 0) {
        hccp_err("rs_roce_user_api_init fail! ret=[%d]", ret);
        rs_close_ibverbs_so();
        return ret;
    }
#endif
#endif
#endif
    return 0;
}

DL_ATTRI_VISI_DEF void rs_api_deinit(void)
{
    rs_close_ibverbs_so();
    rs_close_roce_user_so();
    return;
}

struct ibv_mr *rs_ibv_exp_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access,
    struct roce_process_sign roce_sign)
{
    if (g_roce_user_ops.rs_ibv_exp_reg_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_reg_mr is null");
        return NULL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_reg_mr(pd, addr, length, access, roce_sign);
}

struct ibv_qp *rs_ibv_exp_create_qp(
    struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qp_init_attr, struct rdma_lite_device_qp_attr *qp_resp)
{
    if (g_roce_user_ops.rs_ibv_exp_create_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_create_qp is null");
        return NULL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_create_qp(pd, qp_init_attr, qp_resp);
}

int rs_roce_set_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth)
{
    if (g_roce_user_ops.rs_roce_set_tsqp_depth == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_set_tsqp_depth is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_set_tsqp_depth(dev_name, rdev_index, temp_depth, qp_num, sq_depth);
}

int rs_roce_get_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth)
{
    if (g_roce_user_ops.rs_roce_get_tsqp_depth == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_tsqp_depth is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_get_tsqp_depth(dev_name, rdev_index, temp_depth, qp_num, sq_depth);
}

int rs_roce_get_roce_dev_data(const char *dev_name, struct roce_dev_data *rdev_data)
{
    if (g_roce_user_ops.rs_roce_get_roce_dev_data == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_roce_dev_data is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_get_roce_dev_data(dev_name, rdev_data);
}

int rs_ibv_exp_query_notify(struct ibv_context *context, unsigned long long *notify_va,
    unsigned long long *size)
{
    if (g_roce_user_ops.rs_ibv_exp_query_notify == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_query_notify is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_query_notify(context, notify_va, size);
}
int rs_ibv_exp_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct wr_exp_rsp *exp_rsp)
{
    if (g_roce_user_ops.rs_ibv_exp_post_send == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_post_send is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_post_send(qp, wr, bad_wr, exp_rsp);
}

int rs_ibv_ext_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct ibv_post_send_ext_attr *ext_attr, struct ibv_post_send_ext_resp *ext_resp)
{
    if (g_roce_user_ops.rs_ibv_ext_post_send == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_ext_post_send is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_ibv_ext_post_send(qp, wr, bad_wr, ext_attr, ext_resp);
}

void rs_ibv_free_device_list(struct ibv_device **list)
{
    if (g_ibverbs_ops.rs_ibv_free_device_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_free_device_list is null");
        return;
#endif
    }
    g_ibverbs_ops.rs_ibv_free_device_list(list);
}

void rs_ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents)
{
    if (g_ibverbs_ops.rs_ibv_ack_cq_events == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_ack_cq_events is null");
        return;
#endif
    }
    g_ibverbs_ops.rs_ibv_ack_cq_events(cq, nevents);
}

const char *rs_ibv_get_device_name(struct ibv_device *device)
{
    if (g_ibverbs_ops.rs_ibv_get_device_name == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_get_device_name is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_get_device_name(device);
}

const char *rs_ibv_wc_status_str(enum ibv_wc_status status)
{
    if (g_ibverbs_ops.rs_ibv_wc_status_str == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_wc_status_str is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_wc_status_str(status);
}

int rs_ibv_query_gid_type(struct ibv_context *context, uint8_t port_num, unsigned int index,
    enum ibv_gid_type_sysfs *type)
{
    if (g_ibverbs_ops.rs_ibv_query_gid_type == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_gid_type is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_query_gid_type(context, port_num, index, type);
}

int rs_ibv_dereg_mr(struct ibv_mr *mr)
{
    if (g_ibverbs_ops.rs_ibv_dereg_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_dereg_mr is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_dereg_mr(mr);
}

int rs_ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{
    return ibv_post_send(qp, wr, bad_wr);
}

int rs_ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
    return ibv_post_recv(qp, wr, bad_wr);
}

int rs_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
    if (g_ibverbs_ops.rs_ibv_query_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_qp is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_query_qp(qp, attr, attr_mask, init_attr);
}

int rs_ibv_destroy_qp(struct ibv_qp *qp)
{
    if (g_ibverbs_ops.rs_ibv_destroy_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_qp is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_destroy_qp(qp);
}

int rs_ibv_get_cq_event(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context)
{
    if (g_ibverbs_ops.rs_ibv_get_cq_event == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_get_cq_event is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_get_cq_event(channel, cq, cq_context);
}

int rs_ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    return ibv_poll_cq(cq, num_entries, wc);
}

int rs_ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only)
{
    return ibv_req_notify_cq(cq, solicited_only);
}

int rs_ibv_destroy_cq(struct ibv_cq *cq)
{
    if (g_ibverbs_ops.rs_ibv_destroy_cq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_cq is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_destroy_cq(cq);
}

int rs_ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask)
{
    if (g_ibverbs_ops.rs_ibv_modify_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_modify_qp is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_modify_qp(qp, attr, attr_mask);
}

int rs_ibv_query_port(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr)
{
    if (g_ibverbs_ops.rs_ibv_query_port == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_port is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_query_port(context, port_num, port_attr);
}

int rs_ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid)
{
    if (g_ibverbs_ops.rs_ibv_query_gid == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_gid is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_query_gid(context, port_num, index, gid);
}

int rs_ibv_close_device(struct ibv_context *context)
{
    if (g_ibverbs_ops.rs_ibv_close_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_close_device is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_close_device(context);
}

int rs_ibv_dealloc_pd(struct ibv_pd *pd)
{
    if (g_ibverbs_ops.rs_ibv_dealloc_pd == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_dealloc_pd is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_dealloc_pd(pd);
}

int rs_ibv_destroy_comp_channel(struct ibv_comp_channel *channel)
{
    if (g_ibverbs_ops.rs_ibv_destroy_comp_channel == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_comp_channel is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_destroy_comp_channel(channel);
}

struct ibv_context *rs_ibv_open_device(struct ibv_device *device)
{
    if (g_ibverbs_ops.rs_ibv_open_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_open_device is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_open_device(device);
}

struct ibv_pd *rs_ibv_alloc_pd(struct ibv_context *context)
{
    if (g_ibverbs_ops.rs_ibv_alloc_pd == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_alloc_pd is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_alloc_pd(context);
}

struct ibv_device **rs_ibv_get_device_list(int *num_devices)
{
    if (g_ibverbs_ops.rs_ibv_get_device_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_get_device_list is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_get_device_list(num_devices);
}

struct ibv_cq *rs_ibv_create_cq(struct ibv_context *context, int cqe, void *cq_context,
    struct ibv_comp_channel *channel, int comp_vector)
{
    if (g_ibverbs_ops.rs_ibv_create_cq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_cq is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_create_cq(context, cqe, cq_context, channel, comp_vector);
}

struct ibv_mr *rs_ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access)
{
    if (g_ibverbs_ops.rs_ibv_reg_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_reg_mr is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_reg_mr(pd, addr, length, access);
}

struct ibv_qp *rs_ibv_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr)
{
    if (g_ibverbs_ops.rs_ibv_create_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_qp is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_create_qp(pd, qp_init_attr);
}

struct ibv_comp_channel *rs_ibv_create_comp_channel(struct ibv_context *context)
{
    if (g_ibverbs_ops.rs_ibv_create_comp_channel == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_comp_channel is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_create_comp_channel(context);
}

struct ibv_srq *rs_ibv_create_srq(struct ibv_pd *pd, struct ibv_srq_init_attr *srq_init_attr)
{
    if (g_ibverbs_ops.rs_ibv_create_srq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_srq is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_create_srq(pd, srq_init_attr);
}

int rs_ibv_destroy_srq(struct ibv_srq *srq)
{
    if (g_ibverbs_ops.rs_ibv_destroy_srq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_srq is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_destroy_srq(srq);
}

struct ibv_ah *rs_ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
    if (g_ibverbs_ops.rs_ibv_create_ah == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_ah is null");
        return NULL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_create_ah(pd, attr);
}

int rs_ibv_destroy_ah(struct ibv_ah *ah)
{
    if (g_ibverbs_ops.rs_ibv_destroy_ah == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_ah is null");
        return -EINVAL;
#endif
    }
    return g_ibverbs_ops.rs_ibv_destroy_ah(ah);
}

struct ibv_cq *rs_ibv_exp_create_cq(struct ibv_context *context, int cqe, void *cq_context,
    struct ibv_comp_channel *channel, int comp_vector, struct rdma_lite_device_cq_init_attr *attr,
    struct rdma_lite_device_cq_attr *cq_resp)
{
    if (g_roce_user_ops.rs_ibv_exp_create_cq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_create_cq is null");
        return NULL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_create_cq(context, cqe, cq_context, channel, comp_vector, attr, cq_resp);
}

int rs_ibv_exp_query_device(struct ibv_context *context, struct dev_cap_info *cap)
{
    if (g_roce_user_ops.rs_ibv_exp_query_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_query_device is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_query_device(context, cap);
}

int rs_roce_init_mem_pool(const struct roce_mem_cq_qp_attr *mem_attr, struct rdma_lite_device_mem_attr *mem_data,
    unsigned int dev_id)
{
    if (g_roce_user_ops.rs_roce_init_mem_pool == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_init_mem_pool is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_init_mem_pool(mem_attr, mem_data, dev_id);
}

int rs_roce_deinit_mem_pool(unsigned int mem_idx)
{
    if (g_roce_user_ops.rs_roce_deinit_mem_pool == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_deinit_mem_pool is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_deinit_mem_pool(mem_idx);
}

int rs_roce_query_qpc(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attr_val, unsigned int attr_mask)
{
    if (g_roce_user_ops.rs_roce_query_qpc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_query_qpc is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_query_qpc(qp, attr_val, attr_mask);
}

struct ibv_ah *rs_ibv_exp_create_ah(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx)
{
    if (g_roce_user_ops.rs_ibv_exp_create_ah == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_create_ah is null");
        return NULL;
#endif
    }
    return g_roce_user_ops.rs_ibv_exp_create_ah(pd, attrx);
}

int rs_roce_mmap_ai_db_reg(struct ibv_context *context, unsigned int tgid)
{
    if (g_roce_user_ops.rs_roce_mmap_ai_db_reg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_mmap_ai_db_reg is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_mmap_ai_db_reg(context, tgid);
}

int rs_roce_unmmap_ai_db_reg(struct ibv_context *context)
{
    if (g_roce_user_ops.rs_roce_unmmap_ai_db_reg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_unmmap_ai_db_reg is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_unmmap_ai_db_reg(context);
}

int rs_roce_get_cq_data_plane_info(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info)
{
    if (g_roce_user_ops.rs_roce_get_cq_data_plane_info == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_cq_data_plane_info is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_get_cq_data_plane_info(cq, info);
}

int rs_roce_get_qp_data_plane_info(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info)
{
    if (g_roce_user_ops.rs_roce_get_qp_data_plane_info == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_qp_data_plane_info is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_get_qp_data_plane_info(qp, info);
}

int rs_roce_remap_mr(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[], unsigned int num)
{
    if (g_roce_user_ops.rs_roce_remap_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_remap_mr is null");
        return -EINVAL;
#endif
    }
    return g_roce_user_ops.rs_roce_remap_mr(mr, info, num);
}

unsigned int rs_roce_get_api_version(void)
{
    if (g_roce_user_ops.rs_roce_get_api_version != NULL) {
        return g_roce_user_ops.rs_roce_get_api_version();
    }

    return 0;
}
