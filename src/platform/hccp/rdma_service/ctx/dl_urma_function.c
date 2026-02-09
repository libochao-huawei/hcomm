/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <urma_api.h>
#include "hccp_dl.h"
#include "dl_urma_function.h"

static pthread_mutex_t g_urma_api_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_urma_api_refcnt = 0;
void *g_urma_api_handle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_urma_ops g_urma_ops;
#else
struct rs_urma_ops g_urma_ops = {
    .rs_urma_init = urma_init,
    .rs_urma_uninit = urma_uninit,
    .rs_urma_get_device_list = urma_get_device_list,
    .rs_urma_get_device_by_eid = urma_get_device_by_eid,
    .rs_urma_free_device_list = urma_free_device_list,
    .rs_urma_get_eid_list = urma_get_eid_list,
    .rs_urma_free_eid_list = urma_free_eid_list,
    .rs_urma_query_device = urma_query_device,
    .rs_urma_get_eid_by_ip = urma_get_eid_by_ip,
    .rs_urma_create_context = urma_create_context,
    .rs_urma_delete_context = urma_delete_context,
    .rs_urma_create_jfr = urma_create_jfr,
    .rs_urma_delete_jfr = urma_delete_jfr,
    .rs_urma_delete_jfr_batch = urma_delete_jfr_batch,
    .rs_urma_create_jfc = urma_create_jfc,
    .rs_urma_modify_jfc = urma_modify_jfc,
    .rs_urma_delete_jfc = urma_delete_jfc,
    .rs_urma_create_jetty = urma_create_jetty,
    .rs_urma_modify_jetty = urma_modify_jetty,
    .rs_urma_query_jetty = urma_query_jetty,
    .rs_urma_delete_jetty = urma_delete_jetty,
    .rs_urma_delete_jetty_batch = urma_delete_jetty_batch,
    .rs_urma_import_jetty = urma_import_jetty,
    .rs_urma_unimport_jetty = urma_unimport_jetty,
    .rs_urma_bind_jetty = urma_bind_jetty,
    .rs_urma_unbind_jetty = urma_unbind_jetty,
    .rs_urma_flush_jetty = urma_flush_jetty,
    .rs_urma_create_jfce = urma_create_jfce,
    .rs_urma_delete_jfce = urma_delete_jfce,
    .rs_urma_get_async_event = urma_get_async_event,
    .rs_urma_ack_async_event = urma_ack_async_event,
    .rs_urma_alloc_token_id = urma_alloc_token_id,
    .rs_urma_free_token_id = urma_free_token_id,
    .rs_urma_register_seg = urma_register_seg,
    .rs_urma_unregister_seg = urma_unregister_seg,
    .rs_urma_import_seg = urma_import_seg,
    .rs_urma_unimport_seg = urma_unimport_seg,
    .rs_urma_post_jetty_send_wr = urma_post_jetty_send_wr,
    .rs_urma_post_jetty_recv_wr = urma_post_jetty_recv_wr,
    .rs_urma_poll_jfc = urma_poll_jfc,
    .rs_urma_rearm_jfc = urma_rearm_jfc,
    .rs_urma_wait_jfc = urma_wait_jfc,
    .rs_urma_ack_jfc = urma_ack_jfc,
    .rs_urma_user_ctl = urma_user_ctl,
    .rs_urma_get_tp_list = urma_get_tp_list,
    .rs_urma_get_tp_attr = urma_get_tp_attr,
    .rs_urma_set_tp_attr = urma_set_tp_attr,
    .rs_urma_import_jetty_ex = urma_import_jetty_ex,
    .rs_urma_alloc_jetty = urma_alloc_jetty,
    .rs_urma_set_jetty_opt = urma_set_jetty_opt,
    .rs_urma_active_jetty = urma_active_jetty,
    .rs_urma_get_jetty_opt = urma_get_jetty_opt,
    .rs_urma_deactive_jetty = urma_deactive_jetty,
    .rs_urma_free_jetty = urma_free_jetty,
    .rs_urma_alloc_jfc = urma_alloc_jfc,
    .rs_urma_set_jfc_opt = urma_set_jfc_opt,
    .rs_urma_active_jfc = urma_active_jfc,
    .rs_urma_get_jfc_opt = urma_get_jfc_opt,
    .rs_urma_deactive_jfc = urma_deactive_jfc,
    .rs_urma_free_jfc = urma_free_jfc,
};
#endif

void rs_ub_api_deinit(void)
{
    if (g_urma_api_handle != NULL) {
        (void)HccpDlclose(g_urma_api_handle);
        g_urma_api_handle = NULL;
    }
    return;
}

