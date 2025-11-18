/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "user_log.h"
#include "ra_hdc.h"
#include "securec.h"
#include "ra.h"
#include "hccp.h"
#include "ra_comm.h"
#include "ra_rs_err.h"
#include "dl_hal_function.h"
#include "ra_rdma_lite.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_socket.h"

int ra_hdc_socket_batch_connect(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num)
{
    union op_socket_connect_data *socket_connect_data = NULL;
    unsigned int interface_version = 0;
    int ret;

    socket_connect_data = (union op_socket_connect_data *)calloc(sizeof(union op_socket_connect_data), sizeof(char));
    CHK_PRT_RETURN(socket_connect_data == NULL, hccp_err("[batch_connect][ra_hdc_socket]calloc socket_connect_data "
        "failed, phy_id(%u).", phy_id), -ENOMEM);

    socket_connect_data->tx_data.num = num;

    ret = ra_get_socket_connect_info(conn, num, socket_connect_data->tx_data.conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]ra_get_socket_connect_info failed, ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

    // check opcode version, use port by default
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_SOCKET_CONN, &interface_version);
    if (ret == 0 && interface_version >= RA_RS_SOCKET_CONN_VERSION) {
        socket_connect_data->tx_data.num |= (1U << SOCKET_USE_PORT_BIT);
    }

    ret = ra_hdc_process_msg(RA_RS_SOCKET_CONN, phy_id, (char *)socket_connect_data,
        sizeof(union op_socket_connect_data));
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]ra hdc message process failed, ret(%d) phy_id(%u)", ret, phy_id);
    }
out:
    free(socket_connect_data);
    socket_connect_data = NULL;
    return ret;
}

int ra_hdc_socket_listen_start(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num)
{
    union op_socket_listen_data socket_listen_data = {0};
    unsigned int interface_version = 0;
    int ret;

    socket_listen_data.tx_data.num = num;
    ret = ra_get_socket_listen_info(conn, num, socket_listen_data.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]ra_get_socket_listen_info failed, "
        "ret(%d) phy_id(%u)", ret, phy_id), -EINVAL);

    // check opcode version, use port by default
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_SOCKET_LISTEN_START, &interface_version);
    if (ret == 0 && interface_version >= RA_RS_SOCKET_LISTEN_VERSION) {
        socket_listen_data.tx_data.num |= (1U << SOCKET_USE_PORT_BIT);
    }

    ret = ra_hdc_process_msg(RA_RS_SOCKET_LISTEN_START, phy_id, (char *)&socket_listen_data,
        sizeof(union op_socket_listen_data));
    CHK_PRT_RETURN(ret == -EADDRINUSE, hccp_run_warn("[listen_start][ra_hdc_socket]ra hdc message process unsuccessful,"
        " ret(%d) phy_id(%u)", ret, phy_id), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]ra hdc message process failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ret = ra_get_socket_listen_result(socket_listen_data.rx_data.conn, num, conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]ra_get_socket_listen_result failed, ret(%d) "
        "phy_id(%u)", ret, phy_id), -EINVAL);

    return ret;
}

int ra_hdc_socket_batch_close(unsigned int phy_id, struct socket_close_info_t conn[], unsigned int num)
{
    union op_socket_close_data socket_close_data = {0};
    unsigned int interface_version = 0;
    unsigned int i;
    int ret;

    socket_close_data.tx_data.num = num;
    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle == NULL) {
            hccp_err("[batch_close][ra_hdc_socket]i(%u), conn fd_handle is null", i);
            ret = -EINVAL;
            goto out;
        }
        socket_close_data.tx_data.conn[i].phy_id = phy_id;
        socket_close_data.tx_data.conn[i].close_fd = ((struct socket_hdc_info *)conn[i].fd_handle)->fd;
    }

    // check opcode version, use RA_RS_GET_SOCKET opcode due to compatibility issue
    // use attr disuse_linger of the fist conn as the common attr for all(0 by default)
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_GET_SOCKET, &interface_version);
    if (ret == 0 && interface_version >= RA_RS_GET_SOCKET_VERSION && conn[0].disuse_linger != 0) {
        socket_close_data.tx_data.num |= (1U << SOCKET_DISUSE_LINGER_BIT);
    }

    ret = ra_hdc_process_msg(RA_RS_SOCKET_CLOSE, phy_id, (char *)&socket_close_data,
        sizeof(union op_socket_close_data));
    if (ret) {
        hccp_err("[batch_close][ra_hdc_socket]ra hdc message process failed, ret(%d) phy_id(%u).", ret, phy_id);
        goto out;
    }

