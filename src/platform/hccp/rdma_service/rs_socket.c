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
#include <unistd.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/tcp.h>
#include "user_log.h"
#include "rs_tls.h"
#include "ssl_adp.h"
#include "securec.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_epoll.h"
#include "rs_inner.h"
#include "ascend_hal.h"
#include "dl_hal_function.h"
#include "rs_socket.h"

static unsigned int gVnics[RS_VNIC_MAX] = {0};

int RsInetNtop(int family, union hccp_ip_addr *ip, char readAddr[], unsigned int len)
{
    // IPv4/IPv6 二进制转字符串
    const char *str = NULL;
    str = inet_ntop(family, ip, readAddr, len);
    CHK_PRT_RETURN(str == NULL, hccp_err("[rs][inet_ntop]ip is a invalid, err(%d), family %d", errno, family), -EINVAL);
    return 0;
}

int RsConvertIpAddr(int family, union hccp_ip_addr *ipAddr, struct rs_ip_addr_info *ip)
{
    // IPv4/IPv6 二进制转内部IP数据格式（含二进制、字符串）
    ip->family = (uint32_t)family;
    ip->bin_addr = *ipAddr;
    return RsInetNtop((int)ip->family, &ip->bin_addr, (char*)&ip->read_addr, sizeof(ip->read_addr));
}

void ShowConnNode(struct rs_list_head *listHead)
{
    struct rs_conn_info *connTmp = NULL;
    struct rs_conn_info *connTmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(connTmp, connTmp2, listHead, list, struct rs_conn_info);  //lint !e613
    for (; (&connTmp->list) != listHead;    //lint !e613
        connTmp = connTmp2, connTmp2 = list_entry(connTmp2->list.next, struct rs_conn_info, list)) { //lint !e453
        hccp_info("current server ip: %s, client ip:%s, fd:%d, state:%d, tag:%s", connTmp->server_ip.read_addr,
            connTmp->client_ip.read_addr, connTmp->connfd, connTmp->state, connTmp->tag); //lint !e453
    }
}

// return: true(IP不同), false(IP相同)
bool RsCompareIpAddr(struct rs_ip_addr_info *a, struct rs_ip_addr_info *b)
{
    if (a->family != b->family) {
        return true;
    }
    if (a->family == AF_INET) {
        return (a->bin_addr.addr.s_addr != b->bin_addr.addr.s_addr);
    } else {
        return memcmp(&a->bin_addr.addr6, &b->bin_addr.addr6, sizeof(b->bin_addr.addr6));
    }
}

STATIC int RsFindListenNode(struct rs_conn_cb *connCb, struct rs_ip_addr_info *ipAddr, uint32_t serverPort,
    struct rs_listen_info **listenInfo)
{
    struct rs_listen_info *listenTmp = NULL;
    struct rs_listen_info *listenTmp2 = NULL;

    RS_CHECK_POINTER_NULL_WITH_RET(connCb);
    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    RS_LIST_GET_HEAD_ENTRY(listenTmp, listenTmp2, &connCb->listen_list, list, struct rs_listen_info);
    for (; (&listenTmp->list) != &connCb->listen_list;
        listenTmp = listenTmp2, listenTmp2 = list_entry(listenTmp2->list.next, struct rs_listen_info, list)) {
        if ((!RsCompareIpAddr(&listenTmp->server_ip_addr, ipAddr)) && (listenTmp->sock_port == serverPort)) {
            *listenInfo = listenTmp;
            RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);

    hccp_info("listen node for IP(%s), serverPort(%u) is not listen!", ipAddr->read_addr, serverPort);
    return -ENODEV;
}

STATIC int RsGetConnInfo(struct rs_conn_cb *connCb, struct socket_connect_info *conn,
    struct rs_conn_info **connInfo, unsigned int serverPort)
{
    int ret;
    struct rs_conn_info *connTmp = NULL;
    struct rs_conn_info *connTmp2 = NULL;
    struct rs_ip_addr_info ipAddr;

    RS_CHECK_POINTER_NULL_RETURN_INT(connCb);
    RS_CHECK_POINTER_NULL_RETURN_INT(conn);

    ret = RsConvertIpAddr(conn->family, &conn->remote_ip, &ipAddr);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    RS_LIST_GET_HEAD_ENTRY(connTmp, connTmp2, &connCb->client_conn_list, list, struct rs_conn_info);
    for (; (&connTmp->list) != &connCb->client_conn_list;
        connTmp = connTmp2, connTmp2 = list_entry(connTmp2->list.next, struct rs_conn_info, list)) {
        if ((!RsCompareIpAddr(&connTmp->server_ip, &ipAddr)) && connTmp->port == serverPort) {
            ret = strcmp(connTmp->tag, conn->tag);
            if (ret == 0) {
                *connInfo = connTmp;
                RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
                return 0;
            }
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);

    conn->tag[SOCK_CONN_TAG_SIZE - 1] = '\0';
    hccp_warn("conn node for IP(%s) server_port(%u) tag(%s) not found", ipAddr.read_addr, serverPort, conn->tag);
    return -ENODEV;
}

STATIC int RsListenCreditLimitInit(struct rs_listen_info *listenInfo)
{
    int ret;

    ret = pthread_mutex_init(&listenInfo->accept_credit_mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("mutex_init accept_credit_mutex failed, ret:%d", ret), -ESYSFUNC);
    return 0;
}

STATIC void RsListenCreditLimitDeinit(struct rs_listen_info *listenInfo)
{
    (void)pthread_mutex_destroy(&listenInfo->accept_credit_mutex);
}

STATIC int RsListenNodeAlloc(struct rs_conn_cb *connCb, struct rs_ip_addr_info *ipAddr, uint32_t serverPort,
    struct rs_listen_info **node)
{
    int ret;
    struct rs_listen_info *listenInfo = NULL;

    ret = RsFindListenNode(connCb, ipAddr, serverPort, &listenInfo);
    CHK_PRT_RETURN(ret == 0,
        hccp_info("listen node for IP(%s) exist! state:%u", ipAddr->read_addr, listenInfo->state), -EEXIST);

    listenInfo = calloc(1, sizeof(struct rs_listen_info));
    CHK_PRT_RETURN(listenInfo == NULL, hccp_err("alloc mem for socket listen info failed!"), -ENOMEM);

    hccp_info("create listen node for IP(%s)!", ipAddr->read_addr);
    listenInfo->server_ip_addr = *ipAddr;
    listenInfo->state = RS_CONN_STATE_RESET;
    ret = RsListenCreditLimitInit(listenInfo);
    if (ret != 0) {
        hccp_err("rs_listen_credit_limit_init failed, ret:%d", ret);
        free(listenInfo);
        listenInfo = NULL;
        return ret;
    }

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    RsListAddTail(&listenInfo->list, &connCb->listen_list);
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);

    *node = listenInfo;

    return 0;
}

STATIC void RsListenNodeFree(struct rs_conn_cb *connCb, struct rs_listen_info *node)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(connCb);
    RS_CHECK_POINTER_NULL_RETURN_VOID(node);

    hccp_dbg("delete listen node for (IP %s : port %u)!", node->server_ip_addr.read_addr, node->sock_port);

    RS_PTHREAD_MUTEX_LOCK(&connCb->rscb->mutex);
    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    RsListDel(&node->list);
    RsListenCreditLimitDeinit(node);
    free(node);
    node = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    RS_PTHREAD_MUTEX_ULOCK(&connCb->rscb->mutex);

    return;
}