STATIC int rs_urma_device_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_urma_ops.rs_urma_init = (urma_status_t (*)(urma_init_attr_t *)) HccpDlsym(g_urma_api_handle, "urma_init");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_init, "urma_init");

    g_urma_ops.rs_urma_uninit = (urma_status_t (*)(void)) HccpDlsym(g_urma_api_handle, "urma_uninit");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_uninit, "urma_uninit");

    g_urma_ops.rs_urma_get_device_list = (urma_device_t **(*)(int *))
        HccpDlsym(g_urma_api_handle, "urma_get_device_list");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_device_list, "urma_get_device_list");

    g_urma_ops.rs_urma_get_device_by_eid = (urma_device_t *(*)(urma_eid_t, urma_transport_type_t))
        HccpDlsym(g_urma_api_handle, "urma_get_device_by_eid");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_device_by_eid, "urma_get_device_by_eid");

    g_urma_ops.rs_urma_free_device_list = (void (*)(urma_device_t **))
        HccpDlsym(g_urma_api_handle, "urma_free_device_list");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_free_device_list, "urma_free_device_list");

    g_urma_ops.rs_urma_get_eid_list = (urma_eid_info_t *(*)(urma_device_t *, uint32_t *))
        HccpDlsym(g_urma_api_handle, "urma_get_eid_list");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_eid_list, "urma_get_eid_list");

    g_urma_ops.rs_urma_free_eid_list = (void (*)(urma_eid_info_t *))
        HccpDlsym(g_urma_api_handle, "urma_free_eid_list");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_free_eid_list, "urma_free_eid_list");

    g_urma_ops.rs_urma_query_device = (urma_status_t (*)(urma_device_t *, urma_device_attr_t *))
        HccpDlsym(g_urma_api_handle, "urma_query_device");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_query_device, "urma_query_device");

    g_urma_ops.rs_urma_get_eid_by_ip = (urma_status_t (*)(const urma_context_t *, const urma_net_addr_t *, urma_eid_t *))
        HccpDlsym(g_urma_api_handle, "urma_get_eid_by_ip");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_eid_by_ip, "urma_get_eid_by_ip");

    g_urma_ops.rs_urma_create_context = (urma_context_t *(*)(urma_device_t *, uint32_t))
        HccpDlsym(g_urma_api_handle, "urma_create_context");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_create_context, "urma_create_context");

    g_urma_ops.rs_urma_delete_context = (urma_status_t (*)(urma_context_t *))
        HccpDlsym(g_urma_api_handle, "urma_delete_context");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_context, "urma_delete_context");
#endif
    return 0;
}

