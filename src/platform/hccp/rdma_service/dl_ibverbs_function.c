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

static pthread_mutex_t gRoceUserApiLock = PTHREAD_MUTEX_INITIALIZER;
static int gRoceUserApiRefcnt = 0;
void *gIbverbsApiHandle = NULL;
void *gRoceUserApiHandle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_ibverbs_ops gIbverbsOps;
struct rs_roce_user_ops gRoceUserOps;
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

STATIC int RsContextOpsApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gIbverbsOps.rs_ibv_query_port = (int (*)(struct ibv_context*, uint8_t, struct ibv_port_attr*))
        HccpDlsym(gIbverbsApiHandle, "ibv_query_port");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_query_port, "ibv_query_port");

    gIbverbsOps.rs_ibv_query_gid = (int (*)(struct ibv_context*, uint8_t, int, union ibv_gid*))
        HccpDlsym(gIbverbsApiHandle, "ibv_query_gid");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_query_gid, "ibv_query_gid");

    gIbverbsOps.rs_ibv_alloc_pd = (struct ibv_pd* (*)(struct ibv_context*))
        HccpDlsym(gIbverbsApiHandle, "ibv_alloc_pd");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_alloc_pd, "ibv_alloc_pd");

    gIbverbsOps.rs_ibv_dealloc_pd = (int (*)(struct ibv_pd*))
        HccpDlsym(gIbverbsApiHandle, "ibv_dealloc_pd");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_dealloc_pd, "ibv_dealloc_pd");

    gIbverbsOps.rs_ibv_create_cq = (struct ibv_cq* (*)(struct ibv_context*, int, void*,
        struct ibv_comp_channel*, int))
        HccpDlsym(gIbverbsApiHandle, "ibv_create_cq");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_create_cq, "ibv_create_cq");

    gIbverbsOps.rs_ibv_destroy_cq = (int (*)(struct ibv_cq*))
        HccpDlsym(gIbverbsApiHandle, "ibv_destroy_cq");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_destroy_cq, "ibv_destroy_cq");

    gIbverbsOps.rs_ibv_create_comp_channel = (struct ibv_comp_channel* (*)(struct ibv_context *))
        HccpDlsym(gIbverbsApiHandle, "ibv_create_comp_channel");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_create_comp_channel, "ibv_create_comp_channel");

    gIbverbsOps.rs_ibv_destroy_comp_channel = (int (*)(struct ibv_comp_channel *))
        HccpDlsym(gIbverbsApiHandle, "ibv_destroy_comp_channel");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_destroy_comp_channel, "ibv_destroy_comp_channel");

    gIbverbsOps.rs_ibv_create_srq = (struct ibv_srq* (*)(struct ibv_pd *pd, struct ibv_srq_init_attr *srqInitAttr))
        HccpDlsym(gIbverbsApiHandle, "ibv_create_srq");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_create_srq, "ibv_create_srq");

    gIbverbsOps.rs_ibv_destroy_srq = (int (*)(struct ibv_srq*))
        HccpDlsym(gIbverbsApiHandle, "ibv_destroy_srq");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_destroy_srq, "ibv_destroy_srq");

    gIbverbsOps.rs_ibv_query_gid_type = (int (*)(struct ibv_context*, uint8_t, unsigned int,
        enum ibv_gid_type_sysfs*))
        HccpDlsym(gIbverbsApiHandle, "ibv_query_gid_type");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_query_gid_type, "ibv_query_gid_type");

    gIbverbsOps.rs_ibv_create_ah = (struct ibv_ah* (*)(struct ibv_pd *, struct ibv_ah_attr *))
        HccpDlsym(gIbverbsApiHandle, "ibv_create_ah");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_create_ah, "ibv_create_ah");

    gIbverbsOps.rs_ibv_destroy_ah = (int (*)(struct ibv_ah *))
        HccpDlsym(gIbverbsApiHandle, "ibv_destroy_ah");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_destroy_ah, "ibv_destroy_ah");
#endif
    return 0;
}

