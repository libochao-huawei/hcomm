/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "ut_dispatch.h"
#include "rs_inner.h"
#include "rs_ub.h"
#include "rs_ctx.h"
#include "rs_ccu.h"
#include "rs_esched.h"
#include "tc_ut_rs_ctx.h"
#include "ascend_hal_dl.h"
#include "dl_hal_function.h"
#include "hccp_msg.h"

extern void rs_ub_ctx_drv_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb);
extern void rs_ub_ctx_free_jetty_cb(struct rs_ctx_jetty_cb *jetty_cb);
extern int rs_ccu_device_api_init(void);
extern int rs_open_ccu_so(void);
extern void rs_close_ccu_so(void);

struct rs_cb stub_rs_cb;
struct rs_ub_dev_cb stub_dev_cb;
struct rs_ub_dev_cb cr_err_dev_cb = {0};

int stub_rs_ub_get_dev_cb_cr_err(struct rs_cb *rscb, unsigned int devIndex, struct rs_ub_dev_cb **dev_cb)
{
    *dev_cb = &cr_err_dev_cb;
    return 0;
}

int stub_rs_get_rs_cb_v1(unsigned int phyId, struct rs_cb **rs_cb)
{
    *rs_cb = &stub_rs_cb;
    return 0;
}

int stub_rs_ub_get_dev_cb(struct rs_cb *rscb, unsigned int dev_index, struct rs_ub_dev_cb **dev_cb)
{
    stub_dev_cb.rscb = &stub_rs_cb;
    *dev_cb = &stub_dev_cb;
    return 0;
}

int stub_rs_get_rs_cb(unsigned int phyId, struct rs_cb **rs_cb)
{
    static struct rs_cb rs_cb_tmp = {0};

    rs_cb_tmp.protocol = 1;
    *rs_cb = &rs_cb_tmp;
    return 0;
}