STATIC int rs_urma_jetty_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_urma_ops.rs_urma_create_jfr = (urma_jfr_t *(*)(urma_context_t *, urma_jfr_cfg_t *))
        HccpDlsym(g_urma_api_handle, "urma_create_jfr");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_create_jfr, "urma_create_jfr");

    g_urma_ops.rs_urma_delete_jfr = (urma_status_t (*)(urma_jfr_t *))
        HccpDlsym(g_urma_api_handle, "urma_delete_jfr");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_jfr, "urma_delete_jfr");

    g_urma_ops.rs_urma_delete_jfr_batch = (urma_status_t (*)(urma_jfr_t **jfr_arr, int jfr_num, urma_jfr_t **bad_jfr))
        HccpDlsym(g_urma_api_handle, "urma_delete_jfr_batch");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_jfr_batch, "urma_delete_jfr_batch");

    g_urma_ops.rs_urma_create_jetty = (urma_jetty_t *(*)(urma_context_t *, urma_jetty_cfg_t *))
        HccpDlsym(g_urma_api_handle, "urma_create_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_create_jetty, "urma_create_jetty");

    g_urma_ops.rs_urma_modify_jetty = (urma_status_t (*)(urma_jetty_t *, urma_jetty_attr_t *))
        HccpDlsym(g_urma_api_handle, "urma_modify_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_modify_jetty, "urma_modify_jetty");

    g_urma_ops.rs_urma_query_jetty = (urma_status_t (*)(urma_jetty_t *, urma_jetty_cfg_t *, urma_jetty_attr_t *))
        HccpDlsym(g_urma_api_handle, "urma_query_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_query_jetty, "urma_query_jetty");

    g_urma_ops.rs_urma_delete_jetty = (urma_status_t (*)(urma_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_delete_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_jetty, "urma_delete_jetty");

    g_urma_ops.rs_urma_delete_jetty_batch = (urma_status_t (*)(urma_jetty_t **jetty_arr, int jetty_num,
        urma_jetty_t **bad_jetty))HccpDlsym(g_urma_api_handle, "urma_delete_jetty_batch");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_jetty_batch, "urma_delete_jetty_batch");

    g_urma_ops.rs_urma_import_jetty = (urma_target_jetty_t *(*)(urma_context_t *, urma_rjetty_t *, urma_token_t *))
        HccpDlsym(g_urma_api_handle, "urma_import_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_import_jetty, "urma_import_jetty");

    g_urma_ops.rs_urma_unimport_jetty = (urma_status_t (*)(urma_target_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_unimport_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_unimport_jetty, "urma_unimport_jetty");

    g_urma_ops.rs_urma_bind_jetty = (urma_status_t (*)(urma_jetty_t *, urma_target_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_bind_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_bind_jetty, "urma_bind_jetty");

    g_urma_ops.rs_urma_unbind_jetty = (urma_status_t (*)(urma_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_unbind_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_unbind_jetty, "urma_unbind_jetty");

    g_urma_ops.rs_urma_flush_jetty = (int (*)(urma_jetty_t *, int, urma_cr_t *))
        HccpDlsym(g_urma_api_handle, "urma_flush_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_flush_jetty, "urma_flush_jetty");

    g_urma_ops.rs_urma_alloc_jetty = (urma_status_t (*)(urma_context_t *, urma_jetty_cfg_t *, urma_jetty_t **))
        HccpDlsym(g_urma_api_handle, "urma_alloc_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_alloc_jetty, "urma_alloc_jetty");

    g_urma_ops.rs_urma_set_jetty_opt = (urma_status_t (*)(urma_jetty_t *, uint64_t, void *, uint32_t))
        HccpDlsym(g_urma_api_handle, "urma_set_jetty_opt");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_set_jetty_opt, "urma_set_jetty_opt");

    g_urma_ops.rs_urma_active_jetty = (urma_status_t (*)(urma_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_active_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_active_jetty, "urma_active_jetty");

    g_urma_ops.rs_urma_get_jetty_opt = (urma_status_t (*)(urma_jetty_t *, uint64_t, void *, uint32_t))
        HccpDlsym(g_urma_api_handle, "urma_get_jetty_opt");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_jetty_opt, "urma_get_jetty_opt");

    g_urma_ops.rs_urma_deactive_jetty = (urma_status_t (*)(urma_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_deactive_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_deactive_jetty, "urma_deactive_jetty");

    g_urma_ops.rs_urma_free_jetty = (urma_status_t (*)(urma_jetty_t *))
        HccpDlsym(g_urma_api_handle, "urma_free_jetty");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_free_jetty, "urma_free_jetty");
#endif
    return 0;
}

STATIC int rs_urma_jfc_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_urma_ops.rs_urma_create_jfc = (urma_jfc_t *(*)(urma_context_t *, urma_jfc_cfg_t *))
        HccpDlsym(g_urma_api_handle, "urma_create_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_create_jfc, "urma_create_jfc");

    g_urma_ops.rs_urma_modify_jfc = (urma_status_t (*)(urma_jfc_t *, urma_jfc_attr_t *))
        HccpDlsym(g_urma_api_handle, "urma_modify_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_modify_jfc, "urma_modify_jfc");

    g_urma_ops.rs_urma_delete_jfc = (urma_status_t (*)(urma_jfc_t *))
        HccpDlsym(g_urma_api_handle, "urma_delete_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_jfc, "urma_delete_jfc");

    g_urma_ops.rs_urma_create_jfce = (urma_jfce_t *(*)(urma_context_t *))
        HccpDlsym(g_urma_api_handle, "urma_create_jfce");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_create_jfce, "urma_create_jfce");

    g_urma_ops.rs_urma_delete_jfce = (urma_status_t (*)(urma_jfce_t *))
        HccpDlsym(g_urma_api_handle, "urma_delete_jfce");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_delete_jfce, "urma_delete_jfce");

    g_urma_ops.rs_urma_get_async_event = (urma_status_t (*)(urma_context_t *, urma_async_event_t *))
        HccpDlsym(g_urma_api_handle, "urma_get_async_event");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_async_event, "urma_get_async_event");

    g_urma_ops.rs_urma_ack_async_event = (void (*)(urma_async_event_t *))
        HccpDlsym(g_urma_api_handle, "urma_ack_async_event");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_ack_async_event, "urma_ack_async_event");

    g_urma_ops.rs_urma_alloc_jfc = (urma_status_t (*)(urma_context_t *, urma_jfc_cfg_t *, urma_jfc_t **))
    HccpDlsym(g_urma_api_handle, "urma_alloc_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_alloc_jfc, "urma_alloc_jfc");

    g_urma_ops.rs_urma_set_jfc_opt = (urma_status_t (*)(urma_jfc_t *, uint64_t , void *, uint32_t))
        HccpDlsym(g_urma_api_handle, "urma_set_jfc_opt");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_set_jfc_opt, "urma_set_jfc_opt");

    g_urma_ops.rs_urma_active_jfc = (urma_status_t (*)(urma_jfc_t *))
        HccpDlsym(g_urma_api_handle, "urma_active_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_active_jfc, "urma_active_jfc");

    g_urma_ops.rs_urma_get_jfc_opt = (urma_status_t (*)(urma_jfc_t *, uint64_t , void *, uint32_t))
        HccpDlsym(g_urma_api_handle, "urma_get_jfc_opt");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_jfc_opt, "urma_get_jfc_opt");

    g_urma_ops.rs_urma_deactive_jfc = (urma_status_t (*)(urma_jfc_t *))
        HccpDlsym(g_urma_api_handle, "urma_deactive_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_deactive_jfc, "urma_deactive_jfc");

    g_urma_ops.rs_urma_free_jfc = (urma_status_t (*)(urma_jfc_t *))
        HccpDlsym(g_urma_api_handle, "urma_free_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_free_jfc, "urma_free_jfc");
#endif
    return 0;
}

STATIC int rs_urma_segment_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_urma_ops.rs_urma_alloc_token_id = (urma_token_id_t *(*)(urma_context_t *))
        HccpDlsym(g_urma_api_handle, "urma_alloc_token_id");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_alloc_token_id, "urma_alloc_token_id");

    g_urma_ops.rs_urma_free_token_id = (urma_status_t (*)(urma_token_id_t *))
        HccpDlsym(g_urma_api_handle, "urma_free_token_id");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_free_token_id, "urma_free_token_id");

    g_urma_ops.rs_urma_register_seg = (urma_target_seg_t *(*)(urma_context_t *, urma_seg_cfg_t *))
        HccpDlsym(g_urma_api_handle, "urma_register_seg");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_register_seg, "urma_register_seg");

    g_urma_ops.rs_urma_unregister_seg = (urma_status_t (*)(urma_target_seg_t *))
        HccpDlsym(g_urma_api_handle, "urma_unregister_seg");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_unregister_seg, "urma_unregister_seg");

    g_urma_ops.rs_urma_import_seg = (urma_target_seg_t *(*)(urma_context_t *, urma_seg_t *,
        urma_token_t *, uint64_t, urma_import_seg_flag_t))
        HccpDlsym(g_urma_api_handle, "urma_import_seg");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_import_seg, "urma_import_seg");

    g_urma_ops.rs_urma_unimport_seg = (urma_status_t (*)(urma_target_seg_t *))
        HccpDlsym(g_urma_api_handle, "urma_unimport_seg");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_unimport_seg, "urma_unimport_seg");
#endif
    return 0;
}

