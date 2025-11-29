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
#include "file_opt.h"
#ifdef CONFIG_TLV
#include "hccp_tlv.h"
#include "rs_tlv.h"
#endif

__thread struct rs_cb *gRsCb = NULL;  //lint !e17
struct rs_cb *gRsCbList[RS_MAX_DEV_NUM] = {0};  //lint !e17
int gInitCounter[RS_MAX_DEV_NUM] = {0};

/* set current phy_id g_rs_cb */
void RsSetCtx(unsigned int phyId)
{
    gRsCb = gRsCbList[phyId];
    hccp_dbg("[rs_set_ctx], phyId[%u], gRsCb[%p]", phyId, gRsCb);
}

/* get current g_rs_cb */
static struct rs_cb *RsGetCurRsCb(void)
{
    for (int i = 0; i < RS_MAX_DEV_NUM; i++) {
        if (gRsCbList[i] != NULL) {
            hccp_info("[rs_get_cur_rs_cb], phyId[%u], rsCb[%p]", i, gRsCbList[i]);
            return gRsCbList[i];
        }
    }
    return NULL;
}

struct opcode_interface_info gInterfaceInfoList[] = {
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
    {RA_RS_TYPICAL_MR_REG_V1, 2},
    {RA_RS_TYPICAL_MR_REG, 1},
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
    {RA_RS_GET_HCCN_CFG, 1},
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

RS_ATTRI_VISI_DEF void RsGetCurTime(struct timeval *time)
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

RS_ATTRI_VISI_DEF void HccpTimeInterval(struct timeval *endTime, struct timeval *startTime, float *msec)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(endTime);
    RS_CHECK_POINTER_NULL_RETURN_VOID(startTime);
    RS_CHECK_POINTER_NULL_RETURN_VOID(msec);

    /* if low position is sufficient, then borrow one from the high position */
    if (endTime->tv_usec < startTime->tv_usec) {
        endTime->tv_sec -= 1;
        endTime->tv_usec += MS_PER_SECOND_I * MS_PER_SECOND_I;
    }

    *msec = (float)((endTime->tv_sec - startTime->tv_sec) * MS_PER_SECOND_F +
            (endTime->tv_usec - startTime->tv_usec) / US_PER_MS_F);

    return;
}

RS_ATTRI_VISI_DEF void RsHeartbeatAlivePrint(struct rs_pthread_info *pthreadInfo)
{
    float timeCost = 0.0;
    struct timeval now;

    if (pthreadInfo == NULL) {
        hccp_err("pthread_info is NULL!");
        return;
    }

    RsGetCurTime(&now);
    HccpTimeInterval(&now, &pthreadInfo->last_check_time, &timeCost);
    if (timeCost >= RS_HEARTBEAT_TIME || timeCost <= 0) {
        hccp_info("pthread[%s] is alive!", pthreadInfo->pthread_name);
        RsGetCurTime(&pthreadInfo->last_check_time);
    }

    return;
}

int RsDev2rscb(uint32_t chipId, struct rs_cb **rsCb, bool initFlag)
{
    if (gRsCb == NULL) {
        if (initFlag == false) {
            hccp_warn("No device initialized !");
        }
        return -ENODEV;
    }

    if (chipId == gRsCb->chip_id) {
        *rsCb = gRsCb;
        return 0;
    }

    hccp_warn("get rs cb unsuccessful for dev %u !", chipId);
    *rsCb = NULL;

    return -ENODEV;
}

int RsGetHccpMode(unsigned int chipId)
{
    struct rs_cb *rsCb = NULL;
    int ret;

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed(%d)", ret), ret);
    return (int)rsCb->hccp_mode;
}

int RsDev2conncb(uint32_t chipId, struct rs_conn_cb **connCb)
{
    int ret;
    struct rs_cb *rsCb = NULL;

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed(%d)", ret), ret);

    *connCb = &(rsCb->conn_cb);

    return 0;
}

int RsGetRdevCb(struct rs_cb *rsCb, unsigned int rdevIndex, struct rs_rdev_cb **rdevCb)
{
    struct rs_rdev_cb *rdevCbTmp = NULL;
    struct rs_rdev_cb *rdevCbTmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(rdevCbTmp, rdevCbTmp2, &rsCb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdevCbTmp->list) != &rsCb->rdev_list;
        rdevCbTmp = rdevCbTmp2, rdevCbTmp2 = list_entry(rdevCbTmp2->list.next, struct rs_rdev_cb, list)) {
        if (rdevCbTmp->rdev_index == rdevIndex) {
            *rdevCb = rdevCbTmp;
            return 0;
        }
    }

    *rdevCb = NULL;
    hccp_err("rdev_cb for rdev_index[%u] do not available!", rdevIndex);

    return -ENODEV;
}