void tc_rs_get_dev_eid_info_num()
{
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_eid_info_num, 1, 0);
    ret = rs_get_dev_eid_info_num(0, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_get_dev_eid_info_list()
{
    struct dev_eid_info info_list[5] = {0};
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_eid_info_list, 1, 0);
    ret = rs_get_dev_eid_info_list(0, info_list, 0, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_init()
{
    struct dev_base_attr dev_attr = {0};
    struct ctx_init_attr attr = {0};
    unsigned int dev_index = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(RsSetupSharemem, 1, 0);
    mocker(rs_ub_ctx_init, 1, 0);
    ret = rs_ctx_init(&attr, &dev_index, &dev_attr);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_deinit()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_deinit, 1, 0);
    ret = rs_ctx_deinit(&dev_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_token_id_alloc()
{
    struct RaRsDevInfo dev_info = {0};
    unsigned long long addr = 0;
    unsigned int token_id = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_token_id_alloc, 1, 0);
    ret = rs_ctx_token_id_alloc(&dev_info, &addr, &token_id);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_token_id_free()
{
    struct RaRsDevInfo dev_info = {0};
    unsigned long long addr = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_token_id_free, 1, 0);
    ret = rs_ctx_token_id_free(&dev_info, addr);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_lmem_reg()
{
    struct RaRsDevInfo dev_info = {0};
    struct mem_reg_attr_t mem_attr = {0};
    struct mem_reg_info_t mem_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_lmem_reg, 1, 0);
    ret = rs_ctx_lmem_reg(&dev_info, &mem_attr, &mem_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_lmem_unreg()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_lmem_unreg, 1, 0);
    ret = rs_ctx_lmem_unreg(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_rmem_import()
{
    struct RaRsDevInfo dev_info = {0};
    struct mem_reg_attr_t mem_attr = {0};
    struct mem_reg_info_t mem_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_rmem_import, 1, 0);
    ret = rs_ctx_rmem_import(&dev_info, &mem_attr, &mem_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_rmem_unimport()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_rmem_unimport, 1, 0);
    ret = rs_ctx_rmem_unimport(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_chan_create()
{
    union data_plane_cstm_flag data_plane_flag;
    struct RaRsDevInfo dev_info = {0};
    unsigned long long addr = 0;
    int ret = 0;
    int fd = 0;

    data_plane_flag.bs.poll_cq_cstm = 1;
    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_chan_create, 1, 0);
    ret = rs_ctx_chan_create(&dev_info, data_plane_flag, &addr, &fd);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_chan_destroy()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_chan_destroy, 1, 0);
    ret = rs_ctx_chan_destroy(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_cq_create()
{
    struct RaRsDevInfo dev_info = {0};
    struct ctx_cq_attr attr = {0};
    struct ctx_cq_info info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jfc_create, 1, 0);
    ret = rs_ctx_cq_create(&dev_info, &attr, &info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_cq_destroy()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jfc_destroy, 1, 0);
    ret = rs_ctx_cq_destroy(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_create()
{
    struct RaRsDevInfo dev_info = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_qp_attr qpAttr = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_create, 1, 0);
    ret = rs_ctx_qp_create(&dev_info, &qpAttr, &qp_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_destroy()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_destroy, 1, 0);
    ret = rs_ctx_qp_destroy(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_import()
{
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_info = {0};
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_import, 1, 0);
    ret = rs_ctx_qp_import(&dev_info, &import_attr, &import_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_unimport()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_unimport, 1, 0);
    ret = rs_ctx_qp_unimport(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_bind()
{
    struct rs_ctx_qp_info remote_qp_info = {0};
    struct rs_ctx_qp_info local_qp_info = {0};
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_bind, 1, 0);
    ret = rs_ctx_qp_bind(&dev_info, &local_qp_info, &remote_qp_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_unbind()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_unbind, 1, 0);
    ret = rs_ctx_qp_unbind(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_batch_send_wr()
{
    struct WrlistSendCompleteNum wrlist_num = {0};
    struct wrlist_base_info base_info = {0};
    struct batch_send_wr_data wr_data = {0};
    struct send_wr_resp wr_resp = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_ctx_batch_send_wr, 1, 0);
    ret = rs_ctx_batch_send_wr(&base_info, &wr_data, &wr_resp, &wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_update_ci()
{
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_update_ci, 1, 0);
    ret = rs_ctx_update_ci(&dev_info, 0, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_custom_channel()
{
    struct custom_chan_info_out out = {0};
    struct custom_chan_info_in in = {0};
    int ret = 0;

    mocker_clean();
    mocker(rs_ctx_ccu_custom_channel, 1, 0);
    ret = rs_ctx_custom_channel(&in, &out);
    mocker_clean();
}

void tc_rs_ctx_esched()
{
    ts_ccu_task_report_t ccu_task_info = {0};
    ts_ub_task_report_t ub_task_info = {0};
    struct rs_ctx_jetty_cb jetty_cb = {0};
    urma_jetty_t jetty = {0};
    struct rs_ub_dev_cb rdevCb = {0};
    struct event_info event = {0};
    struct rs_cb rscb = {0};
    rscb.protocol = PROTOCOL_UDMA;
    int ret = 0;

    mocker_clean();
    mocker(rs_ctx_ccu_custom_channel, 10, 0);
    mocker(rs_ub_free_jetty_cb_list, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(DlHalEschedSubmitEvent, 10, 0);
    mocker(rs_urma_unbind_jetty, 10, 0);
    mocker(rs_ub_ctx_drv_jetty_delete, 10, 0);
    mocker(rs_ub_ctx_free_jetty_cb, 10, 0);

    event.priv.msg_len = sizeof(struct tag_ts_hccp_msg);
    struct tag_ts_hccp_msg  *hccp_msg = (struct tag_ts_hccp_msg *)event.priv.msg;
    ccu_task_info.num = 1;
    ub_task_info.num = 1;
    RS_INIT_LIST_HEAD(&rscb.rdevList);
    RsListAddTail(&rdevCb.list, &rscb.rdevList);
    jetty_cb.jetty = &jetty;
    jetty_cb.state = RS_JETTY_STATE_BIND;
    RS_INIT_LIST_HEAD(&rdevCb.jetty_list);
    RsListAddTail(&jetty_cb.list, &rdevCb.jetty_list);

    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 0;
    ub_task_info.array[0].jetty_id = 1;
    hccp_msg->u.ub_task_info = ub_task_info;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 0;
    ub_task_info.array[0].jetty_id = 0;
    hccp_msg->u.ub_task_info = ub_task_info;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 1;
    hccp_msg->u.ccu_task_info = ccu_task_info;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 2;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, -EINVAL);

    hccp_msg->is_app_exit = 1;
    hccp_msg->cmd_type = 1;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    mocker(DlHalEschedWaitEvent, 10, DRV_ERROR_INVALID_DEVICE);
    rs_esched_handle_event(&rscb);
    mocker_clean();

    mocker(DlHalEschedWaitEvent, 10, 0);
    mocker(rs_ctx_ccu_custom_channel, 10, 0);
    mocker(rs_ub_free_jetty_cb_list, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(DlHalEschedSubmitEvent, 10, 0);
    mocker(rs_urma_unbind_jetty, 10, 0);
    mocker(rs_ub_ctx_drv_jetty_delete, 10, 0);
    mocker(rs_ub_ctx_free_jetty_cb, 10, 0);

    hccp_msg->is_app_exit = 2;
    hccp_msg->cmd_type = 1;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, -EINVAL);

    rs_esched_ack_event(&rscb, &event);

    event.priv.msg_len = sizeof(struct tag_ts_hccp_msg) + 1;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
}

void tc_dl_ccu_api_init()
{
    int ret;

    ret = rs_ccu_device_api_init();
    EXPECT_INT_EQ(ret, 0);

    ret = rs_open_ccu_so();
    EXPECT_INT_EQ(ret, 0);

    rs_close_ccu_so();
}

void tc_rs_get_tp_info_list()
{
    struct RaRsDevInfo dev_info = {0};
    struct tp_info info_list[2] = {0};
    struct get_tp_cfg cfg = {0};
    unsigned int num = 1;
    int ret = 0;

    mocker(RsGetRsCb, 1, -EINVAL);
    ret = rs_get_tp_info_list(&dev_info, &cfg, info_list, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UNSUPPORT;
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v1, 10);
    ret = rs_get_tp_info_list(&dev_info, &cfg, info_list, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UDMA;
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v1, 10);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb, 10);
    ret =  rs_get_tp_info_list(&dev_info, &cfg, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_ctx_qp_destroy_batch()
{
    struct RaRsDevInfo dev_info = {0};
    unsigned int ids[] = {1, 2, 3};
    unsigned int num = 3;
    int ret;

    ret = rs_ctx_qp_destroy_batch(NULL, ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_qp_destroy_batch(&dev_info, NULL, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_qp_destroy_batch(&dev_info, ids, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker(RsGetRsCb, 1, -EINVAL);
    ret = rs_ctx_qp_destroy_batch(&dev_info, ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UDMA;
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v1, 10);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb, 10);
    mocker(rs_ub_ctx_jetty_destroy_batch, 1, 0);
    ret =  rs_ctx_qp_destroy_batch(&dev_info, ids, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_ctx_qp_query_batch()
{
    struct RaRsDevInfo dev_info = {0};
    struct jetty_attr attr[10];
    unsigned int ids[10];
    unsigned int num;
    int ret;

    ret = rs_ctx_qp_query_batch(NULL, ids, attr, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_qp_query_batch(&dev_info, NULL, attr, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_qp_query_batch(&dev_info, ids, NULL, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_qp_query_batch(&dev_info, ids, attr, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker(RsGetRsCb, 1, -EINVAL);
    ret = rs_ctx_qp_query_batch(&dev_info, ids, attr, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UDMA;
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v1, 10);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb, 10);
    mocker(rs_ub_ctx_query_jetty_batch, 1, 0);
    ret =  rs_ctx_qp_query_batch(&dev_info, ids, attr, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_net_api_init_deinit()
{
    int ret = 0;

    ret = rs_net_api_init();
    EXPECT_INT_EQ(0, ret);

    rs_net_api_deinit();
}

void tc_rs_net_alloc_jfc_id()
{
    int ret = 0;

    ret = rs_net_alloc_jfc_id(NULL, 0, NULL);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_net_free_jfc_id()
{
    int ret = 0;

    ret = rs_net_free_jfc_id(NULL, 0, 0);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_net_alloc_jetty_id()
{
    int ret = 0;

    ret = rs_net_alloc_jetty_id(NULL, 0, NULL);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_net_free_jetty_id()
{
    int ret = 0;

    ret = rs_net_free_jetty_id(NULL, 0, 0);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_net_get_cqe_base_addr()
{
    unsigned long long cqe_base_addr;
    int ret = 0;

    ret = rs_net_get_cqe_base_addr(0, &cqe_base_addr);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_ccu_get_cqe_base_addr()
{
    unsigned long long cqe_base_addr;
    int ret = 0;

    ret = rs_ccu_get_cqe_base_addr(0, &cqe_base_addr);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_ctx_get_aux_info()
{
    struct RaRsDevInfo dev_info = {0};
    struct aux_info_in info_in;
    struct aux_info_out info_out;
    int ret = 0;

    (void)memset_s(&info_out, sizeof(struct aux_info_out), 0, sizeof(struct aux_info_out));
    (void)memset_s(&info_in, sizeof(struct aux_info_in), 0, sizeof(struct aux_info_in));

    ret = rs_ctx_get_aux_info(NULL, &info_in, &info_out);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_get_aux_info(&dev_info, NULL, &info_out);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_get_aux_info(&dev_info, &info_in, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker_clean();
    mocker(RsGetRsCb, 1, -EINVAL);
    ret = rs_ctx_get_aux_info(&dev_info, &info_in, &info_out);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, -ENODEV);
    ret = rs_ctx_get_aux_info(&dev_info, &info_in, &info_out);
    EXPECT_INT_EQ(-ENODEV, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_get_aux_info, 1, -1);
    ret = rs_ctx_get_aux_info(&dev_info, &info_in, &info_out);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_get_aux_info, 1, 0);
    ret = rs_ctx_get_aux_info(&dev_info, &info_in, &info_out);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_get_tp_attr()
{
    struct RaRsDevInfo dev_info = {0};
    unsigned int attr_bitmap;
    struct tp_attr attr;
    uint64_t tp_handle;
    int ret;

    ret = rs_get_tp_attr(NULL, &attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_get_tp_attr(&dev_info, NULL, tp_handle, &attr);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_get_tp_attr(&dev_info, &attr_bitmap, tp_handle, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker(RsGetRsCb, 1, -EINVAL);
    ret = rs_get_tp_attr(&dev_info, &attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UDMA;
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v1, 10);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb, 10);
    mocker(rs_ub_get_tp_attr, 1, 0);
    ret =  rs_get_tp_attr(&dev_info, &attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_set_tp_attr()
{
    struct RaRsDevInfo dev_info = {0};
    unsigned int attr_bitmap;
    struct tp_attr attr;
    uint64_t tp_handle;
    int ret;

    ret = rs_set_tp_attr(NULL, attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_set_tp_attr(&dev_info, attr_bitmap, tp_handle, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker(RsGetRsCb, 1, -EINVAL);
    ret = rs_set_tp_attr(&dev_info, attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UDMA;
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v1, 10);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb, 10);
    mocker(rs_ub_set_tp_attr, 1, 0);
    ret =  rs_set_tp_attr(&dev_info, attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_ctx_get_cr_err_info_list()
{
    struct rs_ctx_jetty_cb jetty_cb = {0};
    struct CrErrInfo info_list[1] = {0};
    struct RaRsDevInfo dev_info = {0};
    unsigned int num = 1;
    int ret = 0;

    mocker_clean();
    ret = rs_ctx_get_cr_err_info_list(NULL, NULL, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_get_cr_err_info_list(&dev_info, NULL, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_ctx_get_cr_err_info_list(&dev_info, info_list, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker(RsGetRsCb, 1, -1);
    ret = rs_ctx_get_cr_err_info_list(&dev_info, info_list, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, -1);
    ret = rs_ctx_get_cr_err_info_list(&dev_info, info_list, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb_cr_err, 10);
    RS_INIT_LIST_HEAD(&cr_err_dev_cb.jetty_list);
    ret = rs_ctx_get_cr_err_info_list(&dev_info, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    RsListAddTail(&jetty_cb.list, &cr_err_dev_cb.jetty_list);
    jetty_cb.dev_cb = &cr_err_dev_cb;
    jetty_cb.cr_err_info.info.status = 1;
    mocker(RsGetRsCb, 1, 0);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb_cr_err, 1);
    ret = rs_ctx_get_cr_err_info_list(&dev_info, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}