int RsAllocConnNode(struct rs_conn_info **conn, unsigned short serverPort)
{
    struct rs_conn_info *connInfo;

    connInfo = calloc(1, sizeof(struct rs_conn_info));
    CHK_PRT_RETURN(connInfo == NULL, hccp_err("alloc mem for socket conn info failed!"), -ENOMEM);

    connInfo->port = serverPort;
    connInfo->connfd = RS_FD_INVALID;
    connInfo->state = RS_CONN_STATE_RESET;

    *conn = connInfo;

    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketInit(const unsigned int *vnicIp, unsigned int num)
{
    int ret;

    // vnic_ip max num  is RA_MAX_VNIC_NUM(16) RS_MAX_VNIC_NUM is also 16
    CHK_PRT_RETURN(num > RS_MAX_VNIC_NUM || num == 0 || vnicIp == NULL,
        hccp_err("param error, num:%u is 0 or bigger than %d, or vnicIp is NULL", num, RS_MAX_VNIC_NUM), -EINVAL);

    ret = memcpy_s(&(gVnics), sizeof(gVnics), vnicIp, sizeof(unsigned int) * num);
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s for vnic_ip failed ret[%d]", ret), -ESAFEFUNC);

    return 0;
}

int RsSocketNodeid2vnic(uint32_t nodeId, uint32_t *ipAddr)
{
    if (nodeId >= RS_VNIC_MAX) {
        return -1; /* it means real nic */
    }

    CHK_PRT_RETURN(ipAddr == NULL, hccp_err("ip_addr is NULL, invalid"), -EINVAL);

    *ipAddr = gVnics[nodeId];

    return RS_VNIC_FLAG;
}

STATIC uint32_t RsSocketVnic2nodeid(uint32_t ipAddr)
{
    uint32_t nodeId;

    if (ipAddr < RS_VNIC_MAX) { /* ip_addr is actually dev_id for vnic */
        return ipAddr;
    }

    for (nodeId = 0; nodeId < RS_VNIC_MAX; nodeId++) {
        if (gVnics[nodeId] == ipAddr) {
            break;
        }
    }

    if (nodeId == RS_VNIC_MAX) {
        return ipAddr;
    }

    return nodeId; /* it means virtual nic */
}

STATIC int RsFindWhiteListNode(struct rs_white_list *rsSocketWhiteList,
    struct socket_wlist_info_t *whiteListExpect, int family, struct rs_white_list_info **whiteListNode)
{
    int ret;
    struct rs_white_list_info *whiteListTmp = NULL;
    struct rs_white_list_info *whiteListTmp2 = NULL;

    struct rs_ip_addr_info expectIp;
    ret = RsConvertIpAddr(family, &whiteListExpect->remote_ip, &expectIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

    RS_CHECK_POINTER_NULL_WITH_RET(rsSocketWhiteList);
    RS_LIST_GET_HEAD_ENTRY(whiteListTmp, whiteListTmp2, &rsSocketWhiteList->white_list, list,
        struct rs_white_list_info);
    for (; (&whiteListTmp->list) != &rsSocketWhiteList->white_list;
        whiteListTmp = whiteListTmp2, whiteListTmp2 = list_entry(whiteListTmp2->list.next,
        struct rs_white_list_info, list)) {
        hccp_info("client_ip %s 0x%08x, expectIp %s 0x%08x",
            whiteListTmp->client_ip.read_addr, whiteListTmp->client_ip.bin_addr.addr.s_addr,
            expectIp.read_addr, expectIp.bin_addr.addr.s_addr);
        if ((!RsCompareIpAddr(&whiteListTmp->client_ip, &expectIp)) &&
            (strncmp(whiteListTmp->tag, whiteListExpect->tag, SOCK_CONN_TAG_SIZE) == 0)) {
            *whiteListNode = whiteListTmp;
            return 0;
        }
    }

    whiteListExpect->tag[SOCK_CONN_TAG_SIZE - 1] = '\0';
    hccp_info("white list node for IP(%s), tag(%s) doesn't exist!", expectIp.read_addr, whiteListExpect->tag);
    return -ENODEV;
}

STATIC int RsFindWhiteList(struct rs_conn_cb *connCb, struct rs_ip_addr_info *serverIp,
    struct rs_white_list **whiteList)
{
    struct rs_white_list *whiteListTmp = NULL;
    struct rs_white_list *whiteListTmp2 = NULL;

    RS_CHECK_POINTER_NULL_WITH_RET(connCb);
    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    RS_LIST_GET_HEAD_ENTRY(whiteListTmp, whiteListTmp2, &connCb->white_list, list, struct rs_white_list);
    for (; (&whiteListTmp->list) != &connCb->white_list;
        whiteListTmp = whiteListTmp2, whiteListTmp2 = list_entry(whiteListTmp2->list.next,
        struct rs_white_list, list)) {
        if (!RsCompareIpAddr(serverIp, &whiteListTmp->server_ip)) {
            *whiteList = whiteListTmp;
            RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);

    hccp_info("white list for IP(%s) doesn't exist!", serverIp->read_addr);
    return -ENODEV;
}

STATIC int RsServerSendWlistCheckResult(struct rs_conn_info *conn, bool flag)
{
    int ret;
    char invalid[] = "5a5a5";
    char valid[] = "a5a5a";

    if (flag == 0) {
        if ((gRsCb->ssl_enable == RS_SSL_ENABLE) && (conn->ssl != NULL)) {
            ret = ssl_adp_write(conn->ssl, valid, sizeof(valid));
        } else {
            ret = RsSocketSend(conn->connfd, valid, sizeof(valid));
        }
        CHK_PRT_RETURN(ret != sizeof(valid), hccp_err("white list server send valid flag failed! fd[%d], ret[%d]",
            conn->connfd, ret), -1);
    } else {
        if ((gRsCb->ssl_enable == RS_SSL_ENABLE) && (conn->ssl != NULL)) {
            ret = ssl_adp_write(conn->ssl, invalid, sizeof(invalid));
        } else {
            ret = RsSocketSend(conn->connfd, invalid, sizeof(invalid));
        }
        CHK_PRT_RETURN(ret != sizeof(invalid), hccp_err("white list server send invalid flag failed! fd[%d], ret[%d]",
            conn->connfd, ret), -1);
    }
    return 0;
}

STATIC void RsSocketGetBindByChip(unsigned int chipId, bool *bindIp)
{
#define CHIP_NAME_910_93 "910_93"
    halChipInfo chipInfo = { 0 };
    int64_t deviceInfo = 0;
    unsigned int logicId;
    int ret;

    // get chip info failed, return directly to avoid exit from batch connect
    ret = DlDrvDeviceGetIndexByPhyId(chipId, &logicId);
    if (ret != 0) {
        hccp_warn("dl_drv_device_get_index_by_phy_id unsuccessful, ret(%d), chipId(%u)", ret, chipId);
        return;
    }
    ret = DlHalGetDeviceInfo(logicId, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &deviceInfo);
    if (ret != 0) {
        hccp_warn("dl_hal_get_device_info unsuccessful, ret(%d), logicId(%u)", ret, logicId);
        return;
    }

    // chip force to bind: 310P & 910_93
    if ((DlHalPlatGetChip((uint64_t)deviceInfo) == CHIP_TYPE_310P) ||
        ((DlHalPlatGetChip((uint64_t)deviceInfo) == CHIP_TYPE_910B_910_93) &&
         (DlHalPlatGetVer((uint64_t)deviceInfo) >= VER_BIN5) &&
         (DlHalPlatGetVer((uint64_t)deviceInfo) <= VER_BIN8))) {
        *bindIp = true;
        return;
    }

    // get chip info, chip force to bind: 910_93
    ret = DlHalGetChipInfo(logicId, &chipInfo);
    if (ret != 0) {
        hccp_warn("dl_hal_get_chip_info unsuccessful, ret(%d), logicId(%u)", ret, logicId);
        return;
    }
    if (strncmp((char *)chipInfo.name, CHIP_NAME_910_93, sizeof(CHIP_NAME_910_93) - 1) == 0) {
        *bindIp = true;
    }

    return;
}

STATIC bool RsSocketIsVnicIp(unsigned int chipId, unsigned int ipAddr)
{
    unsigned int vnicIp = 0;
    int64_t deviceInfo = 0;
    unsigned int phyId = 0;
    bool bindIp = false;
    int hccpMode;
    int ret;

    // no need to handle other mode, only need to handle HDC mode
    hccpMode = RsGetHccpMode(chipId);
    if (hccpMode != NETWORK_OFFLINE) {
        return false;
    }

    // check chip info: 310P & 910_93 will force to bind, no need to compare ip_addr with vnic ip
    RsSocketGetBindByChip(chipId, &bindIp);
    if (bindIp) {
        return false;
    }

    // compare ip_addr with current vnic_ip
    ret = rsGetDevIDByLocalDevID(chipId, &phyId);
    if (ret != 0) {
        hccp_warn("rsGetDevIDByLocalDevID unsuccessful, ret(%d), chipId(%u)", ret, chipId);
        return false;
    }

    ret = DlHalGetDeviceInfo(phyId, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &deviceInfo);
    if (ret != 0) {
        hccp_warn("dl_hal_get_device_info unsuccessful, ret(%d), chipId(%u), phyId(%u)", ret, chipId, phyId);
        return false;
    }

    vnicIp = (unsigned int)deviceInfo;
    hccp_dbg("chip_id:%u phy_id:%u vnic_ip:%u ip_addr:%u", chipId, phyId, vnicIp, ipAddr);
    if (vnicIp == ipAddr) {
        return true;
    }

    return false;
}

STATIC int rs_socket_fill_wlist_by_phyID(unsigned int chipId, struct socket_wlist_info_t *whiteListNode,
    struct rs_conn_info *rsConn)
{
    unsigned int vnicIp = 0;
    int64_t deviceInfo = 0;
    char *tagTemp = NULL;
    unsigned int phyId;
    int ret;

    ret = memcpy_s(whiteListNode->tag, SOCK_CONN_TAG_SIZE, rsConn->tag, SOCK_CONN_TAG_SIZE);
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s failed, ret[%d]", ret), -ESAFEFUNC);

    if (rsConn->client_ip.family == AF_INET) {
        // compare server_ip with current vnic_ip: use client_ip as remote_ip if it has bound or not vnic ip
        if (!RsSocketIsVnicIp(chipId, rsConn->server_ip.bin_addr.addr.s_addr)) {
            // NIC IPv4
            whiteListNode->remote_ip.addr.s_addr = rsConn->client_ip.bin_addr.addr.s_addr;
            return 0;
        }
    } else {
        // NIC IPv6
        whiteListNode->remote_ip = rsConn->client_ip.bin_addr;
        return 0;
    }

    tagTemp = rsConn->tag + SOCK_CONN_TAG_SIZE;
    tagTemp[SOCK_CONN_DEV_ID_SIZE - 1] = '\0';
    RS_CHECK_POINTER_NULL_RETURN_INT(tagTemp);
    if (rsConn->client_ip.family == AF_INET) {
        // VNIC
        phyId = (unsigned int)strtol(tagTemp, NULL, 10); // Decimal(10)
        ret = DlHalGetDeviceInfo(phyId, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &deviceInfo);
        CHK_PRT_RETURN(ret, hccp_err("dl_hal_get_device_info failed, ret(%d) tagTemp phyId(%u)", ret, phyId), ret);
        vnicIp = (unsigned int)deviceInfo;
        hccp_dbg("chip_id:%u phy_id:%u vnic_ip:%u", chipId, phyId, vnicIp);
        whiteListNode->remote_ip.addr.s_addr = vnicIp;
    }
    return 0;
}

STATIC int RsServerValidAsyncInit(unsigned int chipId, struct rs_conn_info *conn,
    struct socket_wlist_info_t *whiteListExpect)
{
    int ret;

    ret = memset_s(whiteListExpect, sizeof(struct socket_wlist_info_t), 0, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memset_s socket_wlist_info_t wlist failed, ret:%d", ret), -ESAFEFUNC);

    CHK_PRT_RETURN(conn->state != RS_CONN_STATE_TAG_SYNC, hccp_err("conn state is not RS_CONN_STATE_TAG_SYNC,"
        "state[%u]. ", conn->state), -1);

    ret = rs_socket_fill_wlist_by_phyID(chipId, whiteListExpect, conn);
    CHK_PRT_RETURN(ret, hccp_err("rs_socket_fill_wlist_by_phyID failed, ret[%d]. ", ret), ret);

    return 0;
}

STATIC int RsServerValidAsync(unsigned int chipId, struct rs_conn_cb *connCb, struct rs_conn_info *conn)
{
    int ret;
    struct rs_white_list *whiteListTmp = NULL;
    struct rs_white_list_info *whiteListNodeTmp = NULL;
    struct socket_wlist_info_t whiteListExpect;

    ret = RsServerValidAsyncInit(chipId, conn, &whiteListExpect);
    CHK_PRT_RETURN(ret, hccp_err("rs server valid async init failed, ret:%d", ret), -1);

    ret = RsFindWhiteList(connCb, &conn->server_ip, &whiteListTmp);
    if (ret) {
        ret = RsServerSendWlistCheckResult(conn, 1);
        CHK_PRT_RETURN(ret, hccp_err("rs server send wlist check invalid result failed, connfd[%d], ret[%d]",
            conn->connfd, ret), -1);
        hccp_info("white list can not be found, connfd[%d], serverIp[%s], ret[%d]", conn->connfd,
            conn->server_ip.read_addr, ret);
        return -1;
    }

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    ret = RsFindWhiteListNode(whiteListTmp, &whiteListExpect, (int)conn->client_ip.family,
        &whiteListNodeTmp);
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    if (ret) {
        ret = RsServerSendWlistCheckResult(conn, 1);
        CHK_PRT_RETURN(ret, hccp_err("rs server send wlist check invalid result failed, connfd[%d], ret[%d]",
            conn->connfd, ret), -1);
        hccp_info("white list node can not be found, connfd[%d], ret[%d]", conn->connfd, ret);
        return -1;
    }

    if (whiteListNodeTmp->conn_limit < 1) {
        ret = RsServerSendWlistCheckResult(conn, 1);
        CHK_PRT_RETURN(ret, hccp_err("rs_server_send_wlist_check_result failed, connfd[%d], conn_limit[%u], ret[%d]",
            conn->connfd, whiteListNodeTmp->conn_limit, ret), -1);
        hccp_info("white list node limit has less than 1, connfd[%d], ret[%d]", conn->connfd, ret);
        return -1;
    }

    ret = RsServerSendWlistCheckResult(conn, 0);
    CHK_PRT_RETURN(ret, hccp_err("rs server send wlist check valid result failed, connfd[%d], ret[%d]",
        conn->connfd, ret), -1);
    whiteListNodeTmp->conn_limit--;
    return 0;
}

int RsSocketCopyConnInfo(struct rs_conn_info *connTmp, struct rs_conn_info *conn)
{
    int ret;

    conn->server_ip = connTmp->server_ip;
    conn->client_ip = connTmp->client_ip;
    conn->connfd = connTmp->connfd;
    conn->state = connTmp->state;
    conn->port = connTmp->port;
    conn->ssl = connTmp->ssl;
    ret = memcpy_s(conn->tag, SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE, connTmp->tag, sizeof(connTmp->tag));
    if (ret) {
        hccp_err("rs_conn_info tag copy failed, ret[%d]", ret);
    }
    conn->is_got = false;
    return ret;
}

int RsWhiteListCheckValid(unsigned int chipId, struct rs_conn_cb *connCb, struct rs_conn_info *conn)
{
    int ret;

    ret = RsServerValidAsync(chipId, connCb, conn);
    if (ret) {
        RS_CLOSE_RETRY_FOR_EINTR(ret, conn->connfd);
        hccp_info("rs_server_valid_async, white list doesn't exist, ret[%d]", ret);
        return -1;
    } else {
        conn->state = RS_CONN_STATE_VALID_SYNC;
    }
    return 0;
}

STATIC int RsSetFdNonblock(int connfd)
{
    int flags, ret;

    flags = fcntl(connfd, F_GETFL, 0);
    CHK_PRT_RETURN(flags < 0, hccp_err("fcntl connfd %d GETFL errno %d flags %d", connfd, errno, flags), -EFILEOPER);

    ret = fcntl(connfd, F_SETFL, (unsigned int)flags | O_NONBLOCK);
    if (ret < 0) {
        ret = -EFILEOPER;
        hccp_err("fcntl connfd %d nonblock errno %d ret %d", connfd, errno, ret);
    }

    return ret;
}

STATIC int RsSocketSetFdTimeoutUsec(int connfd, unsigned int tvUsec)
{
    struct timeval tv = { 0 };
    int ret = 0;

    tv.tv_usec = tvUsec;
    ret = setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    CHK_PRT_RETURN(ret < 0, hccp_err("setsockopt connfd %d SO_SNDTIMEO tv_usec %u failed %d", connfd, tvUsec, ret),
        -EFILEOPER);

    ret = setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    CHK_PRT_RETURN(ret < 0, hccp_err("setsockopt connfd %d SO_RCVTIMEO tv_usec %u failed %d", connfd, tvUsec, ret),
        -EFILEOPER);

    return 0;
}

STATIC void RsEpollEventSslListenInHandle(struct rs_cb *rsCb, struct rs_listen_info *listenInfo, int connfd,
    struct rs_ip_addr_info *remoteIp)
{
    /*lint -e593*/
    int ret;
    struct rs_accept_info *acceptInfo = NULL;
    struct rs_list_head *listHead = NULL;

    ret = RsEpollCtl(rsCb->conn_cb.epollfd, EPOLL_CTL_ADD, connfd, EPOLLIN | EPOLLRDHUP);
    if (ret) {
        hccp_err("epoll ctl add fd %d failed", connfd);
        goto out;
    }

    hccp_info("epoll ctl add fd %d success", connfd);
    acceptInfo = calloc(1, sizeof(struct rs_accept_info));
    if (acceptInfo == NULL) {
        hccp_err("alloc mem for socket conn info failed!");
        goto out;
    }

    acceptInfo->sock_port = listenInfo->sock_port;
    acceptInfo->server_ip_addr = listenInfo->server_ip_addr;
    acceptInfo->client_ip_addr = *remoteIp;
    acceptInfo->conn_fd = connfd;
    RS_PTHREAD_MUTEX_LOCK(&rsCb->conn_cb.conn_mutex);
    listHead = &rsCb->conn_cb.server_accept_list;
    RsListAddTail(&acceptInfo->list, listHead);
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->conn_cb.conn_mutex);

    return;

out:
    RS_CLOSE_RETRY_FOR_EINTR(ret, connfd);
    return;
    /*lint +e593*/
}

STATIC int RsTcpRecvTagInHandle(struct rs_listen_info *listenInfo, int connfd, struct rs_conn_info *connTmp,
    struct rs_ip_addr_info *remoteIp)
{
    int expSize = SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE;
    char *recvBuff = connTmp->tag;
    struct timeval startTime, now;
    float timeCost = 0.0;
    int size = expSize;

    RsGetCurTime(&startTime);
    while (expSize > 0 && size != 0) {
        connTmp->tag_sync_time++;
        size = recv(connfd, recvBuff, expSize, 0);
        if ((size < 0) && (errno == EINTR)) {
            connTmp->tag_eintr_time++;
            continue;
        }
        // peer socket session has been closed
        if (size == 0) {
            hccp_run_info("session has been clsoed, server:{%s:%u} client:%s tag_sync_time:%u tag_eintr_time:%u",
                listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, remoteIp->read_addr,
                connTmp->tag_sync_time, connTmp->tag_eintr_time);
            return -ESOCKCLOSED;
        }

        expSize -= size;
        recvBuff += size;
        RsGetCurTime(&now);
        HccpTimeInterval(&now, &startTime, &timeCost);
        // enlarge the timeout threshold to make sure the connection can be established successfully
        if (timeCost >= RS_RECV_TAG_MAX_TIME) {
            hccp_run_info("recv tag time out, server:{%s:%u} client:%s tag_sync_time:%u tag_eintr_time:%u",
                listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, remoteIp->read_addr,
                connTmp->tag_sync_time, connTmp->tag_eintr_time);
            return -ETIME;
        }

        if (timeCost <= 0) {
            RsGetCurTime(&startTime);
        }
    }

    connTmp->server_ip = listenInfo->server_ip_addr;
    connTmp->client_ip = *remoteIp;
    connTmp->connfd = connfd;
    connTmp->state = RS_CONN_STATE_TAG_SYNC;
    connTmp->port = listenInfo->sock_port;
    if (timeCost >= RS_RECV_MAX_TIME) {
        hccp_run_info("recv tag success, server:{%s:%u} client:%s timeCost:%fms tag_sync_time:%u tag_eintr_time:%u",
            listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, remoteIp->read_addr, timeCost,
            connTmp->tag_sync_time, connTmp->tag_eintr_time);
        return 0;
    }

    hccp_info("recv tag success, server:{%s:%u} client:%s timeCost:%fms tag_sync_time:%u tag_eintr_time:%u",
        listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, remoteIp->read_addr, timeCost,
        connTmp->tag_sync_time, connTmp->tag_eintr_time);
    return 0;
}

STATIC void RsEpollEventTcpListenInHandle(struct rs_cb *rsCb, struct rs_listen_info *listenInfo, int connfd,
    struct rs_ip_addr_info *remoteIp)
{
    struct rs_conn_info connTmp = {0};
    int ret;

    ret = RsTcpRecvTagInHandle(listenInfo, connfd, &connTmp, remoteIp);
    if (ret != 0) {
        hccp_warn("rs_tcp_recv_tag_in_handle unsuccessful, ret:%d", ret);
        RS_CLOSE_RETRY_FOR_EINTR(ret, connfd);
        return;
    }

    ret = RsWlistCheckConnAdd(rsCb, &connTmp);
    if (ret != 0) {
        hccp_warn("rs_wlist_check_conn_add unsuccessful, ret %d", ret);
        return;
    }

    return;
}

void RsSocketSaveErrInfo(int action, int errNo, struct socket_err_info *errInfo)
{
    // Only record the first occurrence of err information
    if (errInfo->err_no != 0) {
        return;
    }

    if (errNo == -EAGAIN || errNo == -EINTR) {
        return;
    }

    RsGetCurTime(&errInfo->time);
    errInfo->action = action;
    errInfo->err_no = errNo;
}

STATIC int RsSocketListenDelFromEpoll(struct rs_conn_cb *connCb, struct rs_listen_info *listenInfo)
{
    int ret = 0;

    RS_PTHREAD_MUTEX_LOCK(&listenInfo->accept_credit_mutex);
    if (listenInfo->fd_state == LISTEN_FD_STATE_DELETED) {
        goto out;
    }

    hccp_run_info("IP:%s server_port:%u listen_fd:%d del from epoll:%d", listenInfo->server_ip_addr.read_addr,
        listenInfo->sock_port, listenInfo->listen_fd, connCb->epollfd);
    ret = RsEpollCtl(connCb->epollfd, EPOLL_CTL_DEL, listenInfo->listen_fd, EPOLLIN);
    if (ret != 0) {
        hccp_err("IP:%s server_port:%u listen_fd:%d rs_epoll_ctl failed, ret:%d errno:%d",
            listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, listenInfo->listen_fd, ret, errno);
        goto out;
    }

    listenInfo->fd_state = LISTEN_FD_STATE_DELETED;

out:
    RS_PTHREAD_MUTEX_ULOCK(&listenInfo->accept_credit_mutex);
    return ret;
}

STATIC int RsSocketCheckCredit(struct rs_conn_cb *connCb, struct rs_listen_info *listenInfo)
{
    // not using accept_credit, no need to check
    if (!listenInfo->accept_credit_flag) {
        return 0;
    }

    // accept_credit is exhaused, check failed
    if (listenInfo->accept_credit_limit == 0) {
        return -EINVAL;
    }

    RS_PTHREAD_MUTEX_LOCK(&listenInfo->accept_credit_mutex);
    listenInfo->accept_credit_limit--;
    RS_PTHREAD_MUTEX_ULOCK(&listenInfo->accept_credit_mutex);

    // accept_credit is exhaused, ignore return value to delete from epoll
    if (listenInfo->accept_credit_limit == 0) {
        (void)RsSocketListenDelFromEpoll(connCb, listenInfo);
    }

    return 0;
}

int RsEpollEventListenInHandle(struct rs_cb *rsCb, int fd)
{
    struct rs_listen_info *listenInfo2 = NULL;
    struct rs_listen_info *listenInfo = NULL;
    struct rs_socketaddr_info remoteSAddr;
    struct rs_ip_addr_info remoteIp;
    int connfd = RS_FD_INVALID;
    int tcpNodelayFlag = 1;
    int ret, retClose;
    socklen_t ipLen;

    /* Server event: Connection accept */
    RS_LIST_GET_HEAD_ENTRY(listenInfo, listenInfo2, &rsCb->conn_cb.listen_list, list, struct rs_listen_info);
    for (; (&listenInfo->list) != &rsCb->conn_cb.listen_list;
        listenInfo = listenInfo2, listenInfo2 = list_entry(listenInfo2->list.next, struct rs_listen_info, list)) {
        /* connection request for Server */
        if (fd == listenInfo->listen_fd) {
            ret = RsSocketCheckCredit(&rsCb->conn_cb, listenInfo);
            CHK_PRT_RETURN(ret != 0,
                hccp_warn("[server]rs_socket_check_credit unsuccessful, serverIp:%s serverPort:%u ret:%d",
                listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, ret), -EINVAL);

            remoteSAddr.family = (int)listenInfo->server_ip_addr.family;
            ipLen = (remoteSAddr.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
            do {
                connfd = accept(fd, (struct sockaddr *)&remoteSAddr.addr, &ipLen);
            } while ((connfd < 0) && (errno == EINTR));

            // accept failed and errno is the same with the last time, avoid log flush
            ret = errno;
            if (connfd < 0 && listenInfo->last_accept_errno == ret) {
                hccp_warn("[server]server_ip:%s server_port:%u accept() unsuccessful! errno:%d",
                    listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, ret);
                return -EINVAL;
            }
            listenInfo->last_accept_errno = ret;

            if (connfd < 0) {
                hccp_err("[server]server_ip:%s server_port:%u accept() failed! errno:%d",
                    listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, ret);
                goto err_accept;
            }

            hccp_info("[server]server_ip:%s server_port:%u accept ok @ listen_fd:%d, new fd:%d",
                listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, fd, connfd);

            remoteIp.family = (uint32_t)remoteSAddr.family;
            if (remoteIp.family == AF_INET) {
                remoteIp.bin_addr.addr = remoteSAddr.addr.s_addr.sin_addr;
            } else {
                remoteIp.bin_addr.addr6 = remoteSAddr.addr.s_addr6.sin6_addr;
            }

            ret = RsInetNtop(remoteIp.family, &remoteIp.bin_addr, remoteIp.read_addr, sizeof(remoteIp.read_addr));
            if (ret) {
                hccp_err("[server]convert(ntop) ip failed, remoteIp.family:%d, remoteIp:%d, ret:%d, serverIp:%s "
                    "serverPort:%u", remoteIp.family, remoteIp.bin_addr.addr.s_addr, ret,
                    listenInfo->server_ip_addr.read_addr, listenInfo->sock_port);
                goto err_event_listen;
            }

            if (rsCb->ssl_enable == RS_SSL_ENABLE) {
                ret = RsSetFdNonblock(connfd);
                if (ret) {
                    hccp_err("[server]fcntl connfd %d nonblock failed %d, serverIp:%s serverPort:%u",
                        connfd, ret, listenInfo->server_ip_addr.read_addr, listenInfo->sock_port);
                    goto err_event_listen;
                }
            }

            /* set tcp socket tos RS_TCP_DSCP_0 */
            int tosLocal = (RS_TCP_DSCP_0 & RS_DSCP_MASK) << RS_DSCP_OFF;
            ret = setsockopt(connfd, IPPROTO_IP, IP_TOS, (void *)&tosLocal, sizeof(tosLocal));
            if (ret) {
                hccp_err("[server]setsockopt(IP_TOS) failed, ret:%d, errno:%d, serverIp:%s serverPort:%u",
                    ret, errno, listenInfo->server_ip_addr.read_addr, listenInfo->sock_port);
                goto err_socket_option;
            }

            ret = setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (void *)&tcpNodelayFlag, sizeof(int));
            if (ret < 0) {
                hccp_err("[server]setsockopt(TCP_NODELAY) failed, ret:%d, errno:%d, serverIp:%s serverPort:%u",
                    ret, errno, listenInfo->server_ip_addr.read_addr, listenInfo->sock_port);
                goto err_socket_option;
            }

            if (rsCb->ssl_enable == RS_SSL_ENABLE) {
                RsEpollEventSslListenInHandle(rsCb, listenInfo, connfd, &remoteIp);
            } else {
                RsEpollEventTcpListenInHandle(rsCb, listenInfo, connfd, &remoteIp);
            }
            return 0;
        }
    }

    return -ENODEV;

err_socket_option:
    ret = -errno;
err_event_listen:
    RS_CLOSE_RETRY_FOR_EINTR(retClose, connfd);
err_accept:
    RsSocketSaveErrInfo((int)listenInfo->state, ret, &listenInfo->err_info);
    return -ESYSFUNC;
}

STATIC int RsSocketListenBindListen(int listenFd, struct rs_conn_cb *connCb,
    struct socket_listen_info *conn, struct rs_listen_info *listenInfo, uint32_t serverPort)
{
    int isReuseAddr = 1;
    int ret, errNo;

    ret = setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &isReuseAddr, sizeof(isReuseAddr));
    if (ret) {
        errNo = errno;
        hccp_err("set socket op failed! IP:%s, port:%u, sock:%d, ret:0x%x, error:%d",
            listenInfo->server_ip_addr.read_addr, serverPort, listenFd, ret, errNo);
        conn->phase = LISTEN_BIND_ERR;
        return -ESYSFUNC;
    }

    listenInfo->state = RS_CONN_STATE_INIT;

    hccp_info("listen state:%d, then bind for (IP %s : port %u)",
        listenInfo->state, listenInfo->server_ip_addr.read_addr, serverPort);

    hccp_run_info("socket bind: family %d, addr %s, port %u", conn->family, listenInfo->server_ip_addr.read_addr,
        serverPort);
    if (conn->family == AF_INET) {
        struct sockaddr_in addr = {0};
        addr.sin_family = conn->family;
        addr.sin_port = htons(serverPort);
        addr.sin_addr.s_addr = listenInfo->server_ip_addr.bin_addr.addr.s_addr;
        hccp_info("socket bind: family %d, port %d, addr 0x%08x", addr.sin_family, addr.sin_port, addr.sin_addr.s_addr);
        ret = bind(listenFd, &addr, sizeof(addr));
    } else {
        struct sockaddr_in6 addr = {0};
        addr.sin6_family = conn->family;
        addr.sin6_port = htons(serverPort);
        addr.sin6_addr = listenInfo->server_ip_addr.bin_addr.addr6;
        addr.sin6_scope_id = (uint32_t)connCb->scope_id;
        hccp_info("socket bind: family %d, port %d, scopeId %d", addr.sin6_family, addr.sin6_port, addr.sin6_scope_id);
        for (unsigned long i = 0; i < sizeof(addr.sin6_addr.s6_addr); i++) {
            hccp_info("socket bind: addr[%lu] 0x%02x", i, addr.sin6_addr.s6_addr[i]);
        }
        ret = bind(listenFd, &addr, sizeof(addr));
    }

    if (ret) {
        errNo = errno;
        if (errNo == EADDRINUSE) {
            hccp_run_warn("bind unsuccessful! family:%d, IP:%s, port:%u, sock:%d, ret:0x%x, error:%d, Possible Cause: "\
                "the IP address and port have been bound already", conn->family, listenInfo->server_ip_addr.read_addr,
                serverPort, listenFd, ret, errNo);
        } else {
            hccp_err("bind failed! family:%d, IP:%s, port:%u, sock:%d, ret:0x%x, error:%d", conn->family,
                listenInfo->server_ip_addr.read_addr, serverPort, listenFd, ret, errNo);
        }
        conn->phase = LISTEN_BIND_ERR;
        return errNo;
    }

    listenInfo->state = RS_CONN_STATE_BIND;

    hccp_info("IP %s : port %u begin listen, fd:%d !", listenInfo->server_ip_addr.read_addr, serverPort, listenFd);
    ret = listen(listenFd, RS_SOCK_LISTEN_PARALLEL_NUM);
    if (ret) {
        errNo = errno;
        if (errNo == EADDRINUSE) {
            hccp_run_warn("listen unsuccessful! IP:%s, port:%u, sock:%d, ret:0x%x, errno:%d",
                listenInfo->server_ip_addr.read_addr, serverPort, listenFd, ret, errNo);
        } else {
            hccp_err("listen failed! IP:%s, port:%u, sock:%d, ret:0x%x, errno:%d",
                listenInfo->server_ip_addr.read_addr, serverPort, listenFd, ret, errNo);
        }
        conn->phase = LISTEN_BEGIN_ERR;
        return errNo;
    }

    return 0;
}

static int RsSocketInitListen(struct socket_listen_info *conn, uint32_t i, struct rs_conn_cb **connCb,
    uint32_t serverPort, struct rs_listen_info **listenInfo)
{
    int ret;
    unsigned int chipId;

    CHK_PRT_RETURN(((conn[i].family != AF_INET) && (conn[i].family != AF_INET6)) || conn[i].phy_id >= RS_MAX_DEV_NUM,
        hccp_err("family[%d] invalid, or phyId[%u] invalid, i:%u", conn[i].family, conn[i].phy_id, i), -EINVAL);

    if (conn[i].family == AF_INET) {
        uint32_t *localIp = NULL;
        localIp = &(conn[i].local_ip.addr.s_addr);
        ret = RsSocketNodeid2vnic(*localIp, localIp);
        hccp_info("listen [%u] IP 0x%llx, ret_vnic %d", i, *localIp, ret);
    }

    ret = rsGetLocalDevIDByHostDevID(conn[i].phy_id, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    ret = RsDev2conncb(chipId, connCb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev failed, ret:%d", ret), ret);

    struct rs_ip_addr_info ipInfo = {0};
    ret = RsConvertIpAddr(conn[i].family, &conn[i].local_ip, &ipInfo);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

    struct rs_listen_info *tmpListenInfo;
    ret = RsFindListenNode(*connCb, &ipInfo, serverPort, &tmpListenInfo);
    if (ret == 0) {
        int counter = __sync_fetch_and_add(&(tmpListenInfo->counter), 1);
        if (counter > 0) {
            return -EEXIST;
        }
    }

    ret = RsListenNodeAlloc(*connCb, &ipInfo, serverPort, listenInfo);
    // listen node found, degrade log level make it consistent with inner call
    if (ret == -EEXIST) {
        hccp_info("alloc listen info node unsuccessful, ret:%d, IP:%s, port:%u", ret, ipInfo.read_addr, serverPort);
    } else if (ret != 0) {
        hccp_err("alloc listen info node failed, ret:%d, IP:%s, port:%u", ret, ipInfo.read_addr, serverPort);
    }
    if (ret != 0) {
        conn[i].err = ENOMEM;
        return ret;
    }

    return 0;
}

static void RsSocketSetConnListenInfo(struct rs_listen_info *listenInfo, int listenFd,
    uint32_t serverPort, struct socket_listen_info *conn)
{
    listenInfo->listen_fd = listenFd;
    listenInfo->sock_port = serverPort;
    listenInfo->state = RS_CONN_STATE_LISTENING;

    if (conn->family == AF_INET) {
        conn->local_ip.addr.s_addr = RsSocketVnic2nodeid(conn->local_ip.addr.s_addr);
    }
    conn->err = 0;
    conn->port = serverPort;
    conn->phase = LISTEN_OK;
}

static void RsSocketHandleListenNodeErr(uint32_t i, struct rs_conn_cb *connCb,
    struct socket_listen_info conn[], uint32_t serverPort)
{
    uint32_t j;
    int ret;
    struct rs_listen_info *listenInfo = NULL;

    for (j = 0; j < i; j++) {
        struct rs_ip_addr_info ipInfo = {0};
        ret = RsConvertIpAddr(conn[j].family, &conn[j].local_ip, &ipInfo);
        if (ret) {
            hccp_err("convert(ntop) ip failed");
            continue;
        }
        ret = RsFindListenNode(connCb, &ipInfo, serverPort, &listenInfo);
        if (ret) {
            hccp_dbg("not find listen node, ret %d", ret);
        } else {
            ret = RsEpollCtl(connCb->epollfd, EPOLL_CTL_DEL, listenInfo->listen_fd, EPOLLIN);
            if (ret) {
                hccp_err("delete from epoll failed, ret:%d, epollfd:%d, listenFd:%d", ret, connCb->epollfd,
                    listenInfo->listen_fd);
            }
            RS_CLOSE_RETRY_FOR_EINTR(ret, listenInfo->listen_fd);
            RsListenNodeFree(connCb, listenInfo);
        }
    }
}

STATIC int RsGetIpv6ScopeId(struct in6_addr localIp)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    struct in6_addr ipv6Addr = {0};
    int scopeId = 0;
    int ret, i;

    ret = getifaddrs(&ifaddr);
    CHK_PRT_RETURN(ret == -1, hccp_err("get ifaddrs failed, ret[%d]", ret), -ESYSFUNC);
    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET6) {
            continue;
        }
        ipv6Addr = ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
        for (i = 0; i < IPV6_S6_ADDR_SIZE; i++) {
            if (ipv6Addr.s6_addr[i] != localIp.s6_addr[i]) {
                break;
            }
        }
        if (i == IPV6_S6_ADDR_SIZE) { /* all 16 u6_addr8 in ipv6 are equal */
            scopeId = (int)((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id;
            freeifaddrs(ifaddr);
            ifaddr = NULL;
            return scopeId;
        }
    }

    hccp_err("get scope id failed");
    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return -EINVAL;
}

RS_ATTRI_VISI_DEF int RsSocketListenStart(struct socket_listen_info conn[], uint32_t num)
{
    struct rs_listen_info *listenInfo = NULL;
    union rs_socketaddr serverAddr = {0};
    struct rs_conn_cb *connCb = NULL;
    socklen_t serverAddrLen = 0;
    unsigned int serverPort = 0;
    int listenFd = 0;
    int scopeId = 0;
    int errNo = 0;
    int ret, flag;
    uint32_t i;

    RS_SOCKET_PARA_CHECK(num, conn);
    if (conn[0].family == AF_INET6) {
        scopeId = RsGetIpv6ScopeId(conn[0].local_ip.addr6);
        CHK_PRT_RETURN(scopeId < 0, hccp_err("scope_id[%d] is invalid", scopeId), -EINVAL);
    }

    for (i = 0; i < num; i++) {
        serverPort = conn[i].port;
        ret = RsSocketInitListen(conn, i, &connCb, serverPort, &listenInfo);
        if (ret == -EEXIST) {
            continue;
        }
        if (ret) {
            flag = -ENOMEM;
            hccp_err("listen init failed, ret:%d", ret);
            goto listen_node_err_handle;
        }

        /* socket */
        listenFd = socket(conn[i].family, SOCK_STREAM, 0);
        if (listenFd < 0) {
            errNo = errno;
            hccp_err("create socket for (IP %s : port %u) failed, family %d, errno %d",
                listenInfo->server_ip_addr.read_addr, serverPort, conn[i].family, errNo);
            conn[i].phase = LISTEN_CREATE_FD_ERR;
            goto listen_err_handle;
        }

        /* bind and listen */
        connCb->scope_id = scopeId;
        ret = RsSocketListenBindListen(listenFd, connCb, conn + i, listenInfo, serverPort);
        errNo = ret;
        if (ret == EADDRINUSE) {
            hccp_run_warn("bind and listen unsuccessful, errNo:%d, listenFd:%d, state:%u, IP(%s) serverPort:%u",
                errNo, listenFd, listenInfo->state, listenInfo->server_ip_addr.read_addr, serverPort);
            goto bind_err_handle;
        } else if (ret != 0) {
            hccp_err("bind and listen failed, errNo:%d, listenFd:%d, listen state:%u, IP(%s) serverPort:%u", errNo,
                listenFd, listenInfo->state, listenInfo->server_ip_addr.read_addr, serverPort);
            goto bind_err_handle;
        }

        ret = RsEpollCtl(connCb->epollfd, EPOLL_CTL_ADD, listenFd, EPOLLIN);
        if (ret) {
            errNo = ret;
            hccp_err("rs_epoll_ctl for epollfd[%d] listen_fd[%d]failed, errno:%d", connCb->epollfd, listenFd, errNo);
            goto bind_err_handle;
        }

        serverAddrLen = (conn->family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
        getsockname(listenFd, (struct sockaddr *)&serverAddr, &serverAddrLen);
        serverPort = (conn->family == AF_INET) ? ntohs(serverAddr.s_addr.sin_port) :
            ntohs(serverAddr.s_addr6.sin6_port);
        RsSocketSetConnListenInfo(listenInfo, listenFd, serverPort, &conn[i]);
    }

    return 0;

bind_err_handle:
    RS_CLOSE_RETRY_FOR_EINTR(ret, listenFd);
listen_err_handle:
    RsListenNodeFree(connCb, listenInfo);
    conn[i].err = (unsigned int)errNo;
    flag = -errNo;
listen_node_err_handle:
    RsSocketHandleListenNodeErr(i, connCb, conn, serverPort);
    return flag;
}

STATIC int RsSocketListenAddToEpoll(struct rs_conn_cb *connCb, struct rs_listen_info *listenInfo)
{
    int ret = 0;

    RS_PTHREAD_MUTEX_LOCK(&listenInfo->accept_credit_mutex);
    if (listenInfo->fd_state == LISTEN_FD_STATE_ADDED) {
        goto out;
    }

    // should ctl_add to make sure epoll event can be triggered
    hccp_run_info("IP:%s server_port:%u listen_fd:%d add to epoll:%d", listenInfo->server_ip_addr.read_addr,
        listenInfo->sock_port, listenInfo->listen_fd, connCb->epollfd);
    ret = RsEpollCtl(connCb->epollfd, EPOLL_CTL_ADD, listenInfo->listen_fd, EPOLLIN);
    if (ret != 0) {
        hccp_err("IP:%s server_port:%u listen_fd:%d rs_epoll_ctl failed, ret:%d errno:%d",
            listenInfo->server_ip_addr.read_addr, listenInfo->sock_port, listenInfo->listen_fd, ret, errno);
        goto out;
    }

    listenInfo->fd_state = LISTEN_FD_STATE_ADDED;

out:
    RS_PTHREAD_MUTEX_ULOCK(&listenInfo->accept_credit_mutex);
    return ret;
}

RS_ATTRI_VISI_DEF int RsSocketAcceptCreditAdd(struct socket_listen_info conn[], uint32_t num,
    unsigned int creditLimit)
{
    struct rs_listen_info *listenInfo = NULL;
    struct rs_ip_addr_info ipInfo = {0};
    struct rs_conn_cb *connCb = NULL;
    unsigned int tmpCreditLimit;
    uint32_t i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    for (i = 0; i < num; i++) {
        ret = RsConvertIpAddr(conn[i].family, &conn[i].local_ip, &ipInfo);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, i:%d, ret:%d", i, ret), ret);

        RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
        connCb = &gRsCb->conn_cb;
        ret = RsFindListenNode(connCb, &ipInfo, conn[i].port, &listenInfo);
        if (ret != 0) {
            hccp_err("rs_find_listen_node failed, i:%u, IP:%s serverPort:%u, ret:%d",
                i, ipInfo.read_addr, conn[i].port, ret);
            RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
            return ret;
        }
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);

        // prevent accept_credit_limit from overflow
        tmpCreditLimit = listenInfo->accept_credit_limit + creditLimit;
        if (tmpCreditLimit < creditLimit) {
            hccp_err("credit_limit overflow, IP:%s serverPort:%u tmpCreditLimit:%u, creditLimit:%u",
                ipInfo.read_addr, conn[i].port, tmpCreditLimit, creditLimit);
            return -EINVAL;
        }
        RS_PTHREAD_MUTEX_LOCK(&listenInfo->accept_credit_mutex);
        listenInfo->accept_credit_limit += creditLimit;
        RS_PTHREAD_MUTEX_ULOCK(&listenInfo->accept_credit_mutex);
        RsSocketListenAddToEpoll(connCb, listenInfo);
        listenInfo->accept_credit_flag = true;
    }

    return ret;
}

RS_ATTRI_VISI_DEF int RsSocketListenStop(struct socket_listen_info conn[], uint32_t num)
{
    struct rs_listen_info *listenInfo = NULL;
    struct rs_conn_cb *connCb = NULL;
    unsigned int chipId;
    uint32_t i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(((conn[i].family != AF_INET) && (conn[i].family != AF_INET6)) ||
            conn[i].phy_id >= RS_MAX_DEV_NUM,
            hccp_err("family[%d] invalid, or phyId[%u] invalid, i:%u", conn[i].family, conn[i].phy_id, i), -EINVAL);

        if (conn[i].family == AF_INET) {
            uint32_t *localIp = NULL;
            localIp = &(conn[i].local_ip.addr.s_addr);
            ret = RsSocketNodeid2vnic(*localIp, localIp);
            hccp_info("listen [%d] IP 0x%llx, ret_vnic %d", i, *localIp, ret);
        }
        ret = rsGetLocalDevIDByHostDevID(conn[i].phy_id, &chipId);
        CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);
        ret = RsDev2conncb(chipId, &connCb);
        // degrade log level, make it consistent with inner call
        CHK_PRT_RETURN(ret != 0, hccp_warn("get conncb from dev unsuccessful(%d)!", ret), -ENODEV);

        struct rs_ip_addr_info ipInfo = {0};
        ret = RsConvertIpAddr(conn[i].family, &conn[i].local_ip, &ipInfo);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

        ret = RsFindListenNode(connCb, &ipInfo, conn[i].port, &listenInfo);
        if (ret == 0 && __sync_fetch_and_sub(&(listenInfo->counter), 1) > 1) {
            continue;
        }
        // listen node not found, degrade log level due to this is non-fatal error
        if (ret != 0) {
            hccp_warn("get listen info unsuccessful(%d), IP(%s)!", ret, ipInfo.read_addr);
            conn[i].err = ENODEV;
            continue;
        }

        ret = RsSocketListenDelFromEpoll(connCb, listenInfo);
        CHK_PRT_RETURN(ret, hccp_err("delete from epoll failed, ret:%d, epollfd:%d, listenFd:%d",
            ret, connCb->epollfd, listenInfo->listen_fd), ret);

        /* close socket */
        RS_CLOSE_RETRY_FOR_EINTR(ret, listenInfo->listen_fd);
        hccp_info("IP(%s) close listen fd:%d !", ipInfo.read_addr, listenInfo->listen_fd);

        listenInfo->listen_fd = RS_FD_INVALID;
        listenInfo->state = RS_CONN_STATE_RESET;

        RsListenNodeFree(connCb, listenInfo);
    }

    return 0;
}

STATIC int RsAllocClientConnNode(struct rs_conn_cb *connCb,
    enum rs_conn_role role, struct rs_conn_info **conn, struct socket_connect_info *socketConn,
    struct rs_ip_addr_info *clientIp, struct rs_ip_addr_info *serverIp, int serverPort)
{
    struct rs_list_head *listHead = NULL;
    struct rs_conn_info *connInfo;
    int ret;

    connInfo = calloc(1, sizeof(struct rs_conn_info));
    CHK_PRT_RETURN(connInfo == NULL, hccp_err("alloc mem for socket conn info failed!"), -ENOMEM);

    connInfo->port = serverPort;
    connInfo->connfd = RS_FD_INVALID;
    connInfo->state = RS_CONN_STATE_RESET;
    connInfo->server_ip = *serverIp;
    connInfo->client_ip = *clientIp;
    connInfo->scope_id = connCb->scope_id;

    ret = strcpy_s(connInfo->tag, SOCK_CONN_TAG_SIZE, socketConn->tag);
    if (ret) {
        hccp_err("strcpy_s err, ret:%d, size of dest:%u, size of src:%u", ret, sizeof(connInfo->tag),
            sizeof(socketConn->tag));
        goto out;
    }
    ret = sprintf_s(connInfo->tag + SOCK_CONN_TAG_SIZE, SOCK_CONN_DEV_ID_SIZE, "%u", socketConn->phy_id);
    if (ret < 0) {
        hccp_err("sprintf_s err, ret:%d, phyId:%u", ret, socketConn->phy_id);
        goto out;
    }

    RsGetCurTime(&connInfo->start_time);

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    listHead = (role == RS_CONN_ROLE_SERVER) ? (&connCb->server_conn_list) : (&connCb->client_conn_list);
    RsListAddTail(&connInfo->list, listHead);
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);

    *conn = connInfo;

    return 0;

out:
    free(connInfo);
    connInfo = NULL;
    return -ESAFEFUNC;
}

