/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#define SOCK_CONN_TAG_SIZE 192
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>

#include "ut_dispatch.h"
#include "stub/ibverbs.h"
#include "rs.h"
#include "rs_ub.h"
#include "rs_ccu.h"
#include "rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "tc_ut_rs_ub.h"
#include "rs_drv_rdma.h"
#include "stub/verbs_exp.h"
#include "tls.h"
#include "encrypt.h"
#include "rs_epoll.h"
#include "rs_socket.h"
#include "ra_rs_err.h"

extern uint32_t rs_generate_ue_info(uint32_t die_id, uint32_t func_id);
extern uint32_t rs_generate_dev_index(uint32_t dev_cnt, uint32_t die_id, uint32_t func_id);
extern int rs_ub_get_rdev_cb(struct rs_cb *rs_cb, unsigned int rdevIndex, struct rs_ub_dev_cb **dev_cb);
extern int rs_urma_device_api_init(void);
extern int rs_open_urma_so(void);
extern int rs_urma_jetty_api_init(void);
extern int rs_urma_jfc_api_init(void);
extern int rs_urma_segment_api_init(void);
extern int rs_urma_data_api_init(void);
extern urma_device_t **rs_urma_get_device_list(int *num_devices);
extern urma_eid_info_t *rs_urma_get_eid_list(urma_device_t *dev, uint32_t *cnt);
extern void rs_urma_free_device_list(urma_device_t **device_list);
extern void rs_urma_free_eid_list(urma_eid_info_t *eid_list);
extern int rs_urma_get_eid_by_ip(const urma_context_t *ctx, const urma_net_addr_t *net_addr, urma_eid_t *eid);
extern void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg);
extern int rs_ub_ctx_reg_jetty_db(struct rs_ctx_jetty_cb *jetty_cb, struct udma_u_jetty_info *jetty_info);
extern int RsInitRscbCfg(struct rs_cb *rscb, struct RsInitConfig *cfg);
extern int rs_ub_create_ctx(urma_device_t *urma_dev, unsigned int eid_index, urma_context_t **urma_ctx);
extern int rs_ub_get_ue_info(urma_context_t *urma_ctx, struct dev_base_attr *dev_base_attr);
extern int rs_ub_get_dev_attr(struct rs_ub_dev_cb *dev_cb, struct dev_base_attr *dev_attr, unsigned int *dev_index);
extern int rs_ub_get_jfc_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long addr, struct rs_ctx_jfc_cb **jfc_cb);
extern void rs_ub_free_seg_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *lseg_list,
    struct RsListHead *rseg_list);
extern void rs_ub_free_jetty_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jetty_list,
    struct RsListHead *rjetty_list);
extern void rs_ub_free_jfc_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jfc_list);
extern void rs_ub_free_jfce_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jfce_list);
extern void rs_ub_free_token_id_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *token_id_list);
extern int rs_ub_get_token_id_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long addr,
    struct rs_token_id_cb **token_id_cb);
extern int rs_ub_init_seg_cb(struct mem_reg_attr_t *mem_attr, struct rs_ub_dev_cb *dev_cb, struct rs_seg_cb *seg_cb);
extern int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc);
extern int rs_ub_ctx_init_jetty_cb(struct rs_ub_dev_cb *dev_cb, struct ctx_qp_attr *attr,
    struct rs_ctx_jetty_cb **jetty_cb);
extern int rs_ub_query_jfc_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long scq_index, unsigned long long rcq_index,
                              struct rs_ctx_jfc_cb **send_jfc_cb, struct rs_ctx_jfc_cb **recv_jfc_cb);
extern void rs_ub_ctx_free_jetty_cb(struct rs_ctx_jetty_cb *jetty_cb);
extern int rs_ub_ctx_drv_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, struct rs_ctx_jfc_cb *send_jfc_cb,
    struct rs_ctx_jfc_cb *recv_jfc_cb);
extern int rs_ub_fill_jetty_info(struct rs_ctx_jetty_cb *jetty_cb, struct qp_create_info *jetty_info);
extern void rs_ub_ctx_drv_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb);
extern int rs_ub_ctx_init_rjetty_cb(struct rs_ub_dev_cb *dev_cb, struct rs_jetty_import_attr *import_attr,
    struct rs_ctx_rem_jetty_cb **rjetty_cb);
extern int rs_ub_ctx_drv_jetty_import(struct rs_ctx_rem_jetty_cb *rjetty_cb);
extern int rs_ub_get_jetty_cb(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id, struct rs_ctx_jetty_cb **jetty_cb);
extern void rs_close_urma_so(void);
extern int rs_ub_destroy_jetty_cb_batch(struct jetty_destroy_batch_info *batch_info, unsigned int *num);
extern int rs_ub_get_jetty_destroy_batch_info(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_ids[],
    struct jetty_destroy_batch_info *batch_info, unsigned int *num);
extern int rs_ub_calloc_jetty_batch_info(struct jetty_destroy_batch_info *batch_info, unsigned int num);
extern void rs_ub_free_jetty_cb_batch(struct jetty_destroy_batch_info *batch_info,
    unsigned int *num, urma_jetty_t *bad_jetty, urma_jfr_t *bad_jfr);