STATIC int RsQpOpsApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gIbverbsOps.rs_ibv_query_qp = (int (*)(struct ibv_qp*, struct ibv_qp_attr*, int, struct ibv_qp_init_attr*))
        HccpDlsym(gIbverbsApiHandle, "ibv_query_qp");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_query_qp, "ibv_query_qp");

    gIbverbsOps.rs_ibv_get_cq_event = (int (*)(struct ibv_comp_channel*, struct ibv_cq**, void**))
        HccpDlsym(gIbverbsApiHandle, "ibv_get_cq_event");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_get_cq_event, "ibv_get_cq_event");

    gIbverbsOps.rs_ibv_ack_cq_events = (void (*)(struct ibv_cq*, unsigned int))
        HccpDlsym(gIbverbsApiHandle, "ibv_ack_cq_events");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_ack_cq_events, "ibv_ack_cq_events");

    gIbverbsOps.rs_ibv_modify_qp = (int (*)(struct ibv_qp*, struct ibv_qp_attr*, int))
        HccpDlsym(gIbverbsApiHandle, "ibv_modify_qp");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_modify_qp, "ibv_modify_qp");
#endif
    return 0;
}

STATIC int RsPdOpsApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gIbverbsOps.rs_ibv_reg_mr = (struct ibv_mr* (*)(struct ibv_pd*, void*, size_t, int))
        HccpDlsym(gIbverbsApiHandle, "ibv_reg_mr");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_reg_mr, "ibv_reg_mr");

    gIbverbsOps.rs_ibv_dereg_mr = (int (*)(struct ibv_mr*))
        HccpDlsym(gIbverbsApiHandle, "ibv_dereg_mr");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_dereg_mr, "ibv_dereg_mr");

    gIbverbsOps.rs_ibv_create_qp = (struct ibv_qp* (*)(struct ibv_pd*, struct ibv_qp_init_attr*))
        HccpDlsym(gIbverbsApiHandle, "ibv_create_qp");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_create_qp, "ibv_create_qp");

    gIbverbsOps.rs_ibv_destroy_qp = (int (*)(struct ibv_qp*))
        HccpDlsym(gIbverbsApiHandle, "ibv_destroy_qp");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_destroy_qp, "ibv_destroy_qp");
#endif
    return 0;
}

STATIC int RsDeviceOpsApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gIbverbsOps.rs_ibv_get_device_list = (struct ibv_device** (*)(int *))
        HccpDlsym(gIbverbsApiHandle, "ibv_get_device_list");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_get_device_list, "ibv_get_device_list");

    gIbverbsOps.rs_ibv_free_device_list = (void (*)(struct ibv_device**))
        HccpDlsym(gIbverbsApiHandle, "ibv_free_device_list");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_free_device_list, "ibv_free_device_list");

    gIbverbsOps.rs_ibv_get_device_name = (const char* (*)(struct ibv_device*))
        HccpDlsym(gIbverbsApiHandle, "ibv_get_device_name");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_get_device_name, "ibv_get_device_name");

    gIbverbsOps.rs_ibv_close_device = (int (*)(struct ibv_context*))
        HccpDlsym(gIbverbsApiHandle, "ibv_close_device");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_close_device, "ibv_close_device");

    gIbverbsOps.rs_ibv_open_device = (struct ibv_context* (*)(struct ibv_device*))
        HccpDlsym(gIbverbsApiHandle, "ibv_open_device");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_open_device, "ibv_open_device");

    gIbverbsOps.rs_ibv_wc_status_str = (const char* (*)(enum ibv_wc_status))
        HccpDlsym(gIbverbsApiHandle, "ibv_wc_status_str");
    DL_API_RET_IS_NULL_CHECK(gIbverbsOps.rs_ibv_wc_status_str, "ibv_wc_status_str");
#endif
    return 0;
}

STATIC int RsOpenIbverbsSo(void)
{
#ifndef CA_CONFIG_LLT
    if (gIbverbsApiHandle == NULL) {
        gIbverbsApiHandle = HccpDlopen("libibverbs.so", RTLD_NOW);
        if (gIbverbsApiHandle != NULL) {
            return 0;
        }

        gIbverbsApiHandle = HccpDlopen("libibverbs.so.1", RTLD_NOW);
        if (gIbverbsApiHandle != 0) {
            return 0;
        }
        return -EINVAL;
    } else {
            hccp_run_info("ibverbs_api dlopen again!");
        }
#endif
    return 0;
}