out:
    if (ret != (-EAGAIN)) {
        for (i = 0; i < num; i++) {
            if (conn[i].fd_handle != NULL) {
                free(conn[i].fd_handle);
                conn[i].fd_handle = NULL;
            }
        }
    }
    return ret;
}

int ra_hdc_socket_batch_abort(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num)
{
    union op_socket_connect_data *socket_connect_data = NULL;
    int ret;

    socket_connect_data = (union op_socket_connect_data *)calloc(sizeof(union op_socket_connect_data), sizeof(char));
    CHK_PRT_RETURN(socket_connect_data == NULL, hccp_err("[batch_abort][ra_hdc_socket]calloc socket_connect_data "
        "failed. phy_id(%u).", phy_id), -ENOMEM);

    socket_connect_data->tx_data.num = num;
    ret = ra_get_socket_connect_info(conn, num, socket_connect_data->tx_data.conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[batch_abort][ra_hdc_socket]ra_get_socket_connect_info failed, ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

    ret = ra_hdc_process_msg(RA_RS_SOCKET_ABORT, phy_id, (char *)socket_connect_data,
        sizeof(union op_socket_connect_data));
    if (ret != 0) {
        hccp_err("[batch_abort][ra_hdc_socket]ra hdc message process failed, ret(%d) phy_id(%u)", ret, phy_id);
    }
out:
    free(socket_connect_data);
    socket_connect_data = NULL;
    return ret;
}

int ra_hdc_socket_listen_stop(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num)
{
    union op_socket_listen_data socket_listen_data = {0};
    unsigned int interface_version = 0;
    int ret;

    socket_listen_data.tx_data.num = num;

    ret = ra_get_socket_listen_info(conn, num, socket_listen_data.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_hdc_socket]ra_hdc_socket_listen_stop memcpy_s failed, ret(%d)"
        "phy_id(%u).", ret, phy_id), -EINVAL);

    // check opcode version, use port by default
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_SOCKET_LISTEN_STOP, &interface_version);
    if (ret == 0 && interface_version >= RA_RS_SOCKET_LISTEN_VERSION) {
        socket_listen_data.tx_data.num |= (1U << SOCKET_USE_PORT_BIT);
    }

    ret = ra_hdc_process_msg(RA_RS_SOCKET_LISTEN_STOP, phy_id, (char *)&socket_listen_data,
        sizeof(union op_socket_listen_data));
    CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_hdc_socket]ra hdc message process failed ret(%d) phy_id(%u).",
        ret, phy_id), ret);

    return 0;
}

STATIC int ra_get_ip_and_tag_info(union op_socket_info_data *socket_info_data, struct ra_socket_handle *socket_handle,
    struct socket_info_t conn[], unsigned int index)
{
    int ret;

    ret = memcpy_s(&(socket_info_data->tx_data.conn[index].local_ip), sizeof(union hccp_ip_addr),
        &(socket_handle->rdev_info.local_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_ip_and_tag_info]memcpy_s local_ip failed, ret(%d), index(%u)",
        ret, index), -ESAFEFUNC);
    ret = memcpy_s(&(socket_info_data->tx_data.conn[index].remote_ip), sizeof(union hccp_ip_addr),
        &(conn[index].remote_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_ip_and_tag_info]memcpy_s remote_ip failed, ret(%d), index(%u)",
        ret, index), -ESAFEFUNC);
    ret = memcpy_s(socket_info_data->tx_data.conn[index].tag, sizeof(socket_info_data->tx_data.conn[index].tag),
        conn[index].tag, sizeof(conn[index].tag));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_ip_and_tag_info]memcpy_s tag failed, ret(%d), index(%u)",
        ret, index), -ESAFEFUNC);

    return 0;
}

