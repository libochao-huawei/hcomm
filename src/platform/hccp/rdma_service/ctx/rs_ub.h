/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_UB_H
#define RS_UB_H

#include <udma_u_ctl.h>
#include <urma_types.h>
#include "dl_urma_function.h"
#include "hccp_async_ctx.h"
#include "hccp_async.h"
#include "hccp_ctx.h"
#include "hccp_common.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"
#include "rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs.h"

struct rs_jetty_key_info {
    urma_jetty_id_t jetty_id;
    urma_transport_mode_t trans_mode;
};

enum rs_jetty_state {
    RS_JETTY_STATE_INIT = 0,
    RS_JETTY_STATE_CREATED = 1,
    RS_JETTY_STATE_IMPORTED = 2,
    RS_JETTY_STATE_BIND = 3,
    RS_JETTY_STATE_MAX
};

struct jetty_destroy_batch_info {
    struct rs_ctx_jetty_cb **jetty_cb_arr;
    urma_jetty_t **jetty_arr;
    urma_jfr_t **jfr_arr;
};

int rs_ub_get_dev_eid_info_num(unsigned int phyId, unsigned int *num);
int rs_ub_get_ue_info(urma_context_t *urma_ctx, struct dev_base_attr *dev_attr);
int rs_ub_get_dev_eid_info_list(unsigned int phyId, struct dev_eid_info info_list[], unsigned int start_index,
    unsigned int count);
int rs_ub_ctx_init(struct rs_cb *rs_cb, struct ctx_init_attr *attr, unsigned int *devIndex,
    struct dev_base_attr *dev_attr);
int rs_ub_get_dev_cb(struct rs_cb *rscb, unsigned int devIndex, struct rs_ub_dev_cb **dev_cb);
int rs_ub_ctx_deinit(struct rs_ub_dev_cb *dev_cb);
int rs_ub_get_eid_by_ip(struct rs_ub_dev_cb *dev_cb, struct IpInfo ip[], union hccp_eid eid[], unsigned int *num);
int rs_ub_get_tp_info_list(struct rs_ub_dev_cb *dev_cb, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num);
int rs_ub_get_tp_attr(struct rs_ub_dev_cb *dev_cb, unsigned int *attr_bitmap, const uint64_t tp_handle,
    struct tp_attr *attr);
int rs_ub_set_tp_attr(struct rs_ub_dev_cb *dev_cb, const unsigned int attr_bitmap, const uint64_t tp_handle,
    struct tp_attr *attr);
int rs_ub_ctx_token_id_alloc(struct rs_ub_dev_cb *dev_cb, unsigned long long *addr, unsigned int *token_id);
int rs_ub_ctx_token_id_free(struct rs_ub_dev_cb *dev_cb, unsigned long long addr);
int rs_ub_ctx_lmem_reg(struct rs_ub_dev_cb *dev_cb, struct mem_reg_attr_t *mem_attr, struct mem_reg_info_t *mem_info);
int rs_ub_ctx_lmem_unreg(struct rs_ub_dev_cb *dev_cb, unsigned long long addr);
int rs_ub_ctx_rmem_import(struct rs_ub_dev_cb *dev_cb, struct mem_import_attr_t *mem_attr,
    struct mem_import_info_t *mem_info);
int rs_ub_ctx_rmem_unimport(struct rs_ub_dev_cb *dev_cb, unsigned long long addr);
int rs_ub_ctx_chan_create(struct rs_ub_dev_cb *dev_cb, union data_plane_cstm_flag data_plane_flag,
    unsigned long long *addr, int *fd);
int rs_ub_ctx_chan_destroy(struct rs_ub_dev_cb *dev_cb, unsigned long long addr);
int rs_ub_ctx_jfc_create(struct rs_ub_dev_cb *dev_cb, struct ctx_cq_attr *attr, struct ctx_cq_info *info);
int rs_ub_ctx_jfc_destroy(struct rs_ub_dev_cb *dev_cb, unsigned long long addr);
int rs_ub_ctx_jetty_create(struct rs_ub_dev_cb *dev_cb, struct ctx_qp_attr *attr, struct qp_create_info *info);
int rs_ub_ctx_reg_jetty_db(struct rs_ctx_jetty_cb *jetty_cb, struct udma_u_jetty_info *jetty_info);
int rs_ub_ctx_query_jetty_batch(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_ids[], struct jetty_attr attr[],
    unsigned int *num);
int rs_ub_ctx_jetty_destroy(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id);
int rs_ub_ctx_jetty_destroy_batch(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_ids[], unsigned int *num);
int rs_ub_ctx_jetty_import(struct rs_ub_dev_cb *dev_cb, struct rs_jetty_import_attr *import_attr,
    struct rs_jetty_import_info *import_info);
int rs_ub_ctx_jetty_unimport(struct rs_ub_dev_cb *dev_cb, unsigned int rem_jetty_id);
int rs_ub_ctx_jetty_bind(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_qp_info *jetty_info,
    struct rs_ctx_qp_info *rjetty_info);
int rs_ub_ctx_jetty_unbind(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id);
int rs_ub_ctx_jetty_free(struct rs_cb *rscb, unsigned int ue_info, unsigned int jetty_id);
void rs_ub_free_jetty_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jetty_list,
    struct RsListHead *rjetty_list);
int rs_ub_ctx_batch_send_wr(struct rs_cb *rs_cb, struct wrlist_base_info *base_info,
    struct batch_send_wr_data *wr_data, struct send_wr_resp *wr_resp, struct WrlistSendCompleteNum *wrlist_num);
int rs_ub_ctx_jetty_update_ci(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id, uint16_t ci);
int rs_ub_ctx_get_aux_info(struct rs_ub_dev_cb *dev_cb, struct aux_info_in *info_in, struct aux_info_out *info_out);
int rs_epoll_event_jfc_in_handle(struct rs_cb *rs_cb, int fd);
int RsEpollEventUrmaAsyncEventInHandle(struct rs_cb *rsCb, int fd);
void rs_ub_ctx_get_async_events(struct rs_ub_dev_cb *dev_cb, struct async_event async_events[], unsigned int *num);
#endif // RS_UB_H
