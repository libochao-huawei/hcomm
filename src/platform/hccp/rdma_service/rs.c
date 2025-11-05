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
#include "rs.h"
#include "ra_rs_err.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include "securec.h"
#include "rs_inner.h"
#include "rs_drv_rdma.h"
#include "rs_epoll.h"
#include "rs_tls.h"
#include "ssl_adp.h"
#include "rs_socket.h"
#include "dl_ibverbs_function.h"
#include "dl_hal_function.h"
#include "rs_drv_rdma.h"
#ifdef CONFIG_TLV
#include "hccp_tlv.h"
#include "rs_tlv.h"
#endif

__thread struct rs_cb *g_rs_cb = NULL;  //lint !e17
struct rs_cb *g_rs_cb_list[RS_MAX_DEV_NUM] = {0};  //lint !e17
int g_init_counter[RS_MAX_DEV_NUM] = {0};

/* set current phy_id g_rs_cb */
void rs_set_ctx(unsigned int phy_id)
{
    g_rs_cb = g_rs_cb_list[phy_id];
    hccp_dbg("[rs_set_ctx], phy_id[%u], g_rs_cb[%p]", phy_id, g_rs_cb);
}

/* get current g_rs_cb */
static struct rs_cb *rs_get_cur_rs_cb(void)
{
    for (int i = 0; i < RS_MAX_DEV_NUM; i++) {
        if (g_rs_cb_list[i] != NULL) {
            hccp_info("[rs_get_cur_rs_cb], phy_id[%u], rs_cb[%p]", i, g_rs_cb_list[i]);
            return g_rs_cb_list[i];
        }
    }
    return NULL;
}

struct opcode_interface_info g_interface_info_list[] = {
    // outer opcode version: 1.0
    {RA_RS_SOCKET_CONN, 2},
    {RA_RS_SOCKET_CLOSE, 2},
    {RA_RS_SOCKET_ABORT, 1},
    {RA_RS_SOCKET_LISTEN_START, 2},
    {RA_RS_SOCKET_LISTEN_STOP, 2},
    {RA_RS_GET_SOCKET, 3},
    {RA_RS_SOCKET_SEND, 1},
    {RA_RS_SOCKET_RECV, 1},
    {RA_RS_QP_CREATE, 2},
    {RA_RS_QP_CREATE_WITH_ATTRS, 1},
    {RA_RS_AI_QP_CREATE, 3},
    {RA_RS_AI_QP_CREATE_WITH_ATTRS, 1},
    {RA_RS_TYPICAL_QP_CREATE, 1},
    {RA_RS_QP_DESTROY, 1},
    {RA_RS_QP_CONNECT, 2},
    {RA_RS_TYPICAL_QP_MODIFY, 2},
    {RA_RS_QP_BATCH_MODIFY, 2},
    {RA_RS_QP_STATUS, 1},
    {RA_RS_QP_INFO, 1},
    {RA_RS_MR_REG, 2},
    {RA_RS_MR_DEREG, 1},
    {RA_RS_TYPICAL_MR_REG, 2},
    {RA_RS_REMAP_MR, 1},
    {RA_RS_TYPICAL_MR_DEREG, 1},
    {RA_RS_SEND_WR, 1},
    {RA_RS_GET_NOTIFY_BA, 2},
    {RA_RS_INIT, 2},
    {RA_RS_DEINIT, 1},
    {RA_RS_SOCKET_INIT, 1},
    {RA_RS_SOCKET_DEINIT, 1},
    {RA_RS_RDEV_INIT, 2},
    {RA_RS_RDEV_INIT_WITH_BACKUP, 1},
    {RA_RS_RDEV_GET_PORT_STATUS, 1},
    {RA_RS_RDEV_DEINIT, 1},
    {RA_RS_WLIST_ADD, 1},
    {RA_RS_WLIST_ADD_V2, 1},
    {RA_RS_WLIST_DEL, 1},
    {RA_RS_WLIST_DEL_V2, 1},
    {RA_RS_ACCEPT_CREDIT_ADD, 1},
    {RA_RS_GET_IFADDRS, 2},
    {RA_RS_GET_IFADDRS_V2, 3},
    {RA_RS_GET_INTERFACE_VERSION, 1},
    {RA_RS_SEND_WRLIST, 1},
    {RA_RS_SEND_WRLIST_V2, 1},
    {RA_RS_SEND_WRLIST_EXT, 1},
    {RA_RS_SEND_WRLIST_EXT_V2, 1},
    {RA_RS_SEND_NORMAL_WRLIST, 1},
    {RA_RS_SET_TSQP_DEPTH, 1},
    {RA_RS_GET_TSQP_DEPTH, 1},
    {RA_RS_SET_QP_ATTR_QOS, 1},
    {RA_RS_SET_QP_ATTR_TIMEOUT, 1},
    {RA_RS_SET_QP_ATTR_RETRY_CNT, 1},
    {RA_RS_GET_CQE_ERR_INFO, 1},
    {RA_RS_GET_LITE_SUPPORT, 2},
    {RA_RS_GET_LITE_RDEV_CAP, 1},
    {RA_RS_GET_LITE_QP_CQ_ATTR, 1},
    {RA_RS_GET_LITE_CONNECTED_INFO, 1},
    {RA_RS_GET_LITE_MEM_ATTR, 1},
    {RA_RS_PING_INIT, 1},
    {RA_RS_PING_ADD, 1},
    {RA_RS_PING_START, 1},
    {RA_RS_PING_GET_RESULTS, 1},
    {RA_RS_PING_STOP, 1},
    {RA_RS_PING_DEL, 1},
    {RA_RS_PING_DEINIT, 1},
    {RA_RS_GET_CQE_ERR_INFO_NUM, 1},
    {RA_RS_GET_CQE_ERR_INFO_LIST, 1},
    {RA_RS_GET_VNIC_IP_INFOS_V1, 1},
    {RA_RS_GET_VNIC_IP_INFOS, 1},
#ifdef CONFIG_TLV
    {RA_RS_TLV_INIT, 1},
    {RA_RS_TLV_DEINIT, 1},
    {RA_RS_TLV_REQUEST, 1},
#endif
    {RA_RS_GET_TLS_ENABLE, 1},
    {RA_RS_GET_SEC_RANDOM, 1},
    {RA_RS_GET_ROCE_API_VERSION, 0},

    // inner opcode version
    {RA_RS_HDC_SESSION_CLOSE, 1},
    {RA_RS_GET_VNIC_IP, 1},
    {RA_RS_NOTIFY_CFG_SET, 1},
    {RA_RS_NOTIFY_CFG_GET, 1},
    {RA_RS_SET_PID, 1},
    {RA_RS_ASYNC_HDC_SESSION_CONNECT, 1},
    {RA_RS_ASYNC_HDC_SESSION_CLOSE, 1},
};

RS_ATTRI_VISI_DEF void rs_get_cur_time(struct timeval *time)
{
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_VOID(time);
    ret = gettimeofday(time, NULL);
    if (ret) {
        hccp_warn("gettimeofday unsuccessful, ret[%d] expect 0", ret);
        ret = memset_s(time, sizeof(struct timeval), 0, sizeof(struct timeval));
        if (ret) {
            hccp_warn("memset_s unsuccessful, ret[%d] expect 0", ret);
        }
    }

    return;
}

RS_ATTRI_VISI_DEF void hccp_time_interval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(end_time);
    RS_CHECK_POINTER_NULL_RETURN_VOID(start_time);
    RS_CHECK_POINTER_NULL_RETURN_VOID(msec);

    /* if low position is sufficient, then borrow one from the high position */
    if (end_time->tv_usec < start_time->tv_usec) {
        end_time->tv_sec -= 1;
        end_time->tv_usec += MS_PER_SECOND_I * MS_PER_SECOND_I;
    }

    *msec = (float)((end_time->tv_sec - start_time->tv_sec) * MS_PER_SECOND_F +
            (end_time->tv_usec - start_time->tv_usec) / US_PER_MS_F);

    return;
}

RS_ATTRI_VISI_DEF void rs_heartbeat_alive_print(struct rs_pthread_info *pthread_info)
{
    float time_cost = 0.0;
    struct timeval now;

    if (pthread_info == NULL) {
        hccp_err("pthread_info is NULL!");
        return;
    }

    rs_get_cur_time(&now);
    hccp_time_interval(&now, &pthread_info->last_check_time, &time_cost);
    if (time_cost >= RS_HEARTBEAT_TIME || time_cost <= 0) {
        hccp_info("pthread[%s] is alive!", pthread_info->pthread_name);
        rs_get_cur_time(&pthread_info->last_check_time);
    }

    return;
}

int rs_dev2rscb(uint32_t chip_id, struct rs_cb **rs_cb, bool init_flag)
{
    if (g_rs_cb == NULL) {
        if (init_flag == false) {
            hccp_warn("No device initialized !");
        }
        return -ENODEV;
    }

    if (chip_id == g_rs_cb->chip_id) {
        *rs_cb = g_rs_cb;
        return 0;
    }

    hccp_warn("get rs cb unsuccessful for dev %u !", chip_id);
    *rs_cb = NULL;

    return -ENODEV;
}

int rs_get_hccp_mode(unsigned int chip_id)
{
    struct rs_cb *rs_cb = NULL;
    int ret;

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed(%d)", ret), ret);
    return (int)rs_cb->hccp_mode;
}

int rs_dev2conncb(uint32_t chip_id, struct rs_conn_cb **conn_cb)
{
    int ret;
    struct rs_cb *rs_cb = NULL;

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed(%d)", ret), ret);

    *conn_cb = &(rs_cb->conn_cb);

    return 0;
}

int rs_get_rdev_cb(struct rs_cb *rs_cb, unsigned int rdev_index, struct rs_rdev_cb **rdev_cb)
{
    struct rs_rdev_cb *rdev_cb_tmp = NULL;
    struct rs_rdev_cb *rdev_cb_tmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(rdev_cb_tmp, rdev_cb_tmp2, &rs_cb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdev_cb_tmp->list) != &rs_cb->rdev_list;
        rdev_cb_tmp = rdev_cb_tmp2, rdev_cb_tmp2 = list_entry(rdev_cb_tmp2->list.next, struct rs_rdev_cb, list)) {
        if (rdev_cb_tmp->rdev_index == rdev_index) {
            *rdev_cb = rdev_cb_tmp;
            return 0;
        }
    }

    *rdev_cb = NULL;
    hccp_err("rdev_cb for rdev_index[%u] do not available!", rdev_index);

    return -ENODEV;
}