int RsRdev2rdevCb(unsigned int chipId, unsigned int rdevIndex, struct rs_rdev_cb **rdevCb)
{
    int ret;
    struct rs_cb *rsCb = NULL;

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed for chip_id:%u, ret:%d", chipId, ret), -ENODEV);

    ret = RsGetRdevCb(rsCb, rdevIndex, rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed!, ret %d, rdevIndex %u", ret, rdevIndex), ret);

    return 0;
}

int RsFd2conn(int fd, struct rs_conn_info **conn)
{
    struct rs_cb *rsCb = NULL;
    struct rs_conn_info *connTmp = NULL;
    struct rs_conn_info *connTmp2 = NULL;
    struct rs_list_head *head = NULL;

    if (gRsCb != NULL) {
        rsCb = gRsCb;
    } else {
        hccp_err("g_rs_cb is NULL");
        return -ENODEV;
    }

    RS_PTHREAD_MUTEX_LOCK(&rsCb->conn_cb.conn_mutex);
    head = &rsCb->conn_cb.server_conn_list;
    RS_LIST_GET_HEAD_ENTRY(connTmp, connTmp2, head, list, struct rs_conn_info);
    for (; &connTmp->list != head;
        connTmp = connTmp2, connTmp2 = list_entry(connTmp2->list.next, struct rs_conn_info, list)) {
        if (connTmp->connfd == fd) {
            *conn = connTmp;
            RS_PTHREAD_MUTEX_ULOCK(&rsCb->conn_cb.conn_mutex);
            return 0;
        }
    }

    head = &rsCb->conn_cb.client_conn_list;
    RS_LIST_GET_HEAD_ENTRY(connTmp, connTmp2, head, list, struct rs_conn_info);
    for (; &connTmp->list != head;
        connTmp = connTmp2, connTmp2 = list_entry(connTmp2->list.next, struct rs_conn_info, list)) {
        if (connTmp->connfd == fd) {
            *conn = connTmp;
            RS_PTHREAD_MUTEX_ULOCK(&rsCb->conn_cb.conn_mutex);
            return 0;
        }
    }

    RS_PTHREAD_MUTEX_ULOCK(&rsCb->conn_cb.conn_mutex);

    hccp_warn("cannot find conn node for fd:%d!", fd);
    *conn = NULL;

    return -ENODEV;
}

STATIC int RsPthreadMutexInit(struct rs_cb *rscb, struct rs_init_config *cfg)
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

STATIC int RsGetChipLogicId(unsigned int chipId, enum network_mode hccpMode, unsigned int *logicId)
{
    int ret = 0;

    // other modes skip
    if (hccpMode != NETWORK_OFFLINE) {
        return 0;
    }

    ret = DlDrvDeviceGetIndexByPhyId(chipId, logicId);
    CHK_PRT_RETURN(ret != 0, hccp_err("hal get logic_id failed, chipId[%u], ret[%d]", chipId, ret), -ENODEV);

    return 0;
}

STATIC int RsInitRscbCfg(struct rs_cb *rscb)
{
    struct timeval start, end;
    float timeCost = 0.0;
    int ret;

    ret = RsGetChipLogicId(rscb->chip_id, rscb->hccp_mode, &rscb->logic_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_chip_logic_id failed, ret[%d]", ret), ret);

#ifdef CONFIG_SSL
    ret = rs_ssl_init(rscb);
    if (ret != 0) {
        hccp_err("init ssl failed, ret[%d]", ret);
        goto ssl_init_err;
    }
#endif

    RsGetCurTime(&start);
    ret = RsEpollConnectHandleInit(rscb);
    if (ret != 0) {
        hccp_err("create pthread failed, ret[%d]", ret);
        goto create_pthread_err;
    }

    RsGetCurTime(&end);
    HccpTimeInterval(&end, &start, &timeCost);
    hccp_info("rs_epoll_connect_handle_init ok cost [%f] ms", timeCost);
    return 0;

create_pthread_err:
#ifdef CONFIG_SSL
    rs_ssl_deinit(rscb);
ssl_init_err:
#endif
    return ret;
}

STATIC void RsDeinitRscbCfg(struct rs_cb *rscb)
{
    int tryAgain = RS_TRY_TIME;
    eventfd_t event = 1;
    int ret;

#ifdef CONFIG_SSL
    rs_ssl_deinit(rscb);
#endif

    // deinit resources in rs_epoll_connect_handle_init
    // deinit epoll thread, send event to eventfd to waking up epoll handle thread
    ret = (int)write(rscb->conn_cb.eventfd, &event, sizeof(eventfd_t));
    if (ret != sizeof(eventfd_t)) {
        hccp_warn("eventfd_write unsuccessful(0x%x), chipId:%u, errno:%d", ret, rscb->chip_id, errno);
    }
    while (((rscb->state & RS_STATE_HALT) == 0) && (tryAgain != 0)) {
        usleep(RS_USLEEP_TIME);
        tryAgain--;
    };
    if (tryAgain == 0) {
        hccp_warn("try_again exhausted, epoll thread quit unsuccessful, rscb state:%u", rscb->state);
    }
    rscb->state &= ~RS_STATE_HALT;

    // deinit connect thread, already been RS_CONN_EXIT_FLAG, no need to change conn_flag
    if (rscb->conn_flag != RS_CONN_EXIT_FLAG) {
        rscb->conn_flag = 0;
    }
    tryAgain = RS_TRY_TIME;
    while ((rscb->conn_flag != RS_CONN_EXIT_FLAG) && (tryAgain != 0)) {
        usleep(RS_USLEEP_TIME);
        tryAgain--;
    }
    if (tryAgain == 0) {
        hccp_warn("try_again exhausted, connect thread quit unsuccessful, rscb conn_flag:%d", rscb->conn_flag);
    }

    RsDestroyEpoll(rscb);
}

RS_ATTRI_VISI_DEF int RsInit(struct rs_init_config *cfg)
{
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(cfg);
    ret = DlHalInit();
    if (ret != 0) {
        hccp_err("[init][rs_init]dl_hal_init failed, ret = %d", ret);
        return ret;
    }

    int counter = __sync_fetch_and_add(&(gInitCounter[cfg->chip_id]), 1);
    if (counter > 0) {
        hccp_warn("rs has been init for device %u!", cfg->chip_id);
        return 0;
    }
    ret = RsDev2rscb(cfg->chip_id, &rscb, true);
    CHK_PRT_RETURN(ret == 0, hccp_err("rs_cb exist for device %u! do NOT init it again!", cfg->chip_id), -EEXIST);

    rscb = calloc(1, sizeof(struct rs_cb));
    CHK_PRT_RETURN(rscb == NULL, hccp_err("calloc rscb failed"), -ENOMEM);

    ret = RsPthreadMutexInit(rscb, cfg);
    if (ret != 0) {
        hccp_err("Init mutex failed, ret[%d]", ret);
        goto pthread_mutex_err;
    }

    ret = RsInitRscbCfg(rscb);
    if (ret != 0) {
        hccp_err("rs init rscb configure failed,ret:%d", ret);
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

    gRsCbList[cfg->chip_id] = gRsCb;

    hccp_run_info("rs init success, chipId[%u]", cfg->chip_id);
    return 0;

getifaddrs_err:
    free(rscb->fd_map);
    rscb->fd_map = NULL;

fd_map_err:
    pthread_mutex_destroy(&rscb->mutex);
    pthread_mutex_destroy(&rscb->conn_cb.conn_mutex);
    RsDeinitRscbCfg(rscb);

pthread_mutex_err:
    free(rscb);
    rscb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsGetTlsEnable(unsigned int phyId, bool *tlsEnable)
{
    struct rs_cb *rsCb = NULL;
    int ret;

    CHK_PRT_RETURN(tlsEnable == NULL, hccp_err("param err, tlsEnable is NULL"), -EINVAL);
    ret = RsGetRsCb(phyId, &rsCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phyId(%u) invalid, ret(%d)", phyId, ret), ret);

    *tlsEnable = (rsCb->ssl_enable == 0) ? false : true;
    return 0;
}

RS_ATTRI_VISI_DEF int RsGetHccnCfg(unsigned int phyId, enum hccn_cfg_key key, char *value,
    unsigned int *valueLen)
{
#define HCCN_CFGFILE_PATH "/etc/hccl.cfg"
    const char *keyName[HCCN_CFG_KEY_INVALID] = {"udp_port_mode", "multi_qp_count", "multi_qp_udp_ports"};
    int ret = FILE_OPT_INNER_PARAM_ERR;
    unsigned int valLen = 0;
    unsigned int bufLen;

    CHK_PRT_RETURN(value == NULL || valueLen == NULL, hccp_err("param err, value or valueLen is NULL"), -EINVAL);
    CHK_PRT_RETURN(key >= HCCN_CFG_KEY_INVALID,
        hccp_err("param err, key should < [%d]", HCCN_CFG_KEY_INVALID), -EINVAL);

    bufLen = *valueLen;
    CHK_PRT_RETURN(bufLen < HCCN_CFG_MSG_DATA_LEN,
        hccp_err("param err, bufLen should >= [%d]", HCCN_CFG_MSG_DATA_LEN), -EINVAL);

    *valueLen = 0;
#ifdef CONFIG_TLV
    ret = FileReadCfg(HCCN_CFGFILE_PATH, (int)phyId, keyName[key], value, bufLen);
#endif
    CHK_PRT_RETURN(ret == FILE_OPT_INNER_PARAM_ERR || ret == FILE_OPT_SYS_READ_FILE_ERR,
        hccp_run_warn("get hccn cfg file unsuccessful, ret(%d)", ret), -ENOENT);
    CHK_PRT_RETURN(ret == FILE_OPT_NO_MEM_ERR,
        hccp_err("value_len > buf_len[%d], ret(%d)", bufLen, ret), -ENOMEM);
    CHK_PRT_RETURN(ret != 0, hccp_run_warn("get hccn cfg [%s] unsuccessful, ret(%d)",
        keyName[key], ret), 0);

    valLen = (unsigned int)strlen(value);
    *valueLen = (valLen == 0) ? valLen : (valLen + 1);
    return 0;
}

RS_ATTRI_VISI_DEF int RsBindHostpid(unsigned int chipId, pid_t pid)
{
#define QUERY_BIND_HOST_PID_TIME_US 10000
#define QUERY_BIND_HOST_PID_CNT 12000
    struct rs_cb *rsCb = NULL;
    unsigned int hostPid;
    pid_t devPid;
    int ret;
    int i;

    // get current hccp pid on device
    devPid = getpid();
    CHK_PRT_RETURN(devPid < 0, hccp_err("getpid failed, ret:%d errno:%d", devPid, errno), -EINVAL);

    // query corresponding host_pid every 10ms, total timeout cost 120s
    for (i = 0; i < QUERY_BIND_HOST_PID_CNT; i++) {
        ret = DlDrvQueryProcessHostPid(devPid, NULL, NULL, &hostPid, NULL);
        if (ret == DRV_ERROR_NONE) {
            break;
        }

        usleep(QUERY_BIND_HOST_PID_TIME_US);
    }

    if (i >= QUERY_BIND_HOST_PID_CNT) {
        hccp_err("query process host_pid failed, i:%d >= %d ret:%d", i, QUERY_BIND_HOST_PID_CNT, ret);
        return -EINVAL;
    }

    if (pid != (pid_t)hostPid) {
        hccp_err("check process failed, pid from tsd: %d, process hostPid: %u", pid, hostPid);
        return -EINVAL;
    }

    hccp_dbg("dl_drv_query_process_host_pid success, total retry cnt:%d", i);

    // save host_pid for later setup sharemem
    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed, ret:%d, chipId:%u", ret, chipId), -ENODEV);
    rsCb->host_pid = pid;

    return 0;
}

#ifdef CUSTOM_INTERFACE
STATIC int RsSetRscbGrpId(struct rs_cb *rsCb, unsigned int devId)
{
    GrpQueryGroupIdInfo grpQueryOut = {0};
    unsigned int chipId = rsCb->chip_id;
    GrpQueryGroupId grpQueryIn = {0};
    struct MemInfo memInfo = {0};
    unsigned int outLen;
    unsigned int grpId;
    int ret;

    // query grp_name
    ret = DlHalMemGetInfoEx(devId, MEM_INFO_TYPE_SVM_GRP_INFO, &memInfo);
    CHK_PRT_RETURN(ret, hccp_err("dl_hal_mem_get_info_ex failed, ret:%d chipId:%u devId:%u", ret, chipId,
        devId), ret);

    hccp_dbg("query group name success, chipId:%u devId:%u grp_name:%s", chipId, devId, memInfo.grp_info.name);

    // query grp_id
    ret = memcpy_s(&grpQueryIn.grpName, BUFF_GRP_NAME_LEN, &memInfo.grp_info.name, SVM_GRP_NAME_LEN);
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s failed, ret:%d chipId:%u devId:%u", ret, chipId, devId), ret);
    outLen = (unsigned int)sizeof(grpQueryOut);
    ret = DlHalGrpQuery(GRP_QUERY_GROUP_ID, &grpQueryIn, sizeof(grpQueryIn), &grpQueryOut,
        &outLen);
    CHK_PRT_RETURN(ret, hccp_err("dl_hal_grp_query failed, ret:%d chipId:%u devId:%u", ret, chipId, devId), ret);
    grpId = (unsigned int)grpQueryOut.groupId;

    // set grp_id
    rsCb->grp_id = grpId;

    hccp_dbg("query group id success, chipId:%u devId:%u grpId:%u grp_name:%s", chipId, devId, grpId,
        grpQueryIn.grpName);
    return 0;
}

STATIC int RsBindSibling(struct rs_cb *rsCb, int hostPid, unsigned int vfId, unsigned int devId)
{
#define QUERY_BIND_SIBLING_TIME_US 10000
#define QUERY_BIND_SIBLING_CNT 12000
    struct halQueryDevpidInfo pidInfo = {0};
    pid_t aicpuPid;
    int ret;
    int i;

    // query aicpu pid
    pidInfo.hostpid = hostPid;
    pidInfo.devid = devId;
    pidInfo.proc_type = DEVDRV_PROCESS_CP1;
    ret = DlHalQueryDevPid(pidInfo, &aicpuPid);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_query_dev_pid failed, ret:%d devId:%u", ret, devId), ret);

    // try to bind sibling every 10ms, total timeout cost 120s
    for (i = 0; i < QUERY_BIND_SIBLING_CNT; i++) {
        ret = DlHalMemBindSibling(hostPid, aicpuPid, vfId, devId, SVM_MEM_BIND_SP_GRP);
        if (ret == DRV_ERROR_NONE) {
            break;
        }

        usleep(QUERY_BIND_SIBLING_TIME_US);
    }

    if (i >= QUERY_BIND_SIBLING_CNT) {
        hccp_err("bind sibling to setup sharemem failed, i:%d >= %d ret:%d", i, QUERY_BIND_SIBLING_CNT, ret);
        return -EINVAL;
    }

    rsCb->aicpu_pid = aicpuPid;
    hccp_dbg("dl_hal_mem_bind_sibling success, total retry cnt:%d", i);

    return 0;
}

int RsSetupSharemem(struct rs_cb *rsCb, bool backupFlag, unsigned int backupPhyid)
{
    unsigned int chipId = rsCb->chip_id;
    pid_t pid = rsCb->host_pid;
    int64_t deviceInfo = 0;
    unsigned int logicId;
    int ret;

    // setup sharemem or skipped already, no need to setup again
    if (rsCb->grp_setup_flag) {
        hccp_dbg("grp_setup_flag:%d grp_id:%u chip_id:%u", rsCb->grp_setup_flag, rsCb->grp_id, chipId);
        return 0;
    }

    ret = DlDrvDeviceGetIndexByPhyId(chipId, &logicId);
    CHK_PRT_RETURN(ret, hccp_err("dl_drv_device_get_index_by_phy_id failed, ret:%d chipId:%u", ret, chipId), ret);
    ret = DlHalGetDeviceInfo(logicId, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &deviceInfo);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_get_device_info failed, ret:%d logicId:%u chipId:%u",
        ret, logicId, chipId), ret);
    // not 910b/910_93 and not protocol udma, skip to setup share mem
    if (DlHalPlatGetChip((uint64_t)deviceInfo) != CHIP_TYPE_910B_910_93) {
        hccp_info("logic_id:%u chip_id:%u protocol:%d skip to setup share mem", logicId, chipId, rsCb->protocol);
        rsCb->grp_setup_flag = true;
        return 0;
    }

    // use backup info to setup share mem
    if (backupFlag) {
        ret = rsGetLocalDevIDByHostDevID(backupPhyid, &logicId);
        CHK_PRT_RETURN(ret != 0, hccp_err("rsGetLocalDevIDByHostDevID failed, phyId(%u), ret(%d)",
            backupPhyid, ret), ret);
        hccp_dbg("setup sharemem with backup, phyId:%u logicId:%u", backupPhyid, logicId);
    }

    // bind sibling, default vfid is 0; query & save grp_id on rs_cb
    ret = RsBindSibling(rsCb, pid, 0, logicId);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_bind_sibling failed, ret:%d logicId:%u chipId:%u",
        ret, logicId, chipId), ret);

    // query & save grp_id on rs_cb
    ret = RsSetRscbGrpId(rsCb, logicId);
    CHK_PRT_RETURN(ret, hccp_err("rs_set_rscb_grp_id failed, ret:%d logicId:%u chipId:%u", ret, logicId, chipId),
        ret);

    rsCb->grp_setup_flag = true;
    return 0;
}
#endif

STATIC int RsCompareIpGid(struct rdev rdevInfo, union ibv_gid *gid)
{
    return RsDrvCompareIpGid(rdevInfo.family, rdevInfo.local_ip, gid);
}

int RsQueryGid(struct rdev rdevInfo, struct ibv_context *ibCtxTmp, uint8_t ibPort, int *gidIdx)
{
    static const char *portStates[] = {"Nop", "Down", "Init", "Armed", "", "Active Defer"};
    struct ibv_port_attr attr = {0};
    enum ibv_gid_type_sysfs type;
    union ibv_gid gidTmp;
    int ret;
    int i;

    CHK_PRT_RETURN(gidIdx == NULL, hccp_err("gid_idx is NULL"), -EINVAL);

    ret = RsIbvQueryPort(ibCtxTmp, ibPort, &attr);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_port failed, ret %d ibPort %u", ret, ibPort), -EOPENSRC);

    for (i = 0; i < attr.gid_tbl_len; i++) {
        ret = RsIbvQueryGidType(ibCtxTmp, ibPort, (unsigned int)i, &type);
        CHK_PRT_RETURN(ret, hccp_err("query gid type failed i %d, ret %d", i, ret), -EOPENSRC);
        if (type != IBV_GID_TYPE_SYSFS_ROCE_V2) {
            continue;
        }
        ret = RsIbvQueryGid(ibCtxTmp, ibPort, i, &gidTmp);
        CHK_PRT_RETURN(ret, hccp_err("query gid failed i %d, ret %d", i, ret), -EOPENSRC);
        ret = RsCompareIpGid(rdevInfo, &gidTmp);
        if (ret == 0) {
            CHK_PRT_RETURN(attr.state != IBV_PORT_ACTIVE, hccp_err("port number %u state is %s",
                ibPort, portStates[attr.state]), -ENOLINK);
            *gidIdx = i;
            return 0;
        }
    }

    if (i == attr.gid_tbl_len) {
        return -EEXIST;
    }
    return 0;
}

STATIC int RsGetDevRdevIndex(struct rs_rdev_cb *rdevCb, unsigned int *rdevIndex, int index)
{
#ifdef CUSTOM_INTERFACE
    struct roce_dev_data rdevData = {0};  //lint !e565
    int retVal;
    RS_PTHREAD_MUTEX_LOCK(&rdevCb->rs_cb->mutex);
    /*lint -e132*/
    rdevCb->dev_name = RsIbvGetDeviceName(rdevCb->dev_list[index]);  //lint !e101
    retVal = RsRoceGetRoceDevData(rdevCb->dev_name, &rdevData); //lint !e101
    /*lint +e132*/
    if (retVal) {
        hccp_err("rs_roce_get_roce_dev_data failed, retVal:%d, dev_name:%s", retVal, rdevCb->dev_name);
        RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rs_cb->mutex);
        return retVal;
    }
    *rdevIndex = rdevData.rdev_index; // rdev_index is same to port_id
    rdevCb->rdev_index = *rdevIndex;
    RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rs_cb->mutex);