STATIC int ra_assemble_sockets(union op_socket_info_data *socket_info_data, struct socket_info_t *conn,
    unsigned int num, const int sock_fd[], size_t sock_fd_len)
{
    unsigned int i;
    int ret;
    struct ra_socket_handle *socket_handle = NULL;

    for (i = 0; (i < num) && (i < sock_fd_len); i++) {
        if (conn[i].fd_handle == NULL) {
            conn[i].fd_handle = (struct socket_hdc_info *)calloc(1, sizeof(struct socket_hdc_info));
            if (conn[i].fd_handle == NULL) {
                hccp_err("[assemble][ra_sockets]fd handle calloc failed.");
                ret = -ENOMEM;
                goto calloc_err;
            }
        }
        ((socket_info_data->tx_data).conn[i]).fd = sock_fd[i];
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        socket_info_data->tx_data.conn[i].phy_id = socket_handle->rdev_info.phy_id;
        socket_info_data->tx_data.conn[i].family = socket_handle->rdev_info.family;
        ret = ra_get_ip_and_tag_info(socket_info_data, socket_handle, conn, i);
        if (ret) {
            hccp_err("[assemble][ra_sockets]ra_get_ip_and_tag_info failed, ret(%d)", ret);
            ret = -EINVAL;
            goto mem_err;
        }
    }

    return 0;

mem_err:
    i++;
calloc_err:
    for (; i > 0;) {
        i--;
        if (conn[i].fd_handle != NULL) {
            free(conn[i].fd_handle);
            conn[i].fd_handle = NULL;
        }
    }

    return ret;
}

static void free_assemble_sockets(struct socket_info_t conn[], unsigned int num)
{
    unsigned int i;

    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle != NULL) {
            free(conn[i].fd_handle);
            conn[i].fd_handle = NULL;
        }
    }
}

int ra_hdc_socket_send(unsigned int phy_id, const void *handle, const void *data, unsigned long long size)
{
    union op_socket_send_data *send_data_head = NULL;
    int real_send_size;
    int ret;

    if (size > SOCKET_SEND_MAXLEN) {
        size = SOCKET_SEND_MAXLEN;
    }
    send_data_head = (union op_socket_send_data *)calloc(sizeof(union op_socket_send_data), sizeof(char));
    CHK_PRT_RETURN(send_data_head == NULL, hccp_err("[send][ra_hdc_socket]calloc failed, phy_id(%u)",
        phy_id), -ENOMEM);

    send_data_head->tx_data.fd = (unsigned int)((const struct socket_hdc_info *)handle)->fd;
    send_data_head->tx_data.send_size = size;

    ret = memcpy_s(send_data_head->tx_data.data_send, SOCKET_SEND_MAXLEN, data, size);
    if (ret) {
        hccp_err("[send][ra_hdc_socket]memcpy_s failed, ret(%d) phy_id(%u)", ret, phy_id);
        real_send_size = -ESAFEFUNC;
        goto out;
    }

    ret = ra_hdc_process_msg(RA_RS_SOCKET_SEND, phy_id, (char *)send_data_head,
        sizeof(union op_socket_send_data));
    if (ret) {
        if (ret > 0) {
            ret = -EINVAL; /* 0:success; ret > 0:failed maybe drv interface return; ret < 0:failed maybe rs return */
        }
        if (ret != -EAGAIN) {
            hccp_warn("[send][ra_hdc_socket]ra hdc message process unsuccessful, ret(%d) phy_id(%u)", ret, phy_id);
        }
        real_send_size = ret;
        goto out;
    }

    real_send_size = (int)send_data_head->rx_data.real_send_size;

out:
    free(send_data_head);
    send_data_head = NULL;
    return real_send_size;
}