STATIC void RsSocketClientValidSync(struct rs_conn_info *conn)
{
    char isvalid[RS_WLIST_VALID_FLAG_SIZE] = {0};
    int ret, retClose;

    do {
        ret = RsSocketRecv(conn->connfd, isvalid, RS_WLIST_VALID_FLAG_SIZE);
        if (ret == RS_WLIST_VALID_FLAG_SIZE && (strncmp(isvalid, "a5a5a", strlen("a5a5a")) == 0)) {
            hccp_info("[client]client is valid, ret:%d, clientIp:%s serverIp:%s serverPort:%u",
                ret, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port);
            conn->state = RS_CONN_STATE_VALID_SYNC;
            return;
        } else if (ret == RS_WLIST_VALID_FLAG_SIZE && (strncmp(isvalid, "5a5a5", strlen("5a5a5")) == 0)) {
            hccp_info("[client]client is invalid, errNo:%d, clientIp:%s serverIp:%s serverPort:%u",
                errno, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port);
            goto out;
        } else if (ret == -EAGAIN) {
            return;
        }
    } while ((ret < 0) && (errno == EINTR));

    // ret is -EFILEOPER or recv unexpected data. state machine will connect again
    hccp_run_warn("[client]recv isvalid unsuccessful, ret:%d errNo:%d, clientIp:%s serverIp:%s serverPort:%u fd:%d."
        " retry connect", ret, errno, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->connfd);
out:
    if (gRsCb->ssl_enable == RS_SSL_ENABLE) {
        ssl_adp_shutdown(conn->ssl);
        ssl_adp_free(conn->ssl);
        conn->ssl = NULL;
    }
    RS_CLOSE_RETRY_FOR_EINTR(retClose, conn->connfd);
    conn->connfd = RS_FD_INVALID;
    conn->state = RS_CONN_STATE_RESET;
    conn->tag_sync_time = 0;
    return;
}