STATIC int rs_urma_data_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_urma_ops.rs_urma_post_jetty_send_wr = (urma_status_t (*)(urma_jetty_t *, urma_jfs_wr_t *, urma_jfs_wr_t **))
        HccpDlsym(g_urma_api_handle, "urma_post_jetty_send_wr");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_post_jetty_send_wr, "urma_post_jetty_send_wr");

    g_urma_ops.rs_urma_post_jetty_recv_wr = (urma_status_t (*)(urma_jetty_t *, urma_jfr_wr_t *, urma_jfr_wr_t **))
        HccpDlsym(g_urma_api_handle, "urma_post_jetty_recv_wr");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_post_jetty_recv_wr, "urma_post_jetty_recv_wr");

    g_urma_ops.rs_urma_poll_jfc = (int (*)(urma_jfc_t *, int, urma_cr_t *))
        HccpDlsym(g_urma_api_handle, "urma_poll_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_poll_jfc, "urma_poll_jfc");

    g_urma_ops.rs_urma_rearm_jfc = (urma_status_t (*)(urma_jfc_t *, bool))
        HccpDlsym(g_urma_api_handle, "urma_rearm_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_rearm_jfc, "urma_rearm_jfc");

    g_urma_ops.rs_urma_wait_jfc = (int (*)(urma_jfce_t *, uint32_t, int, urma_jfc_t *[]))
        HccpDlsym(g_urma_api_handle, "urma_wait_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_wait_jfc, "urma_wait_jfc");

    g_urma_ops.rs_urma_ack_jfc = (void (*)(urma_jfc_t *[], uint32_t [], uint32_t))
        HccpDlsym(g_urma_api_handle, "urma_ack_jfc");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_ack_jfc, "urma_ack_jfc");

    g_urma_ops.rs_urma_user_ctl = (urma_status_t (*)(urma_context_t *, urma_user_ctl_in_t *, urma_user_ctl_out_t *))
        HccpDlsym(g_urma_api_handle, "urma_user_ctl");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_user_ctl, "urma_user_ctl");

    g_urma_ops.rs_urma_get_tp_list = (urma_status_t (*)(urma_context_t *, urma_get_tp_cfg_t *, uint32_t *,
        urma_tp_info_t *))HccpDlsym(g_urma_api_handle, "urma_get_tp_list");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_tp_list, "urma_get_tp_list");

    g_urma_ops.rs_urma_get_tp_attr = (urma_status_t (*)(const urma_context_t *, const uint64_t, uint8_t *, uint32_t *,
        urma_tp_attr_value_t *))HccpDlsym(g_urma_api_handle, "urma_get_tp_attr");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_get_tp_attr, "urma_get_tp_attr");

    g_urma_ops.rs_urma_set_tp_attr = (urma_status_t (*)(const urma_context_t *, const uint64_t , const uint8_t,
        const uint32_t, const urma_tp_attr_value_t *))HccpDlsym(g_urma_api_handle, "urma_set_tp_attr");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_set_tp_attr, "urma_set_tp_attr");

    g_urma_ops.rs_urma_import_jetty_ex = (urma_target_jetty_t *(*)(urma_context_t *, urma_rjetty_t *, urma_token_t *,
        urma_import_jetty_ex_cfg_t *))HccpDlsym(g_urma_api_handle, "urma_import_jetty_ex");
    DL_API_RET_IS_NULL_CHECK(g_urma_ops.rs_urma_import_jetty_ex, "urma_import_jetty_ex");