int ra_hdc_socket_recv(unsigned int phy_id, const void *handle, void *data, unsigned long long size)
{
    union op_socket_recv_data *recv_data_head = NULL;
    int real_recv_size;
    int ret;

    if (size > SOCKET_SEND_MAXLEN) {
        size = SOCKET_SEND_MAXLEN;
    }

    recv_data_head = (union op_socket_recv_data *)calloc(size + sizeof(union op_socket_recv_data), sizeof(char));
    CHK_PRT_RETURN(recv_data_head == NULL, hccp_err("[recv][ra_hdc_socket]calloc failed. phy_id(%u)", phy_id), -ENOMEM);
    recv_data_head->tx_data.fd = (unsigned int)((const struct socket_hdc_info *)handle)->fd;
    recv_data_head->tx_data.recv_size = size;

    ret = ra_hdc_process_msg(RA_RS_SOCKET_RECV, phy_id, (char *)recv_data_head,
        sizeof(union op_socket_recv_data) + size);
    if (ret) {
        if (ret > 0) {
            ret = -EINVAL; /* 0:success; ret > 0:failed maybe drv interface return; ret < 0:failed maybe rs return */
        }
        if (ret != -EAGAIN) {
            hccp_warn("[recv][ra_hdc_socket]ra hdc message process unsuccessful, ret(%d) phy_id(%u)", ret, phy_id);
        }
        real_recv_size = ret;
        goto out;
    }

    real_recv_size = (int)recv_data_head->rx_data.real_recv_size;
    if (real_recv_size <= 0) {
        goto out;
    } else {
        ret = memcpy_s(data, size, (char *)recv_data_head + sizeof(union op_socket_recv_data), real_recv_size);
        if (ret) {
            hccp_err("[recv][ra_hdc_socket]memcpy_s failed, ret(%d) phy_id(%u)", ret, phy_id);
            real_recv_size = -ESAFEFUNC;
            goto out;
        }
    }

out:
    free(recv_data_head);
    recv_data_head = NULL;
    return real_recv_size;
}

STATIC int ra_get_recv_sockets(union op_socket_info_data *socket_info_data, struct socket_info_t conn[],
    unsigned int num)
{
    unsigned int i;
    int real_num;
    int ret;
    struct ra_socket_handle *socket_handle = NULL;

    for (i = 0; i < num; i++) {
        ((struct socket_hdc_info *)conn[i].fd_handle)->phy_id = socket_info_data->rx_data.conn[i].phy_id;
        ((struct socket_hdc_info *)conn[i].fd_handle)->fd = socket_info_data->rx_data.conn[i].fd;
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        ((struct socket_hdc_info *)conn[i].fd_handle)->socket_handle = socket_handle;
        ret = memcpy_s(&(socket_handle->rdev_info.local_ip), sizeof(union hccp_ip_addr),
            &(socket_info_data->rx_data.conn[i].local_ip), sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_recv_sockets]memcpy_s local_ip failed, ret(%d)", ret), -ESAFEFUNC);
        ret = memcpy_s(&(conn[i].remote_ip), sizeof(union hccp_ip_addr), &(socket_info_data->rx_data.conn[i].remote_ip),
            sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_recv_sockets]memcpy_s remote_ip failed, ret(%d)", ret), -ESAFEFUNC);
        conn[i].status = socket_info_data->rx_data.conn[i].status;
    }

    real_num = socket_info_data->rx_data.num;
    return real_num;
}