#endif
    return 0;
}

STATIC int RsGetHostRdevIndex(struct rdev rdevInfo, struct rs_rdev_cb *rdevCb, unsigned int *rdevIndex, int index)
{
    struct rs_rdev_cb *rdevCbTmp2 = NULL;
    struct rs_rdev_cb *rdevCbTmp = NULL;
    unsigned int tmpRdevIndex = 0;

    RS_PTHREAD_MUTEX_LOCK(&rdevCb->rs_cb->mutex);
    rdevCb->dev_name = RsIbvGetDeviceName(rdevCb->dev_list[index]);
    if (rdevCb->dev_name == NULL) {
        hccp_err("rs_ibv_get_device_name failed, errno:%d", errno);
        RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rs_cb->mutex);
        return -EINVAL;
    }

    struct rs_ip_addr_info localIp;
    int ret = RsConvertIpAddr(rdevInfo.family, &rdevInfo.local_ip, &localIp);
    if (ret != 0) {
        hccp_err("convert(ntop) ip failed, ret:%d", ret);
        RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rs_cb->mutex);
        return ret;
    }

    RS_LIST_GET_HEAD_ENTRY(rdevCbTmp, rdevCbTmp2, &rdevCb->rs_cb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdevCbTmp->list) != &rdevCb->rs_cb->rdev_list;
        rdevCbTmp = rdevCbTmp2, rdevCbTmp2 = list_entry(rdevCbTmp2->list.next, struct rs_rdev_cb, list)) {
        tmpRdevIndex = rdevCbTmp->rdev_index;
        if (!RsCompareIpAddr(&rdevCbTmp->local_ip, &localIp)) {
            *rdevIndex = tmpRdevIndex;
            rdevCb->rdev_index = *rdevIndex;
            rdevCb->local_ip = localIp;
            RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rs_cb->mutex);
            return 0;
        }
    }

    *rdevIndex = tmpRdevIndex + 1;
    rdevCb->rdev_index = *rdevIndex;
    rdevCb->local_ip = localIp;
    RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rs_cb->mutex);
    return 0;
}

STATIC int RsGetIbCtxAndRdevIndex(struct rdev rdevInfo, struct rs_rdev_cb *rdevCb, unsigned int *rdevIndex)
{
    struct ibv_context *ibCtxTmp = NULL;
    int gidIndex = -1;
    int ret;
    int i;

    for (i = 0; (i < rdevCb->dev_num) && (rdevCb->dev_list[i] != NULL); ++i) {  //lint !e101
        ibCtxTmp = RsIbvOpenDevice(rdevCb->dev_list[i]);
        CHK_PRT_RETURN(ibCtxTmp == NULL, hccp_err("ibv_open_device failed !"), -ENODEV);
        ret = RsQueryGid(rdevInfo, ibCtxTmp, rdevCb->ib_port, &gidIndex);
        if (ret == 0) {
            if (rdevCb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
                ret = RsGetHostRdevIndex(rdevInfo, rdevCb, rdevIndex, i);
            } else {
                ret = RsGetDevRdevIndex(rdevCb, rdevIndex, i);
            }
            if (ret) {
                hccp_err("get index failed, ret:%d", ret);
                RsIbvCloseDevice(ibCtxTmp);
                return ret;
            }
            rdevCb->ib_ctx = ibCtxTmp;
            return 0;
        } else if (ret == -EEXIST) {
            RsIbvCloseDevice(ibCtxTmp);
        } else {
            RsIbvCloseDevice(ibCtxTmp);
            hccp_err("rs_query_gid failed, ret:%d", ret);
            return ret;
        }
    }

    CHK_PRT_RETURN(i == rdevCb->dev_num, hccp_err("can not find ib_ctx for phy_id[%u] local_ip[0x%x] in dev_list!",
        rdevInfo.phy_id, rdevInfo.local_ip.addr.s_addr), -EEXIST);
    return 0;
}