STATIC void RsSocketTagSync(struct rs_conn_info *conn)
{
    int ret;

    /* sync tag to server */
    conn->tag_sync_time++;
    ret = RsDrvSocketSend(conn->connfd, conn->tag, SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE, 0);
    if (ret == SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE) {
        conn->state = RS_CONN_STATE_TAG_SYNC;
        hccp_info("[client]send tag success! ret:%d, tag_sync_time:%u, clientIp:%s serverIp:%s serverPort:%u tag:%s",
            ret, conn->tag_sync_time, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag);
    } else if (ret == -EAGAIN) {
        conn->state = RS_CONN_STATE_TIMEOUT;
        hccp_info("[client]send tag incomplete! ret:%d, tag_sync_time:%u, clientIp:%s serverIp:%s serverPort:%u "
            "tag:%s", ret, conn->tag_sync_time, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port,
            conn->tag);
    } else {
        hccp_run_info("[client]send tag unsuccessful, ret:%d, tag_sync_time:%u, retry connect, clientIp:%s "
            "serverIp:%s serverPort:%u tag:%s", ret, conn->tag_sync_time, conn->client_ip.read_addr,
            conn->server_ip.read_addr, conn->port, conn->tag);

        if (gRsCb->ssl_enable == RS_SSL_ENABLE) {
            ssl_adp_shutdown(conn->ssl);
            ssl_adp_free(conn->ssl);
            conn->ssl = NULL;
        }
        RS_CLOSE_RETRY_FOR_EINTR(ret, conn->connfd);
        conn->connfd = RS_FD_INVALID;
        conn->state = RS_CONN_STATE_RESET;
        conn->tag_sync_time = 0;
    }

    return;
}

STATIC void RsConnCostTime(struct rs_conn_info *conn)
{
    float timeCost = 0.0;

    RsGetCurTime(&conn->end_time);
    HccpTimeInterval(&conn->end_time, &conn->start_time, &timeCost);
    if (timeCost > RS_EXPECT_TIME_MAX) {
        hccp_warn("socket [%d] connect success cost [%f] ms more than[%f]ms!", conn->connfd, timeCost,
            RS_EXPECT_TIME_MAX);
    } else {
        hccp_info("socket [%d] connect success! cost [%f] ms", conn->connfd, timeCost);
    }

    return;
}

/* ssl will connect again and again, HCCL get socke timeout after period time */
STATIC int RsSocketSslConnect(struct rs_conn_info *conn, struct rs_cb *rscb)
{
    int ret, err;

    ret = ssl_adp_do_handshake(conn->ssl);
    if (ret != 1) {
        err = ssl_adp_get_error(conn->ssl, ret);
        if (err == SSL_ERROR_WANT_WRITE) {
            hccp_dbg("ssl fd %d return want write", conn->connfd);
        } else if (err == SSL_ERROR_WANT_READ) {
            hccp_dbg("ssl fd %d return want read", conn->connfd);
        } else {
#ifdef CONFIG_SSL
            rs_ssl_err_string(conn->connfd, err);
#endif
        }

        return -EAGAIN;
    }

#ifdef CONFIG_SSL
    ret = rs_tls_peer_cert_verify(conn->ssl, rscb);
    CHK_PRT_RETURN(ret, hccp_err("verify peer cert failed ret %d", ret), ret);
#endif

    return 0;
}