STATIC void RsCloseIbverbsSo(void)
{
    if (gIbverbsApiHandle != NULL) {
        (void)HccpDlclose(gIbverbsApiHandle);
        gIbverbsApiHandle = NULL;
    }
    return;
}

int RsIbverbsApiInit(void)
{
#ifndef CA_CONFIG_LLT
    int ret;

    ret = RsOpenIbverbsSo();
    CHK_PRT_RETURN(ret, hccp_err("hccp_dlopen[libibverbs.so or libibverbs.so.1] failed! ret=[%d],"\
    "Please check network adapter driver has been installed", ret), ret);

    ret = RsContextOpsApiInit();
    if (ret) {
        hccp_err("[rs_context_ops_api_init]hccp_dlopen failed! ret=[%d]", ret);
        RsCloseIbverbsSo();
        return ret;
    }

    ret = RsQpOpsApiInit();
    if (ret) {
        hccp_err("[rs_qp_ops_api_init]hccp_dlopen failed! ret=[%d]", ret);
        RsCloseIbverbsSo();
        return ret;
    }

    ret = RsPdOpsApiInit();
    if (ret) {
        hccp_err("[rs_pd_ops_api_init]hccp_dlopen failed! ret=[%d]", ret);
        RsCloseIbverbsSo();
        return ret;
    }

    ret = RsDeviceOpsApiInit();
    if (ret) {
        hccp_err("[rs_device_ops_api_init]hccp_dlopen failed! ret=[%d]", ret);
        RsCloseIbverbsSo();
        return ret;
    }
#endif
    return 0;
}

STATIC int RsOpenRoceUserSo(enum so_type *type)
{
    pthread_mutex_lock(&gRoceUserApiLock);
#ifndef CA_CONFIG_LLT
    if (gRoceUserApiHandle == NULL) {
        gRoceUserApiHandle = HccpDlopen("libhns-rdmav17.so", RTLD_NOW);
        if (gRoceUserApiHandle != NULL) {
            *type = SO_TYPE_EXP;
            goto out;
        }

        gRoceUserApiHandle = HccpDlopen("libhns-rdmav25.so", RTLD_NOW);
        if (gRoceUserApiHandle != NULL) {
            *type = SO_TYPE_EXT;
            goto out;
        }

        gRoceUserApiHandle = HccpDlopen("libhrn0-rdmav17.so", RTLD_NOW);
        if (gRoceUserApiHandle != NULL) {
            *type = SO_TYPE_EXP;
            goto out;
        }

        pthread_mutex_unlock(&gRoceUserApiLock);
        return -EINVAL;
    } else {
        hccp_run_info("dlopen roce_user_api again, gRoceUserApiRefcnt:%d", gRoceUserApiRefcnt + 1);
    }

out:
#endif
    gRoceUserApiRefcnt++;
    pthread_mutex_unlock(&gRoceUserApiLock);
    return 0;
}

STATIC void RsCloseRoceUserSo(void)
{
    pthread_mutex_lock(&gRoceUserApiLock);
#ifndef CA_CONFIG_LLT
    if (gRoceUserApiHandle != NULL) {
        gRoceUserApiRefcnt--;
        if (gRoceUserApiRefcnt > 0) {
            goto out;
        }

        hccp_run_info("dlclose roce_user_api, gRoceUserApiRefcnt:%d", gRoceUserApiRefcnt);
        (void)HccpDlclose(gRoceUserApiHandle);
        gRoceUserApiHandle = NULL;
        gRoceUserApiRefcnt = 0;
    }
out:
#endif
    pthread_mutex_unlock(&gRoceUserApiLock);
    return;
}

