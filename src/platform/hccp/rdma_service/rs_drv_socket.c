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

int RsDrvSslBindFd(struct RsConnInfo *conn, int fd)
{
    int ret;
    if (conn->ssl == NULL) {
        conn->ssl = ssl_adp_new(gRsCb->clientSslCtx);
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

int RsDrvConnect(int fd, struct RsIpAddrInfo *serverIp, struct RsIpAddrInfo *clientIp, uint16_t port)
{
    int ret;
    int errNo;
    union RsSocketaddr clientAddr = { 0 };
    socklen_t clientAddrLen = 0;
    uint16_t clientPort = 0;

    hccp_info("IP(%s) port %d family %d fd:%d begin", serverIp->readAddr, port, clientIp->family, fd);
    if (clientIp->family == AF_INET) {
        struct sockaddr_in addr = {0};
        addr.sin_family = clientIp->family;
        addr.sin_port = htons(port);
        addr.sin_addr = serverIp->binAddr.addr;
        ret = connect(fd, &addr, sizeof(addr));
    } else {
        struct sockaddr_in6 addr = {0};
        addr.sin6_family = clientIp->family;
        addr.sin6_port = htons(port);
        addr.sin6_addr = serverIp->binAddr.addr6;
        ret = connect(fd, &addr, sizeof(addr));
    }

    if (ret) {
        errNo = errno;
        if (errNo == -EISCONN) {
            goto out;
        }

        /*
         * if the errno is EINTR, it can not retry directly,
         * otherwise it will directly return an error
         */
        hccp_warn("connect not success, need to try again! server IP:%s, port:%d, fd:%d, ret:%d, errNo:%d",
            serverIp->readAddr, port, fd, ret, errNo);

        return -errNo;
    }

out:
    clientAddrLen = (clientIp->family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    getsockname(fd, (struct sockaddr *)&clientAddr, &clientAddrLen);
    clientPort =
        (clientIp->family == AF_INET) ? ntohs(clientAddr.sAddr.sin_port) : ntohs(clientAddr.sAddr6.sin6_port);

    if ((clientPort < 60000) || (clientPort > 60015)) { // HCCL默认监听60000-60015端口,如client使用该端口，记录EVENT日志
        hccp_info("client connect success. client family %d addr %s:%u, server addr %s:%u, fd:%d", clientIp->family,
            clientIp->readAddr, clientPort, serverIp->readAddr, port, fd);
    } else {
        hccp_run_info("client connect success. client family %d addr %s:%u, server addr %s:%u, fd:%d",
            clientIp->family, clientIp->readAddr, clientPort, serverIp->readAddr, port, fd);
    }

    return 0;
}

int RsDrvSocketSend(int fd, const void *data, uint64_t size, int flags)
{
    int ret = 0;
    int errNo;

    CHK_PRT_RETURN(fd < 0 || size == 0 || data == NULL, hccp_err("param error ! fd:%d < 0, size:%llu or data is NULL",
        fd, size), -EINVAL);

    if (gRsCb->sslEnable == RS_SSL_ENABLE) {
#ifdef CONFIG_SSL
        int err;
        struct RsConnInfo *conn = NULL;

        ret = RsFd2conn(fd, &conn);
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
            errNo = errno;
            if (errNo == EAGAIN || errNo == EINTR) {
                hccp_dbg("send to fd:%d need retry, send size:%llu, ret:%d, errno:%d", fd, size, ret, errNo);
                ret = -EAGAIN;
            } else {
                hccp_warn("send to fd:%d not success, send size:%llu, ret:%d, errno:%d", fd, size, ret, errNo);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}

int RsDrvSocketRecv(int fd, void *data, uint64_t size, int flags)
{
    int ret = 0;
    int errNo;

    CHK_PRT_RETURN(fd < 0 || data == NULL || size == 0, hccp_err("param error ! fd:%d < 0 or data is NULL, size:%llu",
        fd, size), -EINVAL);

    if (gRsCb->sslEnable == RS_SSL_ENABLE) {
#ifdef CONFIG_SSL
        int err;
        struct RsConnInfo *conn = NULL;

        ret = RsFd2conn(fd, &conn);
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
            errNo = errno;
            // not to print to avoid log flush
            if (errNo == EAGAIN || errNo == EINTR) {
                ret = -EAGAIN;
            } else {
                hccp_warn("recv for fd:%d not success, recv size:%llu, ret:%d, errNo:%d", fd, size, ret, errNo);
                ret = -EFILEOPER;
            }
        }
    }

    return ret;
}


