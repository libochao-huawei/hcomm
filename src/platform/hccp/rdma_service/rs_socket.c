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

static unsigned int g_vnics[RS_VNIC_MAX] = {0};

int rs_inet_ntop(int family, union hccp_ip_addr *ip, char read_addr[], unsigned int len)
{
    // IPv4/IPv6 二进制转字符串
    const char *str = NULL;
    str = inet_ntop(family, ip, read_addr, len);
    CHK_PRT_RETURN(str == NULL, hccp_err("[rs][inet_ntop]ip is a invalid, err(%d), family %d", errno, family), -EINVAL);
    return 0;
}

int rs_convert_ip_addr(int family, union hccp_ip_addr *ip_addr, struct rs_ip_addr_info *ip)
{
    // IPv4/IPv6 二进制转内部IP数据格式（含二进制、字符串）
    ip->family = (uint32_t)family;
    ip->bin_addr = *ip_addr;
    return rs_inet_ntop((int)ip->family, &ip->bin_addr, (char*)&ip->read_addr, sizeof(ip->read_addr));
}

void show_conn_node(struct rs_list_head *list_head)
{
    struct rs_conn_info *conn_tmp = NULL;
    struct rs_conn_info *conn_tmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, list_head, list, struct rs_conn_info);  //lint !e613
    for (; (&conn_tmp->list) != list_head;    //lint !e613
        conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list)) { //lint !e453
        hccp_info("current server ip: %s, client ip:%s, fd:%d, state:%d, tag:%s", conn_tmp->server_ip.read_addr,
            conn_tmp->client_ip.read_addr, conn_tmp->connfd, conn_tmp->state, conn_tmp->tag); //lint !e453
    }
}

// return: true(IP不同), false(IP相同)
bool rs_compare_ip_addr(struct rs_ip_addr_info *a, struct rs_ip_addr_info *b)
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

STATIC int rs_find_listen_node(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *ip_addr, uint32_t server_port,
    struct rs_listen_info **listen_info)
{
    struct rs_listen_info *listen_tmp = NULL;
    struct rs_listen_info *listen_tmp2 = NULL;

    RS_CHECK_POINTER_NULL_WITH_RET(conn_cb);
    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    RS_LIST_GET_HEAD_ENTRY(listen_tmp, listen_tmp2, &conn_cb->listen_list, list, struct rs_listen_info);
    for (; (&listen_tmp->list) != &conn_cb->listen_list;
        listen_tmp = listen_tmp2, listen_tmp2 = list_entry(listen_tmp2->list.next, struct rs_listen_info, list)) {
        if ((!rs_compare_ip_addr(&listen_tmp->server_ip_addr, ip_addr)) && (listen_tmp->sock_port == server_port)) {
            *listen_info = listen_tmp;
            RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);

    hccp_info("listen node for IP(%s), server_port(%u) is not listen!", ip_addr->read_addr, server_port);
    return -ENODEV;
}

STATIC int rs_get_conn_info(struct rs_conn_cb *conn_cb, struct socket_connect_info *conn,
    struct rs_conn_info **conn_info, unsigned int server_port)
{
    int ret;
    struct rs_conn_info *conn_tmp = NULL;
    struct rs_conn_info *conn_tmp2 = NULL;
    struct rs_ip_addr_info ip_addr;

    RS_CHECK_POINTER_NULL_RETURN_INT(conn_cb);
    RS_CHECK_POINTER_NULL_RETURN_INT(conn);

    ret = rs_convert_ip_addr(conn->family, &conn->remote_ip, &ip_addr);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, &conn_cb->client_conn_list, list, struct rs_conn_info);
    for (; (&conn_tmp->list) != &conn_cb->client_conn_list;
        conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list)) {
        if ((!rs_compare_ip_addr(&conn_tmp->server_ip, &ip_addr)) && conn_tmp->port == server_port) {
            ret = strcmp(conn_tmp->tag, conn->tag);
            if (ret == 0) {
                *conn_info = conn_tmp;
                RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
                return 0;
            }
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);

    conn->tag[SOCK_CONN_TAG_SIZE - 1] = '\0';
    hccp_warn("conn node for IP(%s) server_port(%u) tag(%s) not found", ip_addr.read_addr, server_port, conn->tag);
    return -ENODEV;
}

STATIC int rs_listen_credit_limit_init(struct rs_listen_info *listen_info)
{
    int ret;

    ret = pthread_mutex_init(&listen_info->accept_credit_mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("mutex_init accept_credit_mutex failed, ret:%d", ret), -ESYSFUNC);
    return 0;
}

STATIC void rs_listen_credit_limit_deinit(struct rs_listen_info *listen_info)
{
    (void)pthread_mutex_destroy(&listen_info->accept_credit_mutex);
}

STATIC int rs_listen_node_alloc(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *ip_addr, uint32_t server_port,
    struct rs_listen_info **node)
{
    int ret;
    struct rs_listen_info *listen_info = NULL;

    ret = rs_find_listen_node(conn_cb, ip_addr, server_port, &listen_info);
    CHK_PRT_RETURN(ret == 0,
        hccp_info("listen node for IP(%s) exist! state:%u", ip_addr->read_addr, listen_info->state), -EEXIST);

    listen_info = calloc(1, sizeof(struct rs_listen_info));
    CHK_PRT_RETURN(listen_info == NULL, hccp_err("alloc mem for socket listen info fail !"), -ENOMEM);

    hccp_info("create listen node for IP(%s)!", ip_addr->read_addr);
    listen_info->server_ip_addr = *ip_addr;
    listen_info->state = RS_CONN_STATE_RESET;
    ret = rs_listen_credit_limit_init(listen_info);
    if (ret != 0) {
        hccp_err("rs_listen_credit_limit_init failed, ret:%d", ret);
        free(listen_info);
        listen_info = NULL;
        return ret;
    }

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    rs_list_add_tail(&listen_info->list, &conn_cb->listen_list);
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);

    *node = listen_info;

    return 0;
}

STATIC void rs_listen_node_free(struct rs_conn_cb *conn_cb, struct rs_listen_info *node)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(conn_cb);
    RS_CHECK_POINTER_NULL_RETURN_VOID(node);

    hccp_dbg("delete listen node for (IP %s : port %u)!", node->server_ip_addr.read_addr, node->sock_port);

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->rscb->mutex);
    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    rs_list_del(&node->list);
    rs_listen_credit_limit_deinit(node);
    free(node);
    node = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->rscb->mutex);

    return;
}