int rs_rdev2rdev_cb(unsigned int chip_id, unsigned int rdev_index, struct rs_rdev_cb **rdev_cb)
{
    int ret;
    struct rs_cb *rs_cb = NULL;

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb fail for chip_id:%u, ret:%d", chip_id, ret), -ENODEV);

    ret = rs_get_rdev_cb(rs_cb, rdev_index, rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb fail!, ret %d, rdev_index %u", ret, rdev_index), ret);

    return 0;
}

int rs_fd2conn(int fd, struct rs_conn_info **conn)
{
    struct rs_cb *rs_cb = NULL;
    struct rs_conn_info *conn_tmp = NULL;
    struct rs_conn_info *conn_tmp2 = NULL;
    struct rs_list_head *head = NULL;

    if (g_rs_cb != NULL) {
        rs_cb = g_rs_cb;
    } else {
        hccp_err("g_rs_cb is NULL");
        return -ENODEV;
    }

    RS_PTHREAD_MUTEX_LOCK(&rs_cb->conn_cb.conn_mutex);
    head = &rs_cb->conn_cb.server_conn_list;
    RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, head, list, struct rs_conn_info);
    for (; &conn_tmp->list != head;
        conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list)) {
        if (conn_tmp->connfd == fd) {
            *conn = conn_tmp;
            RS_PTHREAD_MUTEX_ULOCK(&rs_cb->conn_cb.conn_mutex);
            return 0;
        }
    }

    head = &rs_cb->conn_cb.client_conn_list;
    RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, head, list, struct rs_conn_info);
    for (; &conn_tmp->list != head;
        conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list)) {
        if (conn_tmp->connfd == fd) {
            *conn = conn_tmp;
            RS_PTHREAD_MUTEX_ULOCK(&rs_cb->conn_cb.conn_mutex);
            return 0;
        }
    }

    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->conn_cb.conn_mutex);

    hccp_warn("cannot find conn node for fd:%d!", fd);
    *conn = NULL;

    return -ENODEV;
}