#endif
    return 0;
}

STATIC int rs_open_urma_so(void)
{
    pthread_mutex_lock(&g_urma_api_lock);
#ifndef CA_CONFIG_LLT
    if (g_urma_api_handle == NULL) {
        g_urma_api_handle = HccpDlopen("liburma-udma.so", RTLD_NOW);
        if (g_urma_api_handle != NULL) {
            goto out;
        }

        g_urma_api_handle = HccpDlopen("/usr/lib64/urma/liburma-udma.so", RTLD_NOW);
        if (g_urma_api_handle != NULL) {
            goto out;
        }
        pthread_mutex_unlock(&g_urma_api_lock);
        return -EINVAL;
    } else {
        hccp_run_info("urma_api dlopen again, g_urma_api_refcnt:%d", g_urma_api_refcnt + 1);
    }
out:
#endif
    g_urma_api_refcnt++;
    pthread_mutex_unlock(&g_urma_api_lock);
    return 0;
}

STATIC void rs_close_urma_so(void)
{
    pthread_mutex_lock(&g_urma_api_lock);
#ifndef CA_CONFIG_LLT
    if (g_urma_api_handle != NULL) {
        g_urma_api_refcnt--;
        if (g_urma_api_refcnt > 0) {
            goto out;
        }

        hccp_run_info("dlclose urma_api, g_urma_api_refcnt:%d", g_urma_api_refcnt);
        (void)HccpDlclose(g_urma_api_handle);
        g_urma_api_handle = NULL;
        g_urma_api_refcnt = 0;
    }
out:
#endif
    pthread_mutex_unlock(&g_urma_api_lock);
    return;
}

STATIC int rs_urma_api_init(void)
{
    int ret;

    ret = rs_open_urma_so();
    CHK_PRT_RETURN(ret, hccp_err("HccpDlopen[liburma-udma.so] failed! ret=[%d], "
    "Please check network adapter driver has been installed", ret), ret);

    ret = rs_urma_device_api_init();
    if (ret != 0) {
        hccp_err("[rs_urma_device_api_init]HccpDlopen failed! ret=[%d]", ret);
        rs_close_urma_so();
        return ret;
    }

    ret = rs_urma_jetty_api_init();
    if (ret != 0) {
        hccp_err("[rs_urma_jetty_api_init]HccpDlopen failed! ret=[%d]", ret);
        rs_close_urma_so();
        return ret;
    }

    ret = rs_urma_jfc_api_init();
    if (ret != 0) {
        hccp_err("[rs_urma_jfc_api_init]HccpDlopen failed! ret=[%d]", ret);
        rs_close_urma_so();
        return ret;
    }

    ret = rs_urma_segment_api_init();
    if (ret != 0) {
        hccp_err("[rs_urma_segment_api_init]HccpDlopen failed! ret=[%d]", ret);
        rs_close_urma_so();
        return ret;
    }

    ret = rs_urma_data_api_init();
    if (ret != 0) {
        hccp_err("[rs_urma_data_api_init]HccpDlopen failed! ret=[%d]", ret);
        rs_close_urma_so();
        return ret;
    }

    return 0;
}

int rs_ub_api_init(void)
{
    int ret;

    ret = rs_urma_api_init();
    CHK_PRT_RETURN(ret, hccp_err("rs_urma_api_init failed! ret=[%d]", ret), ret);

    return 0;
}