STATIC int RsRoceUserIbvApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gRoceUserOps.rs_ibv_exp_create_qp = (struct ibv_qp* (*)(struct ibv_pd *pd,
        struct ibv_exp_qp_init_attr *qpInitAttr, struct rdma_lite_device_qp_attr *qpResp))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_create_qp");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_create_qp, "ibv_exp_create_qp");
    gRoceUserOps.rs_ibv_exp_reg_mr = (struct ibv_mr* (*)(struct ibv_pd *pd, void *addr, size_t length,
        int access, struct roce_process_sign roceSign))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_reg_mr");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_reg_mr, "ibv_exp_reg_mr");
    gRoceUserOps.rs_ibv_exp_query_notify = (int (*)(struct ibv_context *context,
        unsigned long long *notifyVa, unsigned long long *size))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_query_notify");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_query_notify, "ibv_exp_query_notify");
    gRoceUserOps.rs_ibv_exp_post_send = (int (*)(struct ibv_qp *qp, struct ibv_send_wr *wr,
        struct ibv_send_wr **badWr, struct wr_exp_rsp *expRsp))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_post_send");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_post_send, "ibv_exp_post_send");
    gRoceUserOps.rs_ibv_exp_create_cq = (struct ibv_cq* (*)(struct ibv_context*, int, void*,
        struct ibv_comp_channel*, int, struct rdma_lite_device_cq_init_attr*, struct rdma_lite_device_cq_attr*))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_create_cq");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_create_cq, "ibv_exp_create_cq");
    gRoceUserOps.rs_ibv_exp_query_device = (int (*)(struct ibv_context*, struct dev_cap_info*))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_query_device");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_query_device, "ibv_exp_query_device");
    gRoceUserOps.rs_ibv_exp_create_ah = (struct ibv_ah* (*)(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx))
        HccpDlsym(gRoceUserApiHandle, "ibv_exp_create_ah");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_ibv_exp_create_ah, "ibv_exp_create_ah");
#endif
    return 0;
}

STATIC int RsRoceUserDrvApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gRoceUserOps.rs_roce_get_roce_dev_data = (int (*)(const char *devName, struct roce_dev_data *rdevData))
        HccpDlsym(gRoceUserApiHandle, "roce_get_roce_dev_data");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_get_roce_dev_data, "roce_get_roce_dev_data");
    gRoceUserOps.rs_roce_get_tsqp_depth = (int (*)(const char *devName, unsigned int rdevIndex,
        unsigned int *tempDepth, unsigned int *qpNum, unsigned int *sqDepth))
        HccpDlsym(gRoceUserApiHandle, "roce_get_tsqp_depth");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_get_tsqp_depth, "roce_get_tsqp_depth");
    gRoceUserOps.rs_roce_set_tsqp_depth = (int (*)(const char *devName, unsigned int rdevIndex,
        unsigned int tempDepth, unsigned int *qpNum, unsigned int *sqDepth))
        HccpDlsym(gRoceUserApiHandle, "roce_set_tsqp_depth");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_set_tsqp_depth, "roce_set_tsqp_depth");
    gRoceUserOps.rs_roce_init_mem_pool = (int (*)(const struct roce_mem_cq_qp_attr *,
        struct rdma_lite_device_mem_attr *, unsigned int)) HccpDlsym(gRoceUserApiHandle, "roce_init_mem_pool");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_init_mem_pool, "roce_init_mem_pool");
    gRoceUserOps.rs_roce_deinit_mem_pool = (int (*)(unsigned int))
        HccpDlsym(gRoceUserApiHandle, "roce_deinit_mem_pool");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_deinit_mem_pool, "roce_deinit_mem_pool");
    gRoceUserOps.rs_roce_query_qpc = (int (*)(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attrVal,
        unsigned int attrMask)) HccpDlsym(gRoceUserApiHandle, "roce_query_qpc");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_query_qpc, "roce_query_qpc");
    gRoceUserOps.rs_roce_mmap_ai_db_reg = (int (*)(struct ibv_context *context, unsigned int tgid))
        HccpDlsym(gRoceUserApiHandle, "roce_mmap_ai_db_reg");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_mmap_ai_db_reg, "roce_mmap_ai_db_reg");
    gRoceUserOps.rs_roce_unmmap_ai_db_reg = (int (*)(struct ibv_context *context))
        HccpDlsym(gRoceUserApiHandle, "roce_unmmap_ai_db_reg");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_unmmap_ai_db_reg, "roce_unmmap_ai_db_reg");
    gRoceUserOps.rs_roce_get_cq_data_plane_info = (int (*)(struct ibv_cq *cq,
        struct hns_roce_cq_data_plane_info *info)) HccpDlsym(gRoceUserApiHandle, "roce_get_cq_data_plane_info");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_get_cq_data_plane_info, "roce_get_cq_data_plane_info");
    gRoceUserOps.rs_roce_get_qp_data_plane_info = (int (*)(struct ibv_qp *qp,
        struct hns_roce_qp_data_plane_info *info)) HccpDlsym(gRoceUserApiHandle, "roce_get_qp_data_plane_info");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_get_qp_data_plane_info, "roce_get_qp_data_plane_info");
    gRoceUserOps.rs_roce_remap_mr = (int (*)(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[],
        unsigned int num)) HccpDlsym(gRoceUserApiHandle, "roce_remap_mr");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_remap_mr, "roce_remap_mr");
    gRoceUserOps.rs_roce_get_api_version = (unsigned int (*)(void))
        HccpDlsym(gRoceUserApiHandle, "roce_get_api_version");
    DL_API_RET_IS_NULL_CHECK(gRoceUserOps.rs_roce_get_api_version, "roce_get_api_version");