STATIC int rs_pthread_mutex_init(struct rs_cb *rscb, struct rs_init_config *cfg)
{
    int ret;
    int err;

    RS_CHECK_POINTER_NULL_RETURN_INT(cfg);
    RS_CHECK_POINTER_NULL_RETURN_INT(rscb);
    rscb->chip_id = cfg->chip_id;
    rscb->hccp_mode = cfg->hccp_mode;
    rscb->conn_cb.rscb = rscb;

    ret = pthread_mutex_init(&rscb->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("rscb mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);
    ret = pthread_mutex_init(&rscb->conn_cb.conn_mutex, NULL);
    if (ret) {
        hccp_err("conn_cb mutex_init failed ret %d, normal ret 0!", ret);
        err = pthread_mutex_destroy(&rscb->mutex);
        hccp_dbg("pthread destroy ret %d", err);
        return -ESYSFUNC;
    }

    hccp_info("mutex init ok");

    RS_INIT_LIST_HEAD(&rscb->conn_cb.listen_list);
    RS_INIT_LIST_HEAD(&rscb->conn_cb.server_accept_list);
    RS_INIT_LIST_HEAD(&rscb->conn_cb.client_conn_list);
    RS_INIT_LIST_HEAD(&rscb->conn_cb.server_conn_list);
    RS_INIT_LIST_HEAD(&rscb->conn_cb.white_list);
    RS_INIT_LIST_HEAD(&rscb->rdev_list);
    RS_INIT_LIST_HEAD(&rscb->heterog_tcp_fd_list);
    rscb->conn_cb.wlist_enable = cfg->white_list_status;
    return 0;
}

STATIC int rs_get_chip_logic_id(unsigned int chip_id, enum network_mode hccp_mode, unsigned int *logic_id)
{
    int ret = 0;

    // other modes skip
    if (hccp_mode != NETWORK_OFFLINE) {
        return 0;
    }

    ret = dl_drv_device_get_index_by_phy_id(chip_id, logic_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("hal get logic_id failed, chip_id[%u], ret[%d]", chip_id, ret), -ENODEV);

    return 0;
}

STATIC int rs_init_rscb_cfg(struct rs_cb *rscb)
{
    struct timeval start, end;
    float time_cost = 0.0;
    int ret;

    ret = rs_get_chip_logic_id(rscb->chip_id, rscb->hccp_mode, &rscb->logic_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_chip_logic_id failed, ret[%d]", ret), ret);

#ifdef CONFIG_SSL
    ret = rs_ssl_init(rscb);
    if (ret != 0) {
        hccp_err("init ssl failed, ret[%d]", ret);
        goto ssl_init_err;
    }
#endif

    rs_get_cur_time(&start);
    ret = rs_epoll_connect_handle_init(rscb);
    if (ret != 0) {
        hccp_err("create pthread failed, ret[%d]", ret);
        goto create_pthread_err;
    }

    rs_get_cur_time(&end);
    hccp_time_interval(&end, &start, &time_cost);
    hccp_info("rs_epoll_connect_handle_init ok cost [%f] ms", time_cost);
    return 0;

create_pthread_err:
#ifdef CONFIG_SSL
    rs_ssl_deinit(rscb);
ssl_init_err:
#endif
    return ret;
}

STATIC void rs_deinit_rscb_cfg(struct rs_cb *rscb)
{
    int try_again = RS_TRY_TIME;
    eventfd_t event = 1;
    int ret;

#ifdef CONFIG_SSL
    rs_ssl_deinit(rscb);
#endif

    // deinit resources in rs_epoll_connect_handle_init
    // deinit epoll thread, send event to eventfd to waking up epoll handle thread
    ret = (int)write(rscb->conn_cb.eventfd, &event, sizeof(eventfd_t));
    if (ret != sizeof(eventfd_t)) {
        hccp_warn("eventfd_write unsuccessful(0x%x), chip_id:%u, errno:%d", ret, rscb->chip_id, errno);
    }
    while (((rscb->state & RS_STATE_HALT) == 0) && (try_again != 0)) {
        usleep(RS_USLEEP_TIME);
        try_again--;
    };
    if (try_again == 0) {
        hccp_warn("try_again exhausted, epoll thread quit unsuccessful, rscb state:%u", rscb->state);
    }
    rscb->state &= ~RS_STATE_HALT;

    // deinit connect thread, already been RS_CONN_EXIT_FLAG, no need to change conn_flag
    if (rscb->conn_flag != RS_CONN_EXIT_FLAG) {
        rscb->conn_flag = 0;
    }
    try_again = RS_TRY_TIME;
    while ((rscb->conn_flag != RS_CONN_EXIT_FLAG) && (try_again != 0)) {
        usleep(RS_USLEEP_TIME);
        try_again--;
    }
    if (try_again == 0) {
        hccp_warn("try_again exhausted, connect thread quit unsuccessful, rscb conn_flag:%d", rscb->conn_flag);
    }

    rs_destroy_epoll(rscb);
}

RS_ATTRI_VISI_DEF int rs_init(struct rs_init_config *cfg)
{
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(cfg);
    ret = dl_hal_init();
    if (ret != 0) {
        hccp_err("[init][rs_init]dl_hal_init failed, ret = %d", ret);
        return ret;
    }

    int counter = __sync_fetch_and_add(&(g_init_counter[cfg->chip_id]), 1);
    if (counter > 0) {
        hccp_warn("rs has been init for device %u!", cfg->chip_id);
        return 0;
    }
    ret = rs_dev2rscb(cfg->chip_id, &rscb, true);
    CHK_PRT_RETURN(ret == 0, hccp_err("rs_cb exist for device %u! do NOT init it again!", cfg->chip_id), -EEXIST);

    rscb = calloc(1, sizeof(struct rs_cb));
    CHK_PRT_RETURN(rscb == NULL, hccp_err("calloc rscb failed"), -ENOMEM);

    ret = rs_pthread_mutex_init(rscb, cfg);
    if (ret != 0) {
        hccp_err("Init mutex failed, ret[%d]", ret);
        goto pthread_mutex_err;
    }

    ret = rs_init_rscb_cfg(rscb);
    if (ret != 0) {
        hccp_err("rs init rscb configure fail,ret:%d", ret);
        pthread_mutex_destroy(&rscb->mutex);
        pthread_mutex_destroy(&rscb->conn_cb.conn_mutex);
        goto pthread_mutex_err;
    }

    rscb->fd_map = calloc(1, sizeof(void*) * RS_MAX_FD_NUM);
    if (rscb->fd_map == NULL) {
        hccp_err("no memory for fd_map");
        ret = -ENOMEM;
        goto fd_map_err;
    }

    ret = getifaddrs(&rscb->ifaddr_list);
    if (ret != 0) {
        hccp_err("getifaddrs failed, ret:%d", ret);
        goto getifaddrs_err;
    }

    g_rs_cb_list[cfg->chip_id] = g_rs_cb;

    hccp_run_info("rs init success, chip_id[%u]", cfg->chip_id);
    return 0;

getifaddrs_err:
    free(rscb->fd_map);
    rscb->fd_map = NULL;

fd_map_err:
    pthread_mutex_destroy(&rscb->mutex);
    pthread_mutex_destroy(&rscb->conn_cb.conn_mutex);
    rs_deinit_rscb_cfg(rscb);

pthread_mutex_err:
    free(rscb);
    rscb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_tls_enable(unsigned int phy_id, bool *tls_enable)
{
    struct rs_cb *rs_cb = NULL;
    int ret;

    CHK_PRT_RETURN(tls_enable == NULL, hccp_err("param err, tls_enable is NULL"), -EINVAL);
    ret = rs_get_rs_cb(phy_id, &rs_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phy_id(%u) invalid, ret(%d)", phy_id, ret), ret);

    *tls_enable = (rs_cb->ssl_enable == 0) ? false : true;
    return 0;
}

RS_ATTRI_VISI_DEF int rs_bind_hostpid(unsigned int chip_id, pid_t pid)
{
#define QUERY_BIND_HOST_PID_TIME_US 10000
#define QUERY_BIND_HOST_PID_CNT 12000
    struct rs_cb *rs_cb = NULL;
    unsigned int host_pid;
    pid_t dev_pid;
    int ret;
    int i;

    // get current hccp pid on device
    dev_pid = getpid();
    CHK_PRT_RETURN(dev_pid < 0, hccp_err("getpid failed, ret:%d errno:%d", dev_pid, errno), -EINVAL);

    // query corresponding host_pid every 10ms, total timeout cost 120s
    for (i = 0; i < QUERY_BIND_HOST_PID_CNT; i++) {
        ret = dl_drv_query_process_host_pid(dev_pid, NULL, NULL, &host_pid, NULL);
        if (ret == DRV_ERROR_NONE) {
            break;
        }

        usleep(QUERY_BIND_HOST_PID_TIME_US);
    }

    if (i >= QUERY_BIND_HOST_PID_CNT) {
        hccp_err("query process host_pid failed, i:%d >= %d ret:%d", i, QUERY_BIND_HOST_PID_CNT, ret);
        return -EINVAL;
    }

    if (pid != (pid_t)host_pid) {
        hccp_err("check process failed, pid from tsd: %d, process host_pid: %u", pid, host_pid);
        return -EINVAL;
    }

    hccp_dbg("dl_drv_query_process_host_pid success, total retry cnt:%d", i);

    // save host_pid for later setup sharemem
    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb fail, ret:%d, chip_id:%u", ret, chip_id), -ENODEV);
    rs_cb->host_pid = pid;

    return 0;
}

#ifdef CUSTOM_INTERFACE
STATIC int rs_set_rscb_grp_id(struct rs_cb *rs_cb, unsigned int dev_id)
{
    GrpQueryGroupIdInfo grp_query_out = {0};
    unsigned int chip_id = rs_cb->chip_id;
    GrpQueryGroupId grp_query_in = {0};
    struct MemInfo mem_info = {0};
    unsigned int out_len;
    unsigned int grp_id;
    int ret;

    // query grp_name
    ret = dl_hal_mem_get_info_ex(dev_id, MEM_INFO_TYPE_SVM_GRP_INFO, &mem_info);
    CHK_PRT_RETURN(ret, hccp_err("dl_hal_mem_get_info_ex failed, ret:%d chip_id:%u dev_id:%u", ret, chip_id,
        dev_id), ret);

    hccp_dbg("query group name success, chip_id:%u dev_id:%u grp_name:%s", chip_id, dev_id, mem_info.grp_info.name);

    // query grp_id
    ret = memcpy_s(&grp_query_in.grpName, BUFF_GRP_NAME_LEN, &mem_info.grp_info.name, SVM_GRP_NAME_LEN);
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s failed, ret:%d chip_id:%u dev_id:%u", ret, chip_id, dev_id), ret);
    out_len = (unsigned int)sizeof(grp_query_out);
    ret = dl_hal_grp_query(GRP_QUERY_GROUP_ID, &grp_query_in, sizeof(grp_query_in), &grp_query_out,
        &out_len);
    CHK_PRT_RETURN(ret, hccp_err("dl_hal_grp_query failed, ret:%d chip_id:%u dev_id:%u", ret, chip_id, dev_id), ret);
    grp_id = (unsigned int)grp_query_out.groupId;

    // set grp_id
    rs_cb->grp_id = grp_id;

    hccp_dbg("query group id success, chip_id:%u dev_id:%u grp_id:%u grp_name:%s", chip_id, dev_id, grp_id,
        grp_query_in.grpName);
    return 0;
}

STATIC int rs_bind_sibling(struct rs_cb *rs_cb, int host_pid, unsigned int vf_id, unsigned int dev_id)
{
#define QUERY_BIND_SIBLING_TIME_US 10000
#define QUERY_BIND_SIBLING_CNT 12000
    struct halQueryDevpidInfo pid_info = {0};
    pid_t aicpu_pid;
    int ret;
    int i;

    // query aicpu pid
    pid_info.hostpid = host_pid;
    pid_info.devid = dev_id;
    pid_info.proc_type = DEVDRV_PROCESS_CP1;
    ret = dl_hal_query_dev_pid(pid_info, &aicpu_pid);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_query_dev_pid failed, ret:%d dev_id:%u", ret, dev_id), ret);

    // try to bind sibling every 10ms, total timeout cost 120s
    for (i = 0; i < QUERY_BIND_SIBLING_CNT; i++) {
        ret = dl_hal_mem_bind_sibling(host_pid, aicpu_pid, vf_id, dev_id, SVM_MEM_BIND_SP_GRP);
        if (ret == DRV_ERROR_NONE) {
            break;
        }

        usleep(QUERY_BIND_SIBLING_TIME_US);
    }

    if (i >= QUERY_BIND_SIBLING_CNT) {
        hccp_err("bind sibling to setup sharemem failed, i:%d >= %d ret:%d", i, QUERY_BIND_SIBLING_CNT, ret);
        return -EINVAL;
    }

    rs_cb->aicpu_pid = aicpu_pid;
    hccp_dbg("dl_hal_mem_bind_sibling success, total retry cnt:%d", i);

    return 0;
}

int rs_setup_sharemem(struct rs_cb *rs_cb, bool backup_flag, unsigned int backup_phyid)
{
    unsigned int chip_id = rs_cb->chip_id;
    pid_t pid = rs_cb->host_pid;
    int64_t device_info = 0;
    unsigned int logic_id;
    int ret;

    // setup sharemem or skipped already, no need to setup again
    if (rs_cb->grp_setup_flag) {
        hccp_dbg("grp_setup_flag:%d grp_id:%u chip_id:%u", rs_cb->grp_setup_flag, rs_cb->grp_id, chip_id);
        return 0;
    }

    ret = dl_drv_device_get_index_by_phy_id(chip_id, &logic_id);
    CHK_PRT_RETURN(ret, hccp_err("dl_drv_device_get_index_by_phy_id failed, ret:%d chip_id:%u", ret, chip_id), ret);
    ret = dl_hal_get_device_info(logic_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &device_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_get_device_info failed, ret:%d logic_id:%u chip_id:%u",
        ret, logic_id, chip_id), ret);
    // not 910b/910_93 and not protocol udma, skip to setup share mem
    if (dl_hal_plat_get_chip((uint64_t)device_info) != CHIP_TYPE_910B_910_93 && rs_cb->protocol != PROTOCOL_UDMA) {
        hccp_info("logic_id:%u chip_id:%u protocol:%d skip to setup share mem", logic_id, chip_id, rs_cb->protocol);
        rs_cb->grp_setup_flag = true;
        return 0;
    }

    // use backup info to setup share mem
    if (backup_flag) {
        ret = rsGetLocalDevIDByHostDevID(backup_phyid, &logic_id);
        CHK_PRT_RETURN(ret != 0, hccp_err("rsGetLocalDevIDByHostDevID failed, phy_id(%u), ret(%d)",
            backup_phyid, ret), ret);
        hccp_dbg("setup sharemem with backup, phy_id:%u logic_id:%u", backup_phyid, logic_id);
    }

    // bind sibling, default vfid is 0; query & save grp_id on rs_cb
    ret = rs_bind_sibling(rs_cb, pid, 0, logic_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_bind_sibling failed, ret:%d logic_id:%u chip_id:%u",
        ret, logic_id, chip_id), ret);

    // query & save grp_id on rs_cb
    ret = rs_set_rscb_grp_id(rs_cb, logic_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_set_rscb_grp_id fail, ret:%d logic_id:%u chip_id:%u", ret, logic_id, chip_id),
        ret);

    rs_cb->grp_setup_flag = true;
    return 0;
}
#endif

STATIC int rs_compare_ip_gid(struct rdev rdev_info, union ibv_gid *gid)
{
    return rs_drv_compare_ip_gid(rdev_info.family, rdev_info.local_ip, gid);
}

int rs_query_gid(struct rdev rdev_info, struct ibv_context *ib_ctx_tmp, uint8_t ib_port, int *gid_idx)
{
    static const char *portStates[] = {"Nop", "Down", "Init", "Armed", "", "Active Defer"};
    struct ibv_port_attr attr = {0};
    enum ibv_gid_type_sysfs type;
    union ibv_gid gid_tmp;
    int ret;
    int i;

    CHK_PRT_RETURN(gid_idx == NULL, hccp_err("gid_idx is NULL"), -EINVAL);

    ret = rs_ibv_query_port(ib_ctx_tmp, ib_port, &attr);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_port failed, ret %d ib_port %u", ret, ib_port), -EOPENSRC);

    for (i = 0; i < attr.gid_tbl_len; i++) {
        ret = rs_ibv_query_gid_type(ib_ctx_tmp, ib_port, (unsigned int)i, &type);
        CHK_PRT_RETURN(ret, hccp_err("query gid type failed i %d, ret %d", i, ret), -EOPENSRC);
        if (type != IBV_GID_TYPE_SYSFS_ROCE_V2) {
            continue;
        }
        ret = rs_ibv_query_gid(ib_ctx_tmp, ib_port, i, &gid_tmp);
        CHK_PRT_RETURN(ret, hccp_err("query gid failed i %d, ret %d", i, ret), -EOPENSRC);
        ret = rs_compare_ip_gid(rdev_info, &gid_tmp);
        if (ret == 0) {
            CHK_PRT_RETURN(attr.state != IBV_PORT_ACTIVE, hccp_err("port number %u state is %s",
                ib_port, portStates[attr.state]), -ENOLINK);
            *gid_idx = i;
            return 0;
        }
    }

    if (i == attr.gid_tbl_len) {
        return -EEXIST;
    }
    return 0;
}

STATIC int rs_get_dev_rdev_index(struct rs_rdev_cb *rdev_cb, unsigned int *rdev_index, int index)
{
#ifdef CUSTOM_INTERFACE
    struct roce_dev_data rdev_data = {0};  //lint !e565
    int ret_val;
    RS_PTHREAD_MUTEX_LOCK(&rdev_cb->rs_cb->mutex);
    /*lint -e132*/
    rdev_cb->dev_name = rs_ibv_get_device_name(rdev_cb->dev_list[index]);  //lint !e101
    ret_val = rs_roce_get_roce_dev_data(rdev_cb->dev_name, &rdev_data); //lint !e101
    /*lint +e132*/
    if (ret_val) {
        hccp_err("rs_roce_get_roce_dev_data failed, ret_val:%d, dev_name:%s", ret_val, rdev_cb->dev_name);
        RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rs_cb->mutex);
        return ret_val;
    }
    *rdev_index = rdev_data.rdev_index; // rdev_index is same to port_id
    rdev_cb->rdev_index = *rdev_index;
    RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rs_cb->mutex);
#endif
    return 0;
}

STATIC int rs_get_host_rdev_index(struct rdev rdev_info, struct rs_rdev_cb *rdev_cb, unsigned int *rdev_index, int index)
{
    struct rs_rdev_cb *rdev_cb_tmp2 = NULL;
    struct rs_rdev_cb *rdev_cb_tmp = NULL;
    unsigned int tmp_rdev_index = 0;

    RS_PTHREAD_MUTEX_LOCK(&rdev_cb->rs_cb->mutex);
    rdev_cb->dev_name = rs_ibv_get_device_name(rdev_cb->dev_list[index]);
    if (rdev_cb->dev_name == NULL) {
        hccp_err("rs_ibv_get_device_name failed, errno:%d", errno);
        RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rs_cb->mutex);
        return -EINVAL;
    }

    struct rs_ip_addr_info local_ip;
    int ret = rs_convert_ip_addr(rdev_info.family, &rdev_info.local_ip, &local_ip);
    if (ret != 0) {
        hccp_err("convert(ntop) ip fail, ret:%d", ret);
        RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rs_cb->mutex);
        return ret;
    }

    RS_LIST_GET_HEAD_ENTRY(rdev_cb_tmp, rdev_cb_tmp2, &rdev_cb->rs_cb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdev_cb_tmp->list) != &rdev_cb->rs_cb->rdev_list;
        rdev_cb_tmp = rdev_cb_tmp2, rdev_cb_tmp2 = list_entry(rdev_cb_tmp2->list.next, struct rs_rdev_cb, list)) {
        tmp_rdev_index = rdev_cb_tmp->rdev_index;
        if (!rs_compare_ip_addr(&rdev_cb_tmp->local_ip, &local_ip)) {
            *rdev_index = tmp_rdev_index;
            rdev_cb->rdev_index = *rdev_index;
            rdev_cb->local_ip = local_ip;
            RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rs_cb->mutex);
            return 0;
        }
    }

    *rdev_index = tmp_rdev_index + 1;
    rdev_cb->rdev_index = *rdev_index;
    rdev_cb->local_ip = local_ip;
    RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rs_cb->mutex);
    return 0;
}

