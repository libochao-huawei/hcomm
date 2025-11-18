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
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>

#include "securec.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_tls.h"
#include "rs_drv_socket.h"
#include "ssl_adp.h"

int rs_drv_ssl_bind_fd(struct rs_conn_info *conn, int fd)
{
    int ret;
    if (conn->ssl == NULL) {
        conn->ssl = ssl_adp_new(g_rs_cb->client_ssl_ctx);
        CHK_PRT_RETURN(conn->ssl == NULL, hccp_err("server ssl ctx alloc failed"), -ENOMEM);
    }

    ssl_adp_set_mode(conn->ssl, SSL_MODE_AUTO_RETRY);
    ret = ssl_adp_set_fd(conn->ssl, fd);
    if (ret != 1) {
        hccp_err("bind connfd and ssl failed, ret %d", ret);
        goto out;
    }

    ssl_adp_set_connect_state(conn->ssl);

    return 0;
out:
    ssl_adp_shutdown(conn->ssl);
    ssl_adp_free(conn->ssl);
    conn->ssl = NULL;
    return -EINVAL;
}

int rs_drv_connect(int fd, struct rs_ip_addr_info *server_ip, struct rs_ip_addr_info *client_ip, uint16_t port)
{
    int ret;
    int err_no;
    union rs_socketaddr client_addr = { 0 };
    socklen_t client_addr_len = 0;
    uint16_t client_port = 0;

    hccp_info("IP(%s) port %d family %d fd:%d begin", server_ip->read_addr, port, client_ip->family, fd);
    if (client_ip->family == AF_INET) {
        struct sockaddr_in addr = {0};
        addr.sin_family = client_ip->family;
        addr.sin_port = htons(port);
        addr.sin_addr = server_ip->bin_addr.addr;
        ret = connect(fd, &addr, sizeof(addr));
    } else {
        struct sockaddr_in6 addr = {0};
        addr.sin6_family = client_ip->family;
        addr.sin6_port = htons(port);
        addr.sin6_addr = server_ip->bin_addr.addr6;
        ret = connect(fd, &addr, sizeof(addr));
    }

    if (ret) {
        err_no = errno;
        if (err_no == -EISCONN) {
            goto out;
        }

        /*
         * if the errno is EINTR, it can not retry directly,
         * otherwise it will directly return an error
         */
        hccp_warn("connect not success, need to try again! server IP:%s, port:%d, fd:%d, ret:%d, err_no:%d",
            server_ip->read_addr, port, fd, ret, err_no);

        return -err_no;
    }

out:
    client_addr_len = (client_ip->family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    getsockname(fd, (struct sockaddr *)&client_addr, &client_addr_len);
    client_port =
        (client_ip->family == AF_INET) ? ntohs(client_addr.s_addr.sin_port) : ntohs(client_addr.s_addr6.sin6_port);

    if ((client_port < 60000) || (client_port > 60015)) { // HCCL默认监听60000-60015端口,如client使用该端口，记录EVENT日志
        hccp_info("client connect success. client family %d addr %s:%u, server addr %s:%u, fd:%d", client_ip->family,
            client_ip->read_addr, client_port, server_ip->read_addr, port, fd);
    } else {
        hccp_run_info("client connect success. client family %d addr %s:%u, server addr %s:%u, fd:%d",
            client_ip->family, client_ip->read_addr, client_port, server_ip->read_addr, port, fd);
    }

    return 0;
}

int rs_drv_socket_send(int fd, const void *data, uint64_t size, int flags)
{
    int ret = 0;
    int err_no;

    CHK_PRT_RETURN(fd < 0 || size == 0 || data == NULL, hccp_err("param error ! fd:%d < 0, size:%llu or data is NULL",
        fd, size), -EINVAL);

    if (g_rs_cb->ssl_enable == RS_SSL_ENABLE) {
#ifdef CONFIG_SSL
        int err;
        struct rs_conn_info *conn = NULL;

        ret = rs_fd2conn(fd, &conn);
        CHK_PRT_RETURN(ret, hccp_err("fd to conn failed, ret:%d", ret), ret);
        ret = ssl_adp_write(conn->ssl, data, size);
        if (ret <= 0) {
            hccp_warn("ssl_adp_write ret:%d, size:%llu", ret, size);
            err = ssl_adp_get_error(conn->ssl, ret);
            rs_ssl_err_string(conn->connfd, err);
            CHK_PRT_RETURN((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ), hccp_info("ssl_adp_write need"
                "to retry"), -EAGAIN);
        }
#endif
    } else {
        ret = send(fd, data, size, flags);
        if (ret < 0) {
            err_no = errno;
            if (err_no == EAGAIN || err_no == EINTR) {
                hccp_dbg("send to fd:%d need retry, send size:%llu, ret:%d, errno:%d", fd, size, ret, err_no);
                ret = -EAGAIN;
            } else {
                hccp_warn("send to fd:%d not success, send size:%llu, ret:%d, errno:%d", fd, size, ret, err_no);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}

int rs_drv_socket_recv(int fd, void *data, uint64_t size, int flags)
{
    int ret = 0;
    int err_no;

    CHK_PRT_RETURN(fd < 0 || data == NULL || size == 0, hccp_err("param error ! fd:%d < 0 or data is NULL, size:%llu",
        fd, size), -EINVAL);

    if (g_rs_cb->ssl_enable == RS_SSL_ENABLE) {
#ifdef CONFIG_SSL
        int err;
        struct rs_conn_info *conn = NULL;

        ret = rs_fd2conn(fd, &conn);
        CHK_PRT_RETURN(ret, hccp_warn("can not find conn for fd[%d], ret:%d, the local fd may have been closed ",
            fd, ret), ret);
        ret = ssl_adp_read(conn->ssl, data, size);
        if (ret <= 0) {
            hccp_dbg("ssl_adp_read ret:%d, size:%llu", ret, size);
            err = ssl_adp_get_error(conn->ssl, ret);
            rs_ssl_err_string(conn->connfd, err);
            CHK_PRT_RETURN((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ), hccp_dbg("ssl_adp_read"
                "need to retry"), -EAGAIN);
        }
#endif
    } else {
        ret = recv(fd, data, size, flags);
        if (ret < 0) {
            err_no = errno;
            // not to print to avoid log flush
            if (err_no == EAGAIN || err_no == EINTR) {
                ret = -EAGAIN;
            } else {
                hccp_warn("recv for fd:%d not success, recv size:%llu, ret:%d, err_no:%d", fd, size, ret, err_no);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}