int RsGetRsCb(unsigned int phyId, struct rs_cb **rsCb)
{
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsDev2rscb(chipId, rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed, ret:%d", ret), -ENODEV);
    return 0;
}

STATIC int RsGetSqDepthAndQpMaxNum(struct rs_rdev_cb *rdevCb, unsigned int rdevIndex)
{
#ifdef CUSTOM_INTERFACE
    int ret;
    unsigned int qpMaxNum = 0;
    unsigned int tempDepth = 0;
    unsigned int sqDepth = 0;

    ret = RsRoceGetTsqpDepth(rdevCb->dev_name, rdevIndex, &tempDepth, &qpMaxNum, &sqDepth);
    CHK_PRT_RETURN(ret, hccp_err("rs_roce_get_tsqp_depth failed, ret:%d, dev_name:%s, rdevIndex:%u", ret,
        rdevCb->dev_name, rdevIndex), ret);

    rdevCb->tx_depth = sqDepth;
    rdevCb->rx_depth = sqDepth;
    rdevCb->qp_max_num = qpMaxNum;
    hccp_run_info("qp_max_num:%u, sqDepth:%u", qpMaxNum, sqDepth);
#endif
    return 0;
}

STATIC int RsSetupPdAndNotify(struct rs_rdev_cb *rdevCb)
{
    int ret;

    ret = RsDrvQueryNotifyAndAllocPd(rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_query_notify_and_alloc_pd failed, ret[%d]", ret), ret);

    ret = RsDrvRegNotifyMr(rdevCb);
    if (ret) {
        hccp_err("reg notify mr failed, ret[%d]", ret);
        goto dealloc_pd;
    }

    return 0;
dealloc_pd:
    RsIbvDeallocPd(rdevCb->ib_pd);
    return ret;
}

STATIC int RsRdevCbInfoInit(struct rdev rdevInfo, struct rs_cb *rsCb, struct rs_rdev_cb *rdevCb)
{
    int ret;

    rdevCb->ib_port = RS_PORT_DEF;
    rdevCb->rs_cb = rsCb;
    rdevCb->notify_va_base = rsCb->notify_va_base;
    rdevCb->notify_size = rsCb->notify_size;

    rdevCb->local_ip.family = (uint32_t)rdevInfo.family;
    rdevCb->local_ip.bin_addr = rdevInfo.local_ip;
    ret = RsInetNtop(rdevInfo.family, &(rdevInfo.local_ip), rdevCb->local_ip.read_addr, RS_MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("rs_inet_ntop failed, ret %d", ret), -EINVAL);

    return 0;
}

STATIC int RsRdevCbInit(struct rdev rdevInfo, struct rs_rdev_cb *rdevCb, struct rs_cb *rsCb,
    unsigned int *rdevIndex)
{
    int ret;

    ret = RsRdevCbInfoInit(rdevInfo, rsCb, rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_rdev_cb_info_init failed, ret %d", ret), ret);

    ret = pthread_mutex_init(&rdevCb->rdev_mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("rdev_cb mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);

    ret = pthread_mutex_init(&rdevCb->cqe_err_cnt_mutex, NULL);
    if (ret) {
        hccp_err("rdev_cb cqe_err_cnt_mutex init failed ret %d!, normal ret 0", ret);
        goto destroy_rdev_mutex;
    }

    RS_PTHREAD_MUTEX_LOCK(&rdevCb->rdev_mutex);
    RS_INIT_LIST_HEAD(&rdevCb->qp_list);
    RS_INIT_LIST_HEAD(&rdevCb->typical_mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rdev_mutex);

    ret = RsGetIbCtxAndRdevIndex(rdevInfo, rdevCb, rdevIndex);
    if (ret) {
        hccp_err("rs_get_ib_ctx_and_rdev_index failed, ret:%d", ret);
        goto destroy_cqe_mutex;
    }

    ret = RsGetSqDepthAndQpMaxNum(rdevCb, *rdevIndex);
    if (ret) {
        hccp_err("rs_get_sq_depth_and_qp_max_num failed, ret[%d], rdevIndex[%u]", ret, *rdevIndex);
        goto close_dev;
    }

#ifdef CUSTOM_INTERFACE
    ret = RsRoceMmapAiDbReg(rdevCb->ib_ctx, (unsigned int)rdevCb->rs_cb->aicpu_pid);
    if (ret) {
        hccp_err("rs_roce_mmap_ai_db_reg failed, ret[%d], rdevIndex[%u]", ret, *rdevIndex);
        goto close_dev;
    }
#endif

    ret = RsSetupPdAndNotify(rdevCb);
    if (ret) {
        hccp_err("rs_get_sq_depth_and_qp_max_num failed, ret[%d], rdevIndex[%u]", ret, *rdevIndex);
        goto unmmap_ai_db;
    }

    return 0;

unmmap_ai_db:
#ifdef CUSTOM_INTERFACE
    (void)RsRoceUnmmapAiDbReg(rdevCb->ib_ctx);
#endif
close_dev:
    RsIbvCloseDevice(rdevCb->ib_ctx);
destroy_cqe_mutex:
    pthread_mutex_destroy(&rdevCb->cqe_err_cnt_mutex);
destroy_rdev_mutex:
    pthread_mutex_destroy(&rdevCb->rdev_mutex);
    return ret;
}

STATIC int RsSensorNodeRegister(unsigned int hccpMode, unsigned int phyId, struct rs_rdev_cb *rdevCb)
{
    struct halSensorNodeCfg cfg = { 0 };
    int ret;

    rdevCb->sensor_handle = 0;
    rdevCb->sensor_update_cnt = 0;
    // some non-hdc scenarios don't have corresponding API, skip to register sensor node
    if (hccpMode != NETWORK_OFFLINE) {
        return 0;
    }

    ret = sprintf_s(cfg.name, sizeof(cfg.name), "roce_rs_%d", getpid());
    if (ret <= 0) {
        hccp_err("[init][rs_rdev]sprintf_s name err, ret:%d, phyId:%u", ret, phyId);
        return -ESAFEFUNC;
    }

    cfg.NodeType = HAL_DMS_DEV_TYPE_HCCP;
    cfg.SensorType = RDMA_CQE_ERR_SENSOR_TYPE;
    cfg.AssertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_MASK;
    cfg.DeassertEventMask = RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE_MASK;
    ret = DlHalSensorNodeRegister(rdevCb->logic_devid, &cfg, &rdevCb->sensor_handle);
    if (ret) {
        hccp_err("[init][rs_rdev]dl_hal_sensor_node_register failed, phyId(%u), logic_devid(%u), ret(%d)",
            phyId, rdevCb->logic_devid, ret);
        return ret;
    }

    return 0;
}

STATIC void RsSensorNodeUnregister(struct rs_rdev_cb *rdevCb)
{
    // no need to unregister sensor node
    if (rdevCb->sensor_handle == 0) {
        return;
    }

    (void)DlHalSensorNodeUnregister(rdevCb->logic_devid, rdevCb->sensor_handle);
}

STATIC int RsRdevInitWithBackupInfo(struct rdev rdevInfo, struct rs_backup_info backupInfo,
    unsigned int notifyType, unsigned int *rdevIndex)
{
    unsigned int phyId = rdevInfo.phy_id;
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_cb *rsCb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(rdevIndex);

    ret = RsApiInit();
    CHK_PRT_RETURN(ret, hccp_err("rs_api_init failed! ret[%d]", ret), ret);

    ret = RsGetRsCb(phyId, &rsCb);
    if (ret) {
        hccp_err("rs_get_rs_cb failed, phyId[%u] invalid, ret %d", phyId, ret);
        goto get_rs_cb_fail;
    }

    rdevCb = calloc(1, sizeof(struct rs_rdev_cb));
    if (rdevCb == NULL) {
        hccp_err("calloc for rdev_cb failed");
        ret = -ENOMEM;
        goto get_rs_cb_fail;
    }

    ret = rsGetLocalDevIDByHostDevID(phyId, &rdevCb->logic_devid);
    if (ret) {
        hccp_err("[init][rs_rdev]rsGetLocalDevIDByHostDevID failed, phyId(%u), ret(%d)", phyId, ret);
        goto free_rs_cb;
    }

    rdevCb->backup_info.backup_flag = backupInfo.backup_flag;
    (void)memcpy_s(&rdevCb->backup_info.rdev_info, sizeof(struct rdev),
        &backupInfo.rdev_info, sizeof(struct rdev));
#ifdef CUSTOM_INTERFACE
    // setup sharemem for aicpu rdma unfold
    ret = RsSetupSharemem(rsCb, rdevCb->backup_info.backup_flag, rdevCb->backup_info.rdev_info.phy_id);
    if (ret != 0) {
        hccp_err("[init][rs_rdev]rs_setup_sharemem failed, phyId(%u), ret(%d)", phyId, ret);
        goto free_rs_cb;
    }
#endif

    rdevCb->notify_type = notifyType;
    rdevCb->dev_list = RsIbvGetDeviceList(&(rdevCb->dev_num));
    if (rdevCb->dev_list == NULL || rdevCb->dev_num == 0) {
        hccp_err("dev_list is NULL, or dev_num[%d] is 0", rdevCb->dev_num);
        ret = -EINVAL;
        goto free_rs_cb;
    }

    ret = RsSensorNodeRegister(rsCb->hccp_mode, phyId, rdevCb);
    if (ret != 0) {
        hccp_err("[init][rs_rdev]rs_sensor_node_register failed, phyId(%u), ret(%d)", phyId, ret);
        goto free_dev_list;
    }

    hccp_info("ibv_get_device_list phy_id[%d] dev_num[%d]", phyId, rdevCb->dev_num);

    ret = RsRdevCbInit(rdevInfo, rdevCb, rsCb, rdevIndex);
    if (ret) {
        RsSensorNodeUnregister(rdevCb);
        hccp_err("rs_rdev_cb_init failed ret %d!, normal ret 0", ret);
        goto free_dev_list;
    }

    RS_PTHREAD_MUTEX_LOCK(&rsCb->mutex);
    RsListAddTail(&rdevCb->list, &rsCb->rdev_list);
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->mutex);

    hccp_run_info("rdev init success, phyId:%u, localIp:0x%x, rdevIndex:%u", phyId, rdevInfo.local_ip.addr.s_addr,
        *rdevIndex);
    return 0;

free_dev_list:
    RsIbvFreeDeviceList(rdevCb->dev_list);
free_rs_cb:
    free(rdevCb);
    rdevCb = NULL;
get_rs_cb_fail:
    RsApiDeinit();
    return ret;
}

RS_ATTRI_VISI_DEF int RsRdevInitWithBackup(struct rdev rdevInfo, struct rdev backupRdevInfo,
    unsigned int notifyType, unsigned int *rdevIndex)
{
    struct rs_backup_info backupInfo = { 0 };

    backupInfo.backup_flag = true;
    (void)memcpy_s(&backupInfo.rdev_info, sizeof(struct rdev), &backupRdevInfo, sizeof(struct rdev));

    return RsRdevInitWithBackupInfo(rdevInfo, backupInfo, notifyType, rdevIndex);
}

RS_ATTRI_VISI_DEF int RsRdevInit(struct rdev rdevInfo, unsigned int notifyType, unsigned int *rdevIndex)
{
    struct rs_backup_info backupInfo = { 0 };

    return RsRdevInitWithBackupInfo(rdevInfo, backupInfo, notifyType, rdevIndex);
}

STATIC void RsDestroyQpList(unsigned int phyId, unsigned int rdevIndex,
    struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb, struct rs_qp_cb *qpCb2)
{
    int ret;

    if (!RsListEmpty(&rdevCb->qp_list)) {
        hccp_warn("qp list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(qpCb, qpCb2, &rdevCb->qp_list, list, struct rs_qp_cb);
        for (; (&qpCb->list) != &rdevCb->qp_list;
            qpCb = qpCb2, qpCb2 = list_entry(qpCb2->list.next, struct rs_qp_cb, list)) {
            hccp_info("qpn[%u] will be destroyed", qpCb->ib_qp->qp_num);
            ret = RsQpDestroy(phyId, rdevIndex, qpCb->ib_qp->qp_num);
            if (ret) {
                hccp_err("rs_qp_destroy failed, ret:%d", ret);
                return;
            }
        }
    }

    return;
}

STATIC void RsFreeTypicalMrCb(struct rs_rdev_cb *devCb)
{
    struct rs_list_head *typicalMrList = &devCb->typical_mr_list;
    struct rs_mr_cb *mrCurr = NULL;
    struct rs_mr_cb *mrNext = NULL;

    RS_PTHREAD_MUTEX_LOCK(&devCb->rdev_mutex);
    RS_LIST_GET_HEAD_ENTRY(mrCurr, mrNext, typicalMrList, list, struct rs_mr_cb);
    for (; (&mrCurr->list) != typicalMrList;
        mrCurr = mrNext, mrNext = list_entry(mrNext->list.next, struct rs_mr_cb, list)) {
        (void)RsDrvMrDereg(mrCurr->ib_mr);
        RsListDel(&mrCurr->list);
        free(mrCurr);
        mrCurr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);

    hccp_info("rs_free_typical_mr_cb is succ");
}

RS_ATTRI_VISI_DEF int RsRdevDeinit(unsigned int phyId, unsigned int notifyType, unsigned int rdevIndex)
{
    int ret;
    unsigned int chipId;
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_qp_cb *qpCb2 = NULL;

    hccp_info("rdev deinit start, phyId:%u, rdevIndex:%u", phyId, rdevIndex);
    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    if (rdevCb->notify_type != NO_USE && rdevCb->notify_mr != NULL) {
        ret = RsDrvMrDereg(rdevCb->notify_mr);
        if (ret) {
            hccp_err("rs_drv_mr_dereg failed, ret %d", ret);
        }
    }

    hccp_info("poll_cqe_num[%d]", rdevCb->poll_cqe_num);

    RsDestroyQpList(phyId, rdevIndex, rdevCb, qpCb, qpCb2);

    RsFreeTypicalMrCb(rdevCb);

#ifdef CUSTOM_INTERFACE
    (void)RsRoceUnmmapAiDbReg(rdevCb->ib_ctx);
#endif

    RsIbvDeallocPd(rdevCb->ib_pd);

    RsIbvCloseDevice(rdevCb->ib_ctx);

#ifdef CUSTOM_INTERFACE
    RsCloseBackupIbCtx(rdevCb);
#endif

    pthread_mutex_destroy(&rdevCb->cqe_err_cnt_mutex);

    pthread_mutex_destroy(&rdevCb->rdev_mutex);

    RsSensorNodeUnregister(rdevCb);

    RsIbvFreeDeviceList(rdevCb->dev_list);

    RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);

    RsListDel(&rdevCb->list);
    free(rdevCb);
    rdevCb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
    RsApiDeinit();
    hccp_run_info("rdev deinit success, phyId:%u, rdevIndex:%u", phyId, rdevIndex);
    return 0;
}

STATIC void RsHeterogTcpFreeFdNode(struct rs_heterog_tcp_fd_info *fdNode)
{
    int fd;

    RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
    fd = fdNode->fd;
    RsListDel(&fdNode->list);
    free(fdNode);
    fdNode = NULL;
    gRsCb->fd_map[fd] = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
}

/*lint -e429 */
RS_ATTRI_VISI_DEF int RsEpollCtlAdd(const void *fdHandle, enum RaEpollEvent event)
{
    struct rs_heterog_tcp_fd_info *fdNode = NULL;
    unsigned int tmpEvent = event;
    int fd = RS_FD_INVALID;
    int ret;

    if (event == RA_EPOLLONESHOT) {
        tmpEvent = EPOLLIN | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmpEvent = EPOLLIN;
    } else {
        hccp_err("unknown event[%u]", tmpEvent);
        return -EINVAL;
    }

    if (gRsCb == NULL) {
        gRsCb = RsGetCurRsCb();
        if (gRsCb == NULL) {
            hccp_err("[rs_epoll_ctl_add]rs_get_cur_rs_cb failed rs_cb(NULL)");
            return -EINVAL;
        }
    }
    tmpEvent = tmpEvent | EPOLLRDHUP;
    fdNode = calloc(1, sizeof(struct rs_heterog_tcp_fd_info));
    CHK_PRT_RETURN(fdNode == NULL, hccp_err("no memory for fd_node"), -ENOMEM);

    fd = ((const struct socket_peer_info *)fdHandle)->fd;
    fdNode->fd = fd;
    RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
    RsListAddTail(&fdNode->list, &gRsCb->heterog_tcp_fd_list);
    gRsCb->fd_map[fd] = fdHandle;
    RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
    ret = RsEpollCtl(gRsCb->conn_cb.epollfd, EPOLL_CTL_ADD, fd, tmpEvent);
    if (ret != 0) {
        hccp_err("[rs_epoll_ctl_add]rs_epoll_ctl failed ret(%d), fd:%d, event:%u", ret, fd, event);
        goto out;
    }
    return 0;
out:
    RsHeterogTcpFreeFdNode(fdNode);
    fdNode = NULL;
    return ret;
}
/*lint +e429 */

RS_ATTRI_VISI_DEF int RsEpollCtlMod(const void *fdHandle, enum RaEpollEvent event)
{
    unsigned int tmpEvent = event;
    int fd = RS_FD_INVALID;
    int ret;

    if (event == RA_EPOLLONESHOT) {
        tmpEvent = EPOLLIN | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmpEvent = EPOLLIN;
    } else {
        hccp_err("unknown event[%u]", event);
        return -EINVAL;
    }

    tmpEvent = tmpEvent | EPOLLRDHUP;
    fd = ((const struct socket_peer_info *)fdHandle)->fd;

    if (gRsCb == NULL) {
        gRsCb = RsGetCurRsCb();
        if (gRsCb == NULL) {
            hccp_err("[rs_epoll_ctl_mod]rs_get_cur_rs_cb failed rs_cb(NULL)");
            return -EINVAL;
        }
    }

    ret = RsEpollCtl(gRsCb->conn_cb.epollfd, EPOLL_CTL_MOD, fd, tmpEvent);
    CHK_PRT_RETURN(ret, hccp_err("[rs_epoll_ctl_mod]rs_epoll_ctl failed ret(%d), fd:%d, event:%u",
        ret, fd, event), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int RsEpollCtlDel(int fd)
{
    int ret;
    struct rs_heterog_tcp_fd_info *fdNode = NULL;
    struct rs_heterog_tcp_fd_info *fdNode1 = NULL;

    if (gRsCb == NULL) {
        gRsCb = RsGetCurRsCb();
        if (gRsCb == NULL) {
            hccp_err("[rs_epoll_ctl_del]rs_get_cur_rs_cb failed rs_cb(NULL)");
            return -EINVAL;
        }
    }
    RS_LIST_GET_HEAD_ENTRY(fdNode, fdNode1, &gRsCb->heterog_tcp_fd_list, list, struct rs_heterog_tcp_fd_info);
    for (; (&fdNode->list) != &gRsCb->heterog_tcp_fd_list;
        fdNode = fdNode1, fdNode1 = list_entry(fdNode1->list.next, struct rs_heterog_tcp_fd_info, list)) {
        if (fdNode->fd == fd) {
            // 删除节点
            RsHeterogTcpFreeFdNode(fdNode);
            fdNode = NULL;
            break; //lint !e108
        }
    }

    // 为了兼容epoll不同版本，这里加EPOLLIN参数
    ret = RsEpollCtl(gRsCb->conn_cb.epollfd, EPOLL_CTL_DEL, fd, EPOLLIN);
    CHK_PRT_RETURN(ret, hccp_err("[rs_epoll_ctl_del]rs_epoll_ctl failed ret(%d), fd:%d", ret, fd), ret);
    return 0;
}

RS_ATTRI_VISI_DEF void RsSetTcpRecvCallback(const void *callback)
{
    if (gRsCb == NULL) {
        hccp_err("param error, gRsCb is NULL");
        return;
    }
    gRsCb->tcp_recv_callback = (void (*)(const void *))callback;
}

STATIC void RsFreeAcceptOneNode(struct rs_cb *rscb, struct rs_accept_info *accept)
{
    int ret;

    ret = RsEpollCtl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, accept->conn_fd, EPOLLIN);
    if (ret) {
        hccp_err("epoll ctl del fd %d failed, ret:%d", accept->conn_fd, ret);
    }

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    RsListDel(&accept->list);
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

STATIC void RsFreeAccpetList(struct rs_cb *rscb)
{
    struct rs_accept_info *accept = NULL;
    struct rs_accept_info *accept2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.server_accept_list)) {
        hccp_warn("Server accept list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(accept, accept2, &rscb->conn_cb.server_accept_list, list, struct rs_accept_info);
        for (; (&accept->list) != &rscb->conn_cb.server_accept_list;
            accept = accept2, accept2 = list_entry(accept2->list.next, struct rs_accept_info, list)) {
            RsFreeAcceptOneNode(rscb, accept);
            accept = NULL;
        }
    }

    return ;
}

STATIC void RsFreeDesignatedAccpetNode(struct rs_cb *rscb, struct rs_ip_addr_info *localIp)
{
    struct rs_accept_info *accept = NULL;
    struct rs_accept_info *accept2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.server_accept_list)) {
        RS_LIST_GET_HEAD_ENTRY(accept, accept2, &rscb->conn_cb.server_accept_list, list, struct rs_accept_info);
        for (; (&accept->list) != &rscb->conn_cb.server_accept_list;
            accept = accept2, accept2 = list_entry(accept2->list.next, struct rs_accept_info, list)) {
            if (!RsCompareIpAddr(&accept->server_ip_addr, localIp)) {
                RsFreeAcceptOneNode(rscb, accept);
                accept = NULL;
            }
        }
    }

    return;
}

STATIC void RsFreeConnOneNode(struct rs_cb *rscb, struct rs_conn_info *conn)
{
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    RsListDel(&conn->list);
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

STATIC void RsFreeClientConnList(struct rs_cb *rscb)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.client_conn_list)) {
        hccp_warn("Client conn node do not empty!");
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.client_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.client_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            RsFreeConnOneNode(rscb, conn);
            conn = NULL;
        }
    }

    return;
}