int ra_hdc_get_sockets(unsigned int phy_id, unsigned int role, struct socket_info_t conn[], unsigned int num)
{
    int ret;
    int sock_fd[MAX_SOCKET_NUM] = {0};
    union op_socket_info_data *socket_info_data;

    socket_info_data = (union op_socket_info_data *)calloc(sizeof(union op_socket_info_data), sizeof(char));
    CHK_PRT_RETURN(socket_info_data == NULL, hccp_err("[get][ra_hdc_sockets]socket info data"
        "calloc failed phy_id(%u)", phy_id), -ENOMEM);
    socket_info_data->tx_data.num = num;
    socket_info_data->tx_data.role = role;

    ret = ra_assemble_sockets(socket_info_data, conn, num, sock_fd, sizeof(sock_fd) / sizeof(sock_fd[0]));
    if (ret) {
        hccp_err("[get][ra_hdc_sockets]assemble sockets error ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

    ret = ra_hdc_process_msg(RA_RS_GET_SOCKET, phy_id, (char *)socket_info_data,
        sizeof(union op_socket_info_data));
    if (ret) {
        hccp_err("[get][ra_hdc_sockets]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
        ret = -EINVAL; /* !=0 is error situation return negative value, function normal return >=0 */
        goto err;
    }

    ret = ra_get_recv_sockets(socket_info_data, conn, num);
    // no sockets get, free socket info(fd_handle)
    if (ret == 0) {
        goto err;
    }
    free(socket_info_data);
    socket_info_data = NULL;
    return ret;

err:
    free_assemble_sockets(conn, num);
out:
    free(socket_info_data);
    socket_info_data = NULL;
    return ret;
}

STATIC int ra_hdc_get_all_vnic(unsigned int cur_phy_id, unsigned int *vnic_ip, unsigned int num)
{
    int ret;
    unsigned int logic_id, phy_id;
    unsigned int dev_num;
    union op_get_vnic_ip_data vnic_ip_data;

    ret = dl_drv_get_dev_num(&dev_num);
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_all_vnic]get dev num failed, ret(%d)", ret), ret);

    for (logic_id = 0; logic_id < dev_num; logic_id++) {
        ret = dl_drv_device_get_phy_id_by_index(logic_id, &phy_id);
        CHK_PRT_RETURN(ret != 0 || phy_id >= RA_MAX_PHY_ID_NUM || phy_id >= num, hccp_err("[get][ra_hdc_all_vnic]get phy"
            "id failed, logic_id(%u) ret(%d) phy_id(%u) >= %d or >= %u invalid", logic_id, ret, phy_id,
            RA_MAX_PHY_ID_NUM, num), -ENODEV);

        vnic_ip_data.tx_data.phy_id = phy_id;
        ret = ra_hdc_process_msg(RA_RS_GET_VNIC_IP, cur_phy_id,
            (char *)&vnic_ip_data, sizeof(union op_get_vnic_ip_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_all_vnic]ra hdc message process failed ret(%d), phy_id(%d),"
            "logic_id(%d)", ret, phy_id, logic_id), ret);

        vnic_ip[phy_id] = vnic_ip_data.rx_data.vnic_ip;
        hccp_info("vnic ipaddr:0x%x, get vnic_ip:0x%x, phy_id:%u, logic_id:%u", vnic_ip_data.rx_data.vnic_ip,
                  vnic_ip[phy_id], phy_id, logic_id);
    }

    return 0;
}

int ra_hdc_get_vnic_ip_infos_v1(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    union op_get_vnic_ip_infos_data_v1 vnic_ip_data = {0};
    unsigned int complete_cnt = 0;
    unsigned int send_num = 0;
    int ret;

    while (complete_cnt < num) {
        vnic_ip_data.tx_data.phy_id = phy_id;
        send_num = ((num - complete_cnt) >= MAX_IP_INFO_NUM_V1) ? MAX_IP_INFO_NUM_V1 : (num - complete_cnt);
        ret = memcpy_s(vnic_ip_data.tx_data.ids, sizeof(unsigned int) * MAX_IP_INFO_NUM_V1,
            &ids[complete_cnt], sizeof(unsigned int) * send_num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        vnic_ip_data.tx_data.num = send_num;
        vnic_ip_data.tx_data.type = type;

        ret = ra_hdc_process_msg(RA_RS_GET_VNIC_IP_INFOS_V1, phy_id, (char *)&vnic_ip_data,
            sizeof(union op_get_vnic_ip_infos_data_v1));
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]ra hdc message process failed ret(%d), phy_id(%u)",
            ret, phy_id), ret);

        ret = memcpy_s(&infos[complete_cnt], sizeof(struct ip_info) * (num - complete_cnt),
            vnic_ip_data.rx_data.infos, sizeof(struct ip_info) * send_num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        complete_cnt += send_num;
    }

    return 0;
}