int rs_urma_init(urma_init_attr_t *conf)
{
    if (g_urma_ops.rs_urma_init == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_init is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_init(conf);
}

int rs_urma_uninit(void)
{
    if (g_urma_ops.rs_urma_uninit == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_uninit is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_uninit();
}

urma_device_t **rs_urma_get_device_list(int *num_devices)
{
    if (g_urma_ops.rs_urma_get_device_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_device_list is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_get_device_list(num_devices);
}

urma_device_t *rs_urma_get_device_by_eid(urma_eid_t eid, urma_transport_type_t type)
{
    if (g_urma_ops.rs_urma_get_device_by_eid == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_device_by_eid is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_get_device_by_eid(eid, type);
}

void rs_urma_free_device_list(urma_device_t **device_list)
{
    if (g_urma_ops.rs_urma_free_device_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_free_device_list is null");
        return;
#endif
    }
    g_urma_ops.rs_urma_free_device_list(device_list);
}

urma_eid_info_t *rs_urma_get_eid_list(urma_device_t *dev, uint32_t *cnt)
{
    if (g_urma_ops.rs_urma_get_eid_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_eid_list is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_get_eid_list(dev, cnt);
}

void rs_urma_free_eid_list(urma_eid_info_t *eid_list)
{
    if (g_urma_ops.rs_urma_free_eid_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_free_eid_list is null");
        return;
#endif
    }
    g_urma_ops.rs_urma_free_eid_list(eid_list);
}

int rs_urma_query_device(urma_device_t *dev, urma_device_attr_t *dev_attr)
{
    if (g_urma_ops.rs_urma_query_device == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_query_device is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_query_device(dev, dev_attr);
}

int rs_urma_get_eid_by_ip(const urma_context_t *ctx, const urma_net_addr_t *net_addr, urma_eid_t *eid)
{
    if (g_urma_ops.rs_urma_get_eid_by_ip == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_eid_by_ip is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_get_eid_by_ip(ctx, net_addr, eid);
}

urma_context_t *rs_urma_create_context(urma_device_t *dev, uint32_t eid_index)
{
    if (g_urma_ops.rs_urma_create_context == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_create_context is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_create_context(dev, eid_index);
}

int rs_urma_delete_context(urma_context_t *ctx)
{
    if (g_urma_ops.rs_urma_delete_context == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_context is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_context(ctx);
}

urma_jfr_t *rs_urma_create_jfr(urma_context_t *ctx, urma_jfr_cfg_t *jfr_cfg)
{
    if (g_urma_ops.rs_urma_create_jfr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_create_jfr is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_create_jfr(ctx, jfr_cfg);
}

int rs_urma_delete_jfr(urma_jfr_t *jfr)
{
    if (g_urma_ops.rs_urma_delete_jfr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_jfr is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_jfr(jfr);
}

urma_jfc_t *rs_urma_create_jfc(urma_context_t *ctx, urma_jfc_cfg_t *jfc_cfg)
{
    if (g_urma_ops.rs_urma_create_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_create_jfc is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_create_jfc(ctx, jfc_cfg);
}

int rs_urma_modify_jfc(urma_jfc_t *jfc, urma_jfc_attr_t *attr)
{
    if (g_urma_ops.rs_urma_modify_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_modify_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_modify_jfc(jfc, attr);
}

int rs_urma_delete_jfc(urma_jfc_t *jfc)
{
    if (g_urma_ops.rs_urma_delete_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_jfc(jfc);
}

urma_jetty_t *rs_urma_create_jetty(urma_context_t *ctx, urma_jetty_cfg_t *jetty_cfg)
{
    if (g_urma_ops.rs_urma_create_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_create_jetty is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_create_jetty(ctx, jetty_cfg);
}

int rs_urma_modify_jetty(urma_jetty_t *jetty, urma_jetty_attr_t *attr)
{
    if (g_urma_ops.rs_urma_modify_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_modify_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_modify_jetty(jetty, attr);
}

int rs_urma_query_jetty(urma_jetty_t *jetty, urma_jetty_cfg_t *cfg, urma_jetty_attr_t *attr)
{
    if (g_urma_ops.rs_urma_query_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_query_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_query_jetty(jetty, cfg, attr);
}

int rs_urma_delete_jetty(urma_jetty_t *jetty)
{
    if (g_urma_ops.rs_urma_delete_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_jetty(jetty);
}

urma_target_jetty_t *rs_urma_import_jetty(urma_context_t *ctx, urma_rjetty_t *rjetty,
                                          urma_token_t *token_value)
{
    if (g_urma_ops.rs_urma_import_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_import_jetty is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_import_jetty(ctx, rjetty, token_value);
}

int rs_urma_unimport_jetty(urma_target_jetty_t *tjetty)
{
    if (g_urma_ops.rs_urma_unimport_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_unimport_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_unimport_jetty(tjetty);
}

int rs_urma_bind_jetty(urma_jetty_t *jetty, urma_target_jetty_t *tjetty)
{
    if (g_urma_ops.rs_urma_bind_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_bind_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_bind_jetty(jetty, tjetty);
}

int rs_urma_unbind_jetty(urma_jetty_t *jetty)
{
    if (g_urma_ops.rs_urma_unbind_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_unbind_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_unbind_jetty(jetty);
}

int rs_urma_flush_jetty(urma_jetty_t *jetty, int cr_cnt, urma_cr_t *cr)
{
    if (g_urma_ops.rs_urma_flush_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_flush_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_flush_jetty(jetty, cr_cnt, cr);
}

urma_jfce_t *rs_urma_create_jfce(urma_context_t *ctx)
{
    if (g_urma_ops.rs_urma_create_jfce == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_create_jfce is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_create_jfce(ctx);
}

int rs_urma_delete_jfce(urma_jfce_t *jfce)
{
    if (g_urma_ops.rs_urma_delete_jfce == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_jfce is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_jfce(jfce);
}

int rs_urma_get_async_event(urma_context_t *ctx, urma_async_event_t *event)
{
    if (g_urma_ops.rs_urma_get_async_event == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_async_event is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_get_async_event(ctx, event);
}

void rs_urma_ack_async_event(urma_async_event_t *event)
{
    if (g_urma_ops.rs_urma_ack_async_event == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_ack_async_event is null");
        return;
#endif
    }
    g_urma_ops.rs_urma_ack_async_event(event);
}

urma_token_id_t *rs_urma_alloc_token_id(urma_context_t *ctx)
{
    if (g_urma_ops.rs_urma_alloc_token_id == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_alloc_token_id is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_alloc_token_id(ctx);
}

int rs_urma_free_token_id(urma_token_id_t *token_id)
{
    if (g_urma_ops.rs_urma_free_token_id == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_free_token_id is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_free_token_id(token_id);
}

urma_target_seg_t *rs_urma_register_seg(urma_context_t *ctx, urma_seg_cfg_t *seg_cfg)
{
    if (g_urma_ops.rs_urma_register_seg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_register_seg is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_register_seg(ctx, seg_cfg);
}

int rs_urma_unregister_seg(urma_target_seg_t *target_seg)
{
    if (g_urma_ops.rs_urma_unregister_seg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_unregister_seg is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_unregister_seg(target_seg);
}

urma_target_seg_t *rs_urma_import_seg(urma_context_t *ctx, urma_seg_t *seg, urma_token_t *token_value,
                                      uint64_t addr, urma_import_seg_flag_t flag)
{
    if (g_urma_ops.rs_urma_import_seg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_import_seg is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_import_seg(ctx, seg, token_value, addr, flag);
}

int rs_urma_unimport_seg(urma_target_seg_t *tseg)
{
    if (g_urma_ops.rs_urma_unimport_seg == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_unimport_seg is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_unimport_seg(tseg);
}

int rs_urma_post_jetty_send_wr(urma_jetty_t *jetty, urma_jfs_wr_t *wr, urma_jfs_wr_t **bad_wr)
{
    if (g_urma_ops.rs_urma_post_jetty_send_wr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_post_jetty_send_wr is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_post_jetty_send_wr(jetty, wr, bad_wr);
}

int rs_urma_post_jetty_recv_wr(urma_jetty_t *jetty, urma_jfr_wr_t *wr, urma_jfr_wr_t **bad_wr)
{
    if (g_urma_ops.rs_urma_post_jetty_recv_wr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_post_jetty_recv_wr is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_post_jetty_recv_wr(jetty, wr, bad_wr);
}

int rs_urma_poll_jfc(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr)
{
    if (g_urma_ops.rs_urma_poll_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_poll_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_poll_jfc(jfc, cr_cnt, cr);
}

int rs_urma_rearm_jfc(urma_jfc_t *jfc, bool solicited_only)
{
    if (g_urma_ops.rs_urma_rearm_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_rearm_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_rearm_jfc(jfc, solicited_only);
}

int rs_urma_wait_jfc(urma_jfce_t *jfce, uint32_t jfc_cnt, int time_out, urma_jfc_t *jfc[])
{
    if (g_urma_ops.rs_urma_wait_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_wait_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_wait_jfc(jfce, jfc_cnt, time_out, jfc);
}

void rs_urma_ack_jfc(urma_jfc_t *jfc[], uint32_t nevents[], uint32_t jfc_cnt)
{
    if (g_urma_ops.rs_urma_ack_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_ack_jfc is null");
        return;
#endif
    }
    g_urma_ops.rs_urma_ack_jfc(jfc, nevents, jfc_cnt);
}

int rs_urma_user_ctl(urma_context_t *ctx, urma_user_ctl_in_t *in, urma_user_ctl_out_t *out)
{
    if (g_urma_ops.rs_urma_user_ctl == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_user_ctl is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_user_ctl(ctx, in, out);
}

int rs_urma_get_tp_list(urma_context_t *ctx, urma_get_tp_cfg_t *cfg, uint32_t *tp_cnt, urma_tp_info_t *tp_list)
{
    if (g_urma_ops.rs_urma_get_tp_list == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_tp_list is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_get_tp_list(ctx, cfg, tp_cnt, tp_list);
}

int rs_urma_get_tp_attr(const urma_context_t *ctx, const uint64_t tp_handle, uint8_t *tp_attr_cnt,
    uint32_t *tp_attr_bitmap, urma_tp_attr_value_t *tp_attr)
{
    if (g_urma_ops.rs_urma_get_tp_attr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_tp_attr is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_get_tp_attr(ctx, tp_handle, tp_attr_cnt, tp_attr_bitmap, tp_attr);
}

int rs_urma_set_tp_attr(const urma_context_t *ctx, const uint64_t tp_handle, const uint8_t tp_attr_cnt,
    const uint32_t tp_attr_bitmap, const urma_tp_attr_value_t *tp_attr)
{
    if (g_urma_ops.rs_urma_set_tp_attr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_set_tp_attr is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_set_tp_attr(ctx, tp_handle, tp_attr_cnt, tp_attr_bitmap, tp_attr);
}

urma_target_jetty_t *rs_urma_import_jetty_ex(urma_context_t *ctx, urma_rjetty_t *rjetty, urma_token_t *token_value,
    urma_import_jetty_ex_cfg_t *cfg)
{
    if (g_urma_ops.rs_urma_import_jetty_ex == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_import_jetty_ex is null");
        return NULL;
#endif
    }
    return g_urma_ops.rs_urma_import_jetty_ex(ctx, rjetty, token_value, cfg);
}

int rs_urma_delete_jetty_batch(urma_jetty_t **jetty_arr, int jetty_num, urma_jetty_t **bad_jetty)
{
    if (g_urma_ops.rs_urma_delete_jetty_batch == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_jetty_batch is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_jetty_batch(jetty_arr, jetty_num, bad_jetty);
}

int rs_urma_delete_jfr_batch(urma_jfr_t **jfr_arr, int jfr_num, urma_jfr_t **bad_jfr)
{
    if (g_urma_ops.rs_urma_delete_jfr_batch == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_delete_jfr_batch is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_delete_jfr_batch(jfr_arr, jfr_num, bad_jfr);
}

int rs_urma_alloc_jetty(urma_context_t *urma_ctx, urma_jetty_cfg_t *cfg, urma_jetty_t **jetty)
{
    if (g_urma_ops.rs_urma_alloc_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_alloc_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_alloc_jetty(urma_ctx, cfg, jetty);
}

int rs_urma_set_jetty_opt(urma_jetty_t *jetty, uint64_t opt, void *buf, uint32_t len)
{
    if (g_urma_ops.rs_urma_set_jetty_opt == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_set_jetty_opt is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_set_jetty_opt(jetty, opt, buf, len);
}

int rs_urma_active_jetty(urma_jetty_t *jetty)
{
    if (g_urma_ops.rs_urma_active_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_active_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_active_jetty(jetty);
}

int rs_urma_get_jetty_opt(urma_jetty_t *jetty, uint64_t opt, void *buf, uint32_t len)
{
    if (g_urma_ops.rs_urma_get_jetty_opt == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_jetty_opt is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_get_jetty_opt(jetty, opt, buf, len);
}

int rs_urma_deactive_jetty(urma_jetty_t *jetty)
{
    if (g_urma_ops.rs_urma_deactive_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_deactive_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_deactive_jetty(jetty);
}

int rs_urma_free_jetty(urma_jetty_t *jetty)
{
    if (g_urma_ops.rs_urma_free_jetty == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_free_jetty is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_free_jetty(jetty);
}

int rs_urma_alloc_jfc(urma_context_t *urma_ctx, urma_jfc_cfg_t *cfg, urma_jfc_t **jfc)
{
    if (g_urma_ops.rs_urma_alloc_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_alloc_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_alloc_jfc(urma_ctx, cfg, jfc);
}

int rs_urma_set_jfc_opt(urma_jfc_t *jfc, uint64_t opt, void *buf, uint32_t len)
{
    if (g_urma_ops.rs_urma_set_jfc_opt == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_set_jfc_opt is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_set_jfc_opt(jfc, opt, buf, len);
}

int rs_urma_active_jfc(urma_jfc_t *jfc)
{
    if (g_urma_ops.rs_urma_active_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_active_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_active_jfc(jfc);
}

int rs_urma_get_jfc_opt(urma_jfc_t *jfc, uint64_t opt, void *buf, uint32_t len)
{
    if (g_urma_ops.rs_urma_get_jfc_opt == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_get_jfc_opt is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_get_jfc_opt(jfc, opt, buf, len);
}

int rs_urma_deactive_jfc(urma_jfc_t *jfc)
{
    if (g_urma_ops.rs_urma_deactive_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_deactive_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_deactive_jfc(jfc);
}

int rs_urma_free_jfc(urma_jfc_t *jfc)
{
    if (g_urma_ops.rs_urma_free_jfc == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_urma_free_jfc is null");
        return -EINVAL;
#endif
    }
    return g_urma_ops.rs_urma_free_jfc(jfc);
}