STATIC void RsFreeDesignatedClientConnNode(struct rs_cb *rscb, struct rs_ip_addr_info *localIp)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.client_conn_list)) {
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.client_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.client_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            if (!RsCompareIpAddr(&conn->client_ip, localIp)) {
                hccp_warn("Client conn node for IP[%s] do not empty!", localIp->read_addr);
                RsFreeConnOneNode(rscb, conn);
                conn = NULL;
            }
        }
    }

    return;
}

STATIC void RsFreeServerConnList(struct rs_cb *rscb)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.server_conn_list)) {
        hccp_warn("Server conn node do not empty!");
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.server_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.server_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            RsFreeConnOneNode(rscb, conn);
            conn = NULL;
        }
    }

    return;
}

STATIC void RsFreeDesignatedServerConnNode(struct rs_cb *rscb, struct rs_ip_addr_info *localIp)
{
    struct rs_conn_info *conn = NULL;
    struct rs_conn_info *conn2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.server_conn_list)) {
        RS_LIST_GET_HEAD_ENTRY(conn, conn2, &rscb->conn_cb.server_conn_list, list, struct rs_conn_info);
        for (; (&conn->list) != &rscb->conn_cb.server_conn_list;
            conn = conn2, conn2 = list_entry(conn2->list.next, struct rs_conn_info, list)) {
            if (!RsCompareIpAddr(&conn->server_ip, localIp)) {
                hccp_warn("Server conn node for IP[%s] do not empty!", localIp->read_addr);
                RsFreeConnOneNode(rscb, conn);
                conn = NULL;
            }
        }
    }
    return;
}