STATIC int RsSocketStateSslFdBind(struct rs_conn_info *conn, uint32_t sslEnable, struct rs_cb *rscb)
{
    int ret;

    if (sslEnable == RS_SSL_ENABLE) {
        ret = RsSocketSslConnect(conn, rscb);
        if (ret) {
            return ret;
        }
        conn->state = RS_CONN_STATE_SSL_CONNECTED;
    }

    RsConnCostTime(conn);
    RsSocketTagSync(conn);
    return 0;
}

STATIC int RsSocketStateConnected(struct rs_conn_info *conn, uint32_t sslEnable, struct rs_cb *rscb)
{
    int ret;

    if (sslEnable == RS_SSL_ENABLE) {
        ret = RsDrvSslBindFd(conn, conn->connfd);
        if (ret != 0) {
            RsSocketSaveErrInfo(RS_CONN_STATE_CONNECTED, ret, &conn->err_info);
            hccp_err("[client]ssl bind failed, connfd:%d, ret:%d, clientIp:%s serverIp:%s serverPort:%u tag:%s",
                conn->connfd, ret, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag);
            return ret;
        }
        conn->state = RS_CONN_STATE_SSL_BIND_FD;
    }

    return RsSocketStateSslFdBind(conn, sslEnable, rscb);
}

STATIC int RsSocketStateInit(unsigned int chipId, struct rs_conn_info *conn, uint32_t sslEnable, struct rs_cb *rscb)
{
    int ret;

    conn->tag[SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE - 1] = '\0';

    ret = RsDrvConnect(conn->connfd, &conn->server_ip, &conn->client_ip, conn->port);
    if (ret != 0) {
        RsSocketSaveErrInfo(RS_CONN_STATE_INIT, ret, &conn->err_info);
        hccp_warn("[client]rs_socket_state_init conn unsuccessful! client_ip:%s server_ip:%s server_port:%u tag:%s, "
            "fd:%d, ret:%d", conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag,
            conn->connfd, ret);
        return ret;
    }

    // should set back tcp socket send/recv timeout to OS default when ssl is disabled
    if (sslEnable == RS_SSL_DISABLE) {
        ret = RsSocketSetFdTimeoutUsec(conn->connfd, 0);
        if (ret != 0) {
            hccp_warn("[client]rs_socket_set_fd_timeout_usec conn unsuccessful!, clientIp:%s serverIp:%s "
                "serverPort:%u tag:%s, fd:%d, ret:%d", conn->client_ip.read_addr, conn->server_ip.read_addr,
                conn->port, conn->tag, conn->connfd, ret);
        }
    }

    conn->state = RS_CONN_STATE_CONNECTED;
    /*
     * ssl will connect again and again, HCCL get socke timeout after period time,
     * so there is no log info to prevent over log
     */
    ret = RsSocketStateConnected(conn, sslEnable, rscb);
    if (ret) {
        return ret;
    }

    return 0;
}

STATIC int RsConnectBindClient(int fd, struct rs_conn_info *conn)
{
    int errNo;
    int ret;

    if (conn->client_ip.family == AF_INET) {
        struct sockaddr_in clientAddr = {0};
        clientAddr.sin_family = conn->client_ip.family;
        clientAddr.sin_addr = conn->client_ip.bin_addr.addr;

        hccp_dbg("socket bind: family %d, port %d, addr 0x%08x",
            clientAddr.sin_family, clientAddr.sin_port, clientAddr.sin_addr.s_addr);
        ret = bind(fd, &clientAddr, sizeof(clientAddr));
    } else {
        struct sockaddr_in6 clientAddr = {0};
        clientAddr.sin6_family = conn->client_ip.family;
        clientAddr.sin6_addr = conn->client_ip.bin_addr.addr6;
        clientAddr.sin6_scope_id = (uint32_t)conn->scope_id;

        hccp_dbg("socket bind: family %d, port %d, scopeId %d",
            clientAddr.sin6_family, clientAddr.sin6_port, clientAddr.sin6_scope_id);
        for (unsigned long i = 0; i < sizeof(struct in6_addr); i++) {
            hccp_dbg("socket bind: addr[%lu] 0x%02x", i, clientAddr.sin6_addr.s6_addr[i]);
        }

        ret = bind(fd, &clientAddr, sizeof(clientAddr));
    }
    if (ret) {
        errNo = errno;
        hccp_err("client bind failed! IP:%s, sock:%d, ret:%d, error:%d", conn->client_ip.read_addr, fd, ret, errNo);
        return -errNo;
    }
    union rs_socketaddr clientAddr = { 0 };
    socklen_t clientAddrLen =
        (conn->client_ip.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    getsockname(fd, (struct sockaddr *)&clientAddr, &clientAddrLen);
    uint16_t clientPort =
        (conn->client_ip.family == AF_INET) ? ntohs(clientAddr.s_addr.sin_port) : ntohs(clientAddr.s_addr6.sin6_port);
    if ((clientPort < 60000) || (clientPort > 60015)) { // HCCL默认监听60000-60015端口,如client使用该端口，记录EVENT日志
        hccp_info("client bind success. client family %d addr %s:%u, fd:%d", conn->client_ip.family,
            conn->client_ip.read_addr, clientPort, fd);
    } else {
        hccp_run_info("client bind success. client family %d addr %s:%u, fd:%d", conn->client_ip.family,
            conn->client_ip.read_addr, clientPort, fd);
    }
    return 0;
}

STATIC int RsSocketBindClient(unsigned int chipId, int connFd, struct rs_conn_info *conn, int hccpMode)
{
    bool bindIp = true;

    if (conn->client_ip.family == AF_INET && hccpMode == NETWORK_OFFLINE) {
        // compare client_ip with current vnic_ip for compatibility issues, 910A & 910B no need to bind vnic ip
        bindIp = RsSocketIsVnicIp(chipId, conn->client_ip.bin_addr.addr.s_addr) ? false : true;
    }

    // chip force to bind: 310P & 910_93
    if (!bindIp) {
        RsSocketGetBindByChip(chipId, &bindIp);
    }

    // no need to bind ip
    if (!bindIp) {
        return 0;
    }

    return RsConnectBindClient(connFd, conn);
}

STATIC int RsSocketStateReset(unsigned int chipId, struct rs_conn_info *conn, uint32_t sslEnable, struct rs_cb *rscb)
{
#define RS_SOCKET_CONNECT_TIMEOUT_USECS 100000
    int connFd, retClose, hccpMode;
    int tcpNodelayFlag = 1;
    int ret = 0;

    hccpMode = RsGetHccpMode(chipId);

    connFd = socket(conn->client_ip.family, SOCK_STREAM, 0);
    if (connFd < 0) {
        ret = -errno;
        hccp_err("[client]create socket failed, errno:%d", ret);
        goto err_socket_create;
    }

    ret = RsSocketBindClient(chipId, connFd, conn, hccpMode);
    if (ret != 0) {
        hccp_err("[client]rs_socket_bind_client failed, ret:%d", ret);
        goto err_connect_reset;
    }

    if (sslEnable == RS_SSL_ENABLE) {
        ret = RsSetFdNonblock(connFd);
        if (ret) {
            goto err_connect_reset;
        }
    }

    /* set tcp socket tos RS_TCP_DSCP_0 */
    int tosLocal = (RS_TCP_DSCP_0 & RS_DSCP_MASK) << RS_DSCP_OFF;
    ret = setsockopt(connFd, IPPROTO_IP, IP_TOS, (void *)&tosLocal, sizeof(tosLocal));
    if (ret) {
        hccp_err("[client]setsockopt(IP_TOS) failed, connFd:%d, ret:%d, errno:%d", connFd, ret, errno);
        goto err_socket_option;
    }

    ret = setsockopt(connFd, IPPROTO_TCP, TCP_NODELAY, (void *)&tcpNodelayFlag, sizeof(int));
    if (ret < 0) {
        hccp_err("[client]setsockopt(TCP_NODELAY) failed, connFd:%d, ret:%d, errno:%d", connFd, ret, errno);
        goto err_socket_option;
    }

    // should set tcp socket send/recv timeout when ssl is disabled
    if (sslEnable == RS_SSL_DISABLE) {
        ret = RsSocketSetFdTimeoutUsec(connFd, RS_SOCKET_CONNECT_TIMEOUT_USECS);
        if (ret != 0) {
            goto err_connect_reset;
        }
    }

    conn->connfd = connFd;
    conn->state = RS_CONN_STATE_INIT;
    /*
     * ssl will connect again and again, HCCL get socke timeout after period time,
     * so there is no log info to prevent over log
     */
    ret = RsSocketStateInit(chipId, conn, sslEnable, rscb);
    if (ret) {
        return ret;
    }

    return 0;

err_socket_option:
    ret = -errno;
err_connect_reset:
    RS_CLOSE_RETRY_FOR_EINTR(retClose, connFd);
err_socket_create:
    RsSocketSaveErrInfo(RS_CONN_STATE_RESET, ret, &conn->err_info);
    return -ESYSFUNC;
}

int RsSocketConnectAsync(struct rs_conn_info *conn, struct rs_cb *rscb)
{
    int ret = 0;
    unsigned int chipId = rscb->chip_id;
    uint32_t sslEnable = rscb->ssl_enable;
    RS_CHECK_POINTER_NULL_WITH_RET(conn);
    switch (conn->state) {
        case RS_CONN_STATE_RESET:
            /* create socket for client */
            ret = RsSocketStateReset(chipId, conn, sslEnable, rscb);
            break;

        case RS_CONN_STATE_INIT:
            ret = RsSocketStateInit(chipId, conn, sslEnable, rscb);
            break;

        case RS_CONN_STATE_CONNECTED:
            ret = RsSocketStateConnected(conn, sslEnable, rscb);
            break;

        case RS_CONN_STATE_SSL_BIND_FD:
            ret = RsSocketStateSslFdBind(conn, sslEnable, rscb);
            break;

        case RS_CONN_STATE_SSL_CONNECTED:
            hccp_info("[client]IP(%s) connect port %d, fd:%d OK!", conn->server_ip.read_addr, conn->port, conn->connfd);
            RsSocketTagSync(conn);
            break;

        case RS_CONN_STATE_TAG_SYNC:
            if (gRsCb->conn_cb.wlist_enable == 1) {
                RsSocketClientValidSync(conn);
            }
            break;

        case RS_CONN_STATE_TIMEOUT:
            hccp_info("[client]!send tag again! local_ip:%s server_ip:%s server_port:%u, tag:%s, fd:%d!",
                conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag, conn->connfd);
            RsSocketTagSync(conn);
            break;

        case RS_CONN_STATE_VALID_SYNC:
            break;

        case RS_CONN_STATE_TX_TO_HCCL:
            break;

        case RS_CONN_STATE_ERR:
            break;

        default:
            hccp_err("[client]Unknown state:%u, localIp:%s serverIp:%s serverPort:%u, tag:%s, fd:%d", conn->state,
                conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag, conn->connfd);
            return -EINVAL;
    }

    return ret;
}

// 获取socket connect状态；返回值 0:connect中，1:connect完成
int RsGetSocketConnectState(struct rs_conn_info *conn)
{
    if ((conn->state == RS_CONN_STATE_TX_TO_HCCL) ||
        ((gRsCb->conn_cb.wlist_enable == 1) && (conn->state == RS_CONN_STATE_VALID_SYNC)) ||
        ((gRsCb->conn_cb.wlist_enable == 0) && (conn->state == RS_CONN_STATE_TAG_SYNC))) {
        return 1;
    } else {
        return 0;
    }
}

STATIC void RsSocketsIpAddrConverter(struct socket_connect_info conn[], int num)
{
    int j;

    for (j = 0; j < num; j++) {
        if (conn[j].family == AF_INET) {
            conn[j].local_ip.addr.s_addr = RsSocketVnic2nodeid(conn[j].local_ip.addr.s_addr);
            conn[j].remote_ip.addr.s_addr = RsSocketVnic2nodeid(conn[j].remote_ip.addr.s_addr);
        }
    }
}

static void RsSocketHandleConnNodeErr(uint32_t i, struct rs_conn_cb *connCb,
    struct socket_connect_info conn[], uint32_t serverPort)
{
    struct rs_conn_info *connInfo = NULL;
    uint32_t j;
    int ret;

    for (j = 0; j < i; j++) {
        ret = RsGetConnInfo(connCb, conn + j, &connInfo, serverPort);
        if (ret) {
            hccp_dbg("not find conn node, ret %d", ret);
        } else {
            RS_PTHREAD_MUTEX_LOCK(&connCb->rscb->mutex);
            RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
            RsListDel(&connInfo->list);
            free(connInfo);
            connInfo = NULL;
            RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
            RS_PTHREAD_MUTEX_ULOCK(&connCb->rscb->mutex);
        }
    }

    return;
}

STATIC int RsSocketConnectCheckPara(struct socket_connect_info *connInfo)
{
    if (((connInfo->family != AF_INET) && (connInfo->family != AF_INET6)) || connInfo->phy_id >= RS_MAX_DEV_NUM ||
        strlen(connInfo->tag) >= SOCK_CONN_TAG_SIZE) {
        hccp_err("family[%d] invalid, or phyId[%u] invalid, or conn tag len:%u more than max len:%d",
            connInfo->family, connInfo->phy_id, strlen(connInfo->tag), SOCK_CONN_TAG_SIZE);
        return -EINVAL;
    }

    return 0;
}

STATIC int rs_socket_IP_convert(struct socket_connect_info *connInfo, struct rs_ip_addr_info *remoteIp,
    struct rs_ip_addr_info *localIp)
{
    int retVal = 0;
    int ret = 0;

    if (connInfo->family == AF_INET) {
        uint32_t *remoteIpTmp = &(connInfo->remote_ip.addr.s_addr);
        uint32_t *localIpTmp = &(connInfo->local_ip.addr.s_addr);
        retVal = RsSocketNodeid2vnic(*remoteIpTmp, remoteIpTmp);
        ret = RsSocketNodeid2vnic(*localIpTmp, localIpTmp);
        hccp_info("local IP[0x%llx], ret:%d, remote IP[0x%llx], ret:%d", *localIpTmp, ret, *remoteIpTmp, retVal);
    }

    ret = RsConvertIpAddr(connInfo->family, &connInfo->remote_ip, remoteIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) remote ip failed, ret:%d", ret), ret);

    ret = RsConvertIpAddr(connInfo->family, &connInfo->local_ip, localIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) local ip failed, ret:%d", ret), ret);

    hccp_info("local IP[%s], ret:%d, remote IP[%s], ret:%d", localIp->read_addr, ret, remoteIp->read_addr, retVal);
    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketBatchConnect(struct socket_connect_info conn[], uint32_t num)
{
    struct rs_conn_info *connInfo = NULL;
    struct rs_conn_cb *connCb = NULL;
    unsigned int chipId, serverPort;
    struct rs_ip_addr_info remoteIp;
    struct rs_ip_addr_info localIp;
    unsigned int i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    for (i = 0; i < num; i++) {
        serverPort = conn[i].port;
        ret = RsSocketConnectCheckPara(&conn[i]);
        if (ret) {
            hccp_err("rs_socket_connect_check_para for failed, ret:%d, i:%u", ret, i);
            goto conn_node_err_handle;
        }

        ret = rs_socket_IP_convert(&conn[i], &remoteIp, &localIp);
        if (ret) {
            hccp_err("convert ip invalid, ret %d", ret);
            goto conn_node_err_handle;
        }
        ret = rsGetLocalDevIDByHostDevID(conn[i].phy_id, &chipId);
        if (ret) {
            hccp_err("phy_id invalid, ret %d", ret);
            goto conn_node_err_handle;
        }

        ret = RsDev2conncb(chipId, &connCb);
        if (ret) {
            hccp_err("get conncb from dev failed(%d)!", ret);
            goto conn_node_err_handle;
        }

        if (conn[i].family == AF_INET6) {
            connCb->scope_id = RsGetIpv6ScopeId(conn[i].local_ip.addr6);
            if (connCb->scope_id < 0) {
                hccp_err("scope_id[%d] is invalid", connCb->scope_id);
                connCb->scope_id = 0;
                goto conn_node_err_handle;
            }
        }

        ret = RsGetConnInfo(connCb, conn + i, &connInfo, serverPort);
        if (ret) {
            ret = RsAllocClientConnNode(connCb, RS_CONN_ROLE_CLIENT, &connInfo, &conn[i], &localIp, &remoteIp,
                serverPort);
            if (ret) {
                hccp_err("rs_alloc_client_conn_node failed, ret:%d, role:%d, localIp:%s, remoteIp:%s, serverPort:%u,"
                    " tag:%s", ret, RS_CONN_ROLE_CLIENT, localIp.read_addr, remoteIp.read_addr, serverPort,
                    conn[i].tag);
                goto conn_node_err_handle;
            }

            hccp_info("create conn node for {remote_ip(%s), serverPort(%u), tag(%s)}!",
                remoteIp.read_addr, serverPort, connInfo->tag);
        } else {
            hccp_info("conn node for {remote_ip(%s), serverPort(%u), tag(%s)} exist! state:%u",
                remoteIp.read_addr, serverPort, connInfo->tag, connInfo->state);
        }
    }
    sem_post(&gRsCb->connect_trig_sem);
    RsSocketsIpAddrConverter(conn, num);
    return 0;

conn_node_err_handle:
    RsSocketHandleConnNodeErr(i, connCb, conn, serverPort);
    return ret;
}

STATIC int RsSocketCloseFd(int fd)
{
    int errNo = -1;
    int ret;

    do {
        ret = close(fd);
        if (ret < 0) {
            errNo = errno;
            CHK_PRT_RETURN(errNo != EINTR, hccp_err("close fd[%d] failed, ret:%d, errNo[%d]",
                fd, ret, errNo), -errNo);
        }
    } while ((ret < 0) && (errNo == EINTR));

    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketBatchClose(int disuseLinger, struct rs_socket_close_info_t conn[], uint32_t num)
{
    struct rs_conn_info *connInfo = NULL;
    struct linger soLinger;
    int fd = RS_FD_INVALID;
    int retVal = 0;
    unsigned int i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);

    for (i = 0; i < num; i++) {
        fd = conn[i].fd;
        CHK_PRT_RETURN(fd < 0, hccp_err("param error ! fd:%d, i:%d, num:%d", fd, i, num), -EINVAL);

        // strict mutex lock before find to make sure conn_info is valid on concurrent scenario
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
        ret = RsFd2conn(fd, &connInfo);
        if (ret != 0) {
            hccp_err("get conn failed! ret:%d", ret);
            RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
            return ret;
        }

        hccp_info("conn node of IP(%s) fd:%d, state:%d",
            connInfo->server_ip.read_addr, connInfo->connfd, connInfo->state);

        RS_PTHREAD_MUTEX_LOCK(&gRsCb->conn_cb.conn_mutex);
        RsListDel(&connInfo->list);
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->conn_cb.conn_mutex);
        if (gRsCb->ssl_enable == RS_SSL_ENABLE) {
            ssl_adp_shutdown(connInfo->ssl);
            ssl_adp_free(connInfo->ssl);
            connInfo->ssl = NULL;
        }
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);

        if (connInfo->state > RS_CONN_STATE_RESET) {
            soLinger.l_onoff = 1;
            soLinger.l_linger = disuseLinger == 0 ? RS_CLOSE_TIMEOUT : 0;
            ret = setsockopt(connInfo->connfd, SOL_SOCKET, SO_LINGER, &soLinger, sizeof(soLinger));
            if (ret) {
                hccp_err("setsockopt l_onoff:%d l_linger:%d failed err:%d", soLinger.l_onoff, soLinger.l_linger,
                    errno);
                retVal = ret;
            }

            ret = RsSocketCloseFd(connInfo->connfd);
            if (ret) {
                hccp_err("rs_socket_close_fd for fd[%d] failed, ret[%d]", connInfo->connfd, ret);
                retVal = ret;
            }
        }

        free(connInfo);
        connInfo = NULL;
    }

    return retVal;
}