int rs_alloc_conn_node(struct rs_conn_info **conn, unsigned short server_port)
{
    struct rs_conn_info *conn_info;

    conn_info = calloc(1, sizeof(struct rs_conn_info));
    CHK_PRT_RETURN(conn_info == NULL, hccp_err("alloc mem for socket conn info fail !"), -ENOMEM);

    conn_info->port = server_port;
    conn_info->connfd = RS_FD_INVALID;
    conn_info->state = RS_CONN_STATE_RESET;

    *conn = conn_info;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_init(const unsigned int *vnic_ip, unsigned int num)
{
    int ret;

    // vnic_ip max num  is RA_MAX_VNIC_NUM(16) RS_MAX_VNIC_NUM is also 16
    CHK_PRT_RETURN(num > RS_MAX_VNIC_NUM || num == 0 || vnic_ip == NULL,
        hccp_err("param error, num:%u is 0 or bigger than %d, or vnic_ip is NULL", num, RS_MAX_VNIC_NUM), -EINVAL);

    ret = memcpy_s(&(g_vnics), sizeof(g_vnics), vnic_ip, sizeof(unsigned int) * num);
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s for vnic_ip failed ret[%d]", ret), -ESAFEFUNC);

    return 0;
}

int rs_socket_nodeid2vnic(uint32_t node_id, uint32_t *ip_addr)
{
    if (node_id >= RS_VNIC_MAX) {
        return -1; /* it means real nic */
    }

    CHK_PRT_RETURN(ip_addr == NULL, hccp_err("ip_addr is NULL, invalid"), -EINVAL);

    *ip_addr = g_vnics[node_id];

    return RS_VNIC_FLAG;
}

STATIC uint32_t rs_socket_vnic2nodeid(uint32_t ip_addr)
{
    uint32_t node_id;

    if (ip_addr < RS_VNIC_MAX) { /* ip_addr is actually dev_id for vnic */
        return ip_addr;
    }

    for (node_id = 0; node_id < RS_VNIC_MAX; node_id++) {
        if (g_vnics[node_id] == ip_addr) {
            break;
        }
    }

    if (node_id == RS_VNIC_MAX) {
        return ip_addr;
    }

    return node_id; /* it means virtual nic */
}

STATIC int rs_find_white_list_node(struct rs_white_list *rs_socket_white_list,
    struct socket_wlist_info_t *white_list_expect, int family, struct rs_white_list_info **white_list_node)
{
    int ret;
    struct rs_white_list_info *white_list_tmp = NULL;
    struct rs_white_list_info *white_list_tmp_2 = NULL;

    struct rs_ip_addr_info expect_ip;
    ret = rs_convert_ip_addr(family, &white_list_expect->remote_ip, &expect_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

    RS_CHECK_POINTER_NULL_WITH_RET(rs_socket_white_list);
    RS_LIST_GET_HEAD_ENTRY(white_list_tmp, white_list_tmp_2, &rs_socket_white_list->white_list, list,
        struct rs_white_list_info);
    for (; (&white_list_tmp->list) != &rs_socket_white_list->white_list;
        white_list_tmp = white_list_tmp_2, white_list_tmp_2 = list_entry(white_list_tmp_2->list.next,
        struct rs_white_list_info, list)) {
        hccp_info("client_ip %s 0x%08x, expect_ip %s 0x%08x",
            white_list_tmp->client_ip.read_addr, white_list_tmp->client_ip.bin_addr.addr.s_addr,
            expect_ip.read_addr, expect_ip.bin_addr.addr.s_addr);
        if ((!rs_compare_ip_addr(&white_list_tmp->client_ip, &expect_ip)) &&
            (strncmp(white_list_tmp->tag, white_list_expect->tag, SOCK_CONN_TAG_SIZE) == 0)) {
            *white_list_node = white_list_tmp;
            return 0;
        }
    }

    white_list_expect->tag[SOCK_CONN_TAG_SIZE - 1] = '\0';
    hccp_info("white list node for IP(%s), tag(%s) doesn't exist!", expect_ip.read_addr, white_list_expect->tag);
    return -ENODEV;
}

STATIC int rs_find_white_list(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *server_ip,
    struct rs_white_list **white_list)
{
    struct rs_white_list *white_list_tmp = NULL;
    struct rs_white_list *white_list_tmp_2 = NULL;

    RS_CHECK_POINTER_NULL_WITH_RET(conn_cb);
    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    RS_LIST_GET_HEAD_ENTRY(white_list_tmp, white_list_tmp_2, &conn_cb->white_list, list, struct rs_white_list);
    for (; (&white_list_tmp->list) != &conn_cb->white_list;
        white_list_tmp = white_list_tmp_2, white_list_tmp_2 = list_entry(white_list_tmp_2->list.next,
        struct rs_white_list, list)) {
        if (!rs_compare_ip_addr(server_ip, &white_list_tmp->server_ip)) {
            *white_list = white_list_tmp;
            RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);

    hccp_info("white list for IP(%s) doesn't exist!", server_ip->read_addr);
    return -ENODEV;
}

STATIC int rs_server_send_wlist_check_result(struct rs_conn_info *conn, bool flag)
{
    int ret;
    char invalid[] = "5a5a5";
    char valid[] = "a5a5a";

    if (flag == 0) {
        if ((g_rs_cb->ssl_enable == RS_SSL_ENABLE) && (conn->ssl != NULL)) {
            ret = ssl_adp_write(conn->ssl, valid, sizeof(valid));
        } else {
            ret = rs_socket_send(conn->connfd, valid, sizeof(valid));
        }
        CHK_PRT_RETURN(ret != sizeof(valid), hccp_err("white list server send valid flag failed! fd[%d], ret[%d]",
            conn->connfd, ret), -1);
    } else {
        if ((g_rs_cb->ssl_enable == RS_SSL_ENABLE) && (conn->ssl != NULL)) {
            ret = ssl_adp_write(conn->ssl, invalid, sizeof(invalid));
        } else {
            ret = rs_socket_send(conn->connfd, invalid, sizeof(invalid));
        }
        CHK_PRT_RETURN(ret != sizeof(invalid), hccp_err("white list server send invalid flag failed! fd[%d], ret[%d]",
            conn->connfd, ret), -1);
    }
    return 0;
}

STATIC void rs_socket_get_bind_by_chip(unsigned int chip_id, bool *bind_ip)
{
#define CHIP_NAME_910_93 "910_93"
    halChipInfo chip_info = { 0 };
    int64_t device_info = 0;
    unsigned int logic_id;
    int ret;

    // get chip info failed, return directly to avoid exit from batch connect
    ret = dl_drv_device_get_index_by_phy_id(chip_id, &logic_id);
    if (ret != 0) {
        hccp_warn("dl_drv_device_get_index_by_phy_id unsuccessful, ret(%d), chip_id(%u)", ret, chip_id);
        return;
    }
    ret = dl_hal_get_device_info(logic_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &device_info);
    if (ret != 0) {
        hccp_warn("dl_hal_get_device_info unsuccessful, ret(%d), logic_id(%u)", ret, logic_id);
        return;
    }

    // chip force to bind: 310P & 910_93
    if ((dl_hal_plat_get_chip((uint64_t)device_info) == CHIP_TYPE_310P) ||
        ((dl_hal_plat_get_chip((uint64_t)device_info) == CHIP_TYPE_910B_910_93) &&
         (dl_hal_plat_get_ver((uint64_t)device_info) >= VER_BIN5) &&
         (dl_hal_plat_get_ver((uint64_t)device_info) <= VER_BIN8))) {
        *bind_ip = true;
        return;
    }

    // get chip info, chip force to bind: 910_93
    ret = dl_hal_get_chip_info(logic_id, &chip_info);
    if (ret != 0) {
        hccp_warn("dl_hal_get_chip_info unsuccessful, ret(%d), logic_id(%u)", ret, logic_id);
        return;
    }
    if (strncmp((char *)chip_info.name, CHIP_NAME_910_93, sizeof(CHIP_NAME_910_93) - 1) == 0) {
        *bind_ip = true;
    }

    return;
}

STATIC bool rs_socket_is_vnic_ip(unsigned int chip_id, unsigned int ip_addr)
{
    unsigned int vnic_ip = 0;
    int64_t device_info = 0;
    unsigned int phy_id = 0;
    bool bind_ip = false;
    int hccp_mode;
    int ret;

    // no need to handle other mode, only need to handle HDC mode
    hccp_mode = rs_get_hccp_mode(chip_id);
    if (hccp_mode != NETWORK_OFFLINE) {
        return false;
    }

    // check chip info: 310P & 910_93 will force to bind, no need to compare ip_addr with vnic ip
    rs_socket_get_bind_by_chip(chip_id, &bind_ip);
    if (bind_ip) {
        return false;
    }

    // compare ip_addr with current vnic_ip
    ret = rsGetDevIDByLocalDevID(chip_id, &phy_id);
    if (ret != 0) {
        hccp_warn("rsGetDevIDByLocalDevID unsuccessful, ret(%d), chip_id(%u)", ret, chip_id);
        return false;
    }

    ret = dl_hal_get_device_info(phy_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &device_info);
    if (ret != 0) {
        hccp_warn("dl_hal_get_device_info unsuccessful, ret(%d), chip_id(%u), phy_id(%u)", ret, chip_id, phy_id);
        return false;
    }

    vnic_ip = (unsigned int)device_info;
    hccp_dbg("chip_id:%u phy_id:%u vnic_ip:%u ip_addr:%u", chip_id, phy_id, vnic_ip, ip_addr);
    if (vnic_ip == ip_addr) {
        return true;
    }

    return false;
}

STATIC int rs_socket_fill_wlist_by_phyID(unsigned int chip_id, struct socket_wlist_info_t *white_list_node,
    struct rs_conn_info *rs_conn)
{
    unsigned int vnic_ip = 0;
    int64_t device_info = 0;
    char *tag_temp = NULL;
    unsigned int phy_id;
    int ret;

    ret = memcpy_s(white_list_node->tag, SOCK_CONN_TAG_SIZE, rs_conn->tag, SOCK_CONN_TAG_SIZE);
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s failed, ret[%d]", ret), -ESAFEFUNC);

    if (rs_conn->client_ip.family == AF_INET) {
        // compare server_ip with current vnic_ip: use client_ip as remote_ip if it has bound or not vnic ip
        if (!rs_socket_is_vnic_ip(chip_id, rs_conn->server_ip.bin_addr.addr.s_addr)) {
            // NIC IPv4
            white_list_node->remote_ip.addr.s_addr = rs_conn->client_ip.bin_addr.addr.s_addr;
            return 0;
        }
    } else {
        // NIC IPv6
        white_list_node->remote_ip = rs_conn->client_ip.bin_addr;
        return 0;
    }

    tag_temp = rs_conn->tag + SOCK_CONN_TAG_SIZE;
    tag_temp[SOCK_CONN_DEV_ID_SIZE - 1] = '\0';
    RS_CHECK_POINTER_NULL_RETURN_INT(tag_temp);
    if (rs_conn->client_ip.family == AF_INET) {
        // VNIC
        phy_id = (unsigned int)strtol(tag_temp, NULL, 10); // Decimal(10)
        ret = dl_hal_get_device_info(phy_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VNIC_IP, &device_info);
        CHK_PRT_RETURN(ret, hccp_err("dl_hal_get_device_info failed, ret(%d) tag_temp phy_id(%u)", ret, phy_id), ret);
        vnic_ip = (unsigned int)device_info;
        hccp_dbg("chip_id:%u phy_id:%u vnic_ip:%u", chip_id, phy_id, vnic_ip);
        white_list_node->remote_ip.addr.s_addr = vnic_ip;
    }
    return 0;
}

STATIC int rs_server_valid_async_init(unsigned int chip_id, struct rs_conn_info *conn,
    struct socket_wlist_info_t *white_list_expect)
{
    int ret;

    ret = memset_s(white_list_expect, sizeof(struct socket_wlist_info_t), 0, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memset_s socket_wlist_info_t wlist failed, ret:%d", ret), -ESAFEFUNC);

    CHK_PRT_RETURN(conn->state != RS_CONN_STATE_TAG_SYNC, hccp_err("conn state is not RS_CONN_STATE_TAG_SYNC,"
        "state[%u]. ", conn->state), -1);

    ret = rs_socket_fill_wlist_by_phyID(chip_id, white_list_expect, conn);
    CHK_PRT_RETURN(ret, hccp_err("rs_socket_fill_wlist_by_phyID failed, ret[%d]. ", ret), ret);

    return 0;
}

STATIC int rs_server_valid_async(unsigned int chip_id, struct rs_conn_cb *conn_cb, struct rs_conn_info *conn)
{
    int ret;
    struct rs_white_list *white_list_tmp = NULL;
    struct rs_white_list_info *white_list_node_tmp = NULL;
    struct socket_wlist_info_t white_list_expect;

    ret = rs_server_valid_async_init(chip_id, conn, &white_list_expect);
    CHK_PRT_RETURN(ret, hccp_err("rs server valid async init failed, ret:%d", ret), -1);

    ret = rs_find_white_list(conn_cb, &conn->server_ip, &white_list_tmp);
    if (ret) {
        ret = rs_server_send_wlist_check_result(conn, 1);
        CHK_PRT_RETURN(ret, hccp_err("rs server send wlist check invalid result failed, connfd[%d], ret[%d]",
            conn->connfd, ret), -1);
        hccp_info("white list can not be found, connfd[%d], server_ip[%s], ret[%d]", conn->connfd,
            conn->server_ip.read_addr, ret);
        return -1;
    }

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    ret = rs_find_white_list_node(white_list_tmp, &white_list_expect, (int)conn->client_ip.family,
        &white_list_node_tmp);
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    if (ret) {
        ret = rs_server_send_wlist_check_result(conn, 1);
        CHK_PRT_RETURN(ret, hccp_err("rs server send wlist check invalid result failed, connfd[%d], ret[%d]",
            conn->connfd, ret), -1);
        hccp_info("white list node can not be found, connfd[%d], ret[%d]", conn->connfd, ret);
        return -1;
    }

    if (white_list_node_tmp->conn_limit < 1) {
        ret = rs_server_send_wlist_check_result(conn, 1);
        CHK_PRT_RETURN(ret, hccp_err("rs_server_send_wlist_check_result failed, connfd[%d], conn_limit[%u], ret[%d]",
            conn->connfd, white_list_node_tmp->conn_limit, ret), -1);
        hccp_info("white list node limit has less than 1, connfd[%d], ret[%d]", conn->connfd, ret);
        return -1;
    }

    ret = rs_server_send_wlist_check_result(conn, 0);
    CHK_PRT_RETURN(ret, hccp_err("rs server send wlist check valid result failed, connfd[%d], ret[%d]",
        conn->connfd, ret), -1);
    white_list_node_tmp->conn_limit--;
    return 0;
}

int rs_socket_copy_conn_info(struct rs_conn_info *conn_tmp, struct rs_conn_info *conn)
{
    int ret;

    conn->server_ip = conn_tmp->server_ip;
    conn->client_ip = conn_tmp->client_ip;
    conn->connfd = conn_tmp->connfd;
    conn->state = conn_tmp->state;
    conn->port = conn_tmp->port;
    conn->ssl = conn_tmp->ssl;
    ret = memcpy_s(conn->tag, SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE, conn_tmp->tag, sizeof(conn_tmp->tag));
    if (ret) {
        hccp_err("rs_conn_info tag copy failed, ret[%d]", ret);
    }
    conn->is_got = false;
    return ret;
}

int rs_white_list_check_valid(unsigned int chip_id, struct rs_conn_cb *conn_cb, struct rs_conn_info *conn)
{
    int ret;

    ret = rs_server_valid_async(chip_id, conn_cb, conn);
    if (ret) {
        RS_CLOSE_RETRY_FOR_EINTR(ret, conn->connfd);
        hccp_info("rs_server_valid_async, white list doesn't exist, ret[%d]", ret);
        return -1;
    } else {
        conn->state = RS_CONN_STATE_VALID_SYNC;
    }
    return 0;
}

STATIC int rs_set_fd_nonblock(int connfd)
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

STATIC int rs_socket_set_fd_timeout_usec(int connfd, unsigned int tv_usec)
{
    struct timeval tv = { 0 };
    int ret = 0;

    tv.tv_usec = tv_usec;
    ret = setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    CHK_PRT_RETURN(ret < 0, hccp_err("setsockopt connfd %d SO_SNDTIMEO tv_usec %u failed %d", connfd, tv_usec, ret),
        -EFILEOPER);

    ret = setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    CHK_PRT_RETURN(ret < 0, hccp_err("setsockopt connfd %d SO_RCVTIMEO tv_usec %u failed %d", connfd, tv_usec, ret),
        -EFILEOPER);

    return 0;
}

STATIC void rs_epoll_event_ssl_listen_in_handle(struct rs_cb *rs_cb, struct rs_listen_info *listen_info, int connfd,
    struct rs_ip_addr_info *remote_ip)
{
    /*lint -e593*/
    int ret;
    struct rs_accept_info *accept_info = NULL;
    struct rs_list_head *list_head = NULL;

    ret = rs_epoll_ctl(rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD, connfd, EPOLLIN | EPOLLRDHUP);
    if (ret) {
        hccp_err("epoll ctl add fd %d failed", connfd);
        goto out;
    }

    hccp_info("epoll ctl add fd %d success", connfd);
    accept_info = calloc(1, sizeof(struct rs_accept_info));
    if (accept_info == NULL) {
        hccp_err("alloc mem for socket conn info fail !");
        goto out;
    }

    accept_info->sock_port = listen_info->sock_port;
    accept_info->server_ip_addr = listen_info->server_ip_addr;
    accept_info->client_ip_addr = *remote_ip;
    accept_info->conn_fd = connfd;
    RS_PTHREAD_MUTEX_LOCK(&rs_cb->conn_cb.conn_mutex);
    list_head = &rs_cb->conn_cb.server_accept_list;
    rs_list_add_tail(&accept_info->list, list_head);
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->conn_cb.conn_mutex);

    return;

out:
    RS_CLOSE_RETRY_FOR_EINTR(ret, connfd);
    return;
    /*lint +e593*/
}

STATIC int rs_tcp_recv_tag_in_handle(struct rs_listen_info *listen_info, int connfd, struct rs_conn_info *conn_tmp,
    struct rs_ip_addr_info *remote_ip)
{
    int exp_size = SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE;
    char *recv_buff = conn_tmp->tag;
    struct timeval start_time, now;
    float time_cost = 0.0;
    int size = exp_size;

    rs_get_cur_time(&start_time);
    while (exp_size > 0 && size != 0) {
        conn_tmp->tag_sync_time++;
        size = recv(connfd, recv_buff, exp_size, 0);
        if ((size < 0) && (errno == EINTR)) {
            conn_tmp->tag_eintr_time++;
            continue;
        }
        // peer socket session has been closed
        if (size == 0) {
            hccp_run_info("session has been clsoed, server:{%s:%u} client:%s tag_sync_time:%u tag_eintr_time:%u",
                listen_info->server_ip_addr.read_addr, listen_info->sock_port, remote_ip->read_addr,
                conn_tmp->tag_sync_time, conn_tmp->tag_eintr_time);
            return -ESOCKCLOSED;
        }

        exp_size -= size;
        recv_buff += size;
        rs_get_cur_time(&now);
        hccp_time_interval(&now, &start_time, &time_cost);
        // enlarge the timeout threshold to make sure the connection can be established successfully
        if (time_cost >= RS_RECV_TAG_MAX_TIME) {
            hccp_run_info("recv tag time out, server:{%s:%u} client:%s tag_sync_time:%u tag_eintr_time:%u",
                listen_info->server_ip_addr.read_addr, listen_info->sock_port, remote_ip->read_addr,
                conn_tmp->tag_sync_time, conn_tmp->tag_eintr_time);
            return -ETIME;
        }

        if (time_cost <= 0) {
            rs_get_cur_time(&start_time);
        }
    }

    conn_tmp->server_ip = listen_info->server_ip_addr;
    conn_tmp->client_ip = *remote_ip;
    conn_tmp->connfd = connfd;
    conn_tmp->state = RS_CONN_STATE_TAG_SYNC;
    conn_tmp->port = listen_info->sock_port;
    if (time_cost >= RS_RECV_MAX_TIME) {
        hccp_run_info("recv tag success, server:{%s:%u} client:%s time_cost:%fms tag_sync_time:%u tag_eintr_time:%u",
            listen_info->server_ip_addr.read_addr, listen_info->sock_port, remote_ip->read_addr, time_cost,
            conn_tmp->tag_sync_time, conn_tmp->tag_eintr_time);
        return 0;
    }

    hccp_info("recv tag success, server:{%s:%u} client:%s time_cost:%fms tag_sync_time:%u tag_eintr_time:%u",
        listen_info->server_ip_addr.read_addr, listen_info->sock_port, remote_ip->read_addr, time_cost,
        conn_tmp->tag_sync_time, conn_tmp->tag_eintr_time);
    return 0;
}

STATIC void rs_epoll_event_tcp_listen_in_handle(struct rs_cb *rs_cb, struct rs_listen_info *listen_info, int connfd,
    struct rs_ip_addr_info *remote_ip)
{
    struct rs_conn_info conn_tmp = {0};
    int ret;

    ret = rs_tcp_recv_tag_in_handle(listen_info, connfd, &conn_tmp, remote_ip);
    if (ret != 0) {
        hccp_warn("rs_tcp_recv_tag_in_handle unsuccessful, ret:%d", ret);
        RS_CLOSE_RETRY_FOR_EINTR(ret, connfd);
        return;
    }

    ret = rs_wlist_check_conn_add(rs_cb, &conn_tmp);
    if (ret != 0) {
        hccp_warn("rs_wlist_check_conn_add unsuccessful, ret %d", ret);
        return;
    }

    return;
}

void rs_socket_save_err_info(int action, int err_no, struct socket_err_info *err_info)
{
    // Only record the first occurrence of err information
    if (err_info->err_no != 0) {
        return;
    }

    if (err_no == -EAGAIN || err_no == -EINTR) {
        return;
    }

    rs_get_cur_time(&err_info->time);
    err_info->action = action;
    err_info->err_no = err_no;
}

STATIC int rs_socket_listen_del_from_epoll(struct rs_conn_cb *conn_cb, struct rs_listen_info *listen_info)
{
    int ret = 0;

    RS_PTHREAD_MUTEX_LOCK(&listen_info->accept_credit_mutex);
    if (listen_info->fd_state == LISTEN_FD_STATE_DELETED) {
        goto out;
    }

    hccp_run_info("IP:%s server_port:%u listen_fd:%d del from epoll:%d", listen_info->server_ip_addr.read_addr,
        listen_info->sock_port, listen_info->listen_fd, conn_cb->epollfd);
    ret = rs_epoll_ctl(conn_cb->epollfd, EPOLL_CTL_DEL, listen_info->listen_fd, EPOLLIN);
    if (ret != 0) {
        hccp_err("IP:%s server_port:%u listen_fd:%d rs_epoll_ctl failed, ret:%d errno:%d",
            listen_info->server_ip_addr.read_addr, listen_info->sock_port, listen_info->listen_fd, ret, errno);
        goto out;
    }

    listen_info->fd_state = LISTEN_FD_STATE_DELETED;

out:
    RS_PTHREAD_MUTEX_ULOCK(&listen_info->accept_credit_mutex);
    return ret;
}

STATIC int rs_socket_check_credit(struct rs_conn_cb *conn_cb, struct rs_listen_info *listen_info)
{
    // not using accept_credit, no need to check
    if (!listen_info->accept_credit_flag) {
        return 0;
    }

    // accept_credit is exhaused, check failed
    if (listen_info->accept_credit_limit == 0) {
        return -EINVAL;
    }

    RS_PTHREAD_MUTEX_LOCK(&listen_info->accept_credit_mutex);
    listen_info->accept_credit_limit--;
    RS_PTHREAD_MUTEX_ULOCK(&listen_info->accept_credit_mutex);

    // accept_credit is exhaused, ignore return value to delete from epoll
    if (listen_info->accept_credit_limit == 0) {
        (void)rs_socket_listen_del_from_epoll(conn_cb, listen_info);
    }

    return 0;
}

int rs_epoll_event_listen_in_handle(struct rs_cb *rs_cb, int fd)
{
    struct rs_listen_info *listen_info2 = NULL;
    struct rs_listen_info *listen_info = NULL;
    struct rs_socketaddr_info remote_s_addr;
    struct rs_ip_addr_info remote_ip;
    int connfd = RS_FD_INVALID;
    int tcp_nodelay_flag = 1;
    int ret, ret_close;
    socklen_t ip_len;

    /* Server event: Connection accept */
    RS_LIST_GET_HEAD_ENTRY(listen_info, listen_info2, &rs_cb->conn_cb.listen_list, list, struct rs_listen_info);
    for (; (&listen_info->list) != &rs_cb->conn_cb.listen_list;
        listen_info = listen_info2, listen_info2 = list_entry(listen_info2->list.next, struct rs_listen_info, list)) {
        /* connection request for Server */
        if (fd == listen_info->listen_fd) {
            ret = rs_socket_check_credit(&rs_cb->conn_cb, listen_info);
            CHK_PRT_RETURN(ret != 0,
                hccp_warn("[server]rs_socket_check_credit unsuccessful, server_ip:%s server_port:%u ret:%d",
                listen_info->server_ip_addr.read_addr, listen_info->sock_port, ret), -EINVAL);

            remote_s_addr.family = (int)listen_info->server_ip_addr.family;
            ip_len = (remote_s_addr.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
            do {
                connfd = accept(fd, (struct sockaddr *)&remote_s_addr.addr, &ip_len);
            } while ((connfd < 0) && (errno == EINTR));

            // accept failed and errno is the same with the last time, avoid log flush
            ret = errno;
            if (connfd < 0 && listen_info->last_accept_errno == ret) {
                hccp_warn("[server]server_ip:%s server_port:%u accept() unsuccessful! errno:%d",
                    listen_info->server_ip_addr.read_addr, listen_info->sock_port, ret);
                return -EINVAL;
            }
            listen_info->last_accept_errno = ret;

            if (connfd < 0) {
                hccp_err("[server]server_ip:%s server_port:%u accept() failed! errno:%d",
                    listen_info->server_ip_addr.read_addr, listen_info->sock_port, ret);
                goto err_accept;
            }

            hccp_info("[server]server_ip:%s server_port:%u accept ok @ listen_fd:%d, new fd:%d",
                listen_info->server_ip_addr.read_addr, listen_info->sock_port, fd, connfd);

            remote_ip.family = (uint32_t)remote_s_addr.family;
            if (remote_ip.family == AF_INET) {
                remote_ip.bin_addr.addr = remote_s_addr.addr.s_addr.sin_addr;
            } else {
                remote_ip.bin_addr.addr6 = remote_s_addr.addr.s_addr6.sin6_addr;
            }

            ret = rs_inet_ntop(remote_ip.family, &remote_ip.bin_addr, remote_ip.read_addr, sizeof(remote_ip.read_addr));
            if (ret) {
                hccp_err("[server]convert(ntop) ip fail:remote_ip.family:%d, remote_ip:%d, ret:%d, server_ip:%s "
                    "server_port:%u", remote_ip.family, remote_ip.bin_addr.addr.s_addr, ret,
                    listen_info->server_ip_addr.read_addr, listen_info->sock_port);
                goto err_event_listen;
            }

            if (rs_cb->ssl_enable == RS_SSL_ENABLE) {
                ret = rs_set_fd_nonblock(connfd);
                if (ret) {
                    hccp_err("[server]fcntl connfd %d nonblock failed %d, server_ip:%s server_port:%u",
                        connfd, ret, listen_info->server_ip_addr.read_addr, listen_info->sock_port);
                    goto err_event_listen;
                }
            }

            /* set tcp socket tos RS_TCP_DSCP_0 */
            int tos_local = (RS_TCP_DSCP_0 & RS_DSCP_MASK) << RS_DSCP_OFF;
            ret = setsockopt(connfd, IPPROTO_IP, IP_TOS, (void *)&tos_local, sizeof(tos_local));
            if (ret) {
                hccp_err("[server]setsockopt(IP_TOS) fail, ret:%d, errno:%d, server_ip:%s server_port:%u",
                    ret, errno, listen_info->server_ip_addr.read_addr, listen_info->sock_port);
                goto err_socket_option;
            }

            ret = setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (void *)&tcp_nodelay_flag, sizeof(int));
            if (ret < 0) {
                hccp_err("[server]setsockopt(TCP_NODELAY) fail, ret:%d, errno:%d, server_ip:%s server_port:%u",
                    ret, errno, listen_info->server_ip_addr.read_addr, listen_info->sock_port);
                goto err_socket_option;
            }

            if (rs_cb->ssl_enable == RS_SSL_ENABLE) {
                rs_epoll_event_ssl_listen_in_handle(rs_cb, listen_info, connfd, &remote_ip);
            } else {
                rs_epoll_event_tcp_listen_in_handle(rs_cb, listen_info, connfd, &remote_ip);
            }
            return 0;
        }
    }

    return -ENODEV;

err_socket_option:
    ret = -errno;
err_event_listen:
    RS_CLOSE_RETRY_FOR_EINTR(ret_close, connfd);
err_accept:
    rs_socket_save_err_info((int)listen_info->state, ret, &listen_info->err_info);
    return -ESYSFUNC;
}

STATIC int rs_socket_listen_bind_listen(int listen_fd, struct rs_conn_cb *conn_cb,
    struct socket_listen_info *conn, struct rs_listen_info *listen_info, uint32_t server_port)
{
    int is_reuse_addr = 1;
    int ret, err_no;

    ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &is_reuse_addr, sizeof(is_reuse_addr));
    if (ret) {
        err_no = errno;
        hccp_err("set socket op fail! IP:%s, port:%u, sock:%d, ret:0x%x, error:%d",
            listen_info->server_ip_addr.read_addr, server_port, listen_fd, ret, err_no);
        conn->phase = LISTEN_BIND_ERR;
        return -ESYSFUNC;
    }

    listen_info->state = RS_CONN_STATE_INIT;

    hccp_info("listen state:%d, then bind for (IP %s : port %u)",
        listen_info->state, listen_info->server_ip_addr.read_addr, server_port);

    hccp_run_info("socket bind: family %d, addr %s, port %u", conn->family, listen_info->server_ip_addr.read_addr,
        server_port);
    if (conn->family == AF_INET) {
        struct sockaddr_in addr = {0};
        addr.sin_family = conn->family;
        addr.sin_port = htons(server_port);
        addr.sin_addr.s_addr = listen_info->server_ip_addr.bin_addr.addr.s_addr;
        hccp_info("socket bind: family %d, port %d, addr 0x%08x", addr.sin_family, addr.sin_port, addr.sin_addr.s_addr);
        ret = bind(listen_fd, &addr, sizeof(addr));
    } else {
        struct sockaddr_in6 addr = {0};
        addr.sin6_family = conn->family;
        addr.sin6_port = htons(server_port);
        addr.sin6_addr = listen_info->server_ip_addr.bin_addr.addr6;
        addr.sin6_scope_id = (uint32_t)conn_cb->scope_id;
        hccp_info("socket bind: family %d, port %d, scope_id %d", addr.sin6_family, addr.sin6_port, addr.sin6_scope_id);
        for (unsigned long i = 0; i < sizeof(addr.sin6_addr.s6_addr); i++) {
            hccp_info("socket bind: addr[%lu] 0x%02x", i, addr.sin6_addr.s6_addr[i]);
        }
        ret = bind(listen_fd, &addr, sizeof(addr));
    }

    if (ret) {
        err_no = errno;
        if (err_no == EADDRINUSE) {
            hccp_run_warn("bind unsuccessful! family:%d, IP:%s, port:%u, sock:%d, ret:0x%x, error:%d, Possible Cause: "\
                "the IP address and port have been bound already", conn->family, listen_info->server_ip_addr.read_addr,
                server_port, listen_fd, ret, err_no);
        } else {
            hccp_err("bind fail! family:%d, IP:%s, port:%u, sock:%d, ret:0x%x, error:%d", conn->family,
                listen_info->server_ip_addr.read_addr, server_port, listen_fd, ret, err_no);
        }
        conn->phase = LISTEN_BIND_ERR;
        return err_no;
    }

    listen_info->state = RS_CONN_STATE_BIND;

    hccp_info("IP %s : port %u begin listen, fd:%d !", listen_info->server_ip_addr.read_addr, server_port, listen_fd);
    ret = listen(listen_fd, RS_SOCK_LISTEN_PARALLEL_NUM);
    if (ret) {
        err_no = errno;
        if (err_no == EADDRINUSE) {
            hccp_run_warn("listen unsuccessful! IP:%s, port:%u, sock:%d, ret:0x%x, errno:%d",
                listen_info->server_ip_addr.read_addr, server_port, listen_fd, ret, err_no);
        } else {
            hccp_err("listen fail! IP:%s, port:%u, sock:%d, ret:0x%x, errno:%d",
                listen_info->server_ip_addr.read_addr, server_port, listen_fd, ret, err_no);
        }
        conn->phase = LISTEN_BEGIN_ERR;
        return err_no;
    }

    return 0;
}

static int rs_socket_init_listen(struct socket_listen_info *conn, uint32_t i, struct rs_conn_cb **conn_cb,
    uint32_t server_port, struct rs_listen_info **listen_info)
{
    int ret;
    unsigned int chip_id;

    CHK_PRT_RETURN(((conn[i].family != AF_INET) && (conn[i].family != AF_INET6)) || conn[i].phy_id >= RS_MAX_DEV_NUM,
        hccp_err("family[%d] invalid, or phy_id[%u] invalid, i:%u", conn[i].family, conn[i].phy_id, i), -EINVAL);

    if (conn[i].family == AF_INET) {
        uint32_t *local_ip = NULL;
        local_ip = &(conn[i].local_ip.addr.s_addr);
        ret = rs_socket_nodeid2vnic(*local_ip, local_ip);
        hccp_info("listen [%u] IP 0x%llx, ret_vnic %d", i, *local_ip, ret);
    }

    ret = rsGetLocalDevIDByHostDevID(conn[i].phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    ret = rs_dev2conncb(chip_id, conn_cb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev fail, ret:%d", ret), ret);

    struct rs_ip_addr_info ip_info = {0};
    ret = rs_convert_ip_addr(conn[i].family, &conn[i].local_ip, &ip_info);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

    struct rs_listen_info *tmp_listen_info;
    ret = rs_find_listen_node(*conn_cb, &ip_info, server_port, &tmp_listen_info);
    if (ret == 0) {
        int counter = __sync_fetch_and_add(&(tmp_listen_info->counter), 1);
        if (counter > 0) {
            return -EEXIST;
        }
    }

    ret = rs_listen_node_alloc(*conn_cb, &ip_info, server_port, listen_info);
    // listen node found, degrade log level make it consistent with inner call
    if (ret == -EEXIST) {
        hccp_info("alloc listen info node unsuccessful, ret:%d, IP:%s, port:%u", ret, ip_info.read_addr, server_port);
    } else if (ret != 0) {
        hccp_err("alloc listen info node fail, ret:%d, IP:%s, port:%u", ret, ip_info.read_addr, server_port);
    }
    if (ret != 0) {
        conn[i].err = ENOMEM;
        return ret;
    }

    return 0;
}

static void rs_socket_set_conn_listen_info(struct rs_listen_info *listen_info, int listen_fd,
    uint32_t server_port, struct socket_listen_info *conn)
{
    listen_info->listen_fd = listen_fd;
    listen_info->sock_port = server_port;
    listen_info->state = RS_CONN_STATE_LISTENING;

    if (conn->family == AF_INET) {
        conn->local_ip.addr.s_addr = rs_socket_vnic2nodeid(conn->local_ip.addr.s_addr);
    }
    conn->err = 0;
    conn->port = server_port;
    conn->phase = LISTEN_OK;
}

static void rs_socket_handle_listen_node_err(uint32_t i, struct rs_conn_cb *conn_cb,
    struct socket_listen_info conn[], uint32_t server_port)
{
    uint32_t j;
    int ret;
    struct rs_listen_info *listen_info = NULL;

    for (j = 0; j < i; j++) {
        struct rs_ip_addr_info ip_info = {0};
        ret = rs_convert_ip_addr(conn[j].family, &conn[j].local_ip, &ip_info);
        if (ret) {
            hccp_err("convert(ntop) ip fail");
            continue;
        }
        ret = rs_find_listen_node(conn_cb, &ip_info, server_port, &listen_info);
        if (ret) {
            hccp_dbg("not find listen node, ret %d", ret);
        } else {
            ret = rs_epoll_ctl(conn_cb->epollfd, EPOLL_CTL_DEL, listen_info->listen_fd, EPOLLIN);
            if (ret) {
                hccp_err("delete from epoll failed, ret:%d, epollfd:%d, listen_fd:%d", ret, conn_cb->epollfd,
                    listen_info->listen_fd);
            }
            RS_CLOSE_RETRY_FOR_EINTR(ret, listen_info->listen_fd);
            rs_listen_node_free(conn_cb, listen_info);
        }
    }
}

STATIC int rs_get_ipv6_scope_id(struct in6_addr local_ip)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    struct in6_addr ipv6_addr = {0};
    int scope_id = 0;
    int ret, i;

    ret = getifaddrs(&ifaddr);
    CHK_PRT_RETURN(ret == -1, hccp_err("get ifaddrs failed, ret[%d]", ret), -ESYSFUNC);
    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET6) {
            continue;
        }
        ipv6_addr = ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
        for (i = 0; i < IPV6_S6_ADDR_SIZE; i++) {
            if (ipv6_addr.s6_addr[i] != local_ip.s6_addr[i]) {
                break;
            }
        }
        if (i == IPV6_S6_ADDR_SIZE) { /* all 16 u6_addr8 in ipv6 are equal */
            scope_id = (int)((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id;
            freeifaddrs(ifaddr);
            ifaddr = NULL;
            return scope_id;
        }
    }

    hccp_err("get scope id failed");
    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return -EINVAL;
}

RS_ATTRI_VISI_DEF int rs_socket_listen_start(struct socket_listen_info conn[], uint32_t num)
{
    struct rs_listen_info *listen_info = NULL;
    union rs_socketaddr server_addr = {0};
    struct rs_conn_cb *conn_cb = NULL;
    socklen_t server_addr_len = 0;
    unsigned int server_port = 0;
    int listen_fd = 0;
    int scope_id = 0;
    int err_no = 0;
    int ret, flag;
    uint32_t i;

    RS_SOCKET_PARA_CHECK(num, conn);
    if (conn[0].family == AF_INET6) {
        scope_id = rs_get_ipv6_scope_id(conn[0].local_ip.addr6);
        CHK_PRT_RETURN(scope_id < 0, hccp_err("scope_id[%d] is invalid", scope_id), -EINVAL);
    }

    for (i = 0; i < num; i++) {
        server_port = conn[i].port;
        ret = rs_socket_init_listen(conn, i, &conn_cb, server_port, &listen_info);
        if (ret == -EEXIST) {
            continue;
        }
        if (ret) {
            flag = -ENOMEM;
            hccp_err("listen init fail, ret:%d", ret);
            goto listen_node_err_handle;
        }

        /* socket */
        listen_fd = socket(conn[i].family, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            err_no = errno;
            hccp_err("create socket for (IP %s : port %u) fail, family %d, errno %d",
                listen_info->server_ip_addr.read_addr, server_port, conn[i].family, err_no);
            conn[i].phase = LISTEN_CREATE_FD_ERR;
            goto listen_err_handle;
        }

        /* bind and listen */
        conn_cb->scope_id = scope_id;
        ret = rs_socket_listen_bind_listen(listen_fd, conn_cb, conn + i, listen_info, server_port);
        err_no = ret;
        if (ret == EADDRINUSE) {
            hccp_run_warn("bind and listen unsuccessful, err_no:%d, listen_fd:%d, state:%u, IP(%s) server_port:%u",
                err_no, listen_fd, listen_info->state, listen_info->server_ip_addr.read_addr, server_port);
            goto bind_err_handle;
        } else if (ret != 0) {
            hccp_err("bind and listen fail, err_no:%d, listen_fd:%d, listen state:%u, IP(%s) server_port:%u", err_no,
                listen_fd, listen_info->state, listen_info->server_ip_addr.read_addr, server_port);
            goto bind_err_handle;
        }

        ret = rs_epoll_ctl(conn_cb->epollfd, EPOLL_CTL_ADD, listen_fd, EPOLLIN);
        if (ret) {
            err_no = ret;
            hccp_err("rs_epoll_ctl for epollfd[%d] listen_fd[%d]fail, errno: %d", conn_cb->epollfd, listen_fd, err_no);
            goto bind_err_handle;
        }

        server_addr_len = (conn->family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
        getsockname(listen_fd, (struct sockaddr *)&server_addr, &server_addr_len);
        server_port = (conn->family == AF_INET) ? ntohs(server_addr.s_addr.sin_port) :
            ntohs(server_addr.s_addr6.sin6_port);
        rs_socket_set_conn_listen_info(listen_info, listen_fd, server_port, &conn[i]);
    }

    return 0;

bind_err_handle:
    RS_CLOSE_RETRY_FOR_EINTR(ret, listen_fd);
listen_err_handle:
    rs_listen_node_free(conn_cb, listen_info);
    conn[i].err = (unsigned int)err_no;
    flag = -err_no;
listen_node_err_handle:
    rs_socket_handle_listen_node_err(i, conn_cb, conn, server_port);
    return flag;
}

STATIC int rs_socket_listen_add_to_epoll(struct rs_conn_cb *conn_cb, struct rs_listen_info *listen_info)
{
    int ret = 0;

    RS_PTHREAD_MUTEX_LOCK(&listen_info->accept_credit_mutex);
    if (listen_info->fd_state == LISTEN_FD_STATE_ADDED) {
        goto out;
    }

    // should ctl_add to make sure epoll event can be triggered
    hccp_run_info("IP:%s server_port:%u listen_fd:%d add to epoll:%d", listen_info->server_ip_addr.read_addr,
        listen_info->sock_port, listen_info->listen_fd, conn_cb->epollfd);
    ret = rs_epoll_ctl(conn_cb->epollfd, EPOLL_CTL_ADD, listen_info->listen_fd, EPOLLIN);
    if (ret != 0) {
        hccp_err("IP:%s server_port:%u listen_fd:%d rs_epoll_ctl failed, ret:%d errno:%d",
            listen_info->server_ip_addr.read_addr, listen_info->sock_port, listen_info->listen_fd, ret, errno);
        goto out;
    }

    listen_info->fd_state = LISTEN_FD_STATE_ADDED;

out:
    RS_PTHREAD_MUTEX_ULOCK(&listen_info->accept_credit_mutex);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_socket_accept_credit_add(struct socket_listen_info conn[], uint32_t num,
    unsigned int credit_limit)
{
    struct rs_listen_info *listen_info = NULL;
    struct rs_ip_addr_info ip_info = {0};
    struct rs_conn_cb *conn_cb = NULL;
    unsigned int tmp_credit_limit;
    uint32_t i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    for (i = 0; i < num; i++) {
        ret = rs_convert_ip_addr(conn[i].family, &conn[i].local_ip, &ip_info);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, i:%d, ret:%d", i, ret), ret);

        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
        conn_cb = &g_rs_cb->conn_cb;
        ret = rs_find_listen_node(conn_cb, &ip_info, conn[i].port, &listen_info);
        if (ret != 0) {
            hccp_err("rs_find_listen_node fail, i:%u, IP:%s server_port:%u, ret:%d",
                i, ip_info.read_addr, conn[i].port, ret);
            RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
            return ret;
        }
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);

        // prevent accept_credit_limit from overflow
        tmp_credit_limit = listen_info->accept_credit_limit + credit_limit;
        if (tmp_credit_limit < credit_limit) {
            hccp_err("credit_limit overflow, IP:%s server_port:%u tmp_credit_limit:%u, credit_limit:%u",
                ip_info.read_addr, conn[i].port, tmp_credit_limit, credit_limit);
            return -EINVAL;
        }
        RS_PTHREAD_MUTEX_LOCK(&listen_info->accept_credit_mutex);
        listen_info->accept_credit_limit += credit_limit;
        RS_PTHREAD_MUTEX_ULOCK(&listen_info->accept_credit_mutex);
        rs_socket_listen_add_to_epoll(conn_cb, listen_info);
        listen_info->accept_credit_flag = true;
    }

    return ret;
}

RS_ATTRI_VISI_DEF int rs_socket_listen_stop(struct socket_listen_info conn[], uint32_t num)
{
    struct rs_listen_info *listen_info = NULL;
    struct rs_conn_cb *conn_cb = NULL;
    unsigned int chip_id;
    uint32_t i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(((conn[i].family != AF_INET) && (conn[i].family != AF_INET6)) ||
            conn[i].phy_id >= RS_MAX_DEV_NUM,
            hccp_err("family[%d] invalid, or phy_id[%u] invalid, i:%u", conn[i].family, conn[i].phy_id, i), -EINVAL);

        if (conn[i].family == AF_INET) {
            uint32_t *local_ip = NULL;
            local_ip = &(conn[i].local_ip.addr.s_addr);
            ret = rs_socket_nodeid2vnic(*local_ip, local_ip);
            hccp_info("listen [%d] IP 0x%llx, ret_vnic %d", i, *local_ip, ret);
        }
        ret = rsGetLocalDevIDByHostDevID(conn[i].phy_id, &chip_id);
        CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);
        ret = rs_dev2conncb(chip_id, &conn_cb);
        // degrade log level, make it consistent with inner call
        CHK_PRT_RETURN(ret != 0, hccp_warn("get conncb from dev unsuccessful(%d)!", ret), -ENODEV);

        struct rs_ip_addr_info ip_info = {0};
        ret = rs_convert_ip_addr(conn[i].family, &conn[i].local_ip, &ip_info);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

        ret = rs_find_listen_node(conn_cb, &ip_info, conn[i].port, &listen_info);
        if (ret == 0 && __sync_fetch_and_sub(&(listen_info->counter), 1) > 1) {
            continue;
        }
        // listen node not found, degrade log level due to this is non-fatal error
        if (ret != 0) {
            hccp_warn("get listen info unsuccessful(%d), IP(%s)!", ret, ip_info.read_addr);
            conn[i].err = ENODEV;
            continue;
        }

        ret = rs_socket_listen_del_from_epoll(conn_cb, listen_info);
        CHK_PRT_RETURN(ret, hccp_err("delete from epoll failed, ret:%d, epollfd:%d, listen_fd:%d",
            ret, conn_cb->epollfd, listen_info->listen_fd), ret);

        /* close socket */
        RS_CLOSE_RETRY_FOR_EINTR(ret, listen_info->listen_fd);
        hccp_info("IP(%s) close listen fd:%d !", ip_info.read_addr, listen_info->listen_fd);

        listen_info->listen_fd = RS_FD_INVALID;
        listen_info->state = RS_CONN_STATE_RESET;

        rs_listen_node_free(conn_cb, listen_info);
    }

    return 0;
}

STATIC int rs_alloc_client_conn_node(struct rs_conn_cb *conn_cb,
    enum rs_conn_role role, struct rs_conn_info **conn, struct socket_connect_info *socket_conn,
    struct rs_ip_addr_info *client_ip, struct rs_ip_addr_info *server_ip, int server_port)
{
    struct rs_list_head *list_head = NULL;
    struct rs_conn_info *conn_info;
    int ret;

    conn_info = calloc(1, sizeof(struct rs_conn_info));
    CHK_PRT_RETURN(conn_info == NULL, hccp_err("alloc mem for socket conn info fail !"), -ENOMEM);

    conn_info->port = server_port;
    conn_info->connfd = RS_FD_INVALID;
    conn_info->state = RS_CONN_STATE_RESET;
    conn_info->server_ip = *server_ip;
    conn_info->client_ip = *client_ip;
    conn_info->scope_id = conn_cb->scope_id;

    ret = strcpy_s(conn_info->tag, SOCK_CONN_TAG_SIZE, socket_conn->tag);
    if (ret) {
        hccp_err("strcpy_s err, ret:%d, size of dest:%u, size of src:%u", ret, sizeof(conn_info->tag),
            sizeof(socket_conn->tag));
        goto out;
    }
    ret = sprintf_s(conn_info->tag + SOCK_CONN_TAG_SIZE, SOCK_CONN_DEV_ID_SIZE, "%u", socket_conn->phy_id);
    if (ret < 0) {
        hccp_err("sprintf_s err, ret:%d, phy_id:%u", ret, socket_conn->phy_id);
        goto out;
    }

    rs_get_cur_time(&conn_info->start_time);

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    list_head = (role == RS_CONN_ROLE_SERVER) ? (&conn_cb->server_conn_list) : (&conn_cb->client_conn_list);
    rs_list_add_tail(&conn_info->list, list_head);
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);

    *conn = conn_info;

    return 0;

out:
    free(conn_info);
    conn_info = NULL;
    return -ESAFEFUNC;
}

STATIC void rs_socket_client_valid_sync(struct rs_conn_info *conn)
{
    char isvalid[RS_WLIST_VALID_FLAG_SIZE] = {0};
    int ret, ret_close;

    do {
        ret = rs_socket_recv(conn->connfd, isvalid, RS_WLIST_VALID_FLAG_SIZE);
        if (ret == RS_WLIST_VALID_FLAG_SIZE && (strncmp(isvalid, "a5a5a", strlen("a5a5a")) == 0)) {
            hccp_info("[client]client is valid, ret:%d, client_ip:%s server_ip:%s server_port:%u",
                ret, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port);
            conn->state = RS_CONN_STATE_VALID_SYNC;
            return;
        } else if (ret == RS_WLIST_VALID_FLAG_SIZE && (strncmp(isvalid, "5a5a5", strlen("5a5a5")) == 0)) {
            hccp_info("[client]client is invalid, err_no:%d, client_ip:%s server_ip:%s server_port:%u",
                errno, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port);
            goto out;
        } else if (ret == -EAGAIN) {
            return;
        }
    } while ((ret < 0) && (errno == EINTR));

    // ret is -EFILEOPER or recv unexpected data. state machine will connect again
    hccp_run_warn("[client]recv isvalid unsuccessful, ret:%d err_no:%d, client_ip:%s server_ip:%s server_port:%u fd:%d."
        " retry connect", ret, errno, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->connfd);
out:
    if (g_rs_cb->ssl_enable == RS_SSL_ENABLE) {
        ssl_adp_shutdown(conn->ssl);
        ssl_adp_free(conn->ssl);
        conn->ssl = NULL;
    }
    RS_CLOSE_RETRY_FOR_EINTR(ret_close, conn->connfd);
    conn->connfd = RS_FD_INVALID;
    conn->state = RS_CONN_STATE_RESET;
    conn->tag_sync_time = 0;
    return;
}

STATIC void rs_socket_tag_sync(struct rs_conn_info *conn)
{
    int ret;

    /* sync tag to server */
    conn->tag_sync_time++;
    ret = rs_drv_socket_send(conn->connfd, conn->tag, SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE, 0);
    if (ret == SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE) {
        conn->state = RS_CONN_STATE_TAG_SYNC;
        hccp_info("[client]send tag success! ret:%d, tag_sync_time:%u, client_ip:%s server_ip:%s server_port:%u tag:%s",
            ret, conn->tag_sync_time, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag);
    } else if (ret == -EAGAIN) {
        conn->state = RS_CONN_STATE_TIMEOUT;
        hccp_info("[client]send tag incomplete! ret:%d, tag_sync_time:%u, client_ip:%s server_ip:%s server_port:%u "
            "tag:%s", ret, conn->tag_sync_time, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port,
            conn->tag);
    } else {
        hccp_run_info("[client]send tag unsuccessful, ret:%d, tag_sync_time:%u, retry connect, client_ip:%s "
            "server_ip:%s server_port:%u tag:%s", ret, conn->tag_sync_time, conn->client_ip.read_addr,
            conn->server_ip.read_addr, conn->port, conn->tag);

        if (g_rs_cb->ssl_enable == RS_SSL_ENABLE) {
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

STATIC void rs_conn_cost_time(struct rs_conn_info *conn)
{
    float time_cost = 0.0;

    rs_get_cur_time(&conn->end_time);
    hccp_time_interval(&conn->end_time, &conn->start_time, &time_cost);
    if (time_cost > RS_EXPECT_TIME_MAX) {
        hccp_warn("socket [%d] connect success cost [%f] ms more than[%f]ms!", conn->connfd, time_cost,
            RS_EXPECT_TIME_MAX);
    } else {
        hccp_info("socket [%d] connect success! cost [%f] ms", conn->connfd, time_cost);
    }

    return;
}

/* ssl will connect again and again, HCCL get socke timeout after period time */
STATIC int rs_socket_ssl_connect(struct rs_conn_info *conn, struct rs_cb *rscb)
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

STATIC int rs_socket_state_ssl_fd_bind(struct rs_conn_info *conn, uint32_t ssl_enable, struct rs_cb *rscb)
{
    int ret;

    if (ssl_enable == RS_SSL_ENABLE) {
        ret = rs_socket_ssl_connect(conn, rscb);
        if (ret) {
            return ret;
        }
        conn->state = RS_CONN_STATE_SSL_CONNECTED;
    }

    rs_conn_cost_time(conn);
    rs_socket_tag_sync(conn);
    return 0;
}

STATIC int rs_socket_state_connected(struct rs_conn_info *conn, uint32_t ssl_enable, struct rs_cb *rscb)
{
    int ret;

    if (ssl_enable == RS_SSL_ENABLE) {
        ret = rs_drv_ssl_bind_fd(conn, conn->connfd);
        if (ret != 0) {
            rs_socket_save_err_info(RS_CONN_STATE_CONNECTED, ret, &conn->err_info);
            hccp_err("[client]ssl bind fail, connfd:%d, ret:%d, client_ip:%s server_ip:%s server_port:%u tag:%s",
                conn->connfd, ret, conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag);
            return ret;
        }
        conn->state = RS_CONN_STATE_SSL_BIND_FD;
    }

    return rs_socket_state_ssl_fd_bind(conn, ssl_enable, rscb);
}

STATIC int rs_socket_state_init(unsigned int chip_id, struct rs_conn_info *conn, uint32_t ssl_enable, struct rs_cb *rscb)
{
    int ret;

    conn->tag[SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE - 1] = '\0';

    ret = rs_drv_connect(conn->connfd, &conn->server_ip, &conn->client_ip, conn->port);
    if (ret != 0) {
        rs_socket_save_err_info(RS_CONN_STATE_INIT, ret, &conn->err_info);
        hccp_warn("[client]rs_socket_state_init conn unsuccessful! client_ip:%s server_ip:%s server_port:%u tag:%s, "
            "fd:%d, ret:%d", conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag,
            conn->connfd, ret);
        return ret;
    }

    // should set back tcp socket send/recv timeout to OS default when ssl is disabled
    if (ssl_enable == RS_SSL_DISABLE) {
        ret = rs_socket_set_fd_timeout_usec(conn->connfd, 0);
        if (ret != 0) {
            hccp_warn("[client]rs_socket_set_fd_timeout_usec conn unsuccessful!, client_ip:%s server_ip:%s "
                "server_port:%u tag:%s, fd:%d, ret:%d", conn->client_ip.read_addr, conn->server_ip.read_addr,
                conn->port, conn->tag, conn->connfd, ret);
        }
    }

    conn->state = RS_CONN_STATE_CONNECTED;
    /*
     * ssl will connect again and again, HCCL get socke timeout after period time,
     * so there is no log info to prevent over log
     */
    ret = rs_socket_state_connected(conn, ssl_enable, rscb);
    if (ret) {
        return ret;
    }

    return 0;
}

STATIC int rs_connect_bind_client(int fd, struct rs_conn_info *conn)
{
    int err_no;
    int ret;

    if (conn->client_ip.family == AF_INET) {
        struct sockaddr_in client_addr = {0};
        client_addr.sin_family = conn->client_ip.family;
        client_addr.sin_addr = conn->client_ip.bin_addr.addr;

        hccp_dbg("socket bind: family %d, port %d, addr 0x%08x",
            client_addr.sin_family, client_addr.sin_port, client_addr.sin_addr.s_addr);
        ret = bind(fd, &client_addr, sizeof(client_addr));
    } else {
        struct sockaddr_in6 client_addr = {0};
        client_addr.sin6_family = conn->client_ip.family;
        client_addr.sin6_addr = conn->client_ip.bin_addr.addr6;
        client_addr.sin6_scope_id = (uint32_t)conn->scope_id;

        hccp_dbg("socket bind: family %d, port %d, scope_id %d",
            client_addr.sin6_family, client_addr.sin6_port, client_addr.sin6_scope_id);
        for (unsigned long i = 0; i < sizeof(struct in6_addr); i++) {
            hccp_dbg("socket bind: addr[%lu] 0x%02x", i, client_addr.sin6_addr.s6_addr[i]);
        }

        ret = bind(fd, &client_addr, sizeof(client_addr));
    }
    if (ret) {
        err_no = errno;
        hccp_err("client bind fail! IP:%s, sock:%d, ret:%d, error:%d", conn->client_ip.read_addr, fd, ret, err_no);
        return -err_no;
    }
    union rs_socketaddr client_addr = { 0 };
    socklen_t client_addr_len =
        (conn->client_ip.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    getsockname(fd, (struct sockaddr *)&client_addr, &client_addr_len);
    uint16_t client_port =
        (conn->client_ip.family == AF_INET) ? ntohs(client_addr.s_addr.sin_port) : ntohs(client_addr.s_addr6.sin6_port);
    if ((client_port < 60000) || (client_port > 60015)) { // HCCL默认监听60000-60015端口,如client使用该端口，记录EVENT日志
        hccp_info("client bind success. client family %d addr %s:%u, fd:%d", conn->client_ip.family,
            conn->client_ip.read_addr, client_port, fd);
    } else {
        hccp_run_info("client bind success. client family %d addr %s:%u, fd:%d", conn->client_ip.family,
            conn->client_ip.read_addr, client_port, fd);
    }
    return 0;
}

STATIC int rs_socket_bind_client(unsigned int chip_id, int conn_fd, struct rs_conn_info *conn, int hccp_mode)
{
    bool bind_ip = true;

    if (conn->client_ip.family == AF_INET && hccp_mode == NETWORK_OFFLINE) {
        // compare client_ip with current vnic_ip for compatibility issues, 910A & 910B no need to bind vnic ip
        bind_ip = rs_socket_is_vnic_ip(chip_id, conn->client_ip.bin_addr.addr.s_addr) ? false : true;
    }

    // chip force to bind: 310P & 910_93
    if (!bind_ip) {
        rs_socket_get_bind_by_chip(chip_id, &bind_ip);
    }

    // no need to bind ip
    if (!bind_ip) {
        return 0;
    }

    return rs_connect_bind_client(conn_fd, conn);
}

STATIC int rs_socket_state_reset(unsigned int chip_id, struct rs_conn_info *conn, uint32_t ssl_enable, struct rs_cb *rscb)
{
#define RS_SOCKET_CONNECT_TIMEOUT_USECS 100000
    int conn_fd, ret_close, hccp_mode;
    int tcp_nodelay_flag = 1;
    int ret = 0;

    hccp_mode = rs_get_hccp_mode(chip_id);

    conn_fd = socket(conn->client_ip.family, SOCK_STREAM, 0);
    if (conn_fd < 0) {
        ret = -errno;
        hccp_err("[client]create socket fail, errno:%d", ret);
        goto err_socket_create;
    }

    ret = rs_socket_bind_client(chip_id, conn_fd, conn, hccp_mode);
    if (ret != 0) {
        hccp_err("[client]rs_socket_bind_client fail, ret:%d", ret);
        goto err_connect_reset;
    }

    if (ssl_enable == RS_SSL_ENABLE) {
        ret = rs_set_fd_nonblock(conn_fd);
        if (ret) {
            goto err_connect_reset;
        }
    }

    /* set tcp socket tos RS_TCP_DSCP_0 */
    int tos_local = (RS_TCP_DSCP_0 & RS_DSCP_MASK) << RS_DSCP_OFF;
    ret = setsockopt(conn_fd, IPPROTO_IP, IP_TOS, (void *)&tos_local, sizeof(tos_local));
    if (ret) {
        hccp_err("[client]setsockopt(IP_TOS) fail, conn_fd:%d, ret:%d, errno:%d", conn_fd, ret, errno);
        goto err_socket_option;
    }

    ret = setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&tcp_nodelay_flag, sizeof(int));
    if (ret < 0) {
        hccp_err("[client]setsockopt(TCP_NODELAY) fail, conn_fd:%d, ret:%d, errno:%d", conn_fd, ret, errno);
        goto err_socket_option;
    }

    // should set tcp socket send/recv timeout when ssl is disabled
    if (ssl_enable == RS_SSL_DISABLE) {
        ret = rs_socket_set_fd_timeout_usec(conn_fd, RS_SOCKET_CONNECT_TIMEOUT_USECS);
        if (ret != 0) {
            goto err_connect_reset;
        }
    }

    conn->connfd = conn_fd;
    conn->state = RS_CONN_STATE_INIT;
    /*
     * ssl will connect again and again, HCCL get socke timeout after period time,
     * so there is no log info to prevent over log
     */
    ret = rs_socket_state_init(chip_id, conn, ssl_enable, rscb);
    if (ret) {
        return ret;
    }

    return 0;

err_socket_option:
    ret = -errno;
err_connect_reset:
    RS_CLOSE_RETRY_FOR_EINTR(ret_close, conn_fd);
err_socket_create:
    rs_socket_save_err_info(RS_CONN_STATE_RESET, ret, &conn->err_info);
    return -ESYSFUNC;
}

int rs_socket_connect_async(struct rs_conn_info *conn, struct rs_cb *rscb)
{
    int ret = 0;
    unsigned int chip_id = rscb->chip_id;
    uint32_t ssl_enable = rscb->ssl_enable;
    RS_CHECK_POINTER_NULL_WITH_RET(conn);
    switch (conn->state) {
        case RS_CONN_STATE_RESET:
            /* create socket for client */
            ret = rs_socket_state_reset(chip_id, conn, ssl_enable, rscb);
            break;

        case RS_CONN_STATE_INIT:
            ret = rs_socket_state_init(chip_id, conn, ssl_enable, rscb);
            break;

        case RS_CONN_STATE_CONNECTED:
            ret = rs_socket_state_connected(conn, ssl_enable, rscb);
            break;

        case RS_CONN_STATE_SSL_BIND_FD:
            ret = rs_socket_state_ssl_fd_bind(conn, ssl_enable, rscb);
            break;

        case RS_CONN_STATE_SSL_CONNECTED:
            hccp_info("[client]IP(%s) connect port %d, fd:%d OK!", conn->server_ip.read_addr, conn->port, conn->connfd);
            rs_socket_tag_sync(conn);
            break;

        case RS_CONN_STATE_TAG_SYNC:
            if (g_rs_cb->conn_cb.wlist_enable == 1) {
                rs_socket_client_valid_sync(conn);
            }
            break;

        case RS_CONN_STATE_TIMEOUT:
            hccp_info("[client]!send tag again! local_ip:%s server_ip:%s server_port:%u, tag:%s, fd:%d!",
                conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag, conn->connfd);
            rs_socket_tag_sync(conn);
            break;

        case RS_CONN_STATE_VALID_SYNC:
            break;

        case RS_CONN_STATE_TX_TO_HCCL:
            break;

        case RS_CONN_STATE_ERR:
            break;

        default:
            hccp_err("[client]Unknown state:%u, local_ip:%s server_ip:%s server_port:%u, tag:%s, fd:%d", conn->state,
                conn->client_ip.read_addr, conn->server_ip.read_addr, conn->port, conn->tag, conn->connfd);
            return -EINVAL;
    }

    return ret;
}

// 获取socket connect状态；返回值 0:connect中，1:connect完成
int rs_get_socket_connect_state(struct rs_conn_info *conn)
{
    if ((conn->state == RS_CONN_STATE_TX_TO_HCCL) ||
        ((g_rs_cb->conn_cb.wlist_enable == 1) && (conn->state == RS_CONN_STATE_VALID_SYNC)) ||
        ((g_rs_cb->conn_cb.wlist_enable == 0) && (conn->state == RS_CONN_STATE_TAG_SYNC))) {
        return 1;
    } else {
        return 0;
    }
}

STATIC void rs_sockets_ip_addr_converter(struct socket_connect_info conn[], int num)
{
    int j;

    for (j = 0; j < num; j++) {
        if (conn[j].family == AF_INET) {
            conn[j].local_ip.addr.s_addr = rs_socket_vnic2nodeid(conn[j].local_ip.addr.s_addr);
            conn[j].remote_ip.addr.s_addr = rs_socket_vnic2nodeid(conn[j].remote_ip.addr.s_addr);
        }
    }
}

static void rs_socket_handle_conn_node_err(uint32_t i, struct rs_conn_cb *conn_cb,
    struct socket_connect_info conn[], uint32_t server_port)
{
    struct rs_conn_info *conn_info = NULL;
    uint32_t j;
    int ret;

    for (j = 0; j < i; j++) {
        ret = rs_get_conn_info(conn_cb, conn + j, &conn_info, server_port);
        if (ret) {
            hccp_dbg("not find conn node, ret %d", ret);
        } else {
            RS_PTHREAD_MUTEX_LOCK(&conn_cb->rscb->mutex);
            RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
            rs_list_del(&conn_info->list);
            free(conn_info);
            conn_info = NULL;
            RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
            RS_PTHREAD_MUTEX_ULOCK(&conn_cb->rscb->mutex);
        }
    }

    return;
}

STATIC int rs_socket_connect_check_para(struct socket_connect_info *conn_info)
{
    if (((conn_info->family != AF_INET) && (conn_info->family != AF_INET6)) || conn_info->phy_id >= RS_MAX_DEV_NUM ||
        strlen(conn_info->tag) >= SOCK_CONN_TAG_SIZE) {
        hccp_err("family[%d] invalid, or phy_id[%u] invalid, or conn tag len:%u more than max len:%d",
            conn_info->family, conn_info->phy_id, strlen(conn_info->tag), SOCK_CONN_TAG_SIZE);
        return -EINVAL;
    }

    return 0;
}

STATIC int rs_socket_IP_convert(struct socket_connect_info *conn_info, struct rs_ip_addr_info *remote_ip,
    struct rs_ip_addr_info *local_ip)
{
    int ret_val = 0;
    int ret = 0;

    if (conn_info->family == AF_INET) {
        uint32_t *remote_ip_tmp = &(conn_info->remote_ip.addr.s_addr);
        uint32_t *local_ip_tmp = &(conn_info->local_ip.addr.s_addr);
        ret_val = rs_socket_nodeid2vnic(*remote_ip_tmp, remote_ip_tmp);
        ret = rs_socket_nodeid2vnic(*local_ip_tmp, local_ip_tmp);
        hccp_info("local IP[0x%llx], ret:%d, remote IP[0x%llx], ret:%d", *local_ip_tmp, ret, *remote_ip_tmp, ret_val);
    }

    ret = rs_convert_ip_addr(conn_info->family, &conn_info->remote_ip, remote_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) remote ip fail, ret:%d", ret), ret);

    ret = rs_convert_ip_addr(conn_info->family, &conn_info->local_ip, local_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) local ip fail, ret:%d", ret), ret);

    hccp_info("local IP[%s], ret:%d, remote IP[%s], ret:%d", local_ip->read_addr, ret, remote_ip->read_addr, ret_val);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_batch_connect(struct socket_connect_info conn[], uint32_t num)
{
    struct rs_conn_info *conn_info = NULL;
    struct rs_conn_cb *conn_cb = NULL;
    unsigned int chip_id, server_port;
    struct rs_ip_addr_info remote_ip;
    struct rs_ip_addr_info local_ip;
    unsigned int i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    for (i = 0; i < num; i++) {
        server_port = conn[i].port;
        ret = rs_socket_connect_check_para(&conn[i]);
        if (ret) {
            hccp_err("rs_socket_connect_check_para for failed, ret:%d, i:%u", ret, i);
            goto conn_node_err_handle;
        }

        ret = rs_socket_IP_convert(&conn[i], &remote_ip, &local_ip);
        if (ret) {
            hccp_err("convert ip invalid, ret %d", ret);
            goto conn_node_err_handle;
        }
        ret = rsGetLocalDevIDByHostDevID(conn[i].phy_id, &chip_id);
        if (ret) {
            hccp_err("phy_id invalid, ret %d", ret);
            goto conn_node_err_handle;
        }

        ret = rs_dev2conncb(chip_id, &conn_cb);
        if (ret) {
            hccp_err("get conncb from dev fail(%d)!", ret);
            goto conn_node_err_handle;
        }

        if (conn[i].family == AF_INET6) {
            conn_cb->scope_id = rs_get_ipv6_scope_id(conn[i].local_ip.addr6);
            if (conn_cb->scope_id < 0) {
                hccp_err("scope_id[%d] is invalid", conn_cb->scope_id);
                conn_cb->scope_id = 0;
                goto conn_node_err_handle;
            }
        }

        ret = rs_get_conn_info(conn_cb, conn + i, &conn_info, server_port);
        if (ret) {
            ret = rs_alloc_client_conn_node(conn_cb, RS_CONN_ROLE_CLIENT, &conn_info, &conn[i], &local_ip, &remote_ip,
                server_port);
            if (ret) {
                hccp_err("rs_alloc_client_conn_node failed, ret:%d, role:%d, local_ip:%s, remote_ip:%s, server_port:%u,"
                    " tag:%s", ret, RS_CONN_ROLE_CLIENT, local_ip.read_addr, remote_ip.read_addr, server_port,
                    conn[i].tag);
                goto conn_node_err_handle;
            }

            hccp_info("create conn node for {remote_ip(%s), server_port(%u), tag(%s)}!",
                remote_ip.read_addr, server_port, conn_info->tag);
        } else {
            hccp_info("conn node for {remote_ip(%s), server_port(%u), tag(%s)} exist! state:%u",
                remote_ip.read_addr, server_port, conn_info->tag, conn_info->state);
        }
    }
    sem_post(&g_rs_cb->connect_trig_sem);
    rs_sockets_ip_addr_converter(conn, num);
    return 0;

conn_node_err_handle:
    rs_socket_handle_conn_node_err(i, conn_cb, conn, server_port);
    return ret;
}

STATIC int rs_socket_close_fd(int fd)
{
    int err_no = -1;
    int ret;

    do {
        ret = close(fd);
        if (ret < 0) {
            err_no = errno;
            CHK_PRT_RETURN(err_no != EINTR, hccp_err("close fd[%d] failed, ret:%d, err_no[%d]",
                fd, ret, err_no), -err_no);
        }
    } while ((ret < 0) && (err_no == EINTR));

    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_batch_close(int disuse_linger, struct rs_socket_close_info_t conn[], uint32_t num)
{
    struct rs_conn_info *conn_info = NULL;
    struct linger so_linger;
    int fd = RS_FD_INVALID;
    int ret_val = 0;
    unsigned int i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);

    for (i = 0; i < num; i++) {
        fd = conn[i].fd;
        CHK_PRT_RETURN(fd < 0, hccp_err("param error ! fd:%d, i:%d, num:%d", fd, i, num), -EINVAL);

        // strict mutex lock before find to make sure conn_info is valid on concurrent scenario
        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
        ret = rs_fd2conn(fd, &conn_info);
        if (ret != 0) {
            hccp_err("get conn fail! ret:%d", ret);
            RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
            return ret;
        }

        hccp_info("conn node of IP(%s) fd:%d, state:%d",
            conn_info->server_ip.read_addr, conn_info->connfd, conn_info->state);

        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->conn_cb.conn_mutex);
        rs_list_del(&conn_info->list);
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->conn_cb.conn_mutex);
        if (g_rs_cb->ssl_enable == RS_SSL_ENABLE) {
            ssl_adp_shutdown(conn_info->ssl);
            ssl_adp_free(conn_info->ssl);
            conn_info->ssl = NULL;
        }
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);

        if (conn_info->state > RS_CONN_STATE_RESET) {
            so_linger.l_onoff = 1;
            so_linger.l_linger = disuse_linger == 0 ? RS_CLOSE_TIMEOUT : 0;
            ret = setsockopt(conn_info->connfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
            if (ret) {
                hccp_err("setsockopt l_onoff:%d l_linger:%d fail err:%d", so_linger.l_onoff, so_linger.l_linger, errno);
                ret_val = ret;
            }

            ret = rs_socket_close_fd(conn_info->connfd);
            if (ret) {
                hccp_err("rs_socket_close_fd for fd[%d] failed, ret[%d]", conn_info->connfd, ret);
                ret_val = ret;
            }
        }

        free(conn_info);
        conn_info = NULL;
    }

    return ret_val;
}

RS_ATTRI_VISI_DEF int rs_socket_batch_abort(struct socket_connect_info conn[], uint32_t num)
{
    struct rs_conn_info *conn_info = NULL;
    struct linger so_linger = { 0 };
    int ret_val = 0;
    unsigned int i;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);

    for (i = 0; i < num; i++) {
        // strict mutex lock before find to make sure conn_info is valid on concurrent scenario
        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
        ret = rs_get_conn_info(&g_rs_cb->conn_cb, &conn[i], &conn_info, conn[i].port);
        if (ret != 0) {
            hccp_err("rs_get_conn_info conn:%u fail! ret:%d", i, ret);
            RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
            return ret;
        }

        hccp_info("abort conn node of IP(%s) fd:%d, state:%d", conn_info->server_ip.read_addr, conn_info->connfd,
            conn_info->state);

        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->conn_cb.conn_mutex);
        rs_list_del(&conn_info->list);
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->conn_cb.conn_mutex);
        if (g_rs_cb->ssl_enable == RS_SSL_ENABLE && conn_info->ssl != NULL) {
            ssl_adp_shutdown(conn_info->ssl);
            ssl_adp_free(conn_info->ssl);
            conn_info->ssl = NULL;
            ssl_adp_clear_error();
        }
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);

        if (conn_info->state > RS_CONN_STATE_RESET && conn_info->connfd != RS_FD_INVALID) {
            // force to close fd
            so_linger.l_onoff = 1;
            ret = setsockopt(conn_info->connfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
            if (ret) {
                hccp_err("setsockopt l_onoff:%d l_linger:%d fail err:%d", so_linger.l_onoff, so_linger.l_linger, errno);
                ret_val = ret;
            }

            ret = rs_socket_close_fd(conn_info->connfd);
            if (ret) {
                hccp_err("rs_socket_close_fd for fd[%d] failed, ret[%d]", conn_info->connfd, ret);
                ret_val = ret;
            }
        }

        free(conn_info);
        conn_info = NULL;
    }

    return ret_val;
}