STATIC int rs_get_ib_ctx_and_rdev_index(struct rdev rdev_info, struct rs_rdev_cb *rdev_cb, unsigned int *rdev_index)
{
    struct ibv_context *ib_ctx_tmp = NULL;
    int gid_index = -1;
    int ret;
    int i;

    for (i = 0; (i < rdev_cb->dev_num) && (rdev_cb->dev_list[i] != NULL); ++i) {  //lint !e101
        ib_ctx_tmp = rs_ibv_open_device(rdev_cb->dev_list[i]);
        CHK_PRT_RETURN(ib_ctx_tmp == NULL, hccp_err("ibv_open_device failed !"), -ENODEV);
        ret = rs_query_gid(rdev_info, ib_ctx_tmp, rdev_cb->ib_port, &gid_index);
        if (ret == 0) {
            if (rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
                ret = rs_get_host_rdev_index(rdev_info, rdev_cb, rdev_index, i);
            } else {
                ret = rs_get_dev_rdev_index(rdev_cb, rdev_index, i);
            }
            if (ret) {
                hccp_err("get index failed, ret:%d", ret);
                rs_ibv_close_device(ib_ctx_tmp);
                return ret;
            }
            rdev_cb->ib_ctx = ib_ctx_tmp;
            return 0;
        } else if (ret == -EEXIST) {
            rs_ibv_close_device(ib_ctx_tmp);
        } else {
            rs_ibv_close_device(ib_ctx_tmp);
            hccp_err("rs_query_gid failed, ret:%d", ret);
            return ret;
        }
    }

    CHK_PRT_RETURN(i == rdev_cb->dev_num, hccp_err("can not find ib_ctx for phy_id[%u] local_ip[0x%x] in dev_list!",
        rdev_info.phy_id, rdev_info.local_ip.addr.s_addr), -EEXIST);
    return 0;
}