RS_ATTRI_VISI_DEF int RsSocketBatchAbort(struct socket_connect_info conn[], uint32_t num)
{
    struct rs_conn_info *connInfo = NULL;
    struct linger soLinger = { 0 };
    int retVal = 0;
    unsigned int i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);

    for (i = 0; i < num; i++) {
        // strict mutex lock before find to make sure conn_info is valid on concurrent scenario
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
        ret = RsGetConnInfo(&gRsCb->conn_cb, &conn[i], &connInfo, conn[i].port);
        if (ret != 0) {
            hccp_err("rs_get_conn_info conn:%u failed! ret:%d", i, ret);
            RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
            return ret;
        }

        hccp_info("abort conn node of IP(%s) fd:%d, state:%d", connInfo->server_ip.read_addr, connInfo->connfd,
            connInfo->state);

        RS_PTHREAD_MUTEX_LOCK(&gRsCb->conn_cb.conn_mutex);
        RsListDel(&connInfo->list);
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->conn_cb.conn_mutex);
        if (gRsCb->ssl_enable == RS_SSL_ENABLE && connInfo->ssl != NULL) {
            ssl_adp_shutdown(connInfo->ssl);
            ssl_adp_free(connInfo->ssl);
            connInfo->ssl = NULL;
            ssl_adp_clear_error();
        }
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);

        if (connInfo->state > RS_CONN_STATE_RESET && connInfo->connfd != RS_FD_INVALID) {
            // force to close fd
            soLinger.l_onoff = 1;
            ret = setsockopt(connInfo->connfd, SOL_SOCKET, SO_LINGER, &soLinger, sizeof(soLinger));
            if (ret) {
                hccp_err("setsockopt l_onoff:%d l_linger:%d failed err:%d", soLinger.l_onoff, soLinger.l_linger,
                    errno);
                retVal = ret;
            }

            ret = RsSocketCloseFd(connInfo->connfd);
            if (ret) {
                hccp_err("rs_socket_close_fd for fd[%d] failed, ret[%d]", connInfo->connfd, ret);
                retVal = ret;
            }
        }

        free(connInfo);
        connInfo = NULL;
    }

    return retVal;
}

STATIC void RsSocketsBackfill(struct socket_fd_data conn[], int sockNum,
    struct rs_conn_info *connTmp, struct rs_vnic_info vnicInfo)
{
    conn[sockNum].fd = connTmp->connfd;

    if (vnicInfo.role == RS_CONN_ROLE_SERVER) {
        conn[sockNum].remote_ip = connTmp->client_ip.bin_addr;
    } else {
        conn[sockNum].remote_ip = connTmp->server_ip.bin_addr;
    }

    conn[sockNum].status = RS_SOCK_STATUS_OK;
    connTmp->state = RS_CONN_STATE_TX_TO_HCCL;
    connTmp->is_got = true;
}

STATIC void RsSocketsServeripConverter(struct socket_fd_data conn[], int num,
    uint32_t vnicFlag)
{
    int j;

    if (vnicFlag) {
        for (j = 0; j < num; j++) {
            if (conn[j].family == AF_INET) {
                conn[j].local_ip.addr.s_addr = RsSocketVnic2nodeid(conn[j].local_ip.addr.s_addr);
                conn[j].remote_ip.addr.s_addr = RsSocketVnic2nodeid(conn[j].remote_ip.addr.s_addr);
            }
        }
    }
}

STATIC int RsFindSockets(struct rs_conn_info *connTmp, struct socket_fd_data conn[], int num,
    int role)
{
    int ret, i;

    /* normal process, no record log */
    if (gRsCb->conn_cb.wlist_enable == 1) {
        if (connTmp->state != RS_CONN_STATE_VALID_SYNC) {
            return -EINVAL;
        }
    } else {
        if (connTmp->state != RS_CONN_STATE_TAG_SYNC) {
            return -EINVAL;
        }
    }

    // server skip to get current socket once socket already been got
    if (role == RS_CONN_ROLE_SERVER && connTmp->is_got) {
        return -EINVAL;
    }

    if (role == RS_CONN_ROLE_SERVER) {
        i = 0;
        struct rs_ip_addr_info localIp;
        ret = RsConvertIpAddr(conn->family, &conn->local_ip, &localIp);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

        CHK_PRT_RETURN(RsCompareIpAddr(&connTmp->server_ip, &localIp), hccp_warn("server_ip[%s] != local_ip[%s]",
            connTmp->server_ip.read_addr, localIp.read_addr), -EINVAL);
    } else {
        for (i = 0; i < num; i++) {
            if (conn[i].status == RS_SOCK_STATUS_OK) {
                continue;
            }

            struct rs_ip_addr_info remoteIp;
            remoteIp.family = (uint32_t)conn[i].family;
            remoteIp.bin_addr = conn[i].remote_ip;
            struct rs_ip_addr_info localIp;
            localIp.family = (uint32_t)conn[i].family;
            localIp.bin_addr = conn[i].local_ip;
            if ((!RsCompareIpAddr(&connTmp->server_ip, &remoteIp)) &&
                (!RsCompareIpAddr(&connTmp->client_ip, &localIp))) {
                break;
            }
        }
    }

    CHK_PRT_RETURN(i == num, hccp_warn("i == num %d, not find serverIp[%s]", num, connTmp->server_ip.read_addr),
        -EINVAL);

    conn[i].tag[SOCK_CONN_TAG_SIZE - 1] = '\0';
    ret = strcmp(conn[i].tag, connTmp->tag);
    CHK_PRT_RETURN(ret, hccp_warn("The %dth conn tag[%s] is different from conn_tmp_tag [%s]",
        i, conn[i].tag, connTmp->tag), -EINVAL);

    return i;
}

/* find it */
STATIC int RsSocketsCompare(struct rs_list_head *listHead, struct socket_fd_data conn[],
    uint32_t num, struct rs_vnic_info vnicInfo, struct rs_conn_cb *connCb)
{
    struct rs_conn_info *connTmp = NULL;
    struct rs_conn_info *connTmp2 = NULL;
    int sockNum = 0;
    int i;
    int sockIndex;

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    connTmp = list_entry((listHead)->next, struct rs_conn_info, list);
    connTmp2 = list_entry(connTmp->list.next, struct rs_conn_info, list);
    for (; &connTmp->list != (listHead);) {
        i = RsFindSockets(connTmp, conn, num, vnicInfo.role);
        if (i < 0) {
            goto renew_conn;
        }
        sockIndex = (vnicInfo.role == RS_CONN_ROLE_SERVER) ? sockNum : i;
        RsSocketsBackfill(conn, sockIndex, connTmp, vnicInfo);

        sockNum++;
        if ((unsigned int)sockNum >= num) {
            break;
        }
renew_conn:
        connTmp = connTmp2;
        connTmp2 = list_entry(connTmp2->list.next, struct rs_conn_info, list);
    }
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    RsSocketsServeripConverter(conn, num, vnicInfo.vnic_flag);
    struct rs_ip_addr_info localIp;
    RsConvertIpAddr(conn[0].family, &conn[0].local_ip, &localIp);
    hccp_dbg("local_ip:%s, fd:%d, sockNum:%d", localIp.read_addr, conn[0].fd, sockNum);
    return sockNum;
}

STATIC int RsGetVnicFlag(uint32_t role, uint32_t *localIp, uint32_t *remoteIp)
{
    int vnicFlag = 0;

    if (role == RS_CONN_ROLE_SERVER) {
        if (RsSocketNodeid2vnic(*localIp, localIp) == RS_VNIC_FLAG) {
            vnicFlag = 1;
        }
    } else {
        if ((RsSocketNodeid2vnic(*remoteIp, remoteIp) == RS_VNIC_FLAG) &&
            (RsSocketNodeid2vnic(*localIp, localIp) == RS_VNIC_FLAG)) {
            vnicFlag = 1;
        }
    }
    return vnicFlag;
}

