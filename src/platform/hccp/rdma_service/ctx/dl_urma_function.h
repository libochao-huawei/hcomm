/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DL_URMA_FUNCTION_H
#define DL_URMA_FUNCTION_H

#include <ccan/list.h>
#include <urma_types.h>

#ifdef CA_CONFIG_LLT
#define STATIC
#else
#define STATIC static
#endif

struct rs_urma_ops {
    urma_status_t (*rs_urma_init)(urma_init_attr_t *conf);
    urma_status_t (*rs_urma_uninit)(void);
    urma_device_t **(*rs_urma_get_device_list)(int *num_devices);
    void (*rs_urma_free_device_list)(urma_device_t **device_list);
    urma_eid_info_t *(*rs_urma_get_eid_list)(urma_device_t *dev, uint32_t *cnt);
    void (*rs_urma_free_eid_list)(urma_eid_info_t *eid_list);
    urma_status_t (*rs_urma_query_device)(urma_device_t *dev, urma_device_attr_t *dev_attr);
    urma_status_t (*rs_urma_get_eid_by_ip)(const urma_context_t *ctx, const urma_net_addr_t *net_addr, urma_eid_t *eid);
    urma_context_t *(*rs_urma_create_context)(urma_device_t *dev, uint32_t eid_index);
    urma_status_t (*rs_urma_delete_context)(urma_context_t *ctx);
    urma_jfr_t *(*rs_urma_create_jfr)(urma_context_t *ctx, urma_jfr_cfg_t *jfr_cfg);
    urma_status_t (*rs_urma_delete_jfr)(urma_jfr_t *jfr);
    urma_status_t (*rs_urma_delete_jfr_batch)(urma_jfr_t **jfr_arr, int jfr_num, urma_jfr_t **bad_jfr);
    urma_jfc_t *(*rs_urma_create_jfc)(urma_context_t *ctx, urma_jfc_cfg_t *jfc_cfg);
    urma_status_t (*rs_urma_modify_jfc)(urma_jfc_t *jfc, urma_jfc_attr_t *attr);
    urma_status_t (*rs_urma_delete_jfc)(urma_jfc_t *jfc);
    urma_jetty_t *(*rs_urma_create_jetty)(urma_context_t *ctx, urma_jetty_cfg_t *jetty_cfg);
    urma_status_t (*rs_urma_modify_jetty)(urma_jetty_t *jetty, urma_jetty_attr_t *attr);
    urma_status_t (*rs_urma_query_jetty)(urma_jetty_t *jetty, urma_jetty_cfg_t *cfg, urma_jetty_attr_t *attr);
    urma_status_t (*rs_urma_delete_jetty)(urma_jetty_t *jetty);
    urma_status_t (*rs_urma_delete_jetty_batch)(urma_jetty_t **jetty_arr, int jetty_num, urma_jetty_t **bad_jetty);
    urma_target_jetty_t *(*rs_urma_import_jetty)(urma_context_t *ctx, urma_rjetty_t *rjetty, urma_token_t *token_value);
    urma_status_t (*rs_urma_unimport_jetty)(urma_target_jetty_t *tjetty);
    urma_status_t (*rs_urma_bind_jetty)(urma_jetty_t *jetty, urma_target_jetty_t *tjetty);
    urma_status_t (*rs_urma_unbind_jetty)(urma_jetty_t *jetty);
    int (*rs_urma_flush_jetty)(urma_jetty_t *jetty, int cr_cnt, urma_cr_t *cr);
    urma_jfce_t *(*rs_urma_create_jfce)(urma_context_t *ctx);
    urma_status_t (*rs_urma_delete_jfce)(urma_jfce_t *jfce);
    urma_status_t (*rs_urma_get_async_event)(urma_context_t *ctx, urma_async_event_t *event);
    void (*rs_urma_ack_async_event)(urma_async_event_t *event);
    urma_token_id_t *(*rs_urma_alloc_token_id)(urma_context_t *ctx);
    urma_status_t (*rs_urma_free_token_id)(urma_token_id_t *token_id);
    urma_target_seg_t *(*rs_urma_register_seg)(urma_context_t *ctx, urma_seg_cfg_t *seg_cfg);
    urma_status_t (*rs_urma_unregister_seg)(urma_target_seg_t *target_seg);
    urma_target_seg_t *(*rs_urma_import_seg)(urma_context_t *ctx, urma_seg_t *seg,
        urma_token_t *token_value, uint64_t addr, urma_import_seg_flag_t flag);
    urma_status_t (*rs_urma_unimport_seg)(urma_target_seg_t *tseg);
    urma_status_t (*rs_urma_post_jetty_send_wr)(urma_jetty_t *jetty, urma_jfs_wr_t *wr, urma_jfs_wr_t **bad_wr);
    urma_status_t (*rs_urma_post_jetty_recv_wr)(urma_jetty_t *jetty, urma_jfr_wr_t *wr, urma_jfr_wr_t **bad_wr);
    int (*rs_urma_poll_jfc)(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr);
    urma_status_t (*rs_urma_rearm_jfc)(urma_jfc_t *jfc, bool solicited_only);
    int (*rs_urma_wait_jfc)(urma_jfce_t *jfce, uint32_t jfc_cnt, int time_out, urma_jfc_t *jfc[]);
    void (*rs_urma_ack_jfc)(urma_jfc_t *jfc[], uint32_t nevents[], uint32_t jfc_cnt);
    urma_device_t *(*rs_urma_get_device_by_eid)(urma_eid_t eid, urma_transport_type_t type);
    urma_status_t (*rs_urma_user_ctl)(urma_context_t *ctx, urma_user_ctl_in_t *in, urma_user_ctl_out_t *out);
    urma_status_t (*rs_urma_get_tp_list)(urma_context_t *ctx, urma_get_tp_cfg_t *cfg, uint32_t *tp_cnt,
        urma_tp_info_t *tp_list);
    urma_status_t (*rs_urma_get_tp_attr)(const urma_context_t *ctx, const uint64_t tp_handle,
        uint8_t *tp_attr_cnt, uint32_t *tp_attr_bitmap, urma_tp_attr_value_t *tp_attr);
    urma_status_t (*rs_urma_set_tp_attr)(const urma_context_t *ctx, const uint64_t tp_handle,
        const uint8_t tp_attr_cnt, const uint32_t tp_attr_bitmap, const urma_tp_attr_value_t *tp_attr);
    urma_target_jetty_t *(*rs_urma_import_jetty_ex)(urma_context_t *ctx, urma_rjetty_t *rjetty,
        urma_token_t *token_value, urma_import_jetty_ex_cfg_t *cfg);
    urma_status_t (*rs_urma_alloc_jetty)(urma_context_t *urma_ctx, urma_jetty_cfg_t *cfg, urma_jetty_t **jetty);
    urma_status_t (*rs_urma_set_jetty_opt)(urma_jetty_t *jetty, uint64_t opt, void *buf, uint32_t len);
    urma_status_t (*rs_urma_get_jetty_opt)(urma_jetty_t *jetty, uint64_t opt, void *buf, uint32_t len);
    urma_status_t (*rs_urma_active_jetty)(urma_jetty_t *jetty);
    urma_status_t (*rs_urma_deactive_jetty)(urma_jetty_t *jetty);
    urma_status_t (*rs_urma_free_jetty)(urma_jetty_t *jetty);
    urma_status_t (*rs_urma_alloc_jfc)(urma_context_t *urma_ctx, urma_jfc_cfg_t *cfg, urma_jfc_t **jfc);
    urma_status_t (*rs_urma_set_jfc_opt)(urma_jfc_t *jfc, uint64_t opt, void *buf, uint32_t len);
    urma_status_t (*rs_urma_active_jfc)(urma_jfc_t *jfc);
    urma_status_t (*rs_urma_get_jfc_opt)(urma_jfc_t *jfc, uint64_t opt, void *buf, uint32_t len);
    urma_status_t (*rs_urma_deactive_jfc)(urma_jfc_t *jfc);
    urma_status_t (*rs_urma_free_jfc)(urma_jfc_t *jfc);
};