int rs_get_rs_cb(unsigned int phy_id, struct rs_cb **rs_cb)
{
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_dev2rscb(chip_id, rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb fail, ret:%d", ret), -ENODEV);
    return 0;
}

STATIC int rs_get_sq_depth_and_qp_max_num(struct rs_rdev_cb *rdev_cb, unsigned int rdev_index)
{
#ifdef CUSTOM_INTERFACE
    int ret;
    unsigned int qp_max_num = 0;
    unsigned int temp_depth = 0;
    unsigned int sq_depth = 0;

    ret = rs_roce_get_tsqp_depth(rdev_cb->dev_name, rdev_index, &temp_depth, &qp_max_num, &sq_depth);
    CHK_PRT_RETURN(ret, hccp_err("rs_roce_get_tsqp_depth failed, ret:%d, dev_name:%s, rdev_index:%u", ret,
        rdev_cb->dev_name, rdev_index), ret);

    rdev_cb->tx_depth = sq_depth;
    rdev_cb->rx_depth = sq_depth;
    rdev_cb->qp_max_num = qp_max_num;
    hccp_run_info("qp_max_num:%u, sq_depth:%u", qp_max_num, sq_depth);
#endif
    return 0;
}

STATIC int rs_setup_pd_and_notify(struct rs_rdev_cb *rdev_cb)
{
    int ret;

    ret = rs_drv_query_notify_and_alloc_pd(rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_query_notify_and_alloc_pd failed, ret[%d]", ret), ret);

    ret = rs_drv_reg_notify_mr(rdev_cb);
    if (ret) {
        hccp_err("reg notify mr failed, ret[%d]", ret);
        goto dealloc_pd;
    }

    return 0;
dealloc_pd:
    rs_ibv_dealloc_pd(rdev_cb->ib_pd);
    return ret;
}

STATIC int rs_rdev_cb_info_init(struct rdev rdev_info, struct rs_cb *rs_cb, struct rs_rdev_cb *rdev_cb)
{
    int ret;

    rdev_cb->ib_port = RS_PORT_DEF;
    rdev_cb->rs_cb = rs_cb;
    rdev_cb->notify_va_base = rs_cb->notify_va_base;
    rdev_cb->notify_size = rs_cb->notify_size;

    rdev_cb->local_ip.family = (uint32_t)rdev_info.family;
    rdev_cb->local_ip.bin_addr = rdev_info.local_ip;
    ret = rs_inet_ntop(rdev_info.family, &(rdev_info.local_ip), rdev_cb->local_ip.read_addr, RS_MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("rs_inet_ntop failed, ret %d", ret), -EINVAL);

    return 0;
}

STATIC int rs_rdev_cb_init(struct rdev rdev_info, struct rs_rdev_cb *rdev_cb, struct rs_cb *rs_cb,
    unsigned int *rdev_index)
{
    int ret;

    ret = rs_rdev_cb_info_init(rdev_info, rs_cb, rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_rdev_cb_info_init failed, ret %d", ret), ret);

    ret = pthread_mutex_init(&rdev_cb->rdev_mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("rdev_cb mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);

    ret = pthread_mutex_init(&rdev_cb->cqe_err_cnt_mutex, NULL);
    if (ret) {
        hccp_err("rdev_cb cqe_err_cnt_mutex init failed ret %d!, normal ret 0", ret);
        goto destroy_rdev_mutex;
    }

    RS_PTHREAD_MUTEX_LOCK(&rdev_cb->rdev_mutex);
    RS_INIT_LIST_HEAD(&rdev_cb->qp_list);
    RS_INIT_LIST_HEAD(&rdev_cb->typical_mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rdev_mutex);

    ret = rs_get_ib_ctx_and_rdev_index(rdev_info, rdev_cb, rdev_index);
    if (ret) {
        hccp_err("rs_get_ib_ctx_and_rdev_index fail, ret:%d", ret);
        goto destroy_cqe_mutex;
    }

    ret = rs_get_sq_depth_and_qp_max_num(rdev_cb, *rdev_index);
    if (ret) {
        hccp_err("rs_get_sq_depth_and_qp_max_num failed, ret[%d], rdev_index[%u]", ret, *rdev_index);
        goto close_dev;
    }

#ifdef CUSTOM_INTERFACE
    ret = rs_roce_mmap_ai_db_reg(rdev_cb->ib_ctx, (unsigned int)rdev_cb->rs_cb->aicpu_pid);
    if (ret) {
        hccp_err("rs_roce_mmap_ai_db_reg failed, ret[%d], rdev_index[%u]", ret, *rdev_index);
        goto close_dev;
    }
#endif

    ret = rs_setup_pd_and_notify(rdev_cb);
    if (ret) {
        hccp_err("rs_get_sq_depth_and_qp_max_num failed, ret[%d], rdev_index[%u]", ret, *rdev_index);
        goto unmmap_ai_db;
    }

    return 0;

unmmap_ai_db:
#ifdef CUSTOM_INTERFACE
    (void)rs_roce_unmmap_ai_db_reg(rdev_cb->ib_ctx);
#endif
close_dev:
    rs_ibv_close_device(rdev_cb->ib_ctx);
destroy_cqe_mutex:
    pthread_mutex_destroy(&rdev_cb->cqe_err_cnt_mutex);
destroy_rdev_mutex:
    pthread_mutex_destroy(&rdev_cb->rdev_mutex);
    return ret;
}

STATIC int rs_sensor_node_register(unsigned int hccp_mode, unsigned int phy_id, struct rs_rdev_cb *rdev_cb)
{
    struct halSensorNodeCfg cfg = { 0 };
    int ret;

    rdev_cb->sensor_handle = 0;
    rdev_cb->sensor_update_cnt = 0;
    // some non-hdc scenarios don't have corresponding API, skip to register sensor node
    if (hccp_mode != NETWORK_OFFLINE) {
        return 0;
    }

    ret = sprintf_s(cfg.name, sizeof(cfg.name), "roce_rs_%d", getpid());
    if (ret <= 0) {
        hccp_err("[init][rs_rdev]sprintf_s name err, ret:%d, phy_id:%u", ret, phy_id);
        return -ESAFEFUNC;
    }

    cfg.NodeType = HAL_DMS_DEV_TYPE_HCCP;
    cfg.SensorType = RDMA_CQE_ERR_SENSOR_TYPE;
    cfg.AssertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_MASK;
    cfg.DeassertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE_MASK;
    ret = dl_hal_sensor_node_register(rdev_cb->logic_devid, &cfg, &rdev_cb->sensor_handle);
    if (ret) {
        hccp_err("[init][rs_rdev]dl_hal_sensor_node_register failed, phy_id(%u), logic_devid(%u), ret(%d)",
            phy_id, rdev_cb->logic_devid, ret);
        return ret;
    }

    return 0;
}

STATIC void rs_sensor_node_unregister(struct rs_rdev_cb *rdev_cb)
{
    // no need to unregister sensor node
    if (rdev_cb->sensor_handle == 0) {
        return;
    }

    (void)dl_hal_sensor_node_unregister(rdev_cb->logic_devid, rdev_cb->sensor_handle);
}

STATIC int rs_rdev_init_with_backup_info(struct rdev rdev_info, struct rs_backup_info backup_info,
    unsigned int notify_type, unsigned int *rdev_index)
{
    unsigned int phy_id = rdev_info.phy_id;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_cb *rs_cb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(rdev_index);

    ret = rs_api_init();
    CHK_PRT_RETURN(ret, hccp_err("rs_api_init fail! ret[%d]", ret), ret);

    ret = rs_get_rs_cb(phy_id, &rs_cb);
    if (ret) {
        hccp_err("rs_get_rs_cb failed, phy_id[%u] invalid, ret %d", phy_id, ret);
        goto get_rs_cb_fail;
    }

    rdev_cb = calloc(1, sizeof(struct rs_rdev_cb));
    if (rdev_cb == NULL) {
        hccp_err("calloc for rdev_cb fail");
        ret = -ENOMEM;
        goto get_rs_cb_fail;
    }

    ret = rsGetLocalDevIDByHostDevID(phy_id, &rdev_cb->logic_devid);
    if (ret) {
        hccp_err("[init][rs_rdev]rsGetLocalDevIDByHostDevID failed, phy_id(%u), ret(%d)", phy_id, ret);
        goto free_rs_cb;
    }

    rdev_cb->backup_info.backup_flag = backup_info.backup_flag;
    (void)memcpy_s(&rdev_cb->backup_info.rdev_info, sizeof(struct rdev),
        &backup_info.rdev_info, sizeof(struct rdev));
#ifdef CUSTOM_INTERFACE
    // setup sharemem for aicpu rdma unfold
    ret = rs_setup_sharemem(rs_cb, rdev_cb->backup_info.backup_flag, rdev_cb->backup_info.rdev_info.phy_id);
    if (ret != 0) {
        hccp_err("[init][rs_rdev]rs_setup_sharemem failed, phy_id(%u), ret(%d)", phy_id, ret);
        goto free_rs_cb;
    }
#endif

    rdev_cb->notify_type = notify_type;
    rdev_cb->dev_list = rs_ibv_get_device_list(&(rdev_cb->dev_num));
    if (rdev_cb->dev_list == NULL || rdev_cb->dev_num == 0) {
        hccp_err("dev_list is NULL, or dev_num[%d] is 0", rdev_cb->dev_num);
        ret = -EINVAL;
        goto free_rs_cb;
    }

    ret = rs_sensor_node_register(rs_cb->hccp_mode, phy_id, rdev_cb);
    if (ret != 0) {
        hccp_err("[init][rs_rdev]rs_sensor_node_register failed, phy_id(%u), ret(%d)", phy_id, ret);
        goto free_dev_list;
    }

    hccp_info("ibv_get_device_list phy_id[%d] dev_num[%d]", phy_id, rdev_cb->dev_num);

    ret = rs_rdev_cb_init(rdev_info, rdev_cb, rs_cb, rdev_index);
    if (ret) {
        rs_sensor_node_unregister(rdev_cb);
        hccp_err("rs_rdev_cb_init failed ret %d!, normal ret 0", ret);
        goto free_dev_list;
    }

    RS_PTHREAD_MUTEX_LOCK(&rs_cb->mutex);
    rs_list_add_tail(&rdev_cb->list, &rs_cb->rdev_list);
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->mutex);

    hccp_run_info("rdev init success, phy_id:%u, local_ip:0x%x, rdev_index:%u", phy_id, rdev_info.local_ip.addr.s_addr,
        *rdev_index);
    return 0;

free_dev_list:
    rs_ibv_free_device_list(rdev_cb->dev_list);
free_rs_cb:
    free(rdev_cb);
    rdev_cb = NULL;
get_rs_cb_fail:
    rs_api_deinit();
    return ret;
}

RS_ATTRI_VISI_DEF int rs_rdev_init_with_backup(struct rdev rdev_info, struct rdev backup_rdev_info,
    unsigned int notify_type, unsigned int *rdev_index)
{
    struct rs_backup_info backup_info = { 0 };

    backup_info.backup_flag = true;
    (void)memcpy_s(&backup_info.rdev_info, sizeof(struct rdev), &backup_rdev_info, sizeof(struct rdev));

    return rs_rdev_init_with_backup_info(rdev_info, backup_info, notify_type, rdev_index);
}

RS_ATTRI_VISI_DEF int rs_rdev_init(struct rdev rdev_info, unsigned int notify_type, unsigned int *rdev_index)
{
    struct rs_backup_info backup_info = { 0 };

    return rs_rdev_init_with_backup_info(rdev_info, backup_info, notify_type, rdev_index);
}

STATIC void rs_destroy_qp_list(unsigned int phy_id, unsigned int rdev_index,
    struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb, struct rs_qp_cb *qp_cb2)
{
    int ret;

    if (!rs_list_empty(&rdev_cb->qp_list)) {
        hccp_warn("qp list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(qp_cb, qp_cb2, &rdev_cb->qp_list, list, struct rs_qp_cb);
        for (; (&qp_cb->list) != &rdev_cb->qp_list;
            qp_cb = qp_cb2, qp_cb2 = list_entry(qp_cb2->list.next, struct rs_qp_cb, list)) {
            hccp_info("qpn[%u] will be destroyed", qp_cb->ib_qp->qp_num);
            ret = rs_qp_destroy(phy_id, rdev_index, qp_cb->ib_qp->qp_num);
            if (ret) {
                hccp_err("rs_qp_destroy failed, ret:%d", ret);
                return;
            }
        }
    }

    return;
}

STATIC void rs_free_typical_mr_cb(struct rs_rdev_cb *dev_cb)
{
    struct rs_list_head *typical_mr_list = &dev_cb->typical_mr_list;
    struct rs_mr_cb *mr_curr = NULL;
    struct rs_mr_cb *mr_next = NULL;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->rdev_mutex);
    RS_LIST_GET_HEAD_ENTRY(mr_curr, mr_next, typical_mr_list, list, struct rs_mr_cb);
    for (; (&mr_curr->list) != typical_mr_list;
        mr_curr = mr_next, mr_next = list_entry(mr_next->list.next, struct rs_mr_cb, list)) {
        (void)rs_drv_mr_dereg(mr_curr->ib_mr);
        rs_list_del(&mr_curr->list);
        free(mr_curr);
        mr_curr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);

    hccp_info("rs_free_typical_mr_cb is succ");
}

RS_ATTRI_VISI_DEF int rs_rdev_deinit(unsigned int phy_id, unsigned int notify_type, unsigned int rdev_index)
{
    int ret;
    unsigned int chip_id;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_qp_cb *qp_cb2 = NULL;

    hccp_info("rdev deinit start, phy_id:%u, rdev_index:%u", phy_id, rdev_index);
    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    if (rdev_cb->notify_type != NO_USE && rdev_cb->notify_mr != NULL) {
        ret = rs_drv_mr_dereg(rdev_cb->notify_mr);
        if (ret) {
            hccp_err("rs_drv_mr_dereg failed, ret %d", ret);
        }
    }

    hccp_info("poll_cqe_num[%d]", rdev_cb->poll_cqe_num);

    rs_destroy_qp_list(phy_id, rdev_index, rdev_cb, qp_cb, qp_cb2);

    rs_free_typical_mr_cb(rdev_cb);

#ifdef CUSTOM_INTERFACE
    (void)rs_roce_unmmap_ai_db_reg(rdev_cb->ib_ctx);
#endif

    rs_ibv_dealloc_pd(rdev_cb->ib_pd);

    rs_ibv_close_device(rdev_cb->ib_ctx);

#ifdef CUSTOM_INTERFACE
    rs_close_backup_ib_ctx(rdev_cb);
#endif

    pthread_mutex_destroy(&rdev_cb->cqe_err_cnt_mutex);

    pthread_mutex_destroy(&rdev_cb->rdev_mutex);

    rs_sensor_node_unregister(rdev_cb);

    rs_ibv_free_device_list(rdev_cb->dev_list);

    RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);

    rs_list_del(&rdev_cb->list);
    free(rdev_cb);
    rdev_cb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
    rs_api_deinit();
    hccp_run_info("rdev deinit success, phy_id:%u, rdev_index:%u", phy_id, rdev_index);
    return 0;
}

STATIC void rs_heterog_tcp_free_fd_node(struct rs_heterog_tcp_fd_info *fd_node)
{
    int fd;

    RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
    fd = fd_node->fd;
    rs_list_del(&fd_node->list);
    free(fd_node);
    fd_node = NULL;
    g_rs_cb->fd_map[fd] = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
}

/*lint -e429 */
RS_ATTRI_VISI_DEF int rs_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event)
{
    struct rs_heterog_tcp_fd_info *fd_node = NULL;
    unsigned int tmp_event = event;
    int fd = RS_FD_INVALID;
    int ret;

    if (event == RA_EPOLLONESHOT) {
        tmp_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmp_event = EPOLLIN;
    } else {
        hccp_err("unknown event[%u]", tmp_event);
        return -EINVAL;
    }

    if (g_rs_cb == NULL) {
        g_rs_cb = rs_get_cur_rs_cb();
        if (g_rs_cb == NULL) {
            hccp_err("[rs_epoll_ctl_add]rs_get_cur_rs_cb failed rs_cb(NULL)");
            return -EINVAL;
        }
    }
    tmp_event = tmp_event | EPOLLRDHUP;
    fd_node = calloc(1, sizeof(struct rs_heterog_tcp_fd_info));
    CHK_PRT_RETURN(fd_node == NULL, hccp_err("no memory for fd_node"), -ENOMEM);

    fd = ((const struct socket_peer_info *)fd_handle)->fd;
    fd_node->fd = fd;
    RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
    rs_list_add_tail(&fd_node->list, &g_rs_cb->heterog_tcp_fd_list);
    g_rs_cb->fd_map[fd] = fd_handle;
    RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
    ret = rs_epoll_ctl(g_rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD, fd, tmp_event);
    if (ret != 0) {
        hccp_err("[rs_epoll_ctl_add]rs_epoll_ctl failed ret(%d), fd:%d, event:%u", ret, fd, event);
        goto out;
    }
    return 0;
out:
    rs_heterog_tcp_free_fd_node(fd_node);
    fd_node = NULL;
    return ret;
}
/*lint +e429 */

RS_ATTRI_VISI_DEF int rs_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event)
{
    unsigned int tmp_event = event;
    int fd = RS_FD_INVALID;
    int ret;

    if (event == RA_EPOLLONESHOT) {
        tmp_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmp_event = EPOLLIN;
    } else {
        hccp_err("unknown event[%u]", event);
        return -EINVAL;
    }

    tmp_event = tmp_event | EPOLLRDHUP;
    fd = ((const struct socket_peer_info *)fd_handle)->fd;

    if (g_rs_cb == NULL) {
        g_rs_cb = rs_get_cur_rs_cb();
        if (g_rs_cb == NULL) {
            hccp_err("[rs_epoll_ctl_mod]rs_get_cur_rs_cb failed rs_cb(NULL)");
            return -EINVAL;
        }
    }

    ret = rs_epoll_ctl(g_rs_cb->conn_cb.epollfd, EPOLL_CTL_MOD, fd, tmp_event);
    CHK_PRT_RETURN(ret, hccp_err("[rs_epoll_ctl_mod]rs_epoll_ctl failed ret(%d), fd:%d, event:%u",
        ret, fd, event), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_epoll_ctl_del(int fd)
{
    int ret;
    struct rs_heterog_tcp_fd_info *fd_node = NULL;
    struct rs_heterog_tcp_fd_info *fd_node1 = NULL;

    if (g_rs_cb == NULL) {
        g_rs_cb = rs_get_cur_rs_cb();
        if (g_rs_cb == NULL) {
            hccp_err("[rs_epoll_ctl_del]rs_get_cur_rs_cb failed rs_cb(NULL)");
            return -EINVAL;
        }
    }
    RS_LIST_GET_HEAD_ENTRY(fd_node, fd_node1, &g_rs_cb->heterog_tcp_fd_list, list, struct rs_heterog_tcp_fd_info);
    for (; (&fd_node->list) != &g_rs_cb->heterog_tcp_fd_list;
        fd_node = fd_node1, fd_node1 = list_entry(fd_node1->list.next, struct rs_heterog_tcp_fd_info, list)) {
        if (fd_node->fd == fd) {
            // 删除节点
            rs_heterog_tcp_free_fd_node(fd_node);
            fd_node = NULL;
            break; //lint !e108
        }
    }

    // 为了兼容epoll不同版本，这里加EPOLLIN参数
    ret = rs_epoll_ctl(g_rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, fd, EPOLLIN);
    CHK_PRT_RETURN(ret, hccp_err("[rs_epoll_ctl_del]rs_epoll_ctl failed ret(%d), fd:%d", ret, fd), ret);
    return 0;
}

RS_ATTRI_VISI_DEF void rs_set_tcp_recv_callback(const void *callback)
{
    if (g_rs_cb == NULL) {
        hccp_err("param error, g_rs_cb is NULL");
        return;
    }
    g_rs_cb->tcp_recv_callback = (void (*)(const void *))callback;
}

STATIC void rs_free_accept_one_node(struct rs_cb *rscb, struct rs_accept_info *accept)
{
    int ret;

    ret = rs_epoll_ctl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, accept->conn_fd, EPOLLIN);
    if (ret) {
        hccp_err("epoll ctl del fd %d failed, ret:%d", accept->conn_fd, ret);
    }

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    rs_list_del(&accept->list);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

    if (rscb->ssl_enable == RS_SSL_ENABLE) {
        if (accept->ssl == NULL) {
            hccp_warn("[Server] accept->ssl is NULL, it maybe has not establish tls link");
        } else {
            ssl_adp_shutdown(accept->ssl);
            ssl_adp_free(accept->ssl);
            accept->ssl = NULL;
        }
    }

    RS_CLOSE_RETRY_FOR_EINTR(ret, accept->conn_fd);

    hccp_info("free accept_server IP:%s, port:%d, conn_fd:%d", accept->server_ip_addr.read_addr,
        accept->sock_port, accept->conn_fd);
    accept->conn_fd = RS_FD_INVALID;

    free(accept);
    accept = NULL;
}

STATIC void rs_free_accpet_list(struct rs_cb *rscb)
{
    struct rs_accept_info *accept = NULL;
    struct rs_accept_info *accept2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.server_accept_list)) {
        hccp_warn("Server accept list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(accept, accept2, &rscb->conn_cb.server_accept_list, list, struct rs_accept_info);
        for (; (&accept->list) != &rscb->conn_cb.server_accept_list;
            accept = accept2, accept2 = list_entry(accept2->list.next, struct rs_accept_info, list)) {
            rs_free_accept_one_node(rscb, accept);
            accept = NULL;
        }
    }

    return ;
}

STATIC void rs_free_designated_accpet_node(struct rs_cb *rscb, struct rs_ip_addr_info *local_ip)
{
    struct rs_accept_info *accept = NULL;
    struct rs_accept_info *accept2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.server_accept_list)) {
        RS_LIST_GET_HEAD_ENTRY(accept, accept2, &rscb->conn_cb.server_accept_list, list, struct rs_accept_info);
        for (; (&accept->list) != &rscb->conn_cb.server_accept_list;
            accept = accept2, accept2 = list_entry(accept2->list.next, struct rs_accept_info, list)) {
            if (!rs_compare_ip_addr(&accept->server_ip_addr, local_ip)) {
                rs_free_accept_one_node(rscb, accept);
                accept = NULL;
            }
        }
    }

    return;
}