RS_ATTRI_VISI_DEF int RsGetSockets(uint32_t role, struct socket_fd_data conn[], uint32_t num)
{
    struct rs_list_head *listHead = NULL;
    struct rs_vnic_info vnicInfo = {0};
    struct rs_conn_cb *connCb = NULL;
    unsigned int chipId;
    uint32_t j;
    int ret;

    vnicInfo.role = role;

    RS_SOCKET_PARA_CHECK(num, conn);
    CHK_PRT_RETURN(role > RS_CONN_ROLE_CLIENT, hccp_err("para invalid. role[%u]", role), -EINVAL);

    /* set conn status to NA */
    for (j = 0; j < num; j++) {
        conn[j].status = 0;
        CHK_PRT_RETURN(((conn[j].family != AF_INET) && (conn[j].family != AF_INET6)) ||
            conn[j].phy_id >= RS_MAX_DEV_NUM,
            hccp_err("family[%d] invalid, or phyId[%u] invalid, j:%u", conn[j].family, conn[j].phy_id, j), -EINVAL);

        CHK_PRT_RETURN(strlen(conn[j].tag) >= SOCK_CONN_TAG_SIZE, hccp_err("conn tag len:%u more than max len:%d",
            strlen(conn[j].tag), SOCK_CONN_TAG_SIZE), -EINVAL);

        if (conn[j].family == AF_INET) {
            uint32_t *localIp = &(conn[j].local_ip.addr.s_addr);
            uint32_t *remoteIp = &(conn[j].remote_ip.addr.s_addr);
            vnicInfo.vnic_flag = (uint32_t)RsGetVnicFlag(role, localIp, remoteIp);
        }
    }

    ret = rsGetLocalDevIDByHostDevID(conn->phy_id, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    ret = RsDev2conncb(chipId, &connCb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev failed! ret(%d)", ret), -ENODEV);

    listHead = (role == RS_CONN_ROLE_SERVER) ? (&connCb->server_conn_list) : (&connCb->client_conn_list);
    return RsSocketsCompare(listHead, conn, num, vnicInfo, connCb);
}

RS_ATTRI_VISI_DEF int RsGetSslEnable(uint32_t *sslEnable)
{
    CHK_PRT_RETURN(gRsCb == NULL, hccp_err("param error, gRsCb is NULL"), -ENODEV);
    CHK_PRT_RETURN(sslEnable == NULL, hccp_err("param error, sslEnable is NULL"), -EINVAL);

    *sslEnable = gRsCb->ssl_enable;
    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketSend(int fd, const void *data, uint64_t size)
{
    int ret;

    ret = RsDrvSocketSend(fd, data, size, MSG_DONTWAIT | MSG_NOSIGNAL);

    hccp_dbg("send fd:%d, size:%llu, send %dB", fd, size, ret);
    return ret;
}

RS_ATTRI_VISI_DEF int RsPeerSocketSend(uint32_t sslEnable, int fd, const void *data, uint64_t size)
{
    int ret = 0;
    int errNo;

    CHK_PRT_RETURN(fd < 0 || size == 0 || data == NULL, hccp_err("param error ! fd:%d < 0, size:%llu or data is NULL",
        fd, size), -EINVAL);

    if (sslEnable != RS_SSL_DISABLE) {
#ifdef CONFIG_SSL
        int err;
        struct rs_conn_info *conn = NULL;

        ret = RsFd2conn(fd, &conn);
        CHK_PRT_RETURN(ret, hccp_err("fd to conn failed, ret:%d", ret), ret);
        ret = ssl_adp_write(conn->ssl, data, (int)size);
        if (ret <= 0) {
            hccp_warn("ssl_adp_write ret:%d, size:%llu", ret, size);
            err = ssl_adp_get_error(conn->ssl, ret);
            rs_ssl_err_string(conn->connfd, err);
            CHK_PRT_RETURN((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ), hccp_info("ssl_adp_write need"
                "to retry"), -EAGAIN);
        }
#endif
    } else {
        ret = (int)send(fd, data, size, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (ret < 0) {
            errNo = errno;
            if (errNo == EAGAIN || errNo == EINTR) {
                hccp_dbg("send to fd:%d need retry, send size:%llu, ret:%d, errno:%d", fd, size, ret, errNo);
                ret = -EAGAIN;
            } else {
                hccp_run_info("send to fd:%d not success, send size:%llu, ret:%d, errno:%d", fd, size, ret, errNo);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}

RS_ATTRI_VISI_DEF int RsSocketRecv(int fd, void *data, uint64_t size)
{
    int ret;

    ret = RsDrvSocketRecv(fd, data, size, MSG_DONTWAIT);

    return ret;
}

RS_ATTRI_VISI_DEF int RsPeerSocketRecv(uint32_t sslEnable, int fd, void *data, uint64_t size)
{
    int ret = 0;
    int errNo;

    CHK_PRT_RETURN(fd < 0 || data == NULL || size == 0, hccp_err("param error ! fd:%d < 0 or data is NULL, size:%llu",
        fd, size), -EINVAL);

    if (sslEnable != RS_SSL_DISABLE) {
#ifdef CONFIG_SSL
        int err;
        struct rs_conn_info *conn = NULL;

        ret = RsFd2conn(fd, &conn);
        CHK_PRT_RETURN(ret, hccp_warn("can not find conn for fd[%d], ret:%d, the local fd may have been closed ",
            fd, ret), ret);
        ret = ssl_adp_read(conn->ssl, data, (int)size);
        if (ret <= 0) {
            hccp_dbg("ssl_adp_read ret:%d, size:%llu", ret, size);
            err = ssl_adp_get_error(conn->ssl, ret);
            rs_ssl_err_string(conn->connfd, err);
            CHK_PRT_RETURN((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ), hccp_dbg("ssl_adp_read"
                "need to retry"), -EAGAIN);
        }
#endif
    } else {
        ret = (int)recv(fd, data, size, MSG_DONTWAIT);
        if (ret < 0) {
            errNo = errno;
            // not to print to avoid log flush
            if (errNo == EAGAIN || errNo == EINTR) {
                ret = -EAGAIN;
            } else {
                hccp_run_info("recv for fd:%d not success, recv size:%llu, ret:%d, errNo:%d", fd, size, ret, errNo);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}

RS_ATTRI_VISI_DEF int RsSocketGetClientSocketErrInfo(struct socket_connect_info conn[],
    struct socket_err_info err[], unsigned int num)
{
    struct rs_conn_info *connInfo = NULL;
    unsigned int i, serverPort;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    RS_CHECK_POINTER_NULL_WITH_RET(err);
    for (i = 0; i < num; i++) {
        serverPort = conn[i].port;
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
        ret = RsGetConnInfo(&gRsCb->conn_cb, &conn[i], &connInfo, serverPort);
        if (ret != 0) {
            hccp_err("rs_get_conn_info failed, i:%u ret:%d", i, ret);
            RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
            return ret;
        }

        (void)memcpy_s(&err[i], sizeof(struct socket_err_info), &connInfo->err_info, sizeof(struct socket_err_info));

        // clear the singer socket connect err info
        (void)memset_s(&connInfo->err_info, sizeof(struct socket_err_info), 0, sizeof(struct socket_err_info));
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketGetServerSocketErrInfo(struct socket_listen_info conn[],
    struct server_socket_err_info err[], unsigned int num)
{
    struct rs_listen_info *listenInfo = NULL;
    struct rs_ip_addr_info ipInfo = {0};
    struct rs_conn_cb *connCb = NULL;
    unsigned int i, serverPort;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    RS_CHECK_POINTER_NULL_WITH_RET(err);
    for (i = 0; i < num; i++) {
        ret = RsConvertIpAddr(conn[i].family, &conn[i].local_ip, &ipInfo);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, i:%u, ret:%d", i, ret), ret);

        serverPort = conn[i].port;
        RS_PTHREAD_MUTEX_LOCK(&gRsCb->mutex);
        connCb = &gRsCb->conn_cb;
        ret = RsFindListenNode(connCb, &ipInfo, serverPort, &listenInfo);
        if (ret != 0) {
            hccp_err("rs_find_listen_node failed, i:%u, ip:%s, serverPort:%u, ret:%d",
                i, ipInfo.read_addr, serverPort, ret);
            RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
            return ret;
        }

        (void)memcpy_s(&err[i].epoll_wait, sizeof(struct socket_err_info),
            &connCb->epoll_err_info, sizeof(struct socket_err_info));
        (void)memcpy_s(&err[i].accept, sizeof(struct socket_err_info),
            &listenInfo->err_info, sizeof(struct socket_err_info));

        // clear the single socket listen err info
        (void)memset_s(&listenInfo->err_info, sizeof(struct socket_err_info), 0, sizeof(struct socket_err_info));
        RS_PTHREAD_MUTEX_ULOCK(&gRsCb->mutex);
    }

    return 0;
}

static void RsSocketGetIpInfo(unsigned int *serverIp, unsigned int *clientIp)
{
    uint32_t serverNodeId = *serverIp;
    uint32_t clientNodeId = *clientIp;
    int ret;

    ret = RsSocketNodeid2vnic(serverNodeId, serverIp);
    hccp_info("white list listen IP 0x%llx, ret_vnic %d", *serverIp, ret);

    ret = RsSocketNodeid2vnic(clientNodeId, clientIp);
    hccp_info("white list client IP 0x%llx, ret_vnic %d", *clientIp, ret);

    return;
}

STATIC int RsSocketWhiteListAlloc(struct rs_conn_cb *connCb,
    struct socket_wlist_info_t *whiteList, struct rs_ip_addr_info *serverIp)
{
    int ret;
    /*lint -e429*/
    struct rs_white_list_info *whiteListNodeTmp = NULL;
    struct rs_white_list *whiteListTmp = NULL;
    struct socket_wlist_info_t wlist;
    struct rs_ip_addr_info clientIp;
    ret = memcpy_s(&wlist, sizeof(struct socket_wlist_info_t), whiteList, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memcpy socket_wlist_info_t wlist failed, ret[%d]!", ret), -ESAFEFUNC);

    if (serverIp->family == AF_INET) {
        RsSocketGetIpInfo(&serverIp->bin_addr.addr.s_addr, &(wlist.remote_ip.addr.s_addr));
        RsInetNtop(serverIp->family, &serverIp->bin_addr, (char *)&serverIp->read_addr,
            sizeof(serverIp->read_addr));
    }

    ret = RsConvertIpAddr(serverIp->family, &wlist.remote_ip, &clientIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

    ret = RsFindWhiteList(connCb, serverIp, &whiteListTmp);
    if (ret) {
        whiteListTmp = calloc(1, sizeof(struct rs_white_list));
        CHK_PRT_RETURN(whiteListTmp == NULL, hccp_err("alloc mem for rs_white_list failed!"), -ENOMEM);
        whiteListTmp->server_ip = *serverIp;
        RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
        RS_INIT_LIST_HEAD(&whiteListTmp->white_list);
        RsListAddTail(&whiteListTmp->list, &connCb->white_list);
        RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    }

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    ret = RsFindWhiteListNode(whiteListTmp, &wlist, (int)serverIp->family, &whiteListNodeTmp);
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    if (ret == 0) {
        whiteListNodeTmp->conn_limit += wlist.conn_limit;
        return 0;
    }

    whiteListNodeTmp = calloc(1, sizeof(struct rs_white_list_info));
    CHK_PRT_RETURN(whiteListNodeTmp == NULL, hccp_err("alloc mem for socket_wlist_info_t failed!"), -ENOMEM);

    whiteListNodeTmp->client_ip = clientIp;
    whiteListNodeTmp->conn_limit = wlist.conn_limit;
    ret = memcpy_s(whiteListNodeTmp->tag, SOCK_CONN_TAG_SIZE, wlist.tag, sizeof(wlist.tag));
    if (ret) {
        hccp_err("memcpy_s failed, ret[%d]. ", ret);
        free(whiteListNodeTmp);
        whiteListNodeTmp = NULL;
        return -ESAFEFUNC;
    }

    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    RsListAddTail(&whiteListNodeTmp->list, &whiteListTmp->white_list);
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    return 0;
    /*lint +e429*/
}

RS_ATTRI_VISI_DEF int RsSocketWhiteListSwitch(unsigned int phyId, unsigned int enable)
{
    struct rs_conn_cb *connCb = NULL;
    int ret;

    ret = RsDev2conncb(phyId, &connCb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev failed, ret:%d", ret), -1);
    connCb->wlist_enable = enable;
    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketWhiteListAdd(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[],
    unsigned int num)
{
    struct rs_conn_cb *connCb = &(gRsCb->conn_cb);
    struct rs_ip_addr_info serverIp;
    unsigned int i, chipId;
    int ret;

    ret = RsConvertIpAddr(rdevInfo.family, &rdevInfo.local_ip, &serverIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), -EINVAL);

    CHK_PRT_RETURN(num <= 0 || whiteList == NULL || num > RS_MAX_WLIST_NUM ||
        ((rdevInfo.family != AF_INET) && (rdevInfo.family != AF_INET6)) || rdevInfo.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("white list add param error, phyId[%u], server ip[%s], num[%u], family[%d]",
        rdevInfo.phy_id, serverIp.read_addr, num, rdevInfo.family), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(rdevInfo.phy_id, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    for (i = 0; i < num; ++i) {
        CHK_PRT_RETURN(strnlen(whiteList[i].tag, SOCK_CONN_TAG_SIZE) >= SOCK_CONN_TAG_SIZE,
            hccp_err("white_list tag len:%u more than max len:%d", strlen(whiteList[i].tag), SOCK_CONN_TAG_SIZE),
            -EINVAL);
        ret = RsSocketWhiteListAlloc(connCb, &whiteList[i], &serverIp);
        if (ret) {
            struct rs_ip_addr_info clientIp;
            ret = RsConvertIpAddr(serverIp.family, &whiteList->remote_ip, &clientIp);
            CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);
            hccp_err("add white list node failed, server ip[%s], client ip[%s], tag[%s], ret:%d",
                serverIp.read_addr, clientIp.read_addr, whiteList[i].tag, ret);
        }
    }
    return 0;
}

STATIC int RsSocketWhiteListNodeDestroy(struct rs_conn_cb *connCb,
    struct socket_wlist_info_t *whiteList, struct rs_ip_addr_info *serverIp)
{
    struct rs_white_list_info *whiteListNodeTmp = NULL;
    struct rs_white_list *whiteListTmp = NULL;
    struct socket_wlist_info_t wlist;
    struct rs_ip_addr_info clientIp;
    int ret;

    ret = RsConvertIpAddr((int)serverIp->family, &whiteList->remote_ip, &clientIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

    ret =  memset_s(&wlist, sizeof(struct socket_wlist_info_t), 0, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memset_s socket_wlist_info_t wlist failed, ret:%d", ret), -ESAFEFUNC);
    ret = memcpy_s(&wlist, sizeof(struct socket_wlist_info_t), whiteList, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memcpy socket_wlist_info_t wlist failed!"), -ESAFEFUNC);

    if (serverIp->family == AF_INET) {
        ret = RsSocketNodeid2vnic(serverIp->bin_addr.addr.s_addr, &serverIp->bin_addr.addr.s_addr);
        hccp_info("listen IP 0x%llx, ret_vnic %d", serverIp->bin_addr.addr.s_addr, ret);
        ret = RsSocketNodeid2vnic(wlist.remote_ip.addr.s_addr, &(wlist.remote_ip.addr.s_addr));
        hccp_info("client IP 0x%llx, ret_vnic %d", wlist.remote_ip.addr.s_addr, ret);
    }

    ret = RsFindWhiteList(connCb, serverIp, &whiteListTmp);
    CHK_PRT_RETURN(ret != 0, hccp_err("white list for IP(%s) doesn't exist! state:%d", serverIp->read_addr, ret), ret);
    RS_PTHREAD_MUTEX_LOCK(&connCb->conn_mutex);
    ret = RsFindWhiteListNode(whiteListTmp, &wlist, (int)serverIp->family, &whiteListNodeTmp);
    if (ret == 0) {
        RsListDel(&whiteListNodeTmp->list);
        free(whiteListNodeTmp);
        whiteListNodeTmp = NULL;
        RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
        return 0;
    }
    RS_PTHREAD_MUTEX_ULOCK(&connCb->conn_mutex);
    hccp_info("can not find white list node: client ip[%s], tag[%s], ret:%d", clientIp.read_addr, wlist.tag, ret);
    return ret;
}

RS_ATTRI_VISI_DEF int RsSocketWhiteListDel(struct rdev rdevInfo,
    struct socket_wlist_info_t whiteList[], unsigned int num)
{
    struct rs_conn_cb *connCb = &(gRsCb->conn_cb);
    unsigned int i, chipId;
    struct rs_ip_addr_info serverIp;
    int ret;

    ret = RsConvertIpAddr(rdevInfo.family, &rdevInfo.local_ip, &serverIp);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);

    CHK_PRT_RETURN(num <= 0 || whiteList == NULL || num > RS_MAX_WLIST_NUM ||
        ((rdevInfo.family != AF_INET) && (rdevInfo.family != AF_INET6)) || rdevInfo.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("white list del param error, phyId[%u], server ip[%s], num[%u] family[%d]", rdevInfo.phy_id,
        serverIp.read_addr, num, rdevInfo.family),
        -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(rdevInfo.phy_id, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    for (i = 0; i < num; ++i) {
        CHK_PRT_RETURN(strlen(whiteList[i].tag) >= SOCK_CONN_TAG_SIZE, hccp_err("white_list tag len:%u more than"
            "max len:%d", strlen(whiteList[i].tag), SOCK_CONN_TAG_SIZE), -EINVAL);
        ret = RsSocketWhiteListNodeDestroy(connCb, &whiteList[i], &serverIp);
        if (ret) {
            struct rs_ip_addr_info clientIp;
            ret = RsConvertIpAddr(serverIp.family, &whiteList->remote_ip, &clientIp);
            CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip failed, ret:%d", ret), ret);
            hccp_info("white list node wait to delete, server ip[%s], client ip[%s], tag[%s], ret:%d",
                serverIp.read_addr, clientIp.read_addr, whiteList[i].tag, ret);
        }
    }
    return 0;
}

enum rs_hardware_type RsGetDeviceType(unsigned int phyId)
{
    int64_t deviceInfo = 0;
    unsigned int boardType;
    unsigned int logicId;
    unsigned int chipId;
    int64_t boardId;
    int ret;
 
    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("invalid param phy_id[%u]", phyId), RS_HARDWARE_UNKNOWN);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret != 0, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), RS_HARDWARE_UNKNOWN);
    ret = DlDrvDeviceGetIndexByPhyId(chipId, &logicId);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_drv_device_get_index_by_phy_id failed, ret(%d), chipId(%u)",
        ret, chipId), RS_HARDWARE_UNKNOWN);

    ret = DlHalGetDeviceInfo(logicId, MODULE_TYPE_SYSTEM, INFO_TYPE_BOARD_ID, &boardId);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_get_device_info board_id failed, ret[%d]", ret), RS_HARDWARE_UNKNOWN);
    hccp_info("board_id is (0x%llx)", boardId);
    boardType = (unsigned int)((uint64_t)boardId & (0xfff0));
    ret = DlHalGetDeviceInfo(logicId, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &deviceInfo);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_get_device_info device_info failed, ret(%d), phyId(%u)", ret, phyId),
        RS_HARDWARE_UNKNOWN);

    // 910A场景判断逻辑
    if (DlHalPlatGetChip((uint64_t)deviceInfo) == CHIP_TYPE_910A) {
        if (((boardType & RS_BOARDID_PCIE_CARD_MASK) == RS_BOARDID_PCIE_CARD_MASK_VALUE) &&
            (boardType != RS_BOARDID_AI_SERVER_MODULE) && (boardType != RS_BOARDID_ARM_SERVER_AG)) {
            return RS_HARDWARE_PCIE;
        }
        return RS_HARDWARE_SERVER;
    }
 
    if (((boardType & RS_BOARDID_PCIE_CARD_MASK) == RS_BOARDID_PCIE_CARD_MASK_VALUE) &&
         (boardType != RS_BOARDID_AI_SERVER_MODULE) && (boardType != RS_BOARDID_ARM_SERVER_AG) &&
         (boardType != RS_BOARDID_ARM_POD) && (boardType != RS_BOARDID_X86_16P) &&
         (boardType != RS_BOARDID_ARM_SERVER_2DIE)) {
        return RS_HARDWARE_PCIE;
    }

    if ((boardType == RS_BOARDID_ARM_SERVER_2DIE)) {
        return RS_HARDWARE_2DIE;
    }
    return RS_HARDWARE_SERVER;
}

STATIC int RsCheckDstInterface(unsigned int phyId, const char *ifaName, enum rs_hardware_type type, bool isAll)
{
    char dstIfaBondName[RS_INTERFACE_BOND_LEN + 1] = {0};
    char dstIfaName[RS_INTERFACE_LEN + 1] = {0};
    int ret, bondRet;

    if (isAll) {
        /* get information of all device with eth or bond prefix */
        if (strncmp("eth", ifaName, RS_INTERFACE_ETH_PREFIX_LEN) != 0 &&
            strncmp("bond", ifaName, RS_INTERFACE_BOND_PREFIX_LEN) != 0) {
            return 0;
        }
        return 1;
    }

    // 标卡场景910B和910A device网卡固定为eth0,处理标卡场景
    if (type == RS_HARDWARE_PCIE) {
        if (strncmp("eth0", ifaName, RS_INTERFACE_LEN) && strncmp("eth1", ifaName, RS_INTERFACE_LEN)) {
            return 0;
        }
        return 1;
    } else if (type == RS_HARDWARE_2DIE) {
        /* 1. For RoH mode, only "bondx" port is supported when binding groups,
         * and "ethx" port is used when unbinding ;
         * 2. For eth mode, only the eth port is supported
         */
        ret = snprintf_s(dstIfaName, RS_INTERFACE_LEN + 1, RS_INTERFACE_LEN, "eth%u", phyId);
        bondRet = snprintf_s(dstIfaBondName, RS_INTERFACE_BOND_LEN + 1, RS_INTERFACE_BOND_LEN, "bond%u", phyId);
        if (ret <= 0 || bondRet <= 0) {
            hccp_err("copy eth or bond name failed, ret(%d), bondRet(%d)", ret, bondRet);
            return -EAGAIN;
        }

        if (strncmp(dstIfaName, ifaName, RS_INTERFACE_LEN) &&
            strncmp(dstIfaBondName, ifaName, RS_INTERFACE_BOND_LEN)) {
            return 0;
        }
    } else {
        ret = snprintf_s(dstIfaName, RS_INTERFACE_LEN + 1, RS_INTERFACE_LEN, "eth%u", phyId);
        CHK_PRT_RETURN(ret <= 0, hccp_err("copy eth name failed, %d", ret), -EAGAIN);
 
        if (strncmp(dstIfaName, ifaName, RS_INTERFACE_LEN)) {
            return 0;
        }
    }
    return 1;
}

STATIC int RsPeerFillIfnum(unsigned int phyId, unsigned int *num, struct ifaddrs *ifaddrList)
{
    struct ifaddrs *ifaddr = ifaddrList;
    struct ifaddrs *ifa = NULL;
    int family;

    *num = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        /* If not an AF_INET/AF_INET6 interface address, continue */
        if ((family != AF_INET) && (family != AF_INET6)) {
            continue;
        }
        (*num)++;
    }

    hccp_dbg("phy_id:%u got interface num:%u", phyId, *num);
    return 0;
}

STATIC int RsPeerFillIfaddrInfos(struct interface_info interfaceInfos[], unsigned int *num,
    unsigned int phyId, struct ifaddrs *ifaddrList)
{
    struct ifaddrs *ifaddr = ifaddrList;
    unsigned int numBak = *num;
    struct ifaddrs *ifa = NULL;
    int family, ret;
    *num = 0;

    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        // /* If not an AF_INET/AF_INET6 interface address, continue */
        if ((family != AF_INET) && (family != AF_INET6)) {
            continue;
        }

        (*num)++;
        if ((*num) > numBak) {
            hccp_err("num of interfaces found is more than expect, expect[%u], actual[%u]", numBak, *num);
            goto out;
        }
        ret = strcpy_s(interfaceInfos[*num - 1].ifname, MAX_INTERFACE_NAME_LEN, ifa->ifa_name);
        if (ret) {
            hccp_err("strcpy interface name failed, ret[%d]", ret);
            goto out;
        }
        interfaceInfos[*num - 1].scope_id = 0;
        if (family == AF_INET) {
            interfaceInfos[*num - 1].ifaddr.ip.addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            interfaceInfos[*num - 1].ifaddr.mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
            hccp_info("ifname[%s] addr[0x%08x]", ifa->ifa_name, ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr);
        } else {
            interfaceInfos[*num - 1].ifaddr.ip.addr6 = ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            interfaceInfos[*num - 1].scope_id = (int)((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id;
            hccp_info("ifname[%s] scope_id[%u] flowinfo[%u]", ifa->ifa_name,
                ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id,
                ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_flowinfo);
            for (unsigned long i = 0; i < sizeof(struct in6_addr); i++) {
                hccp_info("addr[%lu] 0x%02x", i, ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr.s6_addr[i]);
            }
        }
        interfaceInfos[*num - 1].family = family;
    }

    hccp_dbg("phy_id:%u got interface num:%u", phyId, *num);
    return 0;
out:
    hccp_dbg("phy_id:%u got interface num:%u", phyId, *num);
    return -EAGAIN;
}

// 获取device网卡信息，当前device网卡只支持IPv4
STATIC int RsFillIfaddrInfos(struct ifaddr_info ifaddrInfos[], unsigned int *num, unsigned int phyId)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    int family, ret;
    unsigned int numBak = *num;
    *num = 0;
    enum rs_hardware_type type;

    type = RsGetDeviceType(phyId);
    CHK_PRT_RETURN(type == RS_HARDWARE_UNKNOWN, hccp_err("rs_get_device_type failed, type[%d]", type), -EINVAL);
    ret = getifaddrs(&ifaddr);
    CHK_PRT_RETURN(ret == -1, hccp_err("get ifaddrs failed, ret[%d]", ret), -ESYSFUNC);
    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        /* If not an AF_INET/AF_INET6 interface address, continue */
        if (family != AF_INET) {
            continue;
        }
        ret = RsCheckDstInterface(phyId, ifa->ifa_name, type, false);
        if (ret < 0) {
            hccp_err("rs_check_dst_interface failed, ret[%d]", ret);
            goto out;
        }
        if (ret) {
            (*num)++;
            if ((*num) > numBak) {
                hccp_err("num of interfaces found is more than expect, expect[%u], actual[%u]", numBak, *num);
                goto out;
            }
            ifaddrInfos[*num - 1].ip.addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            ifaddrInfos[*num - 1].mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
        }
    }

    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return 0;
out:
    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return -EAGAIN;
}

// 获取device网卡信息，支持IPv4/IPV6
STATIC int RsFillIfaddrInfosV2(struct interface_info interfaceInfos[], unsigned int *num, unsigned int phyId,
    bool isAll)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    enum rs_hardware_type type;
    unsigned int numBak;
    int family, ret;

    numBak = *num;
    *num = 0;
    type = RsGetDeviceType(phyId);
    CHK_PRT_RETURN(type == RS_HARDWARE_UNKNOWN, hccp_err("rs_get_device_type failed, type[%d]", type), -EINVAL);
    ret = getifaddrs(&ifaddr);
    CHK_PRT_RETURN(ret != 0, hccp_err("get ifaddrs failed, ret[%d]", ret), -ESYSFUNC);
    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL) {
            continue;
        }

        /* If not an AF_INET/AF_INET6 interface address, continue */
        family = ifa->ifa_addr->sa_family;
        if ((family != AF_INET) && (family != AF_INET6)) {
            continue;
        }

        ret = RsCheckDstInterface(phyId, ifa->ifa_name, type, isAll);
        if (ret < 0) {
            hccp_err("rs_check_dst_interface failed, ret[%d]", ret);
            ret = -EAGAIN;
            break;
        }
        if (ret) {
            (*num)++;
            if ((*num) > numBak) {
                hccp_err("num of interfaces found is more than expect, expect[%u], actual[%u]", numBak, *num);
                ret = -EAGAIN;
                break;
            }

            ret = strcpy_s(interfaceInfos[*num - 1].ifname, MAX_INTERFACE_NAME_LEN, ifa->ifa_name);
            if (ret) {
                hccp_err("strcpy interface name failed, ret[%d]", ret);
                ret = -EAGAIN;
                break;
            }
            interfaceInfos[*num - 1].scope_id = 0;
            if (family == AF_INET) {
                interfaceInfos[*num - 1].ifaddr.ip.addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                interfaceInfos[*num - 1].ifaddr.mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
            } else {
                interfaceInfos[*num - 1].ifaddr.ip.addr6 = ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
                interfaceInfos[*num - 1].scope_id = (int)((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id;
            }
            interfaceInfos[*num - 1].family = family;
        }
    }

    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return ret;
}

STATIC int RsFillIfnum(unsigned int phyId, bool isAll, unsigned int *num, unsigned int isPeer)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    enum rs_hardware_type type;
    int family, ret;
    *num = 0;

    if (!isPeer) {
        type = RsGetDeviceType(phyId);
        CHK_PRT_RETURN(type == RS_HARDWARE_UNKNOWN, hccp_err("rs_get_device_type failed, type[%d]", type), -EINVAL);
    }
    ret = getifaddrs(&ifaddr);
    CHK_PRT_RETURN(ret == -1, hccp_err("get ifaddrs failed, ret[%d]", ret), -ESYSFUNC);
    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        /* If not an AF_INET/AF_INET6 interface address, continue */
        if ((family != AF_INET) && (family != AF_INET6)) {
            continue;
        }
        if (!isPeer) {
            ret = RsCheckDstInterface(phyId, ifa->ifa_name, type, isAll);
            if (ret < 0) {
                hccp_err("rs_check_dst_interface failed, ret[%d]", ret);
                goto out;
            }
            if (ret) {
                (*num)++;
            }
        } else {
            (*num)++;
        }
    }

    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return 0;
out:
    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return -EAGAIN;
}

RS_ATTRI_VISI_DEF int RsPeerGetIfnum(unsigned int phyId, unsigned int *num)
{
    int ret;
    CHK_PRT_RETURN(num == NULL, hccp_err("param error, num is NULL"), -EINVAL);
    CHK_PRT_RETURN(gRsCb == NULL, hccp_err("param error, gRsCb is NULL"), -EINVAL);
    ret = RsPeerFillIfnum(phyId, num, gRsCb->ifaddr_list);
    CHK_PRT_RETURN(ret, hccp_err("rs_peer_fill_ifnum failed, ret[%d]", ret), ret);
    return ret;
}

RS_ATTRI_VISI_DEF int RsGetIfnum(unsigned int phyId, bool isAll, unsigned int *num)
{
    int ret;
    CHK_PRT_RETURN(num == NULL, hccp_err("rs_get_ifaddrs param error, num is NULL"), -EINVAL);
    ret = RsFillIfnum(phyId, isAll, num, 0);
    CHK_PRT_RETURN(ret, hccp_err("rs_fill_ifnum failed, ret[%d]", ret), ret);
    return ret;
}

RS_ATTRI_VISI_DEF int RsPeerGetIfaddrs(struct interface_info interfaceInfos[], unsigned int *num,
    unsigned int phyId)
{
    int ret;
    CHK_PRT_RETURN(interfaceInfos == NULL || num == NULL,
        hccp_err("param error, interfaceInfos or num is NULL"), -EINVAL);
    CHK_PRT_RETURN(gRsCb == NULL, hccp_err("param error, gRsCb is NULL"), -EINVAL);
    ret = RsPeerFillIfaddrInfos(interfaceInfos, num, phyId, gRsCb->ifaddr_list);
    CHK_PRT_RETURN(ret, hccp_err("rs_peer_fill_ifaddr_infos failed, ret[%d]", ret), ret);
    return ret;
}

RS_ATTRI_VISI_DEF int RsGetIfaddrs(struct ifaddr_info ifaddrInfos[], unsigned int *num, unsigned int phyId)
{
    int ret;

    CHK_PRT_RETURN(ifaddrInfos == NULL || num == NULL, hccp_err("rs_get_ifaddrs param error,"
        "ifaddrInfos or num is NULL"), -EINVAL);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM || *num > MAX_INTERFACE_NUM, hccp_err("rs_get_ifaddrs param error,"
        "phyId[%u], num[%u]", phyId, *num), -EINVAL);

    ret = RsFillIfaddrInfos(ifaddrInfos, num, phyId);
    CHK_PRT_RETURN(ret, hccp_err("rs_fill_ifaddr_infos failed, ret[%d]", ret), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetIfaddrsV2(struct interface_info interfaceInfos[], unsigned int *num,
    unsigned int phyId, bool isAll)
{
    int ret;

    CHK_PRT_RETURN(interfaceInfos == NULL || num == NULL, hccp_err("rs_get_ifaddrs_v2 param error,"
        "interfaceInfos or num is NULL"), -EINVAL);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM || *num > MAX_INTERFACE_NUM, hccp_err("rs_get_ifaddrs_v2 param error,"
        "phyId[%u], num[%u]", phyId, *num), -EINVAL);

    ret = RsFillIfaddrInfosV2(interfaceInfos, num, phyId, isAll);
    CHK_PRT_RETURN(ret, hccp_err("rs_fill_ifaddr_infos_v2 failed, ret[%d]", ret), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int RsSocketSetScopeId(unsigned int devId, int scopeId)
{
    int ret;
    unsigned int chipId;
    struct rs_conn_cb *connCb = NULL;
    ret = rsGetLocalDevIDByHostDevID(devId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    ret = RsDev2conncb(chipId, &connCb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev failed, ret:%d", ret), ret);

    connCb->scope_id = scopeId;
    return 0;
}