int ra_hdc_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    union op_get_vnic_ip_infos_data vnic_ip_data = {0};
    unsigned int interface_version = 0;
    unsigned int complete_cnt = 0;
    unsigned int send_num = 0;
    int ret;

    // origin procedure for compatibility issue
    ret = ra_hdc_get_interface_version(phy_id, RA_RS_GET_VNIC_IP_INFOS, &interface_version);
    if (ret != 0 || interface_version < RA_RS_GET_VNIC_IP_INFOS_VERSION) {
        return ra_hdc_get_vnic_ip_infos_v1(phy_id, type, ids, num, infos);
    }

    while (complete_cnt < num) {
        vnic_ip_data.tx_data.phy_id = phy_id;
        send_num = ((num - complete_cnt) >= MAX_IP_INFO_NUM) ? MAX_IP_INFO_NUM : (num - complete_cnt);
        ret = memcpy_s(vnic_ip_data.tx_data.ids, sizeof(unsigned int) * MAX_IP_INFO_NUM,
            &ids[complete_cnt], sizeof(unsigned int) * send_num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        vnic_ip_data.tx_data.num = send_num;
        vnic_ip_data.tx_data.type = type;

        ret = ra_hdc_process_msg(RA_RS_GET_VNIC_IP_INFOS, phy_id, (char *)&vnic_ip_data,
            sizeof(union op_get_vnic_ip_infos_data));
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]ra hdc message process failed ret(%d), phy_id(%u)",
            ret, phy_id), ret);

        ret = memcpy_s(&infos[complete_cnt], sizeof(struct ip_info) * (num - complete_cnt),
            vnic_ip_data.rx_data.infos, sizeof(struct ip_info) * send_num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        complete_cnt += send_num;
    }

    return 0;
}

int ra_hdc_socket_init(struct rdev rdev_info)
{
    unsigned int vnic_ip[RA_MAX_VNIC_NUM] = {0};
    union op_socket_init_data socket_init_data;
    unsigned int interface_version = 0;
    int ret;

    ret = memset_s(&socket_init_data, sizeof(socket_init_data), 0, sizeof(socket_init_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]memset_s failed ret(%d) phy_id(%u)", ret,
        rdev_info.phy_id), -ESAFEFUNC);

    // check opcode version, init g_vnics with invalid ip mask 0xFFFFFFFF
    ret = ra_hdc_get_interface_version(rdev_info.phy_id, RA_RS_GET_VNIC_IP_INFOS_V1, &interface_version);
    if (ret == 0 && interface_version >= RA_RS_GET_VNIC_IP_INFOS_VERSION) {
        ret = memset_s(vnic_ip, sizeof(vnic_ip), 0xFFFFFFFF, sizeof(vnic_ip));
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]memset_s failed ret(%d) phy_id(%u)", ret,
            rdev_info.phy_id), -ESAFEFUNC);
    } else {
        // origin procedure: init g_vnics with vnic_ip get by phy_id
        ret = ra_hdc_get_all_vnic(rdev_info.phy_id, vnic_ip, RA_MAX_VNIC_NUM);
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]ra_hdc_get_all_vnic failed ret(%d) phy_id(%u)", ret,
            rdev_info.phy_id), ret);
    }

    ret = memcpy_s(&(socket_init_data.tx_data.vnic_ip), sizeof(socket_init_data.tx_data.vnic_ip),
        &vnic_ip, sizeof(vnic_ip));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]memcpy_s for vnic_ip failed ret(%d) phy_id(%u)", ret,
        rdev_info.phy_id), -ESAFEFUNC);

    socket_init_data.tx_data.num = RA_MAX_VNIC_NUM;
    ret = ra_hdc_process_msg(RA_RS_SOCKET_INIT, rdev_info.phy_id, (char *)&socket_init_data,
        sizeof(union op_socket_init_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]ra hdc message process failed ret(%d) phy_id(%u)", ret,
        rdev_info.phy_id), ret);

    return 0;
}

int ra_hdc_socket_deinit(struct rdev rdev_info)
{
    int ret;
    union op_socket_deinit_data socket_deinit_data;

    ret = memset_s(&socket_deinit_data, sizeof(socket_deinit_data), 0, sizeof(socket_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_socket]memset_s failed ret(%d) phy_id(%u)", ret,
        rdev_info.phy_id), -ESAFEFUNC);

    ret = memcpy_s(&(socket_deinit_data.tx_data.rdev_info), sizeof(struct rdev), &rdev_info, sizeof(struct rdev));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_socket]memcpy_s failed ret(%d) phy_id(%u)", ret,
        rdev_info.phy_id), -ESAFEFUNC);

    ret = ra_hdc_process_msg(RA_RS_SOCKET_DEINIT, rdev_info.phy_id, (char *)&socket_deinit_data,
        sizeof(union op_socket_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_socket]ra hdc message process failed ret(%d) phy_id(%u)", ret,
        rdev_info.phy_id), ret);

    return 0;
}