STATIC void rs_free_conn_one_node(struct rs_cb *rscb, struct rs_conn_info *conn)
{
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    rs_list_del(&conn->list);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

    if (rscb->ssl_enable == RS_SSL_ENABLE) {
        if (conn->ssl == NULL) {
            hccp_warn("[Client] conn->ssl is NULL, it maybe has not establish tls link");
        } else {
            ssl_adp_shutdown(conn->ssl);
            ssl_adp_free(conn->ssl);
            conn->ssl = NULL;
        }
    }

    RS_CLOSE_RETRY_FOR_EINTR(ret, conn->connfd);

    hccp_info("free for conn IP:%s, port:%d, connfd:%d, state:%u",
        conn->client_ip.read_addr, conn->port, conn->connfd, conn->state);

    conn->connfd = RS_FD_INVALID;
    conn->state = RS_CONN_STATE_RESET;

    free(conn);
    conn = NULL;
}

STATIC void rs_free_client_conn_list(struct rs_cb *rscb)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.client_conn_list)) {
        hccp_warn("Client conn node do not empty!");
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.client_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.client_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            rs_free_conn_one_node(rscb, conn);
            conn = NULL;
        }
    }

    return;
}

STATIC void rs_free_designated_client_conn_node(struct rs_cb *rscb, struct rs_ip_addr_info *local_ip)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.client_conn_list)) {
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.client_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.client_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            if (!rs_compare_ip_addr(&conn->client_ip, local_ip)) {
                hccp_warn("Client conn node for IP[%s] do not empty!", local_ip->read_addr);
                rs_free_conn_one_node(rscb, conn);
                conn = NULL;
            }
        }
    }

    return;
}

STATIC void rs_free_server_conn_list(struct rs_cb *rscb)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.server_conn_list)) {
        hccp_warn("Server conn node do not empty!");
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.server_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.server_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            rs_free_conn_one_node(rscb, conn);
            conn = NULL;
        }
    }

    return;
}

STATIC void rs_free_designated_server_conn_node(struct rs_cb *rscb, struct rs_ip_addr_info *local_ip)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.server_conn_list)) {
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.server_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.server_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            if (!rs_compare_ip_addr(&conn->server_ip, local_ip)) {
                hccp_warn("Server conn node for IP[%s] do not empty!", local_ip->read_addr);
                rs_free_conn_one_node(rscb, conn);
                conn = NULL;
            }
        }
    }
    return;
}

STATIC void rs_free_listen_one_node(struct rs_cb *rscb, struct rs_listen_info *listen)
{
    int ret;

    ret = rs_epoll_ctl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, listen->listen_fd, EPOLLIN);
    if (ret) {
        hccp_err("delete from epoll failed, ret:%d, epollfd:%d, listen_fd:%d", ret, rscb->conn_cb.epollfd,
            listen->listen_fd);
    }

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    rs_list_del(&listen->list);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

    RS_CLOSE_RETRY_FOR_EINTR(ret, listen->listen_fd);

    hccp_info("free Listen IP:%s, port:%d, listen_fd:%d, state:%u",
        listen->server_ip_addr.read_addr, ntohs(listen->sock_port), listen->listen_fd, listen->state);

    listen->listen_fd = RS_FD_INVALID;
    listen->state = RS_CONN_STATE_RESET;

    free(listen);
}

STATIC void rs_free_listen_list(struct rs_cb *rscb)
{
    struct rs_listen_info *listen = NULL;
    struct rs_listen_info *listen2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.listen_list)) {
        hccp_warn("Server listen node do not empty!");
        RS_LIST_GET_HEAD_ENTRY(listen, listen2, &rscb->conn_cb.listen_list, list, struct rs_listen_info);
        for (; (&listen->list) != &rscb->conn_cb.listen_list;
            listen = listen2, listen2 = list_entry(listen2->list.next, struct rs_listen_info, list)) {
            rs_free_listen_one_node(rscb, listen);
            listen = NULL;
        }
    }

    return;
}

STATIC void rs_free_designated_listen_node(struct rs_cb *rscb, struct rs_ip_addr_info *local_ip)
{
    struct rs_listen_info *listen = NULL;
    struct rs_listen_info *listen2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.listen_list)) {
        RS_LIST_GET_HEAD_ENTRY(listen, listen2, &rscb->conn_cb.listen_list, list, struct rs_listen_info);
        for (; (&listen->list) != &rscb->conn_cb.listen_list;
            listen = listen2, listen2 = list_entry(listen2->list.next, struct rs_listen_info, list)) {
            if (!rs_compare_ip_addr(&listen->server_ip_addr, local_ip)) {
                rs_free_listen_one_node(rscb, listen);
                listen = NULL;
            }
        }
    }

    return;
}

STATIC void rs_white_list_node_free(struct rs_cb *rscb, struct rs_white_list *wlist)
{
    struct rs_white_list_info *wlist_node = NULL;
    struct rs_white_list_info *wlist_node1 = NULL;

    if (!rs_list_empty(&wlist->white_list)) {
        RS_LIST_GET_HEAD_ENTRY(wlist_node, wlist_node1, &wlist->white_list, list, struct rs_white_list_info);
        for (; (&wlist_node->list) != &wlist->white_list;
            wlist_node = wlist_node1, wlist_node1 = list_entry(wlist_node1->list.next,
                struct rs_white_list_info, list)) {
            RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
            rs_list_del(&wlist_node->list);
            RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

            hccp_info("free White list client IP:%s, tag:%s", wlist_node->client_ip.read_addr, wlist_node->tag);
            free(wlist_node);
            wlist_node = NULL;
        }
    }
}

STATIC void rs_free_white_one_node(struct rs_cb *rscb, struct rs_white_list *wlist)
{
    rs_white_list_node_free(rscb, wlist);

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    rs_list_del(&wlist->list);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

    hccp_info("White list server IP:%s", wlist->server_ip.read_addr);
    free(wlist);
    wlist = NULL;
}

STATIC void rs_free_white_list(struct rs_cb *rscb)
{
    struct rs_white_list *wlist = NULL;
    struct rs_white_list *wlist2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.white_list)) {
        hccp_warn("Server white list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(wlist, wlist2, &rscb->conn_cb.white_list, list, struct rs_white_list);
        for (; (&wlist->list) != &rscb->conn_cb.white_list;
            wlist = wlist2, wlist2 = list_entry(wlist2->list.next, struct rs_white_list, list)) {
            rs_free_white_one_node(rscb, wlist);
            wlist = NULL;
        }
    }

    return;
}

STATIC void rs_free_designated_white_node(struct rs_cb *rscb, struct rs_ip_addr_info *local_ip)
{
    struct rs_white_list *wlist = NULL;
    struct rs_white_list *wlist2 = NULL;

    if (!rs_list_empty(&rscb->conn_cb.white_list)) {
        RS_LIST_GET_HEAD_ENTRY(wlist, wlist2, &rscb->conn_cb.white_list, list, struct rs_white_list);
        for (; (&wlist->list) != &rscb->conn_cb.white_list;
            wlist = wlist2, wlist2 = list_entry(wlist2->list.next, struct rs_white_list, list)) {
            if (!rs_compare_ip_addr(&wlist->server_ip, local_ip)) {
                rs_free_white_one_node(rscb, wlist);
                wlist = NULL;
            }
        }
    }

    return;
}

STATIC void rs_free_socket_list(struct rs_cb *rscb, struct rs_ip_addr_info *local_ip)
{
    rs_free_designated_accpet_node(rscb, local_ip);
    rs_free_designated_client_conn_node(rscb, local_ip);

    rs_free_designated_server_conn_node(rscb, local_ip);

    rs_free_designated_listen_node(rscb, local_ip);

    rs_free_designated_white_node(rscb, local_ip);

    return ;
}

RS_ATTRI_VISI_DEF int rs_socket_deinit(struct rdev rdev_info)
{
    int ret;
    unsigned int phy_id = rdev_info.phy_id;
    unsigned int chip_id;
    struct rs_cb *rscb = NULL;

    hccp_info("rs socket deinit start, phy_id:%u", phy_id);
    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    CHK_PRT_RETURN((rdev_info.family != AF_INET) && (rdev_info.family != AF_INET6),
        hccp_err("family[%d] invalid", rdev_info.family), -EPROTONOSUPPORT);

    if (rdev_info.family == AF_INET) {
        unsigned int *local_ip = NULL;
        local_ip = &(rdev_info.local_ip.addr.s_addr);
        ret = rs_socket_nodeid2vnic(*local_ip, local_ip);
        hccp_info("socket deinit local IP is 0x%llx, ret:%d", *local_ip, ret);
    }

    struct rs_ip_addr_info local_ip;
    rs_convert_ip_addr(rdev_info.family, &rdev_info.local_ip, &local_ip);

    ret = rs_dev2rscb(chip_id, &rscb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rscb fail for chip_id:%u, ret:%d", chip_id, ret), -ENODEV);

    RS_PTHREAD_MUTEX_LOCK(&rscb->mutex);
    rs_free_socket_list(rscb, &local_ip);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->mutex);
    hccp_run_info("socket deinit success, phy_id:%u, local_ip:%s", phy_id, local_ip.read_addr);
    return 0;
}

STATIC void rs_free_rdev_list(struct rs_cb *rs_cb)
{
    struct rs_rdev_cb *rdev_cb_curr = NULL;
    struct rs_rdev_cb *rdev_cb_next = NULL;
    unsigned int phy_id = 0;
    int ret;

    ret = rsGetDevIDByLocalDevID(rs_cb->chip_id, &phy_id);
    if (ret != 0) {
        hccp_err("chip_id[%u] invalid, ret %d", rs_cb->chip_id, ret);
        return;
    }

    RS_LIST_GET_HEAD_ENTRY(rdev_cb_curr, rdev_cb_next, &rs_cb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdev_cb_curr->list) != &rs_cb->rdev_list;
        rdev_cb_curr = rdev_cb_next, rdev_cb_next = list_entry(rdev_cb_next->list.next, struct rs_rdev_cb, list)) {
        ret = rs_rdev_deinit(phy_id, rdev_cb_curr->notify_type, rdev_cb_curr->rdev_index);
        if (ret != 0) {
            hccp_err("rs_rdev_deinit failed, ret:%d, phy_id:%u", ret, phy_id);
        }
    }

    return;
}