STATIC void RsFreeListenOneNode(struct rs_cb *rscb, struct rs_listen_info *listen)
{
    int ret;

    ret = RsEpollCtl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, listen->listen_fd, EPOLLIN);
    if (ret) {
        hccp_err("delete from epoll failed, ret:%d, epollfd:%d, listen_fd:%d", ret, rscb->conn_cb.epollfd,
            listen->listen_fd);
    }

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    RsListDel(&listen->list);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

    RS_CLOSE_RETRY_FOR_EINTR(ret, listen->listen_fd);

    hccp_info("free Listen IP:%s, port:%d, listen_fd:%d, state:%u",
        listen->server_ip_addr.read_addr, ntohs(listen->sock_port), listen->listen_fd, listen->state);

    listen->listen_fd = RS_FD_INVALID;
    listen->state = RS_CONN_STATE_RESET;

    free(listen);
}

STATIC void RsFreeListenList(struct rs_cb *rscb)
{
    struct rs_listen_info *listen = NULL;
    struct rs_listen_info *listen2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.listen_list)) {
        hccp_warn("Server listen node do not empty!");
        RS_LIST_GET_HEAD_ENTRY(listen, listen2, &rscb->conn_cb.listen_list, list, struct rs_listen_info);
        for (; (&listen->list) != &rscb->conn_cb.listen_list;
            listen = listen2, listen2 = list_entry(listen2->list.next, struct rs_listen_info, list)) {
            RsFreeListenOneNode(rscb, listen);
            listen = NULL;
        }
    }

    return;
}

STATIC void RsFreeDesignatedListenNode(struct rs_cb *rscb, struct rs_ip_addr_info *localIp)
{
    struct rs_listen_info *listen = NULL;
    struct rs_listen_info *listen2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.listen_list)) {
        RS_LIST_GET_HEAD_ENTRY(listen, listen2, &rscb->conn_cb.listen_list, list, struct rs_listen_info);
        for (; (&listen->list) != &rscb->conn_cb.listen_list;
            listen = listen2, listen2 = list_entry(listen2->list.next, struct rs_listen_info, list)) {
            if (!RsCompareIpAddr(&listen->server_ip_addr, localIp)) {
                RsFreeListenOneNode(rscb, listen);
                listen = NULL;
            }
        }
    }

    return;
}

STATIC void RsWhiteListNodeFree(struct rs_cb *rscb, struct rs_white_list *wlist)
{
    struct rs_white_list_info *wlistNode = NULL;
    struct rs_white_list_info *wlistNode1 = NULL;

    if (!RsListEmpty(&wlist->white_list)) {
        RS_LIST_GET_HEAD_ENTRY(wlistNode, wlistNode1, &wlist->white_list, list, struct rs_white_list_info);
        for (; (&wlistNode->list) != &wlist->white_list;
            wlistNode = wlistNode1, wlistNode1 = list_entry(wlistNode1->list.next,
                struct rs_white_list_info, list)) {
            RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
            RsListDel(&wlistNode->list);
            RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

            hccp_info("free White list client IP:%s, tag:%s", wlistNode->client_ip.read_addr, wlistNode->tag);
            free(wlistNode);
            wlistNode = NULL;
        }
    }
}

STATIC void RsFreeWhiteOneNode(struct rs_cb *rscb, struct rs_white_list *wlist)
{
    RsWhiteListNodeFree(rscb, wlist);

    RS_PTHREAD_MUTEX_LOCK(&rscb->conn_cb.conn_mutex);
    RsListDel(&wlist->list);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->conn_cb.conn_mutex);

    hccp_info("White list server IP:%s", wlist->server_ip.read_addr);
    free(wlist);
    wlist = NULL;
}

STATIC void RsFreeWhiteList(struct rs_cb *rscb)
{
    struct rs_white_list *wlist = NULL;
    struct rs_white_list *wlist2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.white_list)) {
        hccp_warn("Server white list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(wlist, wlist2, &rscb->conn_cb.white_list, list, struct rs_white_list);
        for (; (&wlist->list) != &rscb->conn_cb.white_list;
            wlist = wlist2, wlist2 = list_entry(wlist2->list.next, struct rs_white_list, list)) {
            RsFreeWhiteOneNode(rscb, wlist);
            wlist = NULL;
        }
    }

    return;
}

STATIC void RsFreeDesignatedWhiteNode(struct rs_cb *rscb, struct rs_ip_addr_info *localIp)
{
    struct rs_white_list *wlist = NULL;
    struct rs_white_list *wlist2 = NULL;

    if (!RsListEmpty(&rscb->conn_cb.white_list)) {
        RS_LIST_GET_HEAD_ENTRY(wlist, wlist2, &rscb->conn_cb.white_list, list, struct rs_white_list);
        for (; (&wlist->list) != &rscb->conn_cb.white_list;
            wlist = wlist2, wlist2 = list_entry(wlist2->list.next, struct rs_white_list, list)) {
            if (!RsCompareIpAddr(&wlist->server_ip, localIp)) {
                RsFreeWhiteOneNode(rscb, wlist);
                wlist = NULL;
            }
        }
    }

    return;
}

STATIC void RsFreeSocketList(struct rs_cb *rscb, struct rs_ip_addr_info *localIp)
{
    RsFreeDesignatedAccpetNode(rscb, localIp);
    RsFreeDesignatedClientConnNode(rscb, localIp);

    RsFreeDesignatedServerConnNode(rscb, localIp);

    RsFreeDesignatedListenNode(rscb, localIp);

    RsFreeDesignatedWhiteNode(rscb, localIp);

    return ;
}

RS_ATTRI_VISI_DEF int RsSocketDeinit(struct rdev rdevInfo)
{
    int ret;
    unsigned int phyId = rdevInfo.phy_id;
    unsigned int chipId;
    struct rs_cb *rscb = NULL;

    hccp_info("rs socket deinit start, phyId:%u", phyId);
    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    CHK_PRT_RETURN((rdevInfo.family != AF_INET) && (rdevInfo.family != AF_INET6),
        hccp_err("family[%d] invalid", rdevInfo.family), -EPROTONOSUPPORT);

    if (rdevInfo.family == AF_INET) {
        unsigned int *localIp = NULL;
        localIp = &(rdevInfo.local_ip.addr.s_addr);
        ret = RsSocketNodeid2vnic(*localIp, localIp);
        hccp_info("socket deinit local IP is 0x%llx, ret:%d", *localIp, ret);
    }

    struct rs_ip_addr_info localIp;
    RsConvertIpAddr(rdevInfo.family, &rdevInfo.local_ip, &localIp);

    ret = RsDev2rscb(chipId, &rscb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rscb failed for chip_id:%u, ret:%d", chipId, ret), -ENODEV);

    RS_PTHREAD_MUTEX_LOCK(&rscb->mutex);
    RsFreeSocketList(rscb, &localIp);
    RS_PTHREAD_MUTEX_ULOCK(&rscb->mutex);
    hccp_run_info("socket deinit success, phyId:%u, localIp:%s", phyId, localIp.read_addr);
    return 0;
}

STATIC void RsFreeRdevList(struct rs_cb *rsCb)
{
    struct rs_rdev_cb *rdevCbCurr = NULL;
    struct rs_rdev_cb *rdevCbNext = NULL;
    unsigned int phyId = 0;
    int ret;

    ret = rsGetDevIDByLocalDevID(rsCb->chip_id, &phyId);
    if (ret != 0) {
        hccp_err("chip_id[%u] invalid, ret %d", rsCb->chip_id, ret);
        return;
    }

    RS_LIST_GET_HEAD_ENTRY(rdevCbCurr, rdevCbNext, &rsCb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdevCbCurr->list) != &rsCb->rdev_list;
        rdevCbCurr = rdevCbNext, rdevCbNext = list_entry(rdevCbNext->list.next, struct rs_rdev_cb, list)) {
        ret = RsRdevDeinit(phyId, rdevCbCurr->notify_type, rdevCbCurr->rdev_index);
        if (ret != 0) {
            hccp_err("rs_rdev_deinit failed, ret:%d, phyId:%u", ret, phyId);
        }
    }

    return;
}

STATIC void RsFreeUdevList(struct rs_cb *rsCb)
{
    return;
}