void rs_ub_api_deinit(void);
int rs_ub_api_init(void);

int rs_urma_init(urma_init_attr_t *conf);
int rs_urma_uninit(void);
urma_device_t **rs_urma_get_device_list(int *num_devices);
urma_device_t *rs_urma_get_device_by_eid(urma_eid_t eid, urma_transport_type_t type);
void rs_urma_free_device_list(urma_device_t **device_list);
urma_eid_info_t *rs_urma_get_eid_list(urma_device_t *dev, uint32_t *cnt);
void rs_urma_free_eid_list(urma_eid_info_t *eid_list);
int rs_urma_query_device(urma_device_t *dev, urma_device_attr_t *dev_attr);
int rs_urma_get_eid_by_ip(const urma_context_t *ctx, const urma_net_addr_t *net_addr, urma_eid_t *eid);
urma_context_t *rs_urma_create_context(urma_device_t *dev, uint32_t eid_index);
int rs_urma_delete_context(urma_context_t *ctx);
urma_jfr_t *rs_urma_create_jfr(urma_context_t *ctx, urma_jfr_cfg_t *jfr_cfg);
int rs_urma_delete_jfr(urma_jfr_t *jfr);
int rs_urma_delete_jfr_batch(urma_jfr_t **jfr_arr, int jfr_num, urma_jfr_t **bad_jfr);
urma_jfc_t *rs_urma_create_jfc(urma_context_t *ctx, urma_jfc_cfg_t *jfc_cfg);
int rs_urma_modify_jfc(urma_jfc_t *jfc, urma_jfc_attr_t *attr);
int rs_urma_delete_jfc(urma_jfc_t *jfc);
urma_jetty_t *rs_urma_create_jetty(urma_context_t *ctx, urma_jetty_cfg_t *jetty_cfg);
int rs_urma_modify_jetty(urma_jetty_t *jetty, urma_jetty_attr_t *attr);
int rs_urma_query_jetty(urma_jetty_t *jetty, urma_jetty_cfg_t *cfg, urma_jetty_attr_t *attr);
int rs_urma_delete_jetty(urma_jetty_t *jetty);
int rs_urma_delete_jetty_batch(urma_jetty_t **jetty_arr, int jetty_num, urma_jetty_t **bad_jetty);
urma_target_jetty_t *rs_urma_import_jetty(urma_context_t *ctx, urma_rjetty_t *rjetty, urma_token_t *token_value);
int rs_urma_unimport_jetty(urma_target_jetty_t *tjetty);
int rs_urma_bind_jetty(urma_jetty_t *jetty, urma_target_jetty_t *tjetty);
int rs_urma_unbind_jetty(urma_jetty_t *jetty);
int rs_urma_flush_jetty(urma_jetty_t *jetty, int cr_cnt, urma_cr_t *cr);
urma_jfce_t *rs_urma_create_jfce(urma_context_t *ctx);
int rs_urma_delete_jfce(urma_jfce_t *jfce);
int rs_urma_get_async_event(urma_context_t *ctx, urma_async_event_t *event);
void rs_urma_ack_async_event(urma_async_event_t *event);
urma_target_seg_t *rs_urma_register_seg(urma_context_t *ctx, urma_seg_cfg_t *seg_cfg);
int rs_urma_unregister_seg(urma_target_seg_t *target_seg);
urma_token_id_t *rs_urma_alloc_token_id(urma_context_t *ctx);
int rs_urma_free_token_id(urma_token_id_t *token_id);
urma_target_seg_t *rs_urma_import_seg(urma_context_t *ctx, urma_seg_t *seg, urma_token_t *token_value,
    uint64_t addr, urma_import_seg_flag_t flag);