#endif
    return 0;
}

int RsRoceUserApiInit(void)
{
    enum so_type type = SO_TYPE_INVALID;
    int ret = 0;

    ret = RsOpenRoceUserSo(&type);
    CHK_PRT_RETURN(ret != 0, hccp_err("hccp_dlopen[libhns-rdmav17.so or libhns-rdmav25.so or libhrn0-rdmav17.so]"
        "failed! ret=[%d]. Please check network adapter driver has been installed.", ret), ret);

#ifndef CA_CONFIG_LLT
    ret = RsRoceUserIbvApiInit();
    if (ret != 0) {
        hccp_err("rs_roce_user_ibv_api_init failed! ret=[%d]", ret);
        goto close_roce_user_so;
    }

    ret = RsRoceUserDrvApiInit();
    if (ret != 0) {
        hccp_err("rs_roce_user_drv_api_init failed! ret=[%d]", ret);
        goto close_roce_user_so;
    }

    if (type == SO_TYPE_EXT) {
        gRoceUserOps.rs_ibv_ext_post_send = (int (*)(struct ibv_qp *qp, struct ibv_send_wr *wr,
            struct ibv_send_wr **badWr, struct ibv_post_send_ext_attr *extAttr,
            struct ibv_post_send_ext_resp *extResp))
            HccpDlsym(gRoceUserApiHandle, "ibv_ext_post_send");
        if (gRoceUserOps.rs_ibv_ext_post_send == NULL) {
            ret = -EINVAL;
            hccp_err("ibv_ext_post_send is null");
            goto close_roce_user_so;
        }
    }
    return 0;

close_roce_user_so:
    RsCloseRoceUserSo();
#endif
    return ret;
}

DL_ATTRI_VISI_DEF int RsApiInit(void)
{
#ifndef NOT_INIT_IBVERBS
#ifndef CA_CONFIG_LLT
    int ret;
    ret = RsIbverbsApiInit();
    CHK_PRT_RETURN(ret, hccp_err("rs_ibverbs_api_init failed! ret=[%d]", ret), ret);
#ifdef CUSTOM_INTERFACE
    ret = RsRoceUserApiInit();
    if (ret != 0) {
        hccp_err("rs_roce_user_api_init failed! ret=[%d]", ret);
        RsCloseIbverbsSo();
        return ret;
    }
#endif
#endif
#endif
    return 0;
}

DL_ATTRI_VISI_DEF void RsApiDeinit(void)
{
    RsCloseIbverbsSo();
    RsCloseRoceUserSo();
    return;
}