STATIC void RsFreeDevList(struct rs_cb *rsCb)
{
    if (RsListEmpty(&rsCb->rdev_list)) {
        return;
    }

    hccp_warn("dev list is not empty!");
    switch (rsCb->protocol) {
        case PROTOCOL_RDMA:
            RsFreeRdevList(rsCb);
            break;
        default:
            hccp_err("protocol[%d] not support", rsCb->protocol);
            break;
    }
    return;
}

STATIC void RsFreeHeterogTcpFdList(struct rs_cb *rsCb)
{
    struct rs_heterog_tcp_fd_info *fdNode = NULL;
    struct rs_heterog_tcp_fd_info *fdNode1 = NULL;

    if (!RsListEmpty(&rsCb->heterog_tcp_fd_list)) {
        hccp_warn("heterog_tcp_fd_list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(fdNode, fdNode1, &rsCb->heterog_tcp_fd_list, list, struct rs_heterog_tcp_fd_info);
        for (; (&fdNode->list) != &rsCb->heterog_tcp_fd_list;
            fdNode = fdNode1, fdNode1 = list_entry(fdNode1->list.next, struct rs_heterog_tcp_fd_info, list)) {
            hccp_info(">>>>>fd_node->fd:%d", fdNode->fd);
            // 删除节点
            RS_PTHREAD_MUTEX_LOCK(&rsCb->mutex);
            RsListDel(&fdNode->list);
            free(fdNode);
            fdNode = NULL;
            RS_PTHREAD_MUTEX_ULOCK(&rsCb->mutex);
        }
    }

    return;
}

STATIC void RsListFree(struct rs_cb *rscb)
{
    RsFreeAccpetList(rscb);
    RsFreeClientConnList(rscb);

    RsFreeServerConnList(rscb);

    RsFreeListenList(rscb);

    RsFreeWhiteList(rscb);

    return ;
}

STATIC void RsSslFree(struct rs_cb *rscb)
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

STATIC void RsDeinitFreeRscb(struct rs_cb *rscb)
{
    RS_PTHREAD_MUTEX_LOCK(&rscb->mutex);
    RsListFree(rscb);

    free(rscb->fd_map);
    rscb->fd_map = NULL;
    freeifaddrs(rscb->ifaddr_list);
    rscb->ifaddr_list = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&rscb->mutex);
    RsFreeDevList(rscb);
    RsSslFree(rscb);
    RsFreeHeterogTcpFdList(rscb);

#ifdef CONFIG_TLV
    if (rscb->nslb_cb.netco_init_flag) {
        RsTlvDeinit(TLV_MODULE_TYPE_NSLB, rscb->nslb_cb.phy_id);
    }
#endif
    pthread_mutex_destroy(&rscb->mutex);
    pthread_mutex_destroy(&rscb->conn_cb.conn_mutex);
    RsDestroyEpoll(rscb);

    free(rscb);
    rscb = NULL;
    gRsCb = NULL;
}

RS_ATTRI_VISI_DEF int RsDeinit(struct rs_init_config *cfg)
{
    int ret;
    eventfd_t event;
    struct rs_cb *rscb = gRsCb;
    unsigned int chipId;

    CHK_PRT_RETURN(cfg == NULL, hccp_err("param error, cfg is NULL"), -EINVAL);

    chipId = cfg->chip_id;
    if (__sync_fetch_and_sub(&(gInitCounter[chipId]), 1) > 1) {
        return 0;
    }
    if (rscb && (chipId == rscb->chip_id)) {
        event = 1;
        /* send event to eventfd to waking up epoll handle thread */
        ret = (int)write(rscb->conn_cb.eventfd, &event, sizeof(eventfd_t));
        CHK_PRT_RETURN(ret != sizeof(eventfd_t), hccp_err("eventfd_write failed(0x%x), chipId:%u, errno:%d",
            ret, chipId, errno), -EFILEOPER);

        hccp_info("epoll wait up ok, rscb->conn_flag:%d", rscb->conn_flag);
        // already been RS_CONN_EXIT_FLAG, no need to change conn_flag
        if (rscb->conn_flag != RS_CONN_EXIT_FLAG) {
            rscb->conn_flag = 0;
        }
        int tryAgain = RS_TRY_TIME;
        while (((rscb->state & RS_STATE_HALT) == 0) && tryAgain > 0) {
            usleep(RS_USLEEP_TIME);
            tryAgain--;
        };

        if (tryAgain == 0) {
            hccp_warn("try_again exhausted, rscb state:%u", rscb->state);
        }

        tryAgain = RS_TRY_TIME;
        while ((rscb->conn_flag != RS_CONN_EXIT_FLAG) && tryAgain > 0) {
            usleep(RS_USLEEP_TIME);
            tryAgain--;
        }

        CHK_PRT_RETURN(tryAgain == 0, hccp_warn("connect thread quit unsuccessful"), -EAGAIN);
        rscb->state &= ~RS_STATE_HALT;
        RsDeinitFreeRscb(rscb);
        gRsCbList[chipId] = NULL;
        DlHalDeinit();

        hccp_run_info("rs_deinit chip_id[%u] ok", chipId);

        return 0;
    }

    DlHalDeinit();
    return -ENODEV;
}

RS_ATTRI_VISI_DEF int RsGetVnicIp(unsigned int phyId, unsigned int *vnicIp)
{
    int64_t deviceInfo = 0;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid", phyId,
        RS_MAX_DEV_NUM), -EINVAL);
    CHK_PRT_RETURN(vnicIp == NULL, hccp_err("vnic_ip is null!"), -EINVAL);

    ret = DlHalGetDeviceInfo(phyId, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &deviceInfo);
    CHK_PRT_RETURN(ret != 0, hccp_err("phy_id:%u dl_hal_get_device_info failed! ret:%d", phyId, ret), ret);

    *vnicIp = (unsigned int)deviceInfo;
    return 0;
}

STATIC int RsGetVnicIpInfo(unsigned int phyId, unsigned int id, enum id_type type, struct ip_info *info)
{
    int64_t deviceInfo = 0;
    unsigned int vnicIp;
    int ret;

    // get vnic ip by id with different type
    if (type == PHY_ID_VNIC_IP) {
        ret = DlHalGetDeviceInfo(id, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &deviceInfo);
        CHK_PRT_RETURN(ret != 0, hccp_err("cur_phy_id:%u dl_hal_get_device_info failed! phy_id:%u ret:%d",
            phyId, id, ret), ret);
    } else if (type == SDID_VNIC_IP) {
        ret = DlHalGetDeviceInfo(id, MODULE_TYPE_SYSTEM, INFO_TYPE_SPOD_VNIC_IP, &deviceInfo);
        CHK_PRT_RETURN(ret != 0, hccp_err("phy_id:%u dl_hal_get_device_info failed! sdid:0x%x ret:%d",
            phyId, id, ret), ret);
    } else {
        hccp_err("phy_id:%u get vnic ip failed! id:0x%x, invalid type:%u", phyId, id, type);
        return -EINVAL;
    }

    // prepare ip info, only support IPv4
    vnicIp = (unsigned int)deviceInfo;
    info->family = AF_INET;
    info->ip.addr.s_addr = vnicIp;

    hccp_dbg("phy_id:%u query id:%u type:%u got vnic_ip:%u", phyId, id, type, vnicIp);

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetVnicIpInfos(unsigned int phyId, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(ids == NULL, hccp_err("phy_id:%u, ids is null!", phyId), -EINVAL);
    CHK_PRT_RETURN(infos == NULL, hccp_err("phy_id:%u, infos is null!", phyId), -EINVAL);

    for (i = 0; i < num; i++) {
        ret = RsGetVnicIpInfo(phyId, ids[i], type, &infos[i]);
        if (ret != 0) {
            hccp_err("phy_id:%u get vnic ip info failed! ids[%u]:0x%x type:%u", phyId, i, ids[i], type);
            return ret;
        }
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetInterfaceVersion(unsigned int opcode, unsigned int *version)
{
    int i;
    unsigned int interfaceVersion = 0; // default interface is 0 (0: not support this interface opcode)
    int num = sizeof(gInterfaceInfoList) / sizeof(gInterfaceInfoList[0]);

    CHK_PRT_RETURN(version == NULL, hccp_err("rs_get_interface_version failed! version is null"), -EINVAL);

    for (i = 0; i < num; i++) {
        if (opcode == gInterfaceInfoList[i].opcode && opcode != RA_RS_GET_ROCE_API_VERSION) {
            interfaceVersion = gInterfaceInfoList[i].version;
            break;
        } else if (opcode == RA_RS_GET_ROCE_API_VERSION) {
            interfaceVersion = RsRoceGetApiVersion();
            break;
        }
    }

    *version = interfaceVersion;
    return 0;
}

int rsGetLocalDevIDByHostDevID(unsigned int phyId, unsigned int *chipId)
{
    CHK_PRT_RETURN(gRsCb == NULL, hccp_warn("No device initialized !"), -ENODEV);

    if (gRsCb->hccp_mode == NETWORK_PEER_ONLINE) {
        *chipId = phyId;
        return 0;
    } else {
        return DlDrvGetLocalDevIdByHostDevId(phyId, chipId);
    }
}

int rsGetDevIDByLocalDevID(unsigned int chipId, unsigned int *phyId)
{
    CHK_PRT_RETURN(gRsCb == NULL, hccp_warn("No device initialized !"), -ENODEV);

    if (gRsCb->hccp_mode == NETWORK_PEER_ONLINE) {
        *phyId = chipId;
        return 0;
    } else {
        return DlDrvGetDevIdByLocalDevId(chipId, phyId);
    }
}

RS_ATTRI_VISI_DEF int RsSetQpAttrQos(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct qos_attr *attr)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    qpCb->qos_attr.tc = attr->tc;
    qpCb->qos_attr.sl = attr->sl;

    hccp_info("set qp qos attr: qpn[%u] tc[%u] sl[%u]", qpn, attr->tc, attr->sl);
    return 0;
}

RS_ATTRI_VISI_DEF int RsSetQpAttrTimeout(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    unsigned int *timeout)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    qpCb->timeout = *timeout;

    hccp_info("set qp qos attr: qpn[%u] timeout[%u]", qpn, *timeout);
    return 0;
}

RS_ATTRI_VISI_DEF int RsSetQpAttrRetryCnt(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    unsigned int *retryCnt)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    qpCb->retry_cnt = *retryCnt;

    hccp_info("set qp qos attr: qpn[%u] retry_cnt[%u]", qpn, *retryCnt);
    return 0;
}