STATIC void rs_sockets_backfill(struct socket_fd_data conn[], int sock_num,
    struct rs_conn_info *conn_tmp, struct rs_vnic_info vnic_info)
{
    conn[sock_num].fd = conn_tmp->connfd;

    if (vnic_info.role == RS_CONN_ROLE_SERVER) {
        conn[sock_num].remote_ip = conn_tmp->client_ip.bin_addr;
    } else {
        conn[sock_num].remote_ip = conn_tmp->server_ip.bin_addr;
    }

    conn[sock_num].status = RS_SOCK_STATUS_OK;
    conn_tmp->state = RS_CONN_STATE_TX_TO_HCCL;
    conn_tmp->is_got = true;
}

STATIC void rs_sockets_serverip_converter(struct socket_fd_data conn[], int num,
    uint32_t vnic_flag)
{
    int j;

    if (vnic_flag) {
        for (j = 0; j < num; j++) {
            if (conn[j].family == AF_INET) {
                conn[j].local_ip.addr.s_addr = rs_socket_vnic2nodeid(conn[j].local_ip.addr.s_addr);
                conn[j].remote_ip.addr.s_addr = rs_socket_vnic2nodeid(conn[j].remote_ip.addr.s_addr);
            }
        }
    }
}

STATIC int rs_find_sockets(struct rs_conn_info *conn_tmp, struct socket_fd_data conn[], int num,
    int role)
{
    int ret, i;

    /* normal process, no record log */
    if (g_rs_cb->conn_cb.wlist_enable == 1) {
        if (conn_tmp->state != RS_CONN_STATE_VALID_SYNC) {
            return -EINVAL;
        }
    } else {
        if (conn_tmp->state != RS_CONN_STATE_TAG_SYNC) {
            return -EINVAL;
        }
    }

    // server skip to get current socket once socket already been got
    if (role == RS_CONN_ROLE_SERVER && conn_tmp->is_got) {
        return -EINVAL;
    }

    if (role == RS_CONN_ROLE_SERVER) {
        i = 0;
        struct rs_ip_addr_info local_ip;
        ret = rs_convert_ip_addr(conn->family, &conn->local_ip, &local_ip);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

        CHK_PRT_RETURN(rs_compare_ip_addr(&conn_tmp->server_ip, &local_ip), hccp_warn("server_ip[%s] != local_ip[%s]",
            conn_tmp->server_ip.read_addr, local_ip.read_addr), -EINVAL);
    } else {
        for (i = 0; i < num; i++) {
            if (conn[i].status == RS_SOCK_STATUS_OK) {
                continue;
            }

            struct rs_ip_addr_info remote_ip;
            remote_ip.family = (uint32_t)conn[i].family;
            remote_ip.bin_addr = conn[i].remote_ip;
            struct rs_ip_addr_info local_ip;
            local_ip.family = (uint32_t)conn[i].family;
            local_ip.bin_addr = conn[i].local_ip;
            if ((!rs_compare_ip_addr(&conn_tmp->server_ip, &remote_ip)) &&
                (!rs_compare_ip_addr(&conn_tmp->client_ip, &local_ip))) {
                break;
            }
        }
    }

    CHK_PRT_RETURN(i == num, hccp_warn("i == num %d, not find server_ip[%s]", num, conn_tmp->server_ip.read_addr),
        -EINVAL);

    conn[i].tag[SOCK_CONN_TAG_SIZE - 1] = '\0';
    ret = strcmp(conn[i].tag, conn_tmp->tag);
    CHK_PRT_RETURN(ret, hccp_warn("The %dth conn tag[%s] is different from conn_tmp_tag [%s]",
        i, conn[i].tag, conn_tmp->tag), -EINVAL);

    return i;
}