STATIC void rs_free_udev_list(struct rs_cb *rs_cb)
{
    return;
}

STATIC void rs_free_dev_list(struct rs_cb *rs_cb)
{
    if (rs_list_empty(&rs_cb->rdev_list)) {
        return;
    }

    hccp_warn("dev list is not empty!");
    switch (rs_cb->protocol) {
        case PROTOCOL_RDMA:
            rs_free_rdev_list(rs_cb);
            break;
        case PROTOCOL_UDMA:
            rs_free_udev_list(rs_cb);
            break;
        default:
            hccp_err("protocol[%d] not support", rs_cb->protocol);
            break;
    }
    return;
}

STATIC void rs_free_heterog_tcp_fd_list(struct rs_cb *rs_cb)
{
    struct rs_heterog_tcp_fd_info *fd_node = NULL;
    struct rs_heterog_tcp_fd_info *fd_node1 = NULL;

    if (!rs_list_empty(&rs_cb->heterog_tcp_fd_list)) {
        hccp_warn("heterog_tcp_fd_list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(fd_node, fd_node1, &rs_cb->heterog_tcp_fd_list, list, struct rs_heterog_tcp_fd_info);
        for (; (&fd_node->list) != &rs_cb->heterog_tcp_fd_list;
            fd_node = fd_node1, fd_node1 = list_entry(fd_node1->list.next, struct rs_heterog_tcp_fd_info, list)) {
            hccp_info(">>>>>fd_node->fd:%d", fd_node->fd);
            // 删除节点
            RS_PTHREAD_MUTEX_LOCK(&rs_cb->mutex);
            rs_list_del(&fd_node->list);
            free(fd_node);
            fd_node = NULL;
            RS_PTHREAD_MUTEX_ULOCK(&rs_cb->mutex);
        }
    }

    return;
}

STATIC void rs_list_free(struct rs_cb *rscb)
{
    rs_free_accpet_list(rscb);
    rs_free_client_conn_list(rscb);

    rs_free_server_conn_list(rscb);

    rs_free_listen_list(rscb);

    rs_free_white_list(rscb);

    return ;
}

STATIC void rs_ssl_free(struct rs_cb *rscb)
{
    if (rscb->ssl_enable == RS_SSL_ENABLE) {
        if (rscb->skid_subject_cb != NULL) {
            if (memset_s(rscb->skid_subject_cb, sizeof(struct rs_cert_skid_subject_cb), 0,
                sizeof(struct rs_cert_skid_subject_cb))) {
                hccp_warn("memset_s for skid_subject_cb unsuccessful");
            }
            free(rscb->skid_subject_cb);
            rscb->skid_subject_cb = NULL;
        }
        ssl_adp_ctx_free(rscb->server_ssl_ctx);
        rscb->server_ssl_ctx = NULL;
        ssl_adp_ctx_free(rscb->client_ssl_ctx);
        rscb->client_ssl_ctx = NULL;
    }
}

STATIC void rs_deinit_free_rscb(struct rs_cb *rscb)
{
    RS_PTHREAD_MUTEX_LOCK(&rscb->mutex);
    rs_list_free(rscb);

    free(rscb->fd_map);
    rscb->fd_map = NULL;
    freeifaddrs(rscb->ifaddr_list);
    rscb->ifaddr_list = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&rscb->mutex);
    rs_free_dev_list(rscb);
    rs_ssl_free(rscb);
    rs_free_heterog_tcp_fd_list(rscb);

#ifdef CONFIG_TLV
    if (rscb->nslb_cb.netco_init_flag) {
        rs_tlv_deinit(TLV_MODULE_TYPE_NSLB, rscb->nslb_cb.phy_id);
    }
#endif
    pthread_mutex_destroy(&rscb->mutex);
    pthread_mutex_destroy(&rscb->conn_cb.conn_mutex);
    rs_destroy_epoll(rscb);

    free(rscb);
    rscb = NULL;
    g_rs_cb = NULL;
}

RS_ATTRI_VISI_DEF int rs_deinit(struct rs_init_config *cfg)
{
    int ret;
    eventfd_t event;
    struct rs_cb *rscb = g_rs_cb;
    unsigned int chip_id;

    CHK_PRT_RETURN(cfg == NULL, hccp_err("param error, cfg is NULL"), -EINVAL);

    chip_id = cfg->chip_id;
    if (__sync_fetch_and_sub(&(g_init_counter[chip_id]), 1) > 1) {
        return 0;
    }
    if (rscb && (chip_id == rscb->chip_id)) {
        event = 1;
        /* send event to eventfd to waking up epoll handle thread */
        ret = (int)write(rscb->conn_cb.eventfd, &event, sizeof(eventfd_t));
        CHK_PRT_RETURN(ret != sizeof(eventfd_t), hccp_err("eventfd_write failed(0x%x), chip_id:%u, errno:%d",
            ret, chip_id, errno), -EFILEOPER);

        hccp_info("epoll wait up ok, rscb->conn_flag:%d", rscb->conn_flag);
        // already been RS_CONN_EXIT_FLAG, no need to change conn_flag
        if (rscb->conn_flag != RS_CONN_EXIT_FLAG) {
            rscb->conn_flag = 0;
        }
        int try_again = RS_TRY_TIME;
        while (((rscb->state & RS_STATE_HALT) == 0) && try_again > 0) {
            usleep(RS_USLEEP_TIME);
            try_again--;
        };

        if (try_again == 0) {
            hccp_warn("try_again exhausted, rscb state:%u", rscb->state);
        }

        try_again = RS_TRY_TIME;
        while ((rscb->conn_flag != RS_CONN_EXIT_FLAG) && try_again > 0) {
            usleep(RS_USLEEP_TIME);
            try_again--;
        }

        CHK_PRT_RETURN(try_again == 0, hccp_warn("connect thread quit unsuccessful"), -EAGAIN);
        rscb->state &= ~RS_STATE_HALT;
        rs_deinit_free_rscb(rscb);
        g_rs_cb_list[chip_id] = NULL;
        dl_hal_deinit();

        hccp_run_info("rs_deinit chip_id[%u] ok", chip_id);

        return 0;
    }

    dl_hal_deinit();
    return -ENODEV;
}

RS_ATTRI_VISI_DEF int rs_get_vnic_ip(unsigned int phy_id, unsigned int *vnic_ip)
{
    int64_t device_info = 0;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid", phy_id,
        RS_MAX_DEV_NUM), -EINVAL);
    CHK_PRT_RETURN(vnic_ip == NULL, hccp_err("vnic_ip is null!"), -EINVAL);

    ret = dl_hal_get_device_info(phy_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &device_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("phy_id:%u dl_hal_get_device_info failed! ret:%d", phy_id, ret), ret);

    *vnic_ip = (unsigned int)device_info;
    return 0;
}