struct ibv_mr *RsIbvExpRegMr(struct ibv_pd *pd, void *addr, size_t length, int access,
    struct roce_process_sign roceSign)
{
    if (gRoceUserOps.rs_ibv_exp_reg_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_reg_mr is null");
        return NULL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_reg_mr(pd, addr, length, access, roceSign);
}

struct ibv_qp *RsIbvExpCreateQp(
    struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qpInitAttr, struct rdma_lite_device_qp_attr *qpResp)
{
    if (gRoceUserOps.rs_ibv_exp_create_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_create_qp is null");
        return NULL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_create_qp(pd, qpInitAttr, qpResp);
}

int RsRoceSetTsqpDepth(const char *devName, unsigned int rdevIndex, unsigned int tempDepth,
    unsigned int *qpNum, unsigned int *sqDepth)
{
    if (gRoceUserOps.rs_roce_set_tsqp_depth == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_set_tsqp_depth is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_set_tsqp_depth(devName, rdevIndex, tempDepth, qpNum, sqDepth);
}

int RsRoceGetTsqpDepth(const char *devName, unsigned int rdevIndex, unsigned int *tempDepth,
    unsigned int *qpNum, unsigned int *sqDepth)
{
    if (gRoceUserOps.rs_roce_get_tsqp_depth == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_tsqp_depth is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_get_tsqp_depth(devName, rdevIndex, tempDepth, qpNum, sqDepth);
}

int RsRoceGetRoceDevData(const char *devName, struct roce_dev_data *rdevData)
{
    if (gRoceUserOps.rs_roce_get_roce_dev_data == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_roce_dev_data is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_get_roce_dev_data(devName, rdevData);
}

int RsIbvExpQueryNotify(struct ibv_context *context, unsigned long long *notifyVa,
    unsigned long long *size)
{
    if (gRoceUserOps.rs_ibv_exp_query_notify == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_query_notify is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_query_notify(context, notifyVa, size);
}
int RsIbvExpPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr,
    struct wr_exp_rsp *expRsp)
{
    if (gRoceUserOps.rs_ibv_exp_post_send == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_post_send is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_post_send(qp, wr, badWr, expRsp);
}

int RsIbvExtPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr,
    struct ibv_post_send_ext_attr *extAttr, struct ibv_post_send_ext_resp *extResp)
{
    if (gRoceUserOps.rs_ibv_ext_post_send == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_ext_post_send is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_ibv_ext_post_send(qp, wr, badWr, extAttr, extResp);
}

void RsIbvFreeDeviceList(struct ibv_device **list)
{
    if (gIbverbsOps.rs_ibv_free_device_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_free_device_list is null");
        return;
#endif
    }
    gIbverbsOps.rs_ibv_free_device_list(list);
}

void RsIbvAckCqEvents(struct ibv_cq *cq, unsigned int nevents)
{
    if (gIbverbsOps.rs_ibv_ack_cq_events == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_ack_cq_events is null");
        return;
#endif
    }
    gIbverbsOps.rs_ibv_ack_cq_events(cq, nevents);
}

const char *RsIbvGetDeviceName(struct ibv_device *device)
{
    if (gIbverbsOps.rs_ibv_get_device_name == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_get_device_name is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_get_device_name(device);
}

const char *RsIbvWcStatusStr(enum ibv_wc_status status)
{
    if (gIbverbsOps.rs_ibv_wc_status_str == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_wc_status_str is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_wc_status_str(status);
}

int RsIbvQueryGidType(struct ibv_context *context, uint8_t portNum, unsigned int index,
    enum ibv_gid_type_sysfs *type)
{
    if (gIbverbsOps.rs_ibv_query_gid_type == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_gid_type is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_query_gid_type(context, portNum, index, type);
}

int RsIbvDeregMr(struct ibv_mr *mr)
{
    if (gIbverbsOps.rs_ibv_dereg_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_dereg_mr is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_dereg_mr(mr);
}

int RsIbvPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr)
{
    return ibv_post_send(qp, wr, badWr);
}

int RsIbvPostRecv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **badWr)
{
    return ibv_post_recv(qp, wr, badWr);
}

int RsIbvQueryQp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attrMask, struct ibv_qp_init_attr *initAttr)
{
    if (gIbverbsOps.rs_ibv_query_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_qp is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_query_qp(qp, attr, attrMask, initAttr);
}

int RsIbvDestroyQp(struct ibv_qp *qp)
{
    if (gIbverbsOps.rs_ibv_destroy_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_qp is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_destroy_qp(qp);
}

int RsIbvGetCqEvent(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cqContext)
{
    if (gIbverbsOps.rs_ibv_get_cq_event == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_get_cq_event is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_get_cq_event(channel, cq, cqContext);
}

int RsIbvPollCq(struct ibv_cq *cq, int numEntries, struct ibv_wc *wc)
{
    return ibv_poll_cq(cq, numEntries, wc);
}

int RsIbvReqNotifyCq(struct ibv_cq *cq, int solicitedOnly)
{
    return ibv_req_notify_cq(cq, solicitedOnly);
}

int RsIbvDestroyCq(struct ibv_cq *cq)
{
    if (gIbverbsOps.rs_ibv_destroy_cq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_cq is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_destroy_cq(cq);
}

int RsIbvModifyQp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attrMask)
{
    if (gIbverbsOps.rs_ibv_modify_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_modify_qp is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_modify_qp(qp, attr, attrMask);
}

int RsIbvQueryPort(struct ibv_context *context, uint8_t portNum, struct ibv_port_attr *portAttr)
{
    if (gIbverbsOps.rs_ibv_query_port == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_port is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_query_port(context, portNum, portAttr);
}

int RsIbvQueryGid(struct ibv_context *context, uint8_t portNum, int index, union ibv_gid *gid)
{
    if (gIbverbsOps.rs_ibv_query_gid == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_query_gid is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_query_gid(context, portNum, index, gid);
}

int RsIbvCloseDevice(struct ibv_context *context)
{
    if (gIbverbsOps.rs_ibv_close_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_close_device is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_close_device(context);
}

int RsIbvDeallocPd(struct ibv_pd *pd)
{
    if (gIbverbsOps.rs_ibv_dealloc_pd == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_dealloc_pd is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_dealloc_pd(pd);
}

int RsIbvDestroyCompChannel(struct ibv_comp_channel *channel)
{
    if (gIbverbsOps.rs_ibv_destroy_comp_channel == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_comp_channel is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_destroy_comp_channel(channel);
}

struct ibv_context *RsIbvOpenDevice(struct ibv_device *device)
{
    if (gIbverbsOps.rs_ibv_open_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_open_device is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_open_device(device);
}

struct ibv_pd *RsIbvAllocPd(struct ibv_context *context)
{
    if (gIbverbsOps.rs_ibv_alloc_pd == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_alloc_pd is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_alloc_pd(context);
}

struct ibv_device **RsIbvGetDeviceList(int *numDevices)
{
    if (gIbverbsOps.rs_ibv_get_device_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_get_device_list is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_get_device_list(numDevices);
}

struct ibv_cq *RsIbvCreateCq(struct ibv_context *context, int cqe, void *cqContext,
    struct ibv_comp_channel *channel, int compVector)
{
    if (gIbverbsOps.rs_ibv_create_cq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_cq is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_create_cq(context, cqe, cqContext, channel, compVector);
}

struct ibv_mr *RsIbvRegMr(struct ibv_pd *pd, void *addr, size_t length, int access)
{
    if (gIbverbsOps.rs_ibv_reg_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_reg_mr is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_reg_mr(pd, addr, length, access);
}

struct ibv_qp *RsIbvCreateQp(struct ibv_pd *pd, struct ibv_qp_init_attr *qpInitAttr)
{
    if (gIbverbsOps.rs_ibv_create_qp == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_qp is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_create_qp(pd, qpInitAttr);
}

struct ibv_comp_channel *RsIbvCreateCompChannel(struct ibv_context *context)
{
    if (gIbverbsOps.rs_ibv_create_comp_channel == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_comp_channel is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_create_comp_channel(context);
}

struct ibv_srq *RsIbvCreateSrq(struct ibv_pd *pd, struct ibv_srq_init_attr *srqInitAttr)
{
    if (gIbverbsOps.rs_ibv_create_srq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_srq is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_create_srq(pd, srqInitAttr);
}

int RsIbvDestroySrq(struct ibv_srq *srq)
{
    if (gIbverbsOps.rs_ibv_destroy_srq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_srq is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_destroy_srq(srq);
}

struct ibv_ah *RsIbvCreateAh(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
    if (gIbverbsOps.rs_ibv_create_ah == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_create_ah is null");
        return NULL;
#endif
    }
    return gIbverbsOps.rs_ibv_create_ah(pd, attr);
}

int RsIbvDestroyAh(struct ibv_ah *ah)
{
    if (gIbverbsOps.rs_ibv_destroy_ah == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_destroy_ah is null");
        return -EINVAL;
#endif
    }
    return gIbverbsOps.rs_ibv_destroy_ah(ah);
}

struct ibv_cq *RsIbvExpCreateCq(struct ibv_context *context, int cqe, void *cqContext,
    struct ibv_comp_channel *channel, int compVector, struct rdma_lite_device_cq_init_attr *attr,
    struct rdma_lite_device_cq_attr *cqResp)
{
    if (gRoceUserOps.rs_ibv_exp_create_cq == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_create_cq is null");
        return NULL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_create_cq(context, cqe, cqContext, channel, compVector, attr, cqResp);
}

int RsIbvExpQueryDevice(struct ibv_context *context, struct dev_cap_info *cap)
{
    if (gRoceUserOps.rs_ibv_exp_query_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_query_device is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_query_device(context, cap);
}

int RsRoceInitMemPool(const struct roce_mem_cq_qp_attr *memAttr, struct rdma_lite_device_mem_attr *memData,
    unsigned int devId)
{
    if (gRoceUserOps.rs_roce_init_mem_pool == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_init_mem_pool is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_init_mem_pool(memAttr, memData, devId);
}

int RsRoceDeinitMemPool(unsigned int memIdx)
{
    if (gRoceUserOps.rs_roce_deinit_mem_pool == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_deinit_mem_pool is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_deinit_mem_pool(memIdx);
}

int RsRoceQueryQpc(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attrVal, unsigned int attrMask)
{
    if (gRoceUserOps.rs_roce_query_qpc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_query_qpc is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_query_qpc(qp, attrVal, attrMask);
}

struct ibv_ah *RsIbvExpCreateAh(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx)
{
    if (gRoceUserOps.rs_ibv_exp_create_ah == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_ibv_exp_create_ah is null");
        return NULL;
#endif
    }
    return gRoceUserOps.rs_ibv_exp_create_ah(pd, attrx);
}

int RsRoceMmapAiDbReg(struct ibv_context *context, unsigned int tgid)
{
    if (gRoceUserOps.rs_roce_mmap_ai_db_reg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_mmap_ai_db_reg is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_mmap_ai_db_reg(context, tgid);
}

int RsRoceUnmmapAiDbReg(struct ibv_context *context)
{
    if (gRoceUserOps.rs_roce_unmmap_ai_db_reg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_unmmap_ai_db_reg is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_unmmap_ai_db_reg(context);
}

int RsRoceGetCqDataPlaneInfo(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info)
{
    if (gRoceUserOps.rs_roce_get_cq_data_plane_info == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_cq_data_plane_info is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_get_cq_data_plane_info(cq, info);
}

int RsRoceGetQpDataPlaneInfo(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info)
{
    if (gRoceUserOps.rs_roce_get_qp_data_plane_info == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_get_qp_data_plane_info is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_get_qp_data_plane_info(qp, info);
}

int RsRoceRemapMr(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[], unsigned int num)
{
    if (gRoceUserOps.rs_roce_remap_mr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_roce_remap_mr is null");
        return -EINVAL;
#endif
    }
    return gRoceUserOps.rs_roce_remap_mr(mr, info, num);
}

unsigned int RsRoceGetApiVersion(void)
{
    if (gRoceUserOps.rs_roce_get_api_version != NULL) {
        return gRoceUserOps.rs_roce_get_api_version();
    }

    return 0;
}