/* find it */
STATIC int rs_sockets_compare(struct rs_list_head *list_head, struct socket_fd_data conn[],
    uint32_t num, struct rs_vnic_info vnic_info, struct rs_conn_cb *conn_cb)
{
    struct rs_conn_info *conn_tmp = NULL;
    struct rs_conn_info *conn_tmp2 = NULL;
    int sock_num = 0;
    int i;
    int sock_index;

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    conn_tmp = list_entry((list_head)->next, struct rs_conn_info, list);
    conn_tmp2 = list_entry(conn_tmp->list.next, struct rs_conn_info, list);
    for (; &conn_tmp->list != (list_head);) {
        i = rs_find_sockets(conn_tmp, conn, num, vnic_info.role);
        if (i < 0) {
            goto renew_conn;
        }
        sock_index = (vnic_info.role == RS_CONN_ROLE_SERVER) ? sock_num : i;
        rs_sockets_backfill(conn, sock_index, conn_tmp, vnic_info);

        sock_num++;
        if ((unsigned int)sock_num >= num) {
            break;
        }
renew_conn:
        conn_tmp = conn_tmp2;
        conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list);
    }
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    rs_sockets_serverip_converter(conn, num, vnic_info.vnic_flag);
    struct rs_ip_addr_info local_ip;
    rs_convert_ip_addr(conn[0].family, &conn[0].local_ip, &local_ip);
    hccp_dbg("local_ip:%s, fd:%d, sock_num:%d", local_ip.read_addr, conn[0].fd, sock_num);
    return sock_num;
}