extern int rs_ub_ctx_jfc_create_normal(struct rs_ub_dev_cb *dev_cb, urma_jfc_cfg_t *jfc_cfg, urma_jfc_t **out_jfc);
extern int rs_ub_get_jfce_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long addr, struct rs_ctx_jfce_cb **jfce_cb);
extern int rs_handle_epoll_poll_jfc(struct rs_ub_dev_cb *dev_cb, urma_jfce_t *jfce);

struct RsConnInfo g_conn = {0};
char g_rev_buf[RS_BUF_SIZE] = {0};
extern struct rs_cb stub_rs_cb;
extern struct rs_ub_dev_cb stub_dev_cb;
struct rs_ctx_jetty_cb jetty_cb_stub = {0};
struct rs_ctx_jfce_cb g_jfce_cb = {0};

int rs_ub_get_jfce_cb_stub(struct rs_ub_dev_cb *dev_cb, unsigned long long addr, struct rs_ctx_jfce_cb **jfce_cb)
{
    *jfce_cb = &g_jfce_cb;
    return 0;
}

int rs_ub_get_jetty_cb_stub(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id, struct rs_ctx_jetty_cb **jetty_cb)
{
    *jetty_cb = &jetty_cb_stub;
    return 0;
}

void rs_ub_free_jetty_cb_batch_stub(struct jetty_destroy_batch_info *batch_info,
    unsigned int *num, urma_jetty_t *bad_jetty, urma_jfr_t *bad_jfr)
{
    return;
}

void tc_rs_ub_get_rdev_cb()
{
    struct rs_ub_dev_cb *rdev_cb_out;
    struct rs_ub_dev_cb rdevCb = {0};
    unsigned int rdevIndex = 0;
    struct rs_cb rs_cb;
    int ret;

    RS_INIT_LIST_HEAD(&rs_cb.rdevList);
    RsListAddTail(&rdevCb.list, &rs_cb.rdevList);

    ret = rs_ub_get_dev_cb(&rs_cb, rdevIndex, &rdev_cb_out);
    EXPECT_INT_EQ(ret, 0);

    rdevIndex = 1;
    ret = rs_ub_get_dev_cb(&rs_cb, rdevIndex, &rdev_cb_out);
    EXPECT_INT_EQ(ret, -ENODEV);

    return;
}

void tc_rs_urma_api_init_abnormal()
{
    int ret;

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, 0);
    mocker(rs_urma_jfc_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, 0);
    mocker(rs_urma_jfc_api_init, 100, 0);
    mocker(rs_urma_segment_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, 0);
    mocker(rs_urma_jfc_api_init, 100, 0);
    mocker(rs_urma_segment_api_init, 100, 0);
    mocker(rs_urma_data_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    return;
}