int ra_hdc_get_ifnum(unsigned int phy_id, bool is_all, unsigned int *num)
{
    union op_ifnum_data ifnum_data = {0};
    int ret;

    ifnum_data.tx_data.num = is_all ? RA_RS_GET_ALL_IP_BIT_MASK : 0;
    ifnum_data.tx_data.phy_id = phy_id;
    ret = ra_hdc_process_msg(RA_RS_GET_IFNUM, phy_id, (char *)&ifnum_data, sizeof(union op_ifnum_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifnum]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    *num = ifnum_data.rx_data.num;

    return 0;
}

int ra_hdc_get_ifaddrs(unsigned int phy_id, struct ifaddr_info ifaddr_infos[], unsigned int *num)
{
    union op_ifaddr_data ifaddr_data = {0};
    int ret;

    ifaddr_data.tx_data.num = *num;
    ret = memcpy_s(ifaddr_data.tx_data.ifaddr_infos, sizeof(struct ifaddr_info) * MAX_INTERFACE_NUM, ifaddr_infos,
        sizeof(struct ifaddr_info) * (*num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs]memcpy_s failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);

    ifaddr_data.tx_data.phy_id = phy_id;
    ret = ra_hdc_process_msg(RA_RS_GET_IFADDRS, phy_id, (char *)&ifaddr_data, sizeof(union op_ifaddr_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ret = memcpy_s(ifaddr_infos, sizeof(struct ifaddr_info) * (*num), ifaddr_data.rx_data.ifaddr_infos,
        sizeof(struct ifaddr_info) * (ifaddr_data.rx_data.num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs]memcpy_s failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);

    *num = ifaddr_data.rx_data.num;
    return 0;
}

int ra_hdc_get_ifaddrs_v2(unsigned int phy_id, bool is_all, struct interface_info interface_infos[], unsigned int *num)
{
    union op_ifaddr_data_v2 ifaddr_data = {0};
    int ret;

    ifaddr_data.tx_data.num = is_all ? (*num | RA_RS_GET_ALL_IP_BIT_MASK) : *num;
    ret = memcpy_s(ifaddr_data.tx_data.interface_infos, sizeof(struct interface_info) * MAX_INTERFACE_NUM,
        interface_infos, sizeof(struct interface_info) * (*num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs_v2]memcpy_s tx interface infos failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);

    ifaddr_data.tx_data.phy_id = phy_id;
    ret = ra_hdc_process_msg(RA_RS_GET_IFADDRS_V2, phy_id, (char *)&ifaddr_data, sizeof(union op_ifaddr_data_v2));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs_v2]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ret = memcpy_s(interface_infos, sizeof(struct interface_info) * (*num), ifaddr_data.rx_data.interface_infos,
        sizeof(struct interface_info) * (ifaddr_data.rx_data.num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs_v2]memcpy_s rx interface infos failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);

    *num = ifaddr_data.rx_data.num;
    return 0;
}

static int ra_hdc_socket_white_list_op_v1(unsigned int opcode, struct rdev rdev_info,
    struct socket_wlist_info_t white_list[], unsigned int num)
{
    union op_wlist_data *wlist_data = NULL;
    int ret;
    wlist_data = (union op_wlist_data *)calloc(sizeof(union op_wlist_data), sizeof(char));
    CHK_PRT_RETURN(wlist_data == NULL, hccp_err("[op][ra_hdc_socket_white_list]calloc wlist data failed! phy_id(%u)",
        rdev_info.phy_id), -ENOMEM);
    wlist_data->tx_data.num = num;
    ret = memcpy_s(&(wlist_data->tx_data.rdev_info), sizeof(struct rdev), &(rdev_info), sizeof(struct rdev));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for rdev_info failed, ret(%d) phy_id(%u)",
                 ret, rdev_info.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = memcpy_s(wlist_data->tx_data.wlist, sizeof(struct socket_wlist_info_t) * MAX_WLIST_NUM_V1, white_list,
        sizeof(struct socket_wlist_info_t) * num);
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for wlist failed, ret(%d) phy_id(%u)", ret, rdev_info.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = ra_hdc_process_msg(opcode, rdev_info.phy_id, (char *)wlist_data, sizeof(union op_wlist_data));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]ra hdc process msg failed ret(%d) phy_id(%u)", ret, rdev_info.phy_id);
        goto out;
    }

out:
    free(wlist_data);
    wlist_data = NULL;
    return ret;
}