int rs_urma_unimport_seg(urma_target_seg_t *tseg);
int rs_urma_post_jetty_send_wr(urma_jetty_t *jetty, urma_jfs_wr_t *wr, urma_jfs_wr_t **bad_wr);
int rs_urma_post_jetty_recv_wr(urma_jetty_t *jetty, urma_jfr_wr_t *wr, urma_jfr_wr_t **bad_wr);
int rs_urma_poll_jfc(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr);
int rs_urma_rearm_jfc(urma_jfc_t *jfc, bool solicited_only);
int rs_urma_wait_jfc(urma_jfce_t *jfce, uint32_t jfc_cnt, int time_out, urma_jfc_t *jfc[]);
void rs_urma_ack_jfc(urma_jfc_t *jfc[], uint32_t nevents[], uint32_t jfc_cnt);
int rs_urma_user_ctl(urma_context_t *ctx, urma_user_ctl_in_t *in, urma_user_ctl_out_t *out);
int rs_urma_get_tp_list(urma_context_t *ctx, urma_get_tp_cfg_t *cfg, uint32_t *tp_cnt, urma_tp_info_t *tp_list);
int rs_urma_get_tp_attr(const urma_context_t *ctx, const uint64_t tp_handle, uint8_t *tp_attr_cnt,
    uint32_t *tp_attr_bitmap, urma_tp_attr_value_t *tp_attr);
int rs_urma_set_tp_attr(const urma_context_t *ctx, const uint64_t tp_handle, const uint8_t tp_attr_cnt,
    const uint32_t tp_attr_bitmap, const urma_tp_attr_value_t *tp_attr);
urma_target_jetty_t *rs_urma_import_jetty_ex(urma_context_t *ctx, urma_rjetty_t *rjetty, urma_token_t *token_value,
    urma_import_jetty_ex_cfg_t *cfg);
int rs_urma_alloc_jetty(urma_context_t *urma_ctx, urma_jetty_cfg_t *cfg, urma_jetty_t **jetty);
int rs_urma_set_jetty_opt(urma_jetty_t *jetty, uint64_t opt, void *buf, uint32_t len);
int rs_urma_active_jetty(urma_jetty_t *jetty);
int rs_urma_get_jetty_opt(urma_jetty_t *jetty, uint64_t opt, void *buf, uint32_t len);
int rs_urma_deactive_jetty(urma_jetty_t *jetty);
int rs_urma_free_jetty(urma_jetty_t *jetty);
int rs_urma_alloc_jfc(urma_context_t *urma_ctx, urma_jfc_cfg_t *cfg, urma_jfc_t **jfc);
int rs_urma_set_jfc_opt(urma_jfc_t *jfc, uint64_t opt, void *buf, uint32_t len);
int rs_urma_active_jfc(urma_jfc_t *jfc);
int rs_urma_get_jfc_opt(urma_jfc_t *jfc, uint64_t opt, void *buf, uint32_t len);
int rs_urma_deactive_jfc(urma_jfc_t *jfc);
int rs_urma_free_jfc(urma_jfc_t *jfc);

#endif // DL_URMA_FUNCTION_H