RS_ATTRI_VISI_DEF int RsGetCqeErrInfo(struct cqe_err_info *info)
{
    int ret;

    ret = RsDrvGetCqeErrInfo(info);
    CHK_PRT_RETURN(ret, hccp_err("get failed! ret:%d", ret), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int RsGetCqeErrInfoNum(unsigned int phyId, unsigned int rdevIdx, unsigned int *num)
{
    struct rs_rdev_cb *rdevCb = NULL;
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs get cqe err param error, phyId[%u]", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIdx, &rdevCb);
    CHK_PRT_RETURN(ret != 0 || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    *num = rdevCb->cqe_err_cnt;

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetCqeErrInfoList(unsigned int phyId, unsigned int rdevIdx, struct cqe_err_info *info,
    unsigned int *num)
{
    struct rs_qp_cb *qpCbCurr = NULL;
    struct rs_qp_cb *qpCbNext = NULL;
    struct rs_rdev_cb *rdevCb = NULL;
    unsigned int cqeErrIdx = 0;
    unsigned int numTmp = *num;
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs get cqe err param error, phyId[%u]", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIdx, &rdevCb);
    CHK_PRT_RETURN(ret != 0 || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    if (RsListEmpty(&rdevCb->qp_list)) {
        *num = 0;
        return 0;
    }

    RS_LIST_GET_HEAD_ENTRY(qpCbCurr, qpCbNext, &rdevCb->qp_list, list, struct rs_qp_cb);
    for (; (&qpCbCurr->list) != &rdevCb->qp_list;
        qpCbCurr = qpCbNext, qpCbNext = list_entry(qpCbNext->list.next, struct rs_qp_cb, list)) {
        if (qpCbCurr->cqe_err_info.info.status != 0) {
            RS_PTHREAD_MUTEX_LOCK(&qpCbCurr->cqe_err_info.mutex);
            info[cqeErrIdx].status = qpCbCurr->cqe_err_info.info.status;
            info[cqeErrIdx].qpn = qpCbCurr->cqe_err_info.info.qpn;
            info[cqeErrIdx].time = qpCbCurr->cqe_err_info.info.time;
            qpCbCurr->cqe_err_info.info.status = 0;
            RS_PTHREAD_MUTEX_ULOCK(&qpCbCurr->cqe_err_info.mutex);
            RS_PTHREAD_MUTEX_LOCK(&qpCbCurr->rdev_cb->cqe_err_cnt_mutex);
            qpCbCurr->rdev_cb->cqe_err_cnt--;
            RS_PTHREAD_MUTEX_ULOCK(&qpCbCurr->rdev_cb->cqe_err_cnt_mutex);
            cqeErrIdx++;
            if (cqeErrIdx == numTmp) {
                break;
            }
        }
    }

    *num = cqeErrIdx;

    return 0;
}

RS_ATTRI_VISI_DEF int RsCreateEventHandle(int *eventHandle)
{
    int ret;

    ret = RsEpollCreateEpollfd(eventHandle);
    CHK_PRT_RETURN(ret, hccp_err("[rs_create_event_handle]rs_epoll_create_epollfd failed ret(%d)", ret), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int RsCtlEventHandle(int eventHandle, const void *fdHandle, int opcode,
    enum RaEpollEvent event)
{
    int fd = RS_FD_INVALID;
    unsigned int tmpEvent;
    int ret;

    if (eventHandle < 0) {
        hccp_err("[rs_ctl_event_handle]event_handle[%d] is invalid", eventHandle);
        return -EINVAL;
    }
    if (fdHandle == NULL) {
        hccp_err("[rs_ctl_event_handle]fd_handle is NULL");
        return -EINVAL;
    }
    if (opcode != EPOLL_CTL_ADD && opcode != EPOLL_CTL_DEL && opcode != EPOLL_CTL_MOD) {
        hccp_err("[rs_ctl_event_handle]opcode[%d] invalid, valid opcode includes {%d, %d, %d}",
            opcode, EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD);
        return -EINVAL;
    }

    if (event == RA_EPOLLONESHOT) {
        tmpEvent = EPOLLIN | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmpEvent = EPOLLIN;
    } else if (event == RA_EPOLLOUT) {
        tmpEvent = EPOLLOUT;
    } else if (event == RA_EPOLLOUT_LET_ONESHOT) {
        tmpEvent = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    } else {
        hccp_err("[rs_ctl_event_handle]unknown event[%d]", event);
        return -EINVAL;
    }

    tmpEvent = tmpEvent | EPOLLRDHUP;
    fd = ((const struct socket_peer_info *)fdHandle)->fd;

    ret = RsEpollCtlFdHandle(eventHandle, opcode, fd, tmpEvent, (void*)fdHandle);
    CHK_PRT_RETURN(ret, hccp_err("[rs_ctl_event_handle]rs_epoll_ctl_fd_handle failed ret(%d), fd:%d", ret, fd), ret);
    return 0;
}

RS_ATTRI_VISI_DEF int RsWaitEventHandle(int eventHandle, struct socket_event_info *eventInfos,
    int timeout, unsigned int maxevents, unsigned int *eventsNum)
{
    int ret;

    if (eventHandle < 0) {
        hccp_err("[rs_wait_event_handle]event_handle[%d] is invalid", eventHandle);
        return -EINVAL;
    }

    if (eventInfos == NULL) {
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

    if (eventsNum == NULL) {
        hccp_err("[rs_wait_event_handle]events_num is NULL");
        return -EINVAL;
    }

    ret = RsEpollWaitHandle(eventHandle, (struct epoll_event *)eventInfos,
        timeout, maxevents, eventsNum);
    CHK_PRT_RETURN(ret, hccp_err("[rs_wait_event_handle]rs_epoll_wait_handle failed ret(%d)", ret), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int RsDestroyEventHandle(int *eventHandle)
{
    int ret;

    ret = RsEpollDestroyFd(eventHandle);
    CHK_PRT_RETURN(ret, hccp_err("[rs_destroy_event_handle]rs_epoll_destroy_fd failed ret(%d)", ret), ret);
    return 0;
}

int RsQueryMrCb(struct rs_rdev_cb *devCb, uint64_t addr, struct rs_mr_cb **mrCb,
                   struct rs_list_head *mrList)
{
    struct rs_mr_cb *mrCurr = NULL;
    struct rs_mr_cb *mrNext = NULL;

    RS_PTHREAD_MUTEX_LOCK(&devCb->rdev_mutex);
    RS_LIST_GET_HEAD_ENTRY(mrCurr, mrNext, mrList, list, struct rs_mr_cb);
    for (; (&mrCurr->list) != mrList;
        mrCurr = mrNext, mrNext = list_entry(mrNext->list.next, struct rs_mr_cb, list)) {
        if ((mrCurr->mr_info.addr <= addr) && (addr < mrCurr->mr_info.addr + mrCurr->mr_info.len)) {
            *mrCb = mrCurr;
            RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);
            return 0;
        }
    }

    *mrCb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);

    hccp_info("cannot find mrcb for addr@0x%lx !", addr);

    return -ENODEV;
}

STATIC int RsGetLinuxVersion(struct rs_linux_version_info *verInfo)
{
#define LINUX_VERSION_MAX_CHAR 1024
#define LINUX_VERSION_TYPE_NUM 3
#define LINUX_VERSION_STR "Linux version "
    char buffer[LINUX_VERSION_MAX_CHAR];
    char *versionStr;
    int retClose = 0;
    int ret = 0;
    int fd;

    fd = open("/proc/version", O_RDONLY);
    CHK_PRT_RETURN(fd < 0, hccp_run_warn("open proc/version unsuccessful, errno[%d] fd[%d]", errno, fd), -EFILEOPER);

    do {
        ret = (int)read(fd, buffer, sizeof(buffer));
    } while ((ret < 0) && (errno == EINTR));

    if (ret < 0) {
        hccp_run_warn("read fd unsuccessful[%d]", ret);
        RS_CLOSE_RETRY_FOR_EINTR(retClose, fd);
        return -EFILEOPER;
    }

    versionStr = strstr(buffer, LINUX_VERSION_STR);
    if (versionStr == NULL) {
        hccp_run_warn("can't get Linux version");
        RS_CLOSE_RETRY_FOR_EINTR(retClose, fd);
        return -EFILEOPER;
    }
    versionStr += strlen(LINUX_VERSION_STR);
    if (sscanf_s(versionStr, "%d.%d.%d", &verInfo->major, &verInfo->minor, &verInfo->patch) !=
        LINUX_VERSION_TYPE_NUM) {
        hccp_run_warn("can't extract Linux version");
        RS_CLOSE_RETRY_FOR_EINTR(retClose, fd);
        return -EFILEOPER;
    }

    RS_CLOSE_RETRY_FOR_EINTR(retClose, fd);
    return retClose;
}

RS_ATTRI_VISI_DEF int RsGetSecRandom(unsigned int *value)
{
#define SEC_LINUX_VERSION_MAJOR 5
#define SEC_LINUX_VERSION_MINOR 18
#define SEC_LINUX_VERSION_PATCH 0
    struct rs_linux_version_info verInfo = {0};
    int ret;

    ret = RsGetLinuxVersion(&verInfo);
    CHK_PRT_RETURN(ret, hccp_run_warn("[rs_get_random]get_linux_version unsuccessful ret(%d)", ret), ret);

    // linux_version > 5.18, urandom is secure
    if (verInfo.major > SEC_LINUX_VERSION_MAJOR || (verInfo.major == SEC_LINUX_VERSION_MAJOR &&
        verInfo.minor > SEC_LINUX_VERSION_MINOR) || (verInfo.major == SEC_LINUX_VERSION_MAJOR &&
        verInfo.minor == SEC_LINUX_VERSION_MINOR && verInfo.patch > SEC_LINUX_VERSION_PATCH)) {
        ret = RsDrvGetRandomNum((int *)value);
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