static int ra_hdc_socket_white_list_op_v2(unsigned int opcode, struct rdev rdev_info,
    struct socket_wlist_info_t white_list[], unsigned int num)
{
    int ret;
    union op_wlist_data_v2 *wlist_data = NULL;

    wlist_data = (union op_wlist_data_v2 *)calloc(sizeof(union op_wlist_data_v2), sizeof(char));
    CHK_PRT_RETURN(wlist_data == NULL, hccp_err("[op][ra_hdc_socket_white_list]calloc wlist data failed! phy_id(%u)",
        rdev_info.phy_id), -ENOMEM);
    wlist_data->tx_data.num = num;
    ret = memcpy_s(&(wlist_data->tx_data.rdev_info), sizeof(struct rdev), &(rdev_info), sizeof(struct rdev));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for rdev_info failed, ret(%d) phy_id(%u)",
                 ret, rdev_info.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = memcpy_s(wlist_data->tx_data.wlist, sizeof(struct socket_wlist_info_t) * MAX_WLIST_NUM, white_list,
        sizeof(struct socket_wlist_info_t) * num);
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for wlist failed, ret(%d) phy_id(%u)", ret, rdev_info.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = ra_hdc_process_msg(opcode, rdev_info.phy_id, (char *)wlist_data, sizeof(union op_wlist_data_v2));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]ra hdc process msg failed ret(%d) phy_id(%u)", ret, rdev_info.phy_id);
        goto out;
    }

out:
    free(wlist_data);
    wlist_data = NULL;
    return ret;
}

int ra_hdc_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num)
{
    int ret;
    unsigned int interface_version = 0;

    ret = ra_hdc_get_interface_version(rdev_info.phy_id, RA_RS_WLIST_ADD_V2, &interface_version);
    if (ret != 0 || interface_version != RA_RS_WLIST_ADD_V2_VERSION) {
        return ra_hdc_socket_white_list_op_v1(RA_RS_WLIST_ADD, rdev_info, white_list, num);
    }

    return ra_hdc_socket_white_list_op_v2(RA_RS_WLIST_ADD_V2, rdev_info, white_list, num);
}

int ra_hdc_socket_white_list_del(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num)
{
    int ret;
    unsigned int interface_version = 0;

    ret = ra_hdc_get_interface_version(rdev_info.phy_id, RA_RS_WLIST_DEL_V2, &interface_version);
    if (ret != 0 || interface_version != RA_RS_WLIST_DEL_V2_VERSION) {
        return ra_hdc_socket_white_list_op_v1(RA_RS_WLIST_DEL, rdev_info, white_list, num);
    }

    return ra_hdc_socket_white_list_op_v2(RA_RS_WLIST_DEL_V2, rdev_info, white_list, num);
}

int ra_hdc_socket_accept_credit_add(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    unsigned int credit_limit)
{
    union op_accept_credit_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.credit_limit = credit_limit;
    op_data.tx_data.num = num;
    ret = ra_get_socket_listen_info(conn, num, op_data.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[set][ra_hdc_socket]ra_get_socket_listen_info failed, ret(%d) phy_id(%u)",
        ret, phy_id), -EINVAL);

    ret = ra_hdc_process_msg(RA_RS_ACCEPT_CREDIT_ADD, phy_id, (char *)&op_data, sizeof(union op_accept_credit_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[set][ra_hdc_socket]ra hdc message process failed, ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return ret;
}