STATIC int rs_get_vnic_ip_info(unsigned int phy_id, unsigned int id, enum id_type type, struct ip_info *info)
{
    int64_t device_info = 0;
    unsigned int vnic_ip;
    int ret;

    // get vnic ip by id with different type
    if (type == PHY_ID_VNIC_IP) {
        ret = dl_hal_get_device_info(id, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &device_info);
        CHK_PRT_RETURN(ret != 0, hccp_err("cur_phy_id:%u dl_hal_get_device_info failed! phy_id:%u ret:%d",
            phy_id, id, ret), ret);
    } else if (type == SDID_VNIC_IP) {
        ret = dl_hal_get_device_info(id, MODULE_TYPE_SYSTEM, INFO_TYPE_SPOD_VNIC_IP, &device_info);
        CHK_PRT_RETURN(ret != 0, hccp_err("phy_id:%u dl_hal_get_device_info failed! sdid:0x%x ret:%d",
            phy_id, id, ret), ret);
    } else {
        hccp_err("phy_id:%u get vnic ip fail! id:0x%x, invalid type:%u", phy_id, id, type);
        return -EINVAL;
    }

    // prepare ip info, only support IPv4
    vnic_ip = (unsigned int)device_info;
    info->family = AF_INET;
    info->ip.addr.s_addr = vnic_ip;

    hccp_dbg("phy_id:%u query id:%u type:%u got vnic_ip:%u", phy_id, id, type, vnic_ip);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(ids == NULL, hccp_err("phy_id:%u, ids is null!", phy_id), -EINVAL);
    CHK_PRT_RETURN(infos == NULL, hccp_err("phy_id:%u, infos is null!", phy_id), -EINVAL);

    for (i = 0; i < num; i++) {
        ret = rs_get_vnic_ip_info(phy_id, ids[i], type, &infos[i]);
        if (ret != 0) {
            hccp_err("phy_id:%u get vnic ip info fail! ids[%u]:0x%x type:%u", phy_id, i, ids[i], type);
            return ret;
        }
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_interface_version(unsigned int opcode, unsigned int *version)
{
    int i;
    unsigned int interface_version = 0; // default interface is 0 (0: not support this interface opcode)
    int num = sizeof(g_interface_info_list) / sizeof(g_interface_info_list[0]);

    CHK_PRT_RETURN(version == NULL, hccp_err("rs_get_interface_version fail! version is null"), -EINVAL);

    for (i = 0; i < num; i++) {
        if (opcode == g_interface_info_list[i].opcode && opcode != RA_RS_GET_ROCE_API_VERSION) {
            interface_version = g_interface_info_list[i].version;
            break;
        } else if (opcode == RA_RS_GET_ROCE_API_VERSION) {
            interface_version = rs_roce_get_api_version();
            break;
        }
    }

    *version = interface_version;
    return 0;
}

int rsGetLocalDevIDByHostDevID(unsigned int phy_id, unsigned int *chip_id)
{
    CHK_PRT_RETURN(g_rs_cb == NULL, hccp_warn("No device initialized !"), -ENODEV);

    if (g_rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        *chip_id = phy_id;
        return 0;
    } else {
        return dl_drv_get_local_dev_id_by_host_dev_id(phy_id, chip_id);
    }
}

int rsGetDevIDByLocalDevID(unsigned int chip_id, unsigned int *phy_id)
{
    CHK_PRT_RETURN(g_rs_cb == NULL, hccp_warn("No device initialized !"), -ENODEV);

    if (g_rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        *phy_id = chip_id;
        return 0;
    } else {
        return dl_drv_get_dev_id_by_local_dev_id(chip_id, phy_id);
    }
}

RS_ATTRI_VISI_DEF int rs_set_qp_attr_qos(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct qos_attr *attr)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    qp_cb->qos_attr.tc = attr->tc;
    qp_cb->qos_attr.sl = attr->sl;

    hccp_info("set qp qos attr: qpn[%u] tc[%u] sl[%u]", qpn, attr->tc, attr->sl);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_set_qp_attr_timeout(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    unsigned int *timeout)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    qp_cb->timeout = *timeout;

    hccp_info("set qp qos attr: qpn[%u] timeout[%u]", qpn, *timeout);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_set_qp_attr_retry_cnt(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    unsigned int *retry_cnt)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    qp_cb->retry_cnt = *retry_cnt;

    hccp_info("set qp qos attr: qpn[%u] retry_cnt[%u]", qpn, *retry_cnt);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_cqe_err_info(struct cqe_err_info *info)
{
    int ret;

    ret = rs_drv_get_cqe_err_info(info);
    CHK_PRT_RETURN(ret, hccp_err("get fail! ret:%d", ret), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_cqe_err_info_num(unsigned int phy_id, unsigned int rdev_idx, unsigned int *num)
{
    struct rs_rdev_cb *rdev_cb = NULL;
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs get cqe err param error, phy_id[%u]", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_idx, &rdev_cb);
    CHK_PRT_RETURN(ret != 0 || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    *num = rdev_cb->cqe_err_cnt;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_cqe_err_info_list(unsigned int phy_id, unsigned int rdev_idx, struct cqe_err_info *info,
    unsigned int *num)
{
    struct rs_qp_cb *qp_cb_curr = NULL;
    struct rs_qp_cb *qp_cb_next = NULL;
    struct rs_rdev_cb *rdev_cb = NULL;
    unsigned int cqe_err_idx = 0;
    unsigned int num_tmp = *num;
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs get cqe err param error, phy_id[%u]", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_idx, &rdev_cb);
    CHK_PRT_RETURN(ret != 0 || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    if (rs_list_empty(&rdev_cb->qp_list)) {
        *num = 0;
        return 0;
    }

    RS_LIST_GET_HEAD_ENTRY(qp_cb_curr, qp_cb_next, &rdev_cb->qp_list, list, struct rs_qp_cb);
    for (; (&qp_cb_curr->list) != &rdev_cb->qp_list;
        qp_cb_curr = qp_cb_next, qp_cb_next = list_entry(qp_cb_next->list.next, struct rs_qp_cb, list)) {
        if (qp_cb_curr->cqe_err_info.info.status != 0) {
            RS_PTHREAD_MUTEX_LOCK(&qp_cb_curr->cqe_err_info.mutex);
            info[cqe_err_idx].status = qp_cb_curr->cqe_err_info.info.status;
            info[cqe_err_idx].qpn = qp_cb_curr->cqe_err_info.info.qpn;
            info[cqe_err_idx].time = qp_cb_curr->cqe_err_info.info.time;
            qp_cb_curr->cqe_err_info.info.status = 0;
            RS_PTHREAD_MUTEX_ULOCK(&qp_cb_curr->cqe_err_info.mutex);
            RS_PTHREAD_MUTEX_LOCK(&qp_cb_curr->rdev_cb->cqe_err_cnt_mutex);
            qp_cb_curr->rdev_cb->cqe_err_cnt--;
            RS_PTHREAD_MUTEX_ULOCK(&qp_cb_curr->rdev_cb->cqe_err_cnt_mutex);
            cqe_err_idx++;
            if (cqe_err_idx == num_tmp) {
                break;
            }
        }
    }

    *num = cqe_err_idx;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_create_event_handle(int *event_handle)
{
    int ret;

    ret = rs_epoll_create_epollfd(event_handle);
    CHK_PRT_RETURN(ret, hccp_err("[rs_create_event_handle]rs_epoll_create_epollfd failed ret(%d)", ret), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_ctl_event_handle(int event_handle, const void *fd_handle, int opcode,
    enum RaEpollEvent event)
{
    int fd = RS_FD_INVALID;
    unsigned int tmp_event;
    int ret;

    if (event_handle < 0) {
        hccp_err("[rs_ctl_event_handle]event_handle[%d] is invalid", event_handle);
        return -EINVAL;
    }
    if (fd_handle == NULL) {
        hccp_err("[rs_ctl_event_handle]fd_handle is NULL");
        return -EINVAL;
    }
    if (opcode != EPOLL_CTL_ADD && opcode != EPOLL_CTL_DEL && opcode != EPOLL_CTL_MOD) {
        hccp_err("[rs_ctl_event_handle]opcode[%d] invalid, valid opcode includes {%d, %d, %d}",
            opcode, EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD);
        return -EINVAL;
    }

    if (event == RA_EPOLLONESHOT) {
        tmp_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmp_event = EPOLLIN;
    } else if (event == RA_EPOLLOUT) {
        tmp_event = EPOLLOUT;
    } else if (event == RA_EPOLLOUT_LET_ONESHOT) {
        tmp_event = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    } else {
        hccp_err("[rs_ctl_event_handle]unknown event[%d]", event);
        return -EINVAL;
    }

    tmp_event = tmp_event | EPOLLRDHUP;
    fd = ((const struct socket_peer_info *)fd_handle)->fd;

    ret = rs_epoll_ctl_fd_handle(event_handle, opcode, fd, tmp_event, (void*)fd_handle);
    CHK_PRT_RETURN(ret, hccp_err("[rs_ctl_event_handle]rs_epoll_ctl_fd_handle failed ret(%d), fd:%d", ret, fd), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_wait_event_handle(int event_handle, struct socket_event_info *event_infos,
    int timeout, unsigned int maxevents, unsigned int *events_num)
{
    int ret;

    if (event_handle < 0) {
        hccp_err("[rs_wait_event_handle]event_handle[%d] is invalid", event_handle);
        return -EINVAL;
    }

    if (event_infos == NULL) {
        hccp_err("[rs_wait_event_handle]event_info is NULL");
        return -EINVAL;
    }

    if (timeout < -1) {
        hccp_err("[rs_wait_event_handle]timeout[%d] is invalid", timeout);
        return -EINVAL;
    }

    if (maxevents > MAX_SOCKET_EVENT_NUM) {
        hccp_err("[rs_wait_event_handle]maxevents[%u] exceeds %u", maxevents, MAX_SOCKET_EVENT_NUM);
        return -EINVAL;
    }

    if (events_num == NULL) {
        hccp_err("[rs_wait_event_handle]events_num is NULL");
        return -EINVAL;
    }

    ret = rs_epoll_wait_handle(event_handle, (struct epoll_event *)event_infos,
        timeout, maxevents, events_num);
    CHK_PRT_RETURN(ret, hccp_err("[rs_wait_event_handle]rs_epoll_wait_handle failed ret(%d)", ret), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_destroy_event_handle(int *event_handle)
{
    int ret;

    ret = rs_epoll_destroy_fd(event_handle);
    CHK_PRT_RETURN(ret, hccp_err("[rs_destroy_event_handle]rs_epoll_destroy_fd failed ret(%d)", ret), ret);
    return 0;
}

int rs_query_mr_cb(struct rs_rdev_cb *dev_cb, uint64_t addr, struct rs_mr_cb **mr_cb,
                   struct rs_list_head *mr_list)
{
    struct rs_mr_cb *mr_curr = NULL;
    struct rs_mr_cb *mr_next = NULL;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->rdev_mutex);
    RS_LIST_GET_HEAD_ENTRY(mr_curr, mr_next, mr_list, list, struct rs_mr_cb);
    for (; (&mr_curr->list) != mr_list;
        mr_curr = mr_next, mr_next = list_entry(mr_next->list.next, struct rs_mr_cb, list)) {
        if ((mr_curr->mr_info.addr <= addr) && (addr < mr_curr->mr_info.addr + mr_curr->mr_info.len)) {
            *mr_cb = mr_curr;
            RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);
            return 0;
        }
    }

    *mr_cb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);

    hccp_info("cannot find mrcb for addr@0x%lx !", addr);

    return -ENODEV;
}

STATIC int rs_get_linux_version(struct rs_linux_version_info *ver_info)
{
#define LINUX_VERSION_MAX_CHAR 1024
#define LINUX_VERSION_TYPE_NUM 3
#define LINUX_VERSION_STR "Linux version "
    char buffer[LINUX_VERSION_MAX_CHAR];
    char *version_str;
    int ret_close = 0;
    int ret = 0;
    int fd;

    fd = open("/proc/version", O_RDONLY);
    CHK_PRT_RETURN(fd < 0, hccp_run_warn("open proc/version unsuccessful, errno[%d] fd[%d]", errno, fd), -EFILEOPER);

    do {
        ret = (int)read(fd, buffer, sizeof(buffer));
    } while ((ret < 0) && (errno == EINTR));

    if (ret < 0) {
        hccp_run_warn("read fd unsuccessful[%d]", ret);
        RS_CLOSE_RETRY_FOR_EINTR(ret_close, fd);
        return -EFILEOPER;
    }

    version_str = strstr(buffer, LINUX_VERSION_STR);
    if (version_str == NULL) {
        hccp_run_warn("can't get Linux version");
        RS_CLOSE_RETRY_FOR_EINTR(ret_close, fd);
        return -EFILEOPER;
    }
    version_str += strlen(LINUX_VERSION_STR);
    if (sscanf_s(version_str, "%d.%d.%d", &ver_info->major, &ver_info->minor, &ver_info->patch) !=
        LINUX_VERSION_TYPE_NUM) {
        hccp_run_warn("can't extract Linux version");
        RS_CLOSE_RETRY_FOR_EINTR(ret_close, fd);
        return -EFILEOPER;
    }

    RS_CLOSE_RETRY_FOR_EINTR(ret_close, fd);
    return ret_close;
}

RS_ATTRI_VISI_DEF int rs_get_sec_random(unsigned int *value)
{
#define SEC_LINUX_VERSION_MAJOR 5
#define SEC_LINUX_VERSION_MINOR 18
#define SEC_LINUX_VERSION_PATCH 0
    struct rs_linux_version_info ver_info = {0};
    int ret;

    ret = rs_get_linux_version(&ver_info);
    CHK_PRT_RETURN(ret, hccp_run_warn("[rs_get_random]get_linux_version unsuccessful ret(%d)", ret), ret);

    // linux_version > 5.18, urandom is secure
    if (ver_info.major > SEC_LINUX_VERSION_MAJOR || (ver_info.major == SEC_LINUX_VERSION_MAJOR &&
        ver_info.minor > SEC_LINUX_VERSION_MINOR) || (ver_info.major == SEC_LINUX_VERSION_MAJOR &&
        ver_info.minor == SEC_LINUX_VERSION_MINOR && ver_info.patch > SEC_LINUX_VERSION_PATCH)) {
        ret = rs_drv_get_random_num((int *)value);
    } else {
        hccp_run_warn("[rs_get_random]linux_version is not secure version");
        return -ENOTSUPP;
    }

    if (ret != 0) {
        hccp_run_warn("[get][get_random]rs_get_sec_random unsuccessful, ret(%d)", ret);
    }
    return ret;
}

#ifndef CONFIG_SSL
long ssl_adp_set_mode(SSL *s, long op)
{
    return 0;
};

long ssl_adp_ctrl(SSL *s, int cmd, long larg, void *parg)
{
    return 0;
};

SSL *ssl_adp_new(SSL_CTX *ctx)
{
    return NULL;
};

void ssl_adp_free(SSL *s)
{
    return;
};

int ssl_adp_read(SSL *s, void *buf, int num)
{
    return 0;
};

int ssl_adp_write(SSL *s, const void *buf, int num)
{
    return 0;
};

int ssl_adp_set_fd(SSL *s, int fd)
{
    return 0;
};

void ssl_adp_ctx_free(SSL_CTX *a)
{
    return;
};

int ssl_adp_shutdown(SSL *s)
{
    return 0;
};

int ssl_adp_get_error(const SSL *s, int i)
{
    return 0;
};

int ssl_adp_do_handshake(SSL *s)
{
    return 0;
};

void ssl_adp_set_accept_state(SSL *s)
{
    return;
};

void ssl_adp_set_connect_state(SSL *s)
{
    return;
};

void ssl_adp_clear_error()
{
    return;
};
#endif