void tc_rs_ub_v2()
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct dev_base_attr attr = {0};
    struct RsInitConfig cfg = {0};
    struct ctx_init_attr info = {0};
    struct rs_cb rscb = {0};
    unsigned long long token_id_addr = 0;
    unsigned int token_id_num = 0;
    unsigned int dev_index;
    int ret = 0;

    struct mem_reg_attr_t lmem_attr = {0};
    struct mem_reg_info_t lmem_info = {0};
    struct mem_import_attr_t rmem_attr = {0};
    struct mem_import_info_t rmem_info = {0};
    void *addr = malloc(1);
    lmem_attr.mem.addr = (uintptr_t)addr;
    lmem_attr.mem.size = 1;
    lmem_attr.ub.flags.bs.token_id_valid = 1;

    cfg.chipId = 0;
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    RS_INIT_LIST_HEAD(&rscb.rdevList);
    ret = rs_ub_ctx_init(&rscb, &info, &dev_index, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_get_dev_cb(&rscb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);

    lmem_attr.ub.token_id_addr = token_id_addr;

    ret = rs_ub_ctx_lmem_reg(dev_cb, &lmem_attr, &lmem_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, &rmem_attr, &rmem_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_unimport(dev_cb, rmem_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_unreg(dev_cb, lmem_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_deinit(dev_cb);
    EXPECT_INT_EQ(0, ret);

    free(addr);
    addr = NULL;

    ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}

urma_device_t tc_urma_dev = {0};
urma_device_t *tc_urma_device_list[1] = {&tc_urma_dev};
urma_eid_info_t tc_urma_eid_list[1] = {0};
urma_eid_info_t tc_urma_eid_list2[2] = {0};

urma_device_t **tc_rs_urma_get_device_list_stub(int *num_devices)
{
    *num_devices = 1;
    return tc_urma_device_list;
}

void tc_rs_ub_get_dev_eid_info_num()
{
    int ret;
    unsigned int phyId;
    unsigned int num;

    phyId = RS_MAX_DEV_NUM;
    ret = rs_ub_get_dev_eid_info_num(phyId, &num);
    EXPECT_INT_EQ(0, ret);

    mocker(rs_urma_get_device_list, 10, NULL);
    ret = rs_ub_get_dev_eid_info_num(phyId, &num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_urma_get_eid_list, 10, NULL);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_num(phyId, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_urma_get_eid_list, 10, tc_urma_eid_list);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_num(phyId, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

int rs_ub_get_dev_eid_info_num_stub(unsigned int phyId, unsigned int *num)
{
    *num = 1;
    return 0;
}

urma_eid_info_t *rs_urma_get_eid_list_stub(urma_device_t *dev, uint32_t *cnt)
{
    *cnt = 1;
    return tc_urma_eid_list;
}

urma_eid_info_t *rs_urma_get_eid_list_stub2(urma_device_t *dev, uint32_t *cnt)
{
    *cnt = 2;
    return tc_urma_eid_list2;
}

void tc_rs_ub_get_dev_eid_info_list()
{
    int ret;
    unsigned int phyId;
    unsigned int start_index;
    unsigned int count;
    struct dev_eid_info info_list[1] = {0};

    phyId = 0;
    start_index = 0;
    count = 1;
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(0, ret);

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker(rs_urma_get_device_list, 10, NULL);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_urma_get_eid_list, 10, NULL);
    mocker(rs_urma_free_device_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_ub_create_ctx, 10, 0);
    mocker(rs_ub_get_ue_info, 10, 0);
    mocker_invoke(rs_urma_get_eid_list, rs_urma_get_eid_list_stub2, 10);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    mocker(rs_urma_delete_context, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_ub_create_ctx, 10, -ENODEV);
    mocker_invoke(rs_urma_get_eid_list, rs_urma_get_eid_list_stub, 10);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(-ENODEV, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_ub_create_ctx, 10, 0);
    mocker(rs_ub_get_ue_info, 10, -259);
    mocker_invoke(rs_urma_get_eid_list, rs_urma_get_eid_list_stub, 10);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    mocker(rs_urma_delete_context, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();

    start_index = (UINT_MAX / 2) + 1;
    count = (UINT_MAX / 2);
    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    start_index = 0;
    count = 2;
    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    ret = rs_ub_get_dev_eid_info_list(phyId, info_list, start_index, count);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();
}

struct rs_cb *tc_rs_ub_v2_init(int mode, unsigned int *dev_index)
{
    int ret;
    struct dev_base_attr attr = {0};
    struct RsInitConfig cfg = {0};
    struct ctx_init_attr info = {0};
    struct rs_cb *rs_cb;
    cfg.hccpMode = mode;

    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsGetRsCb(0, &rs_cb);
    EXPECT_INT_EQ(ret, 0);
    RS_INIT_LIST_HEAD(&rs_cb->rdevList);

    ret = rs_ub_ctx_init(rs_cb, &info, dev_index, &attr);
    EXPECT_INT_EQ(0, ret);

    return rs_cb;
}

void tc_rs_ub_v2_deinit(struct rs_cb *rs_cb, int mode, unsigned int dev_index)
{
    int ret;
    struct RsInitConfig cfg = {0};
    cfg.hccpMode = mode;
    struct rs_ub_dev_cb *dev_cb = NULL;

    ret = rs_ub_get_dev_cb(rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_deinit(dev_cb);
    EXPECT_INT_EQ(ret, 0);
    ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ub_ctx_token_id_alloc()
{
    tc_rs_ub_ctx_token_id_alloc1();
    tc_rs_ub_ctx_token_id_alloc2();
    tc_rs_ub_ctx_token_id_alloc3();
}

void tc_rs_ub_ctx_token_id_alloc1()
{
    unsigned long long addr = 0;
    unsigned int dev_index = 0;
    unsigned int token_id = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr, &token_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_token_id_alloc2()
{
    unsigned long long addr = 0;
    unsigned int dev_index = 0;
    unsigned int token_id = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    mocker(rs_urma_alloc_token_id, 10, NULL);
    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr, &token_id);
    EXPECT_INT_NE(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_token_id_alloc3()
{
    unsigned long long addr = 0;
    unsigned long long addr1 = 0;
    unsigned int dev_index = 0;
    unsigned int token_id = 0;
    unsigned int token_id1 = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr, &token_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr1, &token_id1);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr1);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jfce_create()
{
    union data_plane_cstm_flag data_plane_flag;
    struct rs_ub_dev_cb *dev_cb = NULL;
    unsigned long long addr = 0;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    int fd = 0;
    int ret;

    mocker_clean();
    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    data_plane_flag.bs.poll_cq_cstm = 1;
    ret = rs_ub_ctx_chan_create(dev_cb, data_plane_flag, &addr, &fd);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_chan_destroy(dev_cb, addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jfc_create()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_cq_attr attr = {0};
    struct ctx_cq_info info = {0};
    struct rs_ub_dev_cb *dev_cb = NULL;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    attr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, info.addr);
    EXPECT_INT_EQ(0, ret);

    attr.ub.mode = JFC_MODE_CCU_POLL;
    attr.ub.ccu_ex_cfg.valid = 1;

    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, info.addr);
    EXPECT_INT_EQ(0, ret);

    attr.ub.mode = JFC_MODE_MAX;
    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker_clean();
    attr.ub.mode = JFC_MODE_NORMAL;
    mocker(rs_ub_ctx_jfc_create_normal, 1, 0);
    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(rs_ub_ctx_jfc_create_normal, 1, -1);
    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jfc_create_normal()
{
    struct rs_ub_dev_cb dev_cb = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    urma_jfce_t jfce = {0};
    int ret;

    g_jfce_cb.jfce_addr = 1;
    jfc_cfg.jfce = (urma_jfce_t *)(uintptr_t)g_jfce_cb.jfce_addr;
    mocker_clean();
    mocker(rs_ub_get_jfce_cb, 1, -1);
    ret = rs_ub_ctx_jfc_create_normal(&dev_cb, &jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_jfce_cb, rs_ub_get_jfce_cb_stub, 1);
    mocker(rs_urma_create_jfc, 1, NULL);
    ret = rs_ub_ctx_jfc_create_normal(&dev_cb, &jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_jfce_cb, rs_ub_get_jfce_cb_stub, 1);
    mocker(rs_urma_rearm_jfc, 1, -1);
    ret = rs_ub_ctx_jfc_create_normal(&dev_cb, &jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    rs_urma_delete_jfc(out_jfc);
    mocker_clean();

    mocker_invoke(rs_ub_get_jfce_cb, rs_ub_get_jfce_cb_stub, 1);
    ret = rs_ub_ctx_jfc_create_normal(&dev_cb, &jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(0, ret);
    rs_urma_delete_jfc(out_jfc);
    mocker_clean();
}

void tc_rs_ub_ctx_jetty_create()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_qp_attr qpAttr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cqAttr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_ub_dev_cb *dev_cb = NULL;
    unsigned long long token_id_addr = 0;
    unsigned int token_id_num = 0;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cqAttr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    qpAttr.ub.mode = JETTY_MODE_CCU;
    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);
    qpAttr.ub.token_id_addr = token_id_addr;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    cqAttr.ub.mode = JFC_MODE_CCU_POLL;
    cqAttr.ub.ccu_ex_cfg.valid = 1;
    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    qpAttr.ub.mode = JETTY_MODE_CCU_TA_CACHE;
    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);
    qpAttr.ub.token_id_addr = token_id_addr;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    qpAttr.ub.mode = JETTY_MODE_MAX;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(-EINVAL, ret);

    qpAttr.ub.mode = JETTY_MODE_CCU_TA_CACHE;
    qpAttr.ub.ta_cache_mode.lock_flag = 0;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(-EINVAL, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jetty_import()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct ctx_qp_attr qpAttr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cqAttr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cqAttr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jetty_bind()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct ctx_qp_attr qpAttr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cqAttr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};
    struct rs_ctx_qp_info rs_qp_info = {0};

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cqAttr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unbind(dev_cb, rs_qp_info.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_unbind(dev_cb, rs_qp_info.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cqAttr.ub.mode = 10000;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_NE(0, ret);

    cqAttr.ub.mode = JFC_MODE_STARS_POLL;
    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    qpAttr.ub.mode = JFC_MODE_STARS_POLL;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    struct rs_ctx_jetty_cb jetty_cb = {0};
    urma_jetty_t jetty = {0};
    jetty_cb.jetty = &jetty;
    jetty_cb.dev_cb = dev_cb;
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);

}

void tc_rs_ub_ctx_batch_send_wr()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_qp_attr qpAttr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cqAttr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};
    struct rs_ctx_qp_info rs_qp_info = {0};
    struct wrlist_base_info base_info = {0};
    struct batch_send_wr_data wr_data_nop[1] = {0};
    struct batch_send_wr_data wr_data[1] = {0};
    struct send_wr_resp wr_resp[1] = {0};
    struct WrlistSendCompleteNum wrlist_num= {0};
    struct mem_reg_attr_t mem_reg_attr = {0};
    struct mem_reg_info_t mem_reg_info = {0};
    struct mem_import_attr_t mem_import_attr = {0};
    struct mem_import_info_t mem_import_info = {0};
    urma_token_id_t *token_id = NULL;
    unsigned long long token_id_addr = 0;
    unsigned int token_id_num = 0;
    unsigned int complete_num = 0;
    struct rs_ub_dev_cb *dev_cb = NULL;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cqAttr.ub.mode = JFC_MODE_STARS_POLL;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;
    void *addr = malloc(1);
    mem_reg_attr.mem.addr = (uintptr_t)addr;
    mem_reg_attr.mem.size = 1;
    mem_reg_attr.ub.flags.bs.token_id_valid = 1;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);

    mem_reg_attr.ub.token_id_addr = token_id_addr;

    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    qpAttr.ub.mode = JETTY_MODE_CCU;
    qpAttr.ub.token_id_addr = token_id_addr;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_NE(0, ret);

    qpAttr.ub.mode = 0;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_reg(dev_cb, &mem_reg_attr, &mem_reg_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, &mem_import_attr, &mem_import_info);
    EXPECT_INT_EQ(0, ret);

    wr_data[0].dev_rmem_handle = mem_import_info.ub.target_seg_handle;
    wr_data[0].num_sge = 1;
    wr_data[0].sges[0].addr = addr;
    wr_data[0].sges[0].len = 1;
    wr_data[0].sges[0].dev_lmem_handle = mem_reg_info.ub.target_seg_handle;

    base_info.dev_index = dev_index;
    ret = rs_ub_ctx_batch_send_wr(tc_rs_cb, &base_info, wr_data, wr_resp, &wrlist_num);
    EXPECT_INT_EQ(0, ret);

    wr_data[0].ub.rem_jetty = 0xfffff;
    ret = rs_ub_ctx_batch_send_wr(tc_rs_cb, &base_info, wr_data, wr_resp, &wrlist_num);
    EXPECT_INT_NE(0, ret);

    wr_data_nop[0].ub.opcode = RA_UB_OPC_NOP;
    ret = rs_ub_ctx_batch_send_wr(tc_rs_cb, &base_info, wr_data_nop, wr_resp, &wrlist_num);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_unimport(dev_cb, mem_import_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_update_ci(dev_cb, 10000, 0);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_update_ci(dev_cb, 0, 0);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_unreg(dev_cb, mem_reg_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unbind(dev_cb, rs_qp_info.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    free(addr);
    addr = NULL;
    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_free_cb_list()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_qp_attr qpAttr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cqAttr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};
    struct rs_ctx_qp_info rs_qp_info = {0};
    struct mem_reg_attr_t mem_reg_attr = {0};
    struct mem_reg_info_t mem_reg_info = {0};
    struct mem_import_attr_t mem_import_attr = {0};
    struct mem_import_info_t mem_import_info = {0};
    unsigned long long jfce_addr = 0;
    unsigned long long token_id_addr = 0;
    unsigned int token_id = 0;
    unsigned int complete_num = 0;
    struct rs_ub_dev_cb *dev_cb = NULL;
    union data_plane_cstm_flag data_plane_flag;
    int fd = 0;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cqAttr.ub.mode = JFC_MODE_STARS_POLL;
    void *addr = malloc(1);
    mem_reg_attr.mem.addr = (uintptr_t)addr;
    mem_reg_attr.mem.size = 1;
    mem_reg_attr.ub.flags.bs.token_id_valid = 1;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id);
    EXPECT_INT_EQ(0, ret);

    mem_reg_attr.ub.token_id_addr = token_id_addr;

    ret = rs_ub_ctx_jfc_create(dev_cb, &cqAttr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qpAttr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_reg(dev_cb, &mem_reg_attr, &mem_reg_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, &mem_import_attr, &mem_import_info);
    EXPECT_INT_EQ(0, ret);

    data_plane_flag.bs.poll_cq_cstm = 1;
    ret = rs_ub_ctx_chan_create(dev_cb, data_plane_flag, &jfce_addr, &fd);
    EXPECT_INT_EQ(0, ret);

    free(addr);
    addr = NULL;
    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_ext_jetty_create()
{
    struct rs_ctx_jetty_cb jetty_cb = { 0 };
    struct rs_ub_dev_cb dev_cb = { 0 };
    urma_jetty_cfg_t jetty_cfg = { 0 };
    struct rs_cb rscb = { 0 };

    dev_cb.rscb = &rscb;
    jetty_cb.dev_cb = &dev_cb;
    jetty_cb.jetty_mode = JETTY_MODE_USER_CTL_NORMAL;
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);

    mocker_clean();
    mocker(rs_ub_ctx_reg_jetty_db, 1, 0);
    jetty_cb.jetty_mode = JETTY_MODE_CCU_TA_CACHE;
    rs_ub_ctx_ext_jetty_create_ta_cache(&jetty_cb, &jetty_cfg);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
    mocker_clean();
}

void tc_rs_ub_ctx_rmem_import()
{
    struct mem_import_attr_t rmem_attr = {0};
    struct mem_import_info_t rmem_info = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    int ret;

    mocker(rs_ub_get_dev_cb, 2, 0);
    mocker(memcpy_s, 2, -1);
    ret = rs_ub_ctx_rmem_import(&dev_cb, &rmem_attr, &rmem_info);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_drv_jetty_import()
{
    struct rs_ctx_rem_jetty_cb rjetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_context_t urma_ctx = {0};
    int ret = 0;

    rjetty_cb.mode = JETTY_IMPORT_MODE_EXP;
    dev_cb.urma_ctx = &urma_ctx;
    rjetty_cb.dev_cb = &dev_cb;
    ret = rs_ub_ctx_drv_jetty_import(&rjetty_cb);
    EXPECT_INT_EQ(0, ret);

    free(rjetty_cb.tjetty);
    rjetty_cb.tjetty = NULL;
}

void tc_rs_ub_dev_cb_init()
{
    struct dev_base_attr base_attr = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    struct ctx_init_attr attr = {0};
    struct rs_cb rscb = {0};
    int dev_index = 0;
    int ret = 0;

    mocker(pthread_mutex_init, 1, 0);
    mocker(rs_ub_create_ctx, 1, -1);
    mocker(pthread_mutex_destroy, 1, -1);
    ret = rs_ub_dev_cb_init(&attr, &dev_cb, &rscb, &dev_index, &base_attr);
    EXPECT_INT_NE(0, ret);

    mocker_clean();
    mocker(pthread_mutex_init, 1, 0);
    mocker(rs_ub_create_ctx, 1, 0);
    mocker(rs_ub_get_dev_attr, 1, -1);
    mocker(rs_urma_delete_context, 1, -1);
    mocker(pthread_mutex_destroy, 1, -1);
    ret = rs_ub_dev_cb_init(&attr, &dev_cb, &rscb, &dev_index, &base_attr);
    EXPECT_INT_NE(0, ret);

    mocker_clean();
}

void tc_rs_ub_ctx_init()
{
    struct dev_base_attr base_attr = {0};
    struct ctx_init_attr attr = {0};
    struct rs_cb rs_cb = {0};
    int dev_index = 0;
    int ret = 0;

    mocker(rs_urma_get_device_by_eid, 1, NULL);
    ret = rs_ub_ctx_init(&rs_cb, &attr, &dev_index, &base_attr);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_destroy()
{
    struct rs_ub_dev_cb dev_cb = {0};
    unsigned long long addr = 0;
    int ret = 0;

    mocker(rs_ub_get_jfc_cb, 1, -1);
    ret = rs_ub_ctx_jfc_destroy(&dev_cb, addr);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_ext_jetty_delete()
{
    struct rs_ctx_jetty_cb jetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_jetty_t jetty = {0};
    jetty_cb.jetty = &jetty;

    jetty_cb.dev_cb = &dev_cb;
    mocker(rs_urma_user_ctl, 1, -1);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
    mocker_clean();
}

void tc_rs_ub_ctx_chan_create()
{
    union data_plane_cstm_flag data_plane_flag;
    struct rs_ub_dev_cb dev_cb = {0};
    unsigned long long addr = 0;
    struct rs_cb rs_cb = {0};
    int ret = 0;
    int fd = 0;

    dev_cb.rscb = &rs_cb;
    data_plane_flag.bs.poll_cq_cstm = 1;
    mocker(rs_urma_create_jfce, 1, NULL);
    ret = rs_ub_ctx_chan_create(&dev_cb, data_plane_flag, &addr, &fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    data_plane_flag.bs.poll_cq_cstm = 0;
    ret = rs_ub_ctx_chan_create(&dev_cb, data_plane_flag, &addr, &fd);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_deinit()
{
    struct rs_ub_dev_cb *dev_cb = (struct rs_ub_dev_cb *)calloc(1, sizeof(struct rs_ub_dev_cb));
    struct rs_cb rs_cb = {0};
    int ret = 0;

    dev_cb->rscb = &rs_cb;
    mocker(rs_urma_delete_context, 1, -1);
    mocker(rs_ub_free_seg_cb_list, 1, -1);
    mocker(rs_ub_free_jetty_cb_list, 1, -1);
    mocker(rs_ub_free_jfc_cb_list, 1, -1);
    mocker(rs_ub_free_jfce_cb_list, 1, -1);
    mocker(rs_ub_free_token_id_cb_list, 1, -1);
    RS_INIT_LIST_HEAD(&dev_cb->list);
    ret = rs_ub_ctx_deinit(dev_cb);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_ub_init_seg_cb()
{
    struct mem_reg_attr_t mem_attr = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    struct rs_seg_cb seg_cb = {0};
    int ret = 0;

    mocker(rs_ub_get_token_id_cb, 1, 0);
    mocker(rs_urma_register_seg, 1, NULL);
    ret = rs_ub_init_seg_cb(&mem_attr, &dev_cb, &seg_cb);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_lmem_reg()
{
    struct mem_reg_attr_t mem_attr = {0};
    struct mem_reg_info_t mem_info = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    int ret = 0;

    mem_attr.mem.size = 1;
    mocker(rs_ub_init_seg_cb, 1, -1);
    ret = rs_ub_ctx_lmem_reg(&dev_cb, &mem_attr, &mem_info);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(rs_urma_import_seg, 1, NULL);
    ret = rs_ub_ctx_rmem_import(&dev_cb, &mem_attr, &mem_info);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_fail()
{
    struct rs_ub_dev_cb dev_cb = {0};
    struct ctx_cq_attr attr = {0};
    struct ctx_cq_info info = {0};
    int ret = 0;

    attr.ub.mode = JFC_MODE_STARS_POLL;
    mocker(rs_ub_ctx_jfc_create_ext, 1, -1);
    ret = rs_ub_ctx_jfc_create(&dev_cb, &attr, &info);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_init_jetty_cb()
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    struct rs_ub_dev_cb dev_cb = {0};
    struct ctx_qp_attr attr = {0};
    int ret = 0;

    mocker(pthread_mutex_init, 1, -1);
    ret = rs_ub_ctx_init_jetty_cb(&dev_cb, &attr, &jetty_cb);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jetty_create_fail()
{
    struct rs_ub_dev_cb dev_cb = {0};
    struct qp_create_info info = {0};
    struct ctx_qp_attr attr = {0};
    int ret = 0;

    mocker(rs_ub_ctx_init_jetty_cb, 1, 0);
    mocker(rs_ub_query_jfc_cb, 1, -1);
    mocker(rs_ub_ctx_free_jetty_cb, 1, -1);
    ret = rs_ub_ctx_jetty_create(&dev_cb, &attr, &info);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(rs_ub_ctx_init_jetty_cb, 1, 0);
    mocker(rs_ub_query_jfc_cb, 1, 0);
    mocker(rs_ub_ctx_drv_jetty_create, 1, 0);
    mocker(rs_ub_fill_jetty_info, 1, -1);
    mocker(rs_ub_ctx_drv_jetty_delete, 1, -1);
    mocker(rs_ub_ctx_free_jetty_cb, 1, -1);
    ret = rs_ub_ctx_jetty_create(&dev_cb, &attr, &info);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jetty_import_fail()
{
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_info = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    int ret = 0;

    mocker(rs_ub_ctx_init_rjetty_cb, 1, 0);
    mocker(rs_ub_ctx_drv_jetty_import, 1, -1);
    ret = rs_ub_ctx_jetty_import(&dev_cb, &import_attr, &import_info);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_batch_send_wr_fail()
{
    struct WrlistSendCompleteNum wrlist_num = {0};
    struct wrlist_base_info base_info = {0};
    struct batch_send_wr_data wr_data = {0};
    struct send_wr_resp wr_resp = {0};
    struct rs_cb rs_cb = {0};
    int ret = 0;

    wrlist_num.sendNum = 1;
    mocker(rs_ub_get_dev_cb, 1, -1);
    ret = rs_ub_ctx_batch_send_wr(&rs_cb, &base_info, &wr_data, &wr_resp, &wrlist_num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_get_jetty_cb, 1, -1);
    ret = rs_ub_ctx_batch_send_wr(&rs_cb, &base_info, &wr_data, &wr_resp, &wrlist_num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jetty_destroy_batch()
{
    struct jetty_destroy_batch_info batch_info = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    unsigned int jetty_ids[1] = {0};
    unsigned int num = 0;
    int ret;

    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    num = 1;
    mocker(rs_ub_calloc_jetty_batch_info, 1, -1);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    num = 1;
    mocker(rs_ub_get_jetty_destroy_batch_info, 1, -1);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    num = 1;
    mocker(rs_ub_get_jetty_destroy_batch_info, 1, 0);
    mocker(rs_ub_destroy_jetty_cb_batch, 1, -1);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(rs_ub_get_jetty_cb, 1, -1);
    ret = rs_ub_get_jetty_destroy_batch_info(&dev_cb, jetty_ids, &batch_info, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    jetty_cb_stub.state = RS_JETTY_STATE_INIT;
    mocker_invoke(rs_ub_get_jetty_cb, rs_ub_get_jetty_cb_stub, 1);
    mocker(rs_ub_get_jetty_cb, 1, 0);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    num = 1;
    mocker(rs_ub_get_jetty_destroy_batch_info, 1, 0);
    mocker(rs_ub_destroy_jetty_cb_batch, 1, 0);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    num = 1;
    jetty_cb_stub.state = RS_JETTY_STATE_CREATED;
    pthread_mutex_init(&dev_cb.mutex, NULL);
    RS_INIT_LIST_HEAD(&dev_cb.jetty_list);
    RsListAddTail(&jetty_cb_stub.list, &dev_cb.jetty_list);
    dev_cb.jetty_cnt++;
    mocker_invoke(rs_ub_get_jetty_cb, rs_ub_get_jetty_cb_stub, 1);
    mocker(rs_ub_destroy_jetty_cb_batch, 1, 0);
    mocker(rs_ub_get_jetty_cb, 1, 0);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    num = 1;
    jetty_cb_stub.state = RS_JETTY_STATE_CREATED;
    RsListAddTail(&jetty_cb_stub.list, &dev_cb.jetty_list);
    dev_cb.jetty_cnt++;
    mocker_invoke(rs_ub_get_jetty_cb, rs_ub_get_jetty_cb_stub, 1);
    mocker_invoke(rs_ub_free_jetty_cb_batch, rs_ub_free_jetty_cb_batch_stub, 1);
    mocker(rs_urma_delete_jetty_batch, 1, -1);
    mocker(rs_ub_get_jetty_cb, 1, 0);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    num = 1;
    jetty_cb_stub.state = RS_JETTY_STATE_CREATED;
    RsListAddTail(&jetty_cb_stub.list, &dev_cb.jetty_list);
    dev_cb.jetty_cnt++;
    mocker_invoke(rs_ub_get_jetty_cb, rs_ub_get_jetty_cb_stub, 1);
    mocker_invoke(rs_ub_free_jetty_cb_batch, rs_ub_free_jetty_cb_batch_stub, 1);
    mocker(rs_urma_delete_jetty_batch, 1, 0);
    mocker(rs_urma_delete_jfr_batch, 1, -1);
    mocker(rs_ub_get_jetty_cb, 1, 0);
    ret = rs_ub_ctx_jetty_destroy_batch(&dev_cb, jetty_ids, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    pthread_mutex_destroy(&dev_cb.mutex);
}

void tc_rs_ub_ctx_query_jetty_batch()
{
    struct rs_ub_dev_cb dev_cb;
    unsigned int jetty_ids[] = {1, 2, 3};
    struct jetty_attr attr[3];
    unsigned int num = 3;
    int ret;

    mocker(rs_ub_get_jetty_cb, 1, -1);
    ret = rs_ub_ctx_query_jetty_batch(&dev_cb, jetty_ids, attr, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    num = 3;
    mocker_invoke(rs_ub_get_jetty_cb, rs_ub_get_jetty_cb_stub, 1);
    mocker(rs_urma_query_jetty, 1, -1);
    ret = rs_ub_ctx_query_jetty_batch(&dev_cb, jetty_ids, attr, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    num = 3;
    mocker_invoke(rs_ub_get_jetty_cb, rs_ub_get_jetty_cb_stub, 3);
    ret = rs_ub_ctx_query_jetty_batch(&dev_cb, jetty_ids, attr, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_get_eid_by_ip()
{
    struct RaRsDevInfo dev_info = {0};
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;

    mocker(RsGetRsCb, 1, -1);
    ret = rs_get_eid_by_ip(&dev_info, ip, eid, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, -1);
    ret = rs_get_eid_by_ip(&dev_info, ip, eid, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_get_eid_by_ip, 1, -1);
    ret = rs_get_eid_by_ip(&dev_info, ip, eid, &num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RsGetRsCb, 1, 0);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_get_eid_by_ip, 1, 0);
    ret = rs_get_eid_by_ip(&dev_info, ip, eid, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_ub_get_eid_by_ip()
{
    struct rs_ub_dev_cb dev_cb = {0};
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;
    int i = 0;

    for (i = 0; i < 32; i++) {
        ip[i].family = AF_INET;
    }
    ret = rs_ub_get_eid_by_ip(&dev_cb, ip, eid, &num);
    EXPECT_INT_EQ(0, ret);

    mocker(rs_urma_get_eid_by_ip, 1, -1);
    for (i = 0; i < 32; i++) {
        ip[i].family = AF_INET6;
    }
    ret = rs_ub_get_eid_by_ip(&dev_cb, ip, eid, &num);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_get_aux_info()
{
    struct aux_info_out info_out = {0};
    struct aux_info_in info_in = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    int ret = 0;

    mocker_clean();
    info_in.type = AUX_INFO_IN_TYPE_CQE;
    mocker(rs_urma_user_ctl, 1, 0);
    ret = rs_ub_ctx_get_aux_info(&dev_cb, &info_in, &info_out);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(rs_urma_user_ctl, 1, -1);
    ret = rs_ub_ctx_get_aux_info(&dev_cb, &info_in, &info_out);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();

    info_in.type = AUX_INFO_IN_TYPE_AE;
    mocker(rs_urma_user_ctl, 1, 0);
    ret = rs_ub_ctx_get_aux_info(&dev_cb, &info_in, &info_out);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(rs_urma_user_ctl, 1, -1);
    ret = rs_ub_ctx_get_aux_info(&dev_cb, &info_in, &info_out);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();

    info_in.type = AUX_INFO_IN_TYPE_MAX;
    ret = rs_ub_ctx_get_aux_info(&dev_cb, &info_in, &info_out);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();
}

void tc_rs_ub_get_tp_attr()
{
    struct rs_ub_dev_cb dev_cb = {0};
    unsigned int attr_bitmap = 0b101010;
    uint64_t tp_handle = 12345;
    struct tp_attr attr = {0};
    int ret;

    mocker(rs_urma_get_tp_attr, 1, -1);
    ret = rs_ub_get_tp_attr(&dev_cb, &attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    ret = rs_ub_get_tp_attr(&dev_cb, &attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_ub_set_tp_attr()
{
    struct rs_ub_dev_cb dev_cb;
    unsigned int attr_bitmap = 0b101010;
    uint64_t tp_handle = 12345;
    struct tp_attr attr;
    int ret;

    mocker(rs_urma_set_tp_attr, 1, -1);
    ret = rs_ub_set_tp_attr(&dev_cb, attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    ret = rs_ub_set_tp_attr(&dev_cb, attr_bitmap, tp_handle, &attr);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_epoll_event_jfc_in_handle()
{
    struct rs_ctx_jfce_cb jfce_cb1 = {0};
    struct rs_ctx_jfce_cb jfce_cb2 = {0};
    struct rs_ub_dev_cb dev_cb1 = {0};
    struct rs_ub_dev_cb dev_cb2 = {0};
    struct rs_cb rs_cb = {0};
    urma_jfce_t jfce1 = {0};
    urma_jfce_t jfce2 = {0};
    int ret = 0;

    RS_INIT_LIST_HEAD(&rs_cb.rdevList);
    ret = rs_epoll_event_jfc_in_handle(&rs_cb, -ENODEV);
    EXPECT_INT_EQ(-ENODEV, ret);

    RsListAddTail(&dev_cb1.list, &rs_cb.rdevList);
    RsListAddTail(&dev_cb2.list, &rs_cb.rdevList);
    RS_INIT_LIST_HEAD(&dev_cb1.jfce_list);
    RS_INIT_LIST_HEAD(&dev_cb2.jfce_list);
    RsListAddTail(&jfce_cb1.list, &dev_cb2.jfce_list);
    RsListAddTail(&jfce_cb2.list, &dev_cb2.jfce_list);

    jfce1.fd = 1;
    jfce2.fd = 2;
    jfce_cb1.jfce_addr = (uint64_t)(uintptr_t)(&jfce1);
    jfce_cb2.jfce_addr = (uint64_t)(uintptr_t)(&jfce2);
    mocker_clean();
    mocker(rs_handle_epoll_poll_jfc, 1, 0);
    ret = rs_epoll_event_jfc_in_handle(&rs_cb, 2);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_jfc_callback_process()
{
    struct rs_ctx_jetty_cb jetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    struct rs_cb rs_cb = {0};
    urma_jetty_t jetty = {0};
    urma_jfc_t jfc = {0};
    urma_cr_t cr = {0};

    dev_cb.rscb = &rs_cb;
    jetty_cb.dev_cb = &dev_cb;
    jetty_cb.jetty = &jetty;

    cr.status = URMA_CR_RNR_RETRY_CNT_EXC_ERR;
    rs_jfc_callback_process(&jetty_cb, &cr, &jfc);
}