STATIC int rs_get_vnic_flag(uint32_t role, uint32_t *local_ip, uint32_t *remote_ip)
{
    int vnic_flag = 0;

    if (role == RS_CONN_ROLE_SERVER) {
        if (rs_socket_nodeid2vnic(*local_ip, local_ip) == RS_VNIC_FLAG) {
            vnic_flag = 1;
        }
    } else {
        if ((rs_socket_nodeid2vnic(*remote_ip, remote_ip) == RS_VNIC_FLAG) &&
            (rs_socket_nodeid2vnic(*local_ip, local_ip) == RS_VNIC_FLAG)) {
            vnic_flag = 1;
        }
    }
    return vnic_flag;
}

RS_ATTRI_VISI_DEF int rs_get_sockets(uint32_t role, struct socket_fd_data conn[], uint32_t num)
{
    struct rs_list_head *list_head = NULL;
    struct rs_vnic_info vnic_info = {0};
    struct rs_conn_cb *conn_cb = NULL;
    unsigned int chip_id;
    uint32_t j;
    int ret;

    vnic_info.role = role;

    RS_SOCKET_PARA_CHECK(num, conn);
    CHK_PRT_RETURN(role > RS_CONN_ROLE_CLIENT, hccp_err("para invalid. role[%u]", role), -EINVAL);

    /* set conn status to NA */
    for (j = 0; j < num; j++) {
        conn[j].status = 0;
        CHK_PRT_RETURN(((conn[j].family != AF_INET) && (conn[j].family != AF_INET6)) ||
            conn[j].phy_id >= RS_MAX_DEV_NUM,
            hccp_err("family[%d] invalid, or phy_id[%u] invalid, j:%u", conn[j].family, conn[j].phy_id, j), -EINVAL);

        CHK_PRT_RETURN(strlen(conn[j].tag) >= SOCK_CONN_TAG_SIZE, hccp_err("conn tag len:%u more than max len:%d",
            strlen(conn[j].tag), SOCK_CONN_TAG_SIZE), -EINVAL);

        if (conn[j].family == AF_INET) {
            uint32_t *local_ip = &(conn[j].local_ip.addr.s_addr);
            uint32_t *remote_ip = &(conn[j].remote_ip.addr.s_addr);
            vnic_info.vnic_flag = (uint32_t)rs_get_vnic_flag(role, local_ip, remote_ip);
        }
    }

    ret = rsGetLocalDevIDByHostDevID(conn->phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    ret = rs_dev2conncb(chip_id, &conn_cb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev fail(%d)!", ret), -ENODEV);

    list_head = (role == RS_CONN_ROLE_SERVER) ? (&conn_cb->server_conn_list) : (&conn_cb->client_conn_list);
    return rs_sockets_compare(list_head, conn, num, vnic_info, conn_cb);
}

RS_ATTRI_VISI_DEF int rs_get_ssl_enable(uint32_t *ssl_enable)
{
    CHK_PRT_RETURN(g_rs_cb == NULL, hccp_err("param error, g_rs_cb is NULL"), -ENODEV);
    CHK_PRT_RETURN(ssl_enable == NULL, hccp_err("param error, ssl_enable is NULL"), -EINVAL);

    *ssl_enable = g_rs_cb->ssl_enable;
    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_send(int fd, const void *data, uint64_t size)
{
    int ret;

    ret = rs_drv_socket_send(fd, data, size, MSG_DONTWAIT | MSG_NOSIGNAL);

    hccp_dbg("send fd:%d, size:%llu, send %dB", fd, size, ret);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_peer_socket_send(uint32_t ssl_enable, int fd, const void *data, uint64_t size)
{
    int ret = 0;
    int err_no;

    CHK_PRT_RETURN(fd < 0 || size == 0 || data == NULL, hccp_err("param error ! fd:%d < 0, size:%llu or data is NULL",
        fd, size), -EINVAL);

    if (ssl_enable != RS_SSL_DISABLE) {
#ifdef CONFIG_SSL
        int err;
        struct rs_conn_info *conn = NULL;

        ret = rs_fd2conn(fd, &conn);
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
            err_no = errno;
            if (err_no == EAGAIN || err_no == EINTR) {
                hccp_dbg("send to fd:%d need retry, send size:%llu, ret:%d, errno:%d", fd, size, ret, err_no);
                ret = -EAGAIN;
            } else {
                hccp_run_info("send to fd:%d not success, send size:%llu, ret:%d, errno:%d", fd, size, ret, err_no);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}

RS_ATTRI_VISI_DEF int rs_socket_recv(int fd, void *data, uint64_t size)
{
    int ret;

    ret = rs_drv_socket_recv(fd, data, size, MSG_DONTWAIT);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_peer_socket_recv(uint32_t ssl_enable, int fd, void *data, uint64_t size)
{
    int ret = 0;
    int err_no;

    CHK_PRT_RETURN(fd < 0 || data == NULL || size == 0, hccp_err("param error ! fd:%d < 0 or data is NULL, size:%llu",
        fd, size), -EINVAL);

    if (ssl_enable != RS_SSL_DISABLE) {
#ifdef CONFIG_SSL
        int err;
        struct rs_conn_info *conn = NULL;

        ret = rs_fd2conn(fd, &conn);
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
            err_no = errno;
            // not to print to avoid log flush
            if (err_no == EAGAIN || err_no == EINTR) {
                ret = -EAGAIN;
            } else {
                hccp_run_info("recv for fd:%d not success, recv size:%llu, ret:%d, err_no:%d", fd, size, ret, err_no);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}

RS_ATTRI_VISI_DEF int rs_socket_get_client_socket_err_info(struct socket_connect_info conn[],
    struct socket_err_info err[], unsigned int num)
{
    struct rs_conn_info *conn_info = NULL;
    unsigned int i, server_port;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    RS_CHECK_POINTER_NULL_WITH_RET(err);
    for (i = 0; i < num; i++) {
        server_port = conn[i].port;
        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
        ret = rs_get_conn_info(&g_rs_cb->conn_cb, &conn[i], &conn_info, server_port);
        if (ret != 0) {
            hccp_err("rs_get_conn_info fail, i:%u ret:%d", i, ret);
            RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
            return ret;
        }

        (void)memcpy_s(&err[i], sizeof(struct socket_err_info), &conn_info->err_info, sizeof(struct socket_err_info));

        // clear the singer socket connect err info
        (void)memset_s(&conn_info->err_info, sizeof(struct socket_err_info), 0, sizeof(struct socket_err_info));
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_get_server_socket_err_info(struct socket_listen_info conn[],
    struct server_socket_err_info err[], unsigned int num)
{
    struct rs_listen_info *listen_info = NULL;
    struct rs_ip_addr_info ip_info = {0};
    struct rs_conn_cb *conn_cb = NULL;
    unsigned int i, server_port;
    int ret;

    RS_SOCKET_PARA_CHECK(num, conn);
    RS_CHECK_POINTER_NULL_WITH_RET(err);
    for (i = 0; i < num; i++) {
        ret = rs_convert_ip_addr(conn[i].family, &conn[i].local_ip, &ip_info);
        CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, i:%u, ret:%d", i, ret), ret);

        server_port = conn[i].port;
        RS_PTHREAD_MUTEX_LOCK(&g_rs_cb->mutex);
        conn_cb = &g_rs_cb->conn_cb;
        ret = rs_find_listen_node(conn_cb, &ip_info, server_port, &listen_info);
        if (ret != 0) {
            hccp_err("rs_find_listen_node fail, i:%u, ip:%s, server_port:%u, ret:%d",
                i, ip_info.read_addr, server_port, ret);
            RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
            return ret;
        }

        (void)memcpy_s(&err[i].epoll_wait, sizeof(struct socket_err_info),
            &conn_cb->epoll_err_info, sizeof(struct socket_err_info));
        (void)memcpy_s(&err[i].accept, sizeof(struct socket_err_info),
            &listen_info->err_info, sizeof(struct socket_err_info));

        // clear the single socket listen err info
        (void)memset_s(&listen_info->err_info, sizeof(struct socket_err_info), 0, sizeof(struct socket_err_info));
        RS_PTHREAD_MUTEX_ULOCK(&g_rs_cb->mutex);
    }

    return 0;
}

static void rs_socket_get_ip_info(unsigned int *server_ip, unsigned int *client_ip)
{
    uint32_t server_node_id = *server_ip;
    uint32_t client_node_id = *client_ip;
    int ret;

    ret = rs_socket_nodeid2vnic(server_node_id, server_ip);
    hccp_info("white list listen IP 0x%llx, ret_vnic %d", *server_ip, ret);

    ret = rs_socket_nodeid2vnic(client_node_id, client_ip);
    hccp_info("white list client IP 0x%llx, ret_vnic %d", *client_ip, ret);

    return;
}

STATIC int rs_socket_white_list_alloc(struct rs_conn_cb *conn_cb,
    struct socket_wlist_info_t *white_list, struct rs_ip_addr_info *server_ip)
{
    int ret;
    /*lint -e429*/
    struct rs_white_list_info *white_list_node_tmp = NULL;
    struct rs_white_list *white_list_tmp = NULL;
    struct socket_wlist_info_t wlist;
    struct rs_ip_addr_info client_ip;
    ret = memcpy_s(&wlist, sizeof(struct socket_wlist_info_t), white_list, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memcpy socket_wlist_info_t wlist failed, ret[%d]!", ret), -ESAFEFUNC);

    if (server_ip->family == AF_INET) {
        rs_socket_get_ip_info(&server_ip->bin_addr.addr.s_addr, &(wlist.remote_ip.addr.s_addr));
        rs_inet_ntop(server_ip->family, &server_ip->bin_addr, (char *)&server_ip->read_addr,
            sizeof(server_ip->read_addr));
    }

    ret = rs_convert_ip_addr(server_ip->family, &wlist.remote_ip, &client_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

    ret = rs_find_white_list(conn_cb, server_ip, &white_list_tmp);
    if (ret) {
        white_list_tmp = calloc(1, sizeof(struct rs_white_list));
        CHK_PRT_RETURN(white_list_tmp == NULL, hccp_err("alloc mem for rs_white_list failed!"), -ENOMEM);
        white_list_tmp->server_ip = *server_ip;
        RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
        RS_INIT_LIST_HEAD(&white_list_tmp->white_list);
        rs_list_add_tail(&white_list_tmp->list, &conn_cb->white_list);
        RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    }

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    ret = rs_find_white_list_node(white_list_tmp, &wlist, (int)server_ip->family, &white_list_node_tmp);
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    if (ret == 0) {
        white_list_node_tmp->conn_limit += wlist.conn_limit;
        return 0;
    }

    white_list_node_tmp = calloc(1, sizeof(struct rs_white_list_info));
    CHK_PRT_RETURN(white_list_node_tmp == NULL, hccp_err("alloc mem for socket_wlist_info_t failed!"), -ENOMEM);

    white_list_node_tmp->client_ip = client_ip;
    white_list_node_tmp->conn_limit = wlist.conn_limit;
    ret = memcpy_s(white_list_node_tmp->tag, SOCK_CONN_TAG_SIZE, wlist.tag, sizeof(wlist.tag));
    if (ret) {
        hccp_err("memcpy_s failed, ret[%d]. ", ret);
        free(white_list_node_tmp);
        white_list_node_tmp = NULL;
        return -ESAFEFUNC;
    }

    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    rs_list_add_tail(&white_list_node_tmp->list, &white_list_tmp->white_list);
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    return 0;
    /*lint +e429*/
}

RS_ATTRI_VISI_DEF int rs_socket_white_list_switch(unsigned int phy_id, unsigned int enable)
{
    struct rs_conn_cb *conn_cb = NULL;
    int ret;

    ret = rs_dev2conncb(phy_id, &conn_cb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev fail, ret:%d", ret), -1);
    conn_cb->wlist_enable = enable;
    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[],
    unsigned int num)
{
    struct rs_conn_cb *conn_cb = &(g_rs_cb->conn_cb);
    struct rs_ip_addr_info server_ip;
    unsigned int i, chip_id;
    int ret;

    ret = rs_convert_ip_addr(rdev_info.family, &rdev_info.local_ip, &server_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), -EINVAL);

    CHK_PRT_RETURN(num <= 0 || white_list == NULL || num > RS_MAX_WLIST_NUM ||
        ((rdev_info.family != AF_INET) && (rdev_info.family != AF_INET6)) || rdev_info.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("white list add param error, phy_id[%u], server ip[%s], num[%u], family[%d]",
        rdev_info.phy_id, server_ip.read_addr, num, rdev_info.family), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(rdev_info.phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    for (i = 0; i < num; ++i) {
        CHK_PRT_RETURN(strnlen(white_list[i].tag, SOCK_CONN_TAG_SIZE) >= SOCK_CONN_TAG_SIZE,
            hccp_err("white_list tag len:%u more than max len:%d", strlen(white_list[i].tag), SOCK_CONN_TAG_SIZE),
            -EINVAL);
        ret = rs_socket_white_list_alloc(conn_cb, &white_list[i], &server_ip);
        if (ret) {
            struct rs_ip_addr_info client_ip;
            ret = rs_convert_ip_addr(server_ip.family, &white_list->remote_ip, &client_ip);
            CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);
            hccp_err("add white list node fail, server ip[%s], client ip[%s], tag[%s], ret:%d",
                server_ip.read_addr, client_ip.read_addr, white_list[i].tag, ret);
        }
    }
    return 0;
}

STATIC int rs_socket_white_list_node_destroy(struct rs_conn_cb *conn_cb,
    struct socket_wlist_info_t *white_list, struct rs_ip_addr_info *server_ip)
{
    struct rs_white_list_info *white_list_node_tmp = NULL;
    struct rs_white_list *white_list_tmp = NULL;
    struct socket_wlist_info_t wlist;
    struct rs_ip_addr_info client_ip;
    int ret;

    ret = rs_convert_ip_addr((int)server_ip->family, &white_list->remote_ip, &client_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

    ret =  memset_s(&wlist, sizeof(struct socket_wlist_info_t), 0, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memset_s socket_wlist_info_t wlist failed, ret:%d", ret), -ESAFEFUNC);
    ret = memcpy_s(&wlist, sizeof(struct socket_wlist_info_t), white_list, sizeof(struct socket_wlist_info_t));
    CHK_PRT_RETURN(ret, hccp_err("memcpy socket_wlist_info_t wlist failed!"), -ESAFEFUNC);

    if (server_ip->family == AF_INET) {
        ret = rs_socket_nodeid2vnic(server_ip->bin_addr.addr.s_addr, &server_ip->bin_addr.addr.s_addr);
        hccp_info("listen IP 0x%llx, ret_vnic %d", server_ip->bin_addr.addr.s_addr, ret);
        ret = rs_socket_nodeid2vnic(wlist.remote_ip.addr.s_addr, &(wlist.remote_ip.addr.s_addr));
        hccp_info("client IP 0x%llx, ret_vnic %d", wlist.remote_ip.addr.s_addr, ret);
    }

    ret = rs_find_white_list(conn_cb, server_ip, &white_list_tmp);
    CHK_PRT_RETURN(ret != 0, hccp_err("white list for IP(%s) doesn't exist! state:%d", server_ip->read_addr, ret), ret);
    RS_PTHREAD_MUTEX_LOCK(&conn_cb->conn_mutex);
    ret = rs_find_white_list_node(white_list_tmp, &wlist, (int)server_ip->family, &white_list_node_tmp);
    if (ret == 0) {
        rs_list_del(&white_list_node_tmp->list);
        free(white_list_node_tmp);
        white_list_node_tmp = NULL;
        RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
        return 0;
    }
    RS_PTHREAD_MUTEX_ULOCK(&conn_cb->conn_mutex);
    hccp_info("can not find white list node: client ip[%s], tag[%s], ret:%d", client_ip.read_addr, wlist.tag, ret);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_socket_white_list_del(struct rdev rdev_info,
    struct socket_wlist_info_t white_list[], unsigned int num)
{
    struct rs_conn_cb *conn_cb = &(g_rs_cb->conn_cb);
    unsigned int i, chip_id;
    struct rs_ip_addr_info server_ip;
    int ret;

    ret = rs_convert_ip_addr(rdev_info.family, &rdev_info.local_ip, &server_ip);
    CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);

    CHK_PRT_RETURN(num <= 0 || white_list == NULL || num > RS_MAX_WLIST_NUM ||
        ((rdev_info.family != AF_INET) && (rdev_info.family != AF_INET6)) || rdev_info.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("white list del param error, phy_id[%u], server ip[%s], num[%u] family[%d]", rdev_info.phy_id,
        server_ip.read_addr, num, rdev_info.family),
        -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(rdev_info.phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    for (i = 0; i < num; ++i) {
        CHK_PRT_RETURN(strlen(white_list[i].tag) >= SOCK_CONN_TAG_SIZE, hccp_err("white_list tag len:%u more than"
            "max len:%d", strlen(white_list[i].tag), SOCK_CONN_TAG_SIZE), -EINVAL);
        ret = rs_socket_white_list_node_destroy(conn_cb, &white_list[i], &server_ip);
        if (ret) {
            struct rs_ip_addr_info client_ip;
            ret = rs_convert_ip_addr(server_ip.family, &white_list->remote_ip, &client_ip);
            CHK_PRT_RETURN(ret, hccp_err("convert(ntop) ip fail, ret:%d", ret), ret);
            hccp_info("white list node wait to delete, server ip[%s], client ip[%s], tag[%s], ret:%d",
                server_ip.read_addr, client_ip.read_addr, white_list[i].tag, ret);
        }
    }
    return 0;
}

enum rs_hardware_type rs_get_device_type(unsigned int phy_id)
{
    int64_t device_info = 0;
    unsigned int board_type;
    unsigned int logic_id;
    unsigned int chip_id;
    int64_t board_id;
    int ret;
 
    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("invalid param phy_id[%u]", phy_id), RS_HARDWARE_UNKNOWN);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), RS_HARDWARE_UNKNOWN);
    ret = dl_drv_device_get_index_by_phy_id(chip_id, &logic_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_drv_device_get_index_by_phy_id failed, ret(%d), chip_id(%u)",
        ret, chip_id), RS_HARDWARE_UNKNOWN);

    ret = dl_hal_get_device_info(logic_id, MODULE_TYPE_SYSTEM, INFO_TYPE_BOARD_ID, &board_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_get_device_info board_id failed, ret[%d]", ret), RS_HARDWARE_UNKNOWN);
    hccp_info("board_id is (0x%llx)", board_id);
    board_type = (unsigned int)((uint64_t)board_id & (0xfff0));
    ret = dl_hal_get_device_info(logic_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &device_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("dl_hal_get_device_info device_info failed, ret(%d), phy_id(%u)", ret, phy_id),
        RS_HARDWARE_UNKNOWN);

    // 910A场景判断逻辑
    if (dl_hal_plat_get_chip((uint64_t)device_info) == CHIP_TYPE_910A) {
        if (((board_type & RS_BOARDID_PCIE_CARD_MASK) == RS_BOARDID_PCIE_CARD_MASK_VALUE) &&
            (board_type != RS_BOARDID_AI_SERVER_MODULE) && (board_type != RS_BOARDID_ARM_SERVER_AG)) {
            return RS_HARDWARE_PCIE;
        }
        return RS_HARDWARE_SERVER;
    }
 
    if (((board_type & RS_BOARDID_PCIE_CARD_MASK) == RS_BOARDID_PCIE_CARD_MASK_VALUE) &&
         (board_type != RS_BOARDID_AI_SERVER_MODULE) && (board_type != RS_BOARDID_ARM_SERVER_AG) &&
         (board_type != RS_BOARDID_ARM_POD) && (board_type != RS_BOARDID_X86_16P) &&
         (board_type != RS_BOARDID_ARM_SERVER_2DIE)) {
        return RS_HARDWARE_PCIE;
    }

    if ((board_type == RS_BOARDID_ARM_SERVER_2DIE)) {
        return RS_HARDWARE_2DIE;
    }
    return RS_HARDWARE_SERVER;
}

STATIC int rs_check_dst_interface(unsigned int phy_id, const char *ifa_name, enum rs_hardware_type type, bool is_all)
{
    char dst_ifa_bond_name[RS_INTERFACE_BOND_LEN + 1] = {0};
    char dst_ifa_name[RS_INTERFACE_LEN + 1] = {0};
    int ret, bond_ret;

    if (is_all) {
        /* get information of all device with eth or bond prefix */
        if (strncmp("eth", ifa_name, RS_INTERFACE_ETH_PREFIX_LEN) != 0 &&
            strncmp("bond", ifa_name, RS_INTERFACE_BOND_PREFIX_LEN) != 0) {
            return 0;
        }
        return 1;
    }

    // 标卡场景910B和910A device网卡固定为eth0,处理标卡场景
    if (type == RS_HARDWARE_PCIE) {
        if (strncmp("eth0", ifa_name, RS_INTERFACE_LEN) && strncmp("eth1", ifa_name, RS_INTERFACE_LEN)) {
            return 0;
        }
        return 1;
    } else if (type == RS_HARDWARE_2DIE) {
        /* 1. For RoH mode, only "bondx" port is supported when binding groups,
         * and "ethx" port is used when unbinding ;
         * 2. For eth mode, only the eth port is supported
         */
        ret = snprintf_s(dst_ifa_name, RS_INTERFACE_LEN + 1, RS_INTERFACE_LEN, "eth%u", phy_id);
        bond_ret = snprintf_s(dst_ifa_bond_name, RS_INTERFACE_BOND_LEN + 1, RS_INTERFACE_BOND_LEN, "bond%u", phy_id);
        if (ret <= 0 || bond_ret <= 0) {
            hccp_err("copy eth or bond name failed, ret(%d), bond_ret(%d)", ret, bond_ret);
            return -EAGAIN;
        }

        if (strncmp(dst_ifa_name, ifa_name, RS_INTERFACE_LEN) &&
            strncmp(dst_ifa_bond_name, ifa_name, RS_INTERFACE_BOND_LEN)) {
            return 0;
        }
    } else {
        ret = snprintf_s(dst_ifa_name, RS_INTERFACE_LEN + 1, RS_INTERFACE_LEN, "eth%u", phy_id);
        CHK_PRT_RETURN(ret <= 0, hccp_err("copy eth name failed, %d", ret), -EAGAIN);
 
        if (strncmp(dst_ifa_name, ifa_name, RS_INTERFACE_LEN)) {
            return 0;
        }
    }
    return 1;
}

STATIC int rs_peer_fill_ifnum(unsigned int phy_id, unsigned int *num, struct ifaddrs *ifaddr_list)
{
    struct ifaddrs *ifaddr = ifaddr_list;
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

    hccp_dbg("phy_id:%u got interface num:%u", phy_id, *num);
    return 0;
}

STATIC int rs_peer_fill_ifaddr_infos(struct interface_info interface_infos[], unsigned int *num,
    unsigned int phy_id, struct ifaddrs *ifaddr_list)
{
    struct ifaddrs *ifaddr = ifaddr_list;
    unsigned int num_bak = *num;
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
        if ((*num) > num_bak) {
            hccp_err("num of interfaces found is more than expect, expect[%u], actual[%u]", num_bak, *num);
            goto out;
        }
        ret = strcpy_s(interface_infos[*num - 1].ifname, MAX_INTERFACE_NAME_LEN, ifa->ifa_name);
        if (ret) {
            hccp_err("strcpy interface name failed, ret[%d]", ret);
            goto out;
        }
        interface_infos[*num - 1].scope_id = 0;
        if (family == AF_INET) {
            interface_infos[*num - 1].ifaddr.ip.addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            interface_infos[*num - 1].ifaddr.mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
            hccp_info("ifname[%s] addr[0x%08x]", ifa->ifa_name, ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr);
        } else {
            interface_infos[*num - 1].ifaddr.ip.addr6 = ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            interface_infos[*num - 1].scope_id = (int)((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id;
            hccp_info("ifname[%s] scope_id[%u] flowinfo[%u]", ifa->ifa_name,
                ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id,
                ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_flowinfo);
            for (unsigned long i = 0; i < sizeof(struct in6_addr); i++) {
                hccp_info("addr[%lu] 0x%02x", i, ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr.s6_addr[i]);
            }
        }
        interface_infos[*num - 1].family = family;
    }

    hccp_dbg("phy_id:%u got interface num:%u", phy_id, *num);
    return 0;
out:
    hccp_dbg("phy_id:%u got interface num:%u", phy_id, *num);
    return -EAGAIN;
}

// 获取device网卡信息，当前device网卡只支持IPv4
STATIC int rs_fill_ifaddr_infos(struct ifaddr_info ifaddr_infos[], unsigned int *num, unsigned int phy_id)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    int family, ret;
    unsigned int num_bak = *num;
    *num = 0;
    enum rs_hardware_type type;

    type = rs_get_device_type(phy_id);
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
        ret = rs_check_dst_interface(phy_id, ifa->ifa_name, type, false);
        if (ret < 0) {
            hccp_err("rs_check_dst_interface failed, ret[%d]", ret);
            goto out;
        }
        if (ret) {
            (*num)++;
            if ((*num) > num_bak) {
                hccp_err("num of interfaces found is more than expect, expect[%u], actual[%u]", num_bak, *num);
                goto out;
            }
            ifaddr_infos[*num - 1].ip.addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            ifaddr_infos[*num - 1].mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
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
STATIC int rs_fill_ifaddr_infos_v2(struct interface_info interface_infos[], unsigned int *num, unsigned int phy_id,
    bool is_all)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    enum rs_hardware_type type;
    unsigned int num_bak;
    int family, ret;

    num_bak = *num;
    *num = 0;
    type = rs_get_device_type(phy_id);
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

        ret = rs_check_dst_interface(phy_id, ifa->ifa_name, type, is_all);
        if (ret < 0) {
            hccp_err("rs_check_dst_interface failed, ret[%d]", ret);
            ret = -EAGAIN;
            break;
        }
        if (ret) {
            (*num)++;
            if ((*num) > num_bak) {
                hccp_err("num of interfaces found is more than expect, expect[%u], actual[%u]", num_bak, *num);
                ret = -EAGAIN;
                break;
            }

            ret = strcpy_s(interface_infos[*num - 1].ifname, MAX_INTERFACE_NAME_LEN, ifa->ifa_name);
            if (ret) {
                hccp_err("strcpy interface name failed, ret[%d]", ret);
                ret = -EAGAIN;
                break;
            }
            interface_infos[*num - 1].scope_id = 0;
            if (family == AF_INET) {
                interface_infos[*num - 1].ifaddr.ip.addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                interface_infos[*num - 1].ifaddr.mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
            } else {
                interface_infos[*num - 1].ifaddr.ip.addr6 = ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
                interface_infos[*num - 1].scope_id = (int)((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id;
            }
            interface_infos[*num - 1].family = family;
        }
    }

    freeifaddrs(ifaddr);
    ifaddr = NULL;
    return ret;
}

STATIC int rs_fill_ifnum(unsigned int phy_id, bool is_all, unsigned int *num, unsigned int is_peer)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    enum rs_hardware_type type;
    int family, ret;
    *num = 0;

    if (!is_peer) {
        type = rs_get_device_type(phy_id);
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
        if (!is_peer) {
            ret = rs_check_dst_interface(phy_id, ifa->ifa_name, type, is_all);
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

RS_ATTRI_VISI_DEF int rs_peer_get_ifnum(unsigned int phy_id, unsigned int *num)
{
    int ret;
    CHK_PRT_RETURN(num == NULL, hccp_err("param error, num is NULL"), -EINVAL);
    CHK_PRT_RETURN(g_rs_cb == NULL, hccp_err("param error, g_rs_cb is NULL"), -EINVAL);
    ret = rs_peer_fill_ifnum(phy_id, num, g_rs_cb->ifaddr_list);
    CHK_PRT_RETURN(ret, hccp_err("rs_peer_fill_ifnum failed, ret[%d]", ret), ret);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_ifnum(unsigned int phy_id, bool is_all, unsigned int *num)
{
    int ret;
    CHK_PRT_RETURN(num == NULL, hccp_err("rs_get_ifaddrs param error, num is NULL"), -EINVAL);
    ret = rs_fill_ifnum(phy_id, is_all, num, 0);
    CHK_PRT_RETURN(ret, hccp_err("rs_fill_ifnum failed, ret[%d]", ret), ret);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_peer_get_ifaddrs(struct interface_info interface_infos[], unsigned int *num,
    unsigned int phy_id)
{
    int ret;
    CHK_PRT_RETURN(interface_infos == NULL || num == NULL,
        hccp_err("param error, interface_infos or num is NULL"), -EINVAL);
    CHK_PRT_RETURN(g_rs_cb == NULL, hccp_err("param error, g_rs_cb is NULL"), -EINVAL);
    ret = rs_peer_fill_ifaddr_infos(interface_infos, num, phy_id, g_rs_cb->ifaddr_list);
    CHK_PRT_RETURN(ret, hccp_err("rs_peer_fill_ifaddr_infos failed, ret[%d]", ret), ret);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_ifaddrs(struct ifaddr_info ifaddr_infos[], unsigned int *num, unsigned int phy_id)
{
    int ret;

    CHK_PRT_RETURN(ifaddr_infos == NULL || num == NULL, hccp_err("rs_get_ifaddrs param error,"
        "ifaddr_infos or num is NULL"), -EINVAL);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM || *num > MAX_INTERFACE_NUM, hccp_err("rs_get_ifaddrs param error,"
        "phy_id[%u], num[%u]", phy_id, *num), -EINVAL);

    ret = rs_fill_ifaddr_infos(ifaddr_infos, num, phy_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_fill_ifaddr_infos failed, ret[%d]", ret), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_ifaddrs_v2(struct interface_info interface_infos[], unsigned int *num,
    unsigned int phy_id, bool is_all)
{
    int ret;

    CHK_PRT_RETURN(interface_infos == NULL || num == NULL, hccp_err("rs_get_ifaddrs_v2 param error,"
        "interface_infos or num is NULL"), -EINVAL);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM || *num > MAX_INTERFACE_NUM, hccp_err("rs_get_ifaddrs_v2 param error,"
        "phy_id[%u], num[%u]", phy_id, *num), -EINVAL);

    ret = rs_fill_ifaddr_infos_v2(interface_infos, num, phy_id, is_all);
    CHK_PRT_RETURN(ret, hccp_err("rs_fill_ifaddr_infos_v2 failed, ret[%d]", ret), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_socket_set_scope_id(unsigned int dev_id, int scope_id)
{
    int ret;
    unsigned int chip_id;
    struct rs_conn_cb *conn_cb = NULL;
    ret = rsGetLocalDevIDByHostDevID(dev_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id invalid, ret %d", ret), ret);

    ret = rs_dev2conncb(chip_id, &conn_cb);
    CHK_PRT_RETURN(ret, hccp_err("get conncb from dev fail, ret:%d", ret), ret);

    conn_cb->scope_id = scope_id;
    return 0;
}
