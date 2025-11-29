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

int RaHdcSocketBatchConnect(unsigned int phyId, struct socket_connect_info_t conn[], unsigned int num)
{
    union op_socket_connect_data *socketConnectData = NULL;
    unsigned int interfaceVersion = 0;
    int ret;

    socketConnectData = (union op_socket_connect_data *)calloc(sizeof(union op_socket_connect_data), sizeof(char));
    CHK_PRT_RETURN(socketConnectData == NULL, hccp_err("[batch_connect][ra_hdc_socket]calloc socket_connect_data "
        "failed, phyId(%u).", phyId), -ENOMEM);

    socketConnectData->tx_data.num = num;

    ret = RaGetSocketConnectInfo(conn, num, socketConnectData->tx_data.conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]ra_get_socket_connect_info failed, ret(%d) phyId(%u)", ret, phyId);
        goto out;
    }

    // check opcode version, use port by default
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_SOCKET_CONN, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_SOCKET_CONN_VERSION) {
        socketConnectData->tx_data.num |= (1U << SOCKET_USE_PORT_BIT);
    }

    ret = RaHdcProcessMsg(RA_RS_SOCKET_CONN, phyId, (char *)socketConnectData,
        sizeof(union op_socket_connect_data));
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]ra hdc message process failed, ret(%d) phyId(%u)", ret, phyId);
    }
out:
    free(socketConnectData);
    socketConnectData = NULL;
    return ret;
}

int RaHdcSocketListenStart(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num)
{
    union op_socket_listen_data socketListenData = {0};
    unsigned int interfaceVersion = 0;
    int ret;

    socketListenData.tx_data.num = num;
    ret = RaGetSocketListenInfo(conn, num, socketListenData.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]ra_get_socket_listen_info failed, "
        "ret(%d) phyId(%u)", ret, phyId), -EINVAL);

    // check opcode version, use port by default
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_SOCKET_LISTEN_START, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_SOCKET_LISTEN_VERSION) {
        socketListenData.tx_data.num |= (1U << SOCKET_USE_PORT_BIT);
    }

    ret = RaHdcProcessMsg(RA_RS_SOCKET_LISTEN_START, phyId, (char *)&socketListenData,
        sizeof(union op_socket_listen_data));
    CHK_PRT_RETURN(ret == -EADDRINUSE, hccp_run_warn("[listen_start][ra_hdc_socket]ra hdc message process unsuccessful,"
        " ret(%d) phyId(%u)", ret, phyId), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]ra hdc message process failed, ret(%d) phyId(%u)",
        ret, phyId), ret);

    ret = RaGetSocketListenResult(socketListenData.rx_data.conn, num, conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]ra_get_socket_listen_result failed, ret(%d) "
        "phyId(%u)", ret, phyId), -EINVAL);

    return ret;
}

int RaHdcSocketBatchClose(unsigned int phyId, struct socket_close_info_t conn[], unsigned int num)
{
    union op_socket_close_data socketCloseData = {0};
    unsigned int interfaceVersion = 0;
    unsigned int i;
    int ret;

    socketCloseData.tx_data.num = num;
    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle == NULL) {
            hccp_err("[batch_close][ra_hdc_socket]i(%u), conn fd_handle is null", i);
            ret = -EINVAL;
            goto out;
        }
        socketCloseData.tx_data.conn[i].phy_id = phyId;
        socketCloseData.tx_data.conn[i].close_fd = ((struct socket_hdc_info *)conn[i].fd_handle)->fd;
    }

    // check opcode version, use RA_RS_GET_SOCKET opcode due to compatibility issue
    // use attr disuse_linger of the fist conn as the common attr for all(0 by default)
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_GET_SOCKET, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_GET_SOCKET_VERSION && conn[0].disuse_linger != 0) {
        socketCloseData.tx_data.num |= (1U << SOCKET_DISUSE_LINGER_BIT);
    }

    ret = RaHdcProcessMsg(RA_RS_SOCKET_CLOSE, phyId, (char *)&socketCloseData,
        sizeof(union op_socket_close_data));
    if (ret) {
        hccp_err("[batch_close][ra_hdc_socket]ra hdc message process failed, ret(%d) phyId(%u).", ret, phyId);
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

int RaHdcSocketBatchAbort(unsigned int phyId, struct socket_connect_info_t conn[], unsigned int num)
{
    union op_socket_connect_data *socketConnectData = NULL;
    int ret;

    socketConnectData = (union op_socket_connect_data *)calloc(sizeof(union op_socket_connect_data), sizeof(char));
    CHK_PRT_RETURN(socketConnectData == NULL, hccp_err("[batch_abort][ra_hdc_socket]calloc socket_connect_data "
        "failed. phy_id(%u).", phyId), -ENOMEM);

    socketConnectData->tx_data.num = num;
    ret = RaGetSocketConnectInfo(conn, num, socketConnectData->tx_data.conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[batch_abort][ra_hdc_socket]ra_get_socket_connect_info failed, ret(%d) phyId(%u)", ret, phyId);
        goto out;
    }

    ret = RaHdcProcessMsg(RA_RS_SOCKET_ABORT, phyId, (char *)socketConnectData,
        sizeof(union op_socket_connect_data));
    if (ret != 0) {
        hccp_err("[batch_abort][ra_hdc_socket]ra hdc message process failed, ret(%d) phyId(%u)", ret, phyId);
    }
out:
    free(socketConnectData);
    socketConnectData = NULL;
    return ret;
}

int RaHdcSocketListenStop(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num)
{
    union op_socket_listen_data socketListenData = {0};
    unsigned int interfaceVersion = 0;
    int ret;

    socketListenData.tx_data.num = num;

    ret = RaGetSocketListenInfo(conn, num, socketListenData.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_hdc_socket]ra_hdc_socket_listen_stop memcpy_s failed, ret(%d)"
        "phyId(%u).", ret, phyId), -EINVAL);

    // check opcode version, use port by default
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_SOCKET_LISTEN_STOP, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_SOCKET_LISTEN_VERSION) {
        socketListenData.tx_data.num |= (1U << SOCKET_USE_PORT_BIT);
    }

    ret = RaHdcProcessMsg(RA_RS_SOCKET_LISTEN_STOP, phyId, (char *)&socketListenData,
        sizeof(union op_socket_listen_data));
    CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_hdc_socket]ra hdc message process failed ret(%d) phy_id(%u).",
        ret, phyId), ret);

    return 0;
}

STATIC int RaGetIpAndTagInfo(union op_socket_info_data *socketInfoData, struct ra_socket_handle *socketHandle,
    struct socket_info_t conn[], unsigned int index)
{
    int ret;

    ret = memcpy_s(&(socketInfoData->tx_data.conn[index].local_ip), sizeof(union hccp_ip_addr),
        &(socketHandle->rdev_info.local_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_ip_and_tag_info]memcpy_s local_ip failed, ret(%d), index(%u)",
        ret, index), -ESAFEFUNC);
    ret = memcpy_s(&(socketInfoData->tx_data.conn[index].remote_ip), sizeof(union hccp_ip_addr),
        &(conn[index].remote_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_ip_and_tag_info]memcpy_s remote_ip failed, ret(%d), index(%u)",
        ret, index), -ESAFEFUNC);
    ret = memcpy_s(socketInfoData->tx_data.conn[index].tag, sizeof(socketInfoData->tx_data.conn[index].tag),
        conn[index].tag, sizeof(conn[index].tag));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_ip_and_tag_info]memcpy_s tag failed, ret(%d), index(%u)",
        ret, index), -ESAFEFUNC);

    return 0;
}

STATIC int RaAssembleSockets(union op_socket_info_data *socketInfoData, struct socket_info_t *conn,
    unsigned int num, const int sockFd[], size_t sockFdLen)
{
    unsigned int i;
    int ret;
    struct ra_socket_handle *socketHandle = NULL;

    for (i = 0; (i < num) && (i < sockFdLen); i++) {
        if (conn[i].fd_handle == NULL) {
            conn[i].fd_handle = (struct socket_hdc_info *)calloc(1, sizeof(struct socket_hdc_info));
            if (conn[i].fd_handle == NULL) {
                hccp_err("[assemble][ra_sockets]fd handle calloc failed.");
                ret = -ENOMEM;
                goto calloc_err;
            }
        }
        ((socketInfoData->tx_data).conn[i]).fd = sockFd[i];
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        socketInfoData->tx_data.conn[i].phy_id = socketHandle->rdev_info.phy_id;
        socketInfoData->tx_data.conn[i].family = socketHandle->rdev_info.family;
        ret = RaGetIpAndTagInfo(socketInfoData, socketHandle, conn, i);
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

static void FreeAssembleSockets(struct socket_info_t conn[], unsigned int num)
{
    unsigned int i;

    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle != NULL) {
            free(conn[i].fd_handle);
            conn[i].fd_handle = NULL;
        }
    }
}

int RaHdcSocketSend(unsigned int phyId, const void *handle, const void *data, unsigned long long size)
{
    union op_socket_send_data *sendDataHead = NULL;
    int realSendSize;
    int ret;

    if (size > SOCKET_SEND_MAXLEN) {
        size = SOCKET_SEND_MAXLEN;
    }
    sendDataHead = (union op_socket_send_data *)calloc(sizeof(union op_socket_send_data), sizeof(char));
    CHK_PRT_RETURN(sendDataHead == NULL, hccp_err("[send][ra_hdc_socket]calloc failed, phyId(%u)",
        phyId), -ENOMEM);

    sendDataHead->tx_data.fd = (unsigned int)((const struct socket_hdc_info *)handle)->fd;
    sendDataHead->tx_data.send_size = size;

    ret = memcpy_s(sendDataHead->tx_data.data_send, SOCKET_SEND_MAXLEN, data, size);
    if (ret) {
        hccp_err("[send][ra_hdc_socket]memcpy_s failed, ret(%d) phyId(%u)", ret, phyId);
        realSendSize = -ESAFEFUNC;
        goto out;
    }

    ret = RaHdcProcessMsg(RA_RS_SOCKET_SEND, phyId, (char *)sendDataHead,
        sizeof(union op_socket_send_data));
    if (ret) {
        if (ret > 0) {
            ret = -EINVAL; /* 0:success; ret > 0:failed maybe drv interface return; ret < 0:failed maybe rs return */
        }
        if (ret != -EAGAIN) {
            hccp_warn("[send][ra_hdc_socket]ra hdc message process unsuccessful, ret(%d) phyId(%u)", ret, phyId);
        }
        realSendSize = ret;
        goto out;
    }

    realSendSize = (int)sendDataHead->rx_data.real_send_size;

out:
    free(sendDataHead);
    sendDataHead = NULL;
    return realSendSize;
}

int RaHdcSocketRecv(unsigned int phyId, const void *handle, void *data, unsigned long long size)
{
    union op_socket_recv_data *recvDataHead = NULL;
    int realRecvSize;
    int ret;

    if (size > SOCKET_SEND_MAXLEN) {
        size = SOCKET_SEND_MAXLEN;
    }

    recvDataHead = (union op_socket_recv_data *)calloc(size + sizeof(union op_socket_recv_data), sizeof(char));
    CHK_PRT_RETURN(recvDataHead == NULL, hccp_err("[recv][ra_hdc_socket]calloc failed. phy_id(%u)", phyId), -ENOMEM);
    recvDataHead->tx_data.fd = (unsigned int)((const struct socket_hdc_info *)handle)->fd;
    recvDataHead->tx_data.recv_size = size;

    ret = RaHdcProcessMsg(RA_RS_SOCKET_RECV, phyId, (char *)recvDataHead,
        sizeof(union op_socket_recv_data) + size);
    if (ret) {
        if (ret > 0) {
            ret = -EINVAL; /* 0:success; ret > 0:failed maybe drv interface return; ret < 0:failed maybe rs return */
        }
        if (ret != -EAGAIN) {
            hccp_warn("[recv][ra_hdc_socket]ra hdc message process unsuccessful, ret(%d) phyId(%u)", ret, phyId);
        }
        realRecvSize = ret;
        goto out;
    }

    realRecvSize = (int)recvDataHead->rx_data.real_recv_size;
    if (realRecvSize <= 0) {
        goto out;
    } else {
        ret = memcpy_s(data, size, (char *)recvDataHead + sizeof(union op_socket_recv_data), realRecvSize);
        if (ret) {
            hccp_err("[recv][ra_hdc_socket]memcpy_s failed, ret(%d) phyId(%u)", ret, phyId);
            realRecvSize = -ESAFEFUNC;
            goto out;
        }
    }

out:
    free(recvDataHead);
    recvDataHead = NULL;
    return realRecvSize;
}

STATIC int RaGetRecvSockets(union op_socket_info_data *socketInfoData, struct socket_info_t conn[],
    unsigned int num)
{
    unsigned int i;
    int realNum;
    int ret;
    struct ra_socket_handle *socketHandle = NULL;

    for (i = 0; i < num; i++) {
        ((struct socket_hdc_info *)conn[i].fd_handle)->phy_id = socketInfoData->rx_data.conn[i].phy_id;
        ((struct socket_hdc_info *)conn[i].fd_handle)->fd = socketInfoData->rx_data.conn[i].fd;
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        ((struct socket_hdc_info *)conn[i].fd_handle)->socket_handle = socketHandle;
        ret = memcpy_s(&(socketHandle->rdev_info.local_ip), sizeof(union hccp_ip_addr),
            &(socketInfoData->rx_data.conn[i].local_ip), sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_recv_sockets]memcpy_s local_ip failed, ret(%d)", ret), -ESAFEFUNC);
        ret = memcpy_s(&(conn[i].remote_ip), sizeof(union hccp_ip_addr), &(socketInfoData->rx_data.conn[i].remote_ip),
            sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_recv_sockets]memcpy_s remote_ip failed, ret(%d)", ret), -ESAFEFUNC);
        conn[i].status = socketInfoData->rx_data.conn[i].status;
    }

    realNum = socketInfoData->rx_data.num;
    return realNum;
}

int RaHdcGetSockets(unsigned int phyId, unsigned int role, struct socket_info_t conn[], unsigned int num)
{
    int ret;
    int sockFd[MAX_SOCKET_NUM] = {0};
    union op_socket_info_data *socketInfoData;

    socketInfoData = (union op_socket_info_data *)calloc(sizeof(union op_socket_info_data), sizeof(char));
    CHK_PRT_RETURN(socketInfoData == NULL, hccp_err("[get][ra_hdc_sockets]socket info data"
        "calloc failed phy_id(%u)", phyId), -ENOMEM);
    socketInfoData->tx_data.num = num;
    socketInfoData->tx_data.role = role;

    ret = RaAssembleSockets(socketInfoData, conn, num, sockFd, sizeof(sockFd) / sizeof(sockFd[0]));
    if (ret) {
        hccp_err("[get][ra_hdc_sockets]assemble sockets error ret(%d) phy_id(%u)", ret, phyId);
        goto out;
    }

    ret = RaHdcProcessMsg(RA_RS_GET_SOCKET, phyId, (char *)socketInfoData,
        sizeof(union op_socket_info_data));
    if (ret) {
        hccp_err("[get][ra_hdc_sockets]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
        ret = -EINVAL; /* !=0 is error situation return negative value, function normal return >=0 */
        goto err;
    }

    ret = RaGetRecvSockets(socketInfoData, conn, num);
    // no sockets get, free socket info(fd_handle)
    if (ret == 0) {
        goto err;
    }
    free(socketInfoData);
    socketInfoData = NULL;
    return ret;

err:
    FreeAssembleSockets(conn, num);
out:
    free(socketInfoData);
    socketInfoData = NULL;
    return ret;
}

STATIC int RaHdcGetAllVnic(unsigned int curPhyId, unsigned int *vnicIp, unsigned int num)
{
    int ret;
    unsigned int logicId, phyId;
    unsigned int devNum;
    union op_get_vnic_ip_data vnicIpData;

    ret = DlDrvGetDevNum(&devNum);
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_all_vnic]get dev num failed, ret(%d)", ret), ret);

    for (logicId = 0; logicId < devNum; logicId++) {
        ret = DlDrvDeviceGetPhyIdByIndex(logicId, &phyId);
        CHK_PRT_RETURN(ret != 0 || phyId >= RA_MAX_PHY_ID_NUM || phyId >= num, hccp_err("[get][ra_hdc_all_vnic]get phy"
            "id failed, logicId(%u) ret(%d) phyId(%u) >= %d or >= %u invalid", logicId, ret, phyId,
            RA_MAX_PHY_ID_NUM, num), -ENODEV);

        vnicIpData.tx_data.phy_id = phyId;
        ret = RaHdcProcessMsg(RA_RS_GET_VNIC_IP, curPhyId,
            (char *)&vnicIpData, sizeof(union op_get_vnic_ip_data));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_all_vnic]ra hdc message process failed ret(%d), phyId(%d),"
            "logicId(%d)", ret, phyId, logicId), ret);

        vnicIp[phyId] = vnicIpData.rx_data.vnic_ip;
        hccp_info("vnic ipaddr:0x%x, get vnicIp:0x%x, phyId:%u, logicId:%u", vnicIpData.rx_data.vnic_ip,
                  vnicIp[phyId], phyId, logicId);
    }

    return 0;
}

int RaHdcGetVnicIpInfosV1(unsigned int phyId, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    union op_get_vnic_ip_infos_data_v1 vnicIpData = {0};
    unsigned int completeCnt = 0;
    unsigned int sendNum = 0;
    int ret;

    while (completeCnt < num) {
        vnicIpData.tx_data.phy_id = phyId;
        sendNum = ((num - completeCnt) >= MAX_IP_INFO_NUM_V1) ? MAX_IP_INFO_NUM_V1 : (num - completeCnt);
        ret = memcpy_s(vnicIpData.tx_data.ids, sizeof(unsigned int) * MAX_IP_INFO_NUM_V1,
            &ids[completeCnt], sizeof(unsigned int) * sendNum);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        vnicIpData.tx_data.num = sendNum;
        vnicIpData.tx_data.type = type;

        ret = RaHdcProcessMsg(RA_RS_GET_VNIC_IP_INFOS_V1, phyId, (char *)&vnicIpData,
            sizeof(union op_get_vnic_ip_infos_data_v1));
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]ra hdc message process failed ret(%d), phyId(%u)",
            ret, phyId), ret);

        ret = memcpy_s(&infos[completeCnt], sizeof(struct ip_info) * (num - completeCnt),
            vnicIpData.rx_data.infos, sizeof(struct ip_info) * sendNum);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        completeCnt += sendNum;
    }

    return 0;
}

int RaHdcGetVnicIpInfos(unsigned int phyId, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    union op_get_vnic_ip_infos_data vnicIpData = {0};
    unsigned int interfaceVersion = 0;
    unsigned int completeCnt = 0;
    unsigned int sendNum = 0;
    int ret;

    // origin procedure for compatibility issue
    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_GET_VNIC_IP_INFOS, &interfaceVersion);
    if (ret != 0 || interfaceVersion < RA_RS_GET_VNIC_IP_INFOS_VERSION) {
        return RaHdcGetVnicIpInfosV1(phyId, type, ids, num, infos);
    }

    while (completeCnt < num) {
        vnicIpData.tx_data.phy_id = phyId;
        sendNum = ((num - completeCnt) >= MAX_IP_INFO_NUM) ? MAX_IP_INFO_NUM : (num - completeCnt);
        ret = memcpy_s(vnicIpData.tx_data.ids, sizeof(unsigned int) * MAX_IP_INFO_NUM,
            &ids[completeCnt], sizeof(unsigned int) * sendNum);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        vnicIpData.tx_data.num = sendNum;
        vnicIpData.tx_data.type = type;

        ret = RaHdcProcessMsg(RA_RS_GET_VNIC_IP_INFOS, phyId, (char *)&vnicIpData,
            sizeof(union op_get_vnic_ip_infos_data));
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]ra hdc message process failed ret(%d), phyId(%u)",
            ret, phyId), ret);

        ret = memcpy_s(&infos[completeCnt], sizeof(struct ip_info) * (num - completeCnt),
            vnicIpData.rx_data.infos, sizeof(struct ip_info) * sendNum);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ip_infos]memcpy_s for ids failed ret(%d)", ret), -ESAFEFUNC);
        completeCnt += sendNum;
    }

    return 0;
}

int RaHdcSocketInit(struct rdev rdevInfo)
{
    unsigned int vnicIp[RA_MAX_VNIC_NUM] = {0};
    union op_socket_init_data socketInitData;
    unsigned int interfaceVersion = 0;
    int ret;

    ret = memset_s(&socketInitData, sizeof(socketInitData), 0, sizeof(socketInitData));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]memset_s failed ret(%d) phy_id(%u)", ret,
        rdevInfo.phy_id), -ESAFEFUNC);

    // check opcode version, init g_vnics with invalid ip mask 0xFFFFFFFF
    ret = RaHdcGetInterfaceVersion(rdevInfo.phy_id, RA_RS_GET_VNIC_IP_INFOS_V1, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_GET_VNIC_IP_INFOS_VERSION) {
        ret = memset_s(vnicIp, sizeof(vnicIp), 0xFFFFFFFF, sizeof(vnicIp));
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]memset_s failed ret(%d) phy_id(%u)", ret,
            rdevInfo.phy_id), -ESAFEFUNC);
    } else {
        // origin procedure: init g_vnics with vnic_ip get by phy_id
        ret = RaHdcGetAllVnic(rdevInfo.phy_id, vnicIp, RA_MAX_VNIC_NUM);
        CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]ra_hdc_get_all_vnic failed ret(%d) phy_id(%u)", ret,
            rdevInfo.phy_id), ret);
    }

    ret = memcpy_s(&(socketInitData.tx_data.vnic_ip), sizeof(socketInitData.tx_data.vnic_ip),
        &vnicIp, sizeof(vnicIp));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]memcpy_s for vnic_ip failed ret(%d) phy_id(%u)", ret,
        rdevInfo.phy_id), -ESAFEFUNC);

    socketInitData.tx_data.num = RA_MAX_VNIC_NUM;
    ret = RaHdcProcessMsg(RA_RS_SOCKET_INIT, rdevInfo.phy_id, (char *)&socketInitData,
        sizeof(union op_socket_init_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_socket]ra hdc message process failed ret(%d) phy_id(%u)", ret,
        rdevInfo.phy_id), ret);

    return 0;
}

int RaHdcSocketDeinit(struct rdev rdevInfo)
{
    int ret;
    union op_socket_deinit_data socketDeinitData;

    ret = memset_s(&socketDeinitData, sizeof(socketDeinitData), 0, sizeof(socketDeinitData));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_socket]memset_s failed ret(%d) phy_id(%u)", ret,
        rdevInfo.phy_id), -ESAFEFUNC);

    ret = memcpy_s(&(socketDeinitData.tx_data.rdev_info), sizeof(struct rdev), &rdevInfo, sizeof(struct rdev));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_socket]memcpy_s failed ret(%d) phy_id(%u)", ret,
        rdevInfo.phy_id), -ESAFEFUNC);

    ret = RaHdcProcessMsg(RA_RS_SOCKET_DEINIT, rdevInfo.phy_id, (char *)&socketDeinitData,
        sizeof(union op_socket_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_socket]ra hdc message process failed ret(%d) phy_id(%u)", ret,
        rdevInfo.phy_id), ret);

    return 0;
}

int RaHdcGetIfnum(unsigned int phyId, bool isAll, unsigned int *num)
{
    union op_ifnum_data ifnumData = {0};
    int ret;

    ifnumData.tx_data.num = isAll ? RA_RS_GET_ALL_IP_BIT_MASK : 0;
    ifnumData.tx_data.phy_id = phyId;
    ret = RaHdcProcessMsg(RA_RS_GET_IFNUM, phyId, (char *)&ifnumData, sizeof(union op_ifnum_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifnum]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    *num = ifnumData.rx_data.num;

    return 0;
}

int RaHdcGetIfaddrs(unsigned int phyId, struct ifaddr_info ifaddrInfos[], unsigned int *num)
{
    union op_ifaddr_data ifaddrData = {0};
    int ret;

    ifaddrData.tx_data.num = *num;
    ret = memcpy_s(ifaddrData.tx_data.ifaddr_infos, sizeof(struct ifaddr_info) * MAX_INTERFACE_NUM, ifaddrInfos,
        sizeof(struct ifaddr_info) * (*num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs]memcpy_s failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);

    ifaddrData.tx_data.phy_id = phyId;
    ret = RaHdcProcessMsg(RA_RS_GET_IFADDRS, phyId, (char *)&ifaddrData, sizeof(union op_ifaddr_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    ret = memcpy_s(ifaddrInfos, sizeof(struct ifaddr_info) * (*num), ifaddrData.rx_data.ifaddr_infos,
        sizeof(struct ifaddr_info) * (ifaddrData.rx_data.num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs]memcpy_s failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);

    *num = ifaddrData.rx_data.num;
    return 0;
}

int RaHdcGetIfaddrsV2(unsigned int phyId, bool isAll, struct interface_info interfaceInfos[], unsigned int *num)
{
    union op_ifaddr_data_v2 ifaddrData = {0};
    int ret;

    ifaddrData.tx_data.num = isAll ? (*num | RA_RS_GET_ALL_IP_BIT_MASK) : *num;
    ret = memcpy_s(ifaddrData.tx_data.interface_infos, sizeof(struct interface_info) * MAX_INTERFACE_NUM,
        interfaceInfos, sizeof(struct interface_info) * (*num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs_v2]memcpy_s tx interface infos failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);

    ifaddrData.tx_data.phy_id = phyId;
    ret = RaHdcProcessMsg(RA_RS_GET_IFADDRS_V2, phyId, (char *)&ifaddrData, sizeof(union op_ifaddr_data_v2));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs_v2]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    ret = memcpy_s(interfaceInfos, sizeof(struct interface_info) * (*num), ifaddrData.rx_data.interface_infos,
        sizeof(struct interface_info) * (ifaddrData.rx_data.num));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_ifaddrs_v2]memcpy_s rx interface infos failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);

    *num = ifaddrData.rx_data.num;
    return 0;
}

static int RaHdcSocketWhiteListOpV1(unsigned int opcode, struct rdev rdevInfo,
    struct socket_wlist_info_t whiteList[], unsigned int num)
{
    union op_wlist_data *wlistData = NULL;
    int ret;
    wlistData = (union op_wlist_data *)calloc(sizeof(union op_wlist_data), sizeof(char));
    CHK_PRT_RETURN(wlistData == NULL, hccp_err("[op][ra_hdc_socket_white_list]calloc wlist data failed! phy_id(%u)",
        rdevInfo.phy_id), -ENOMEM);
    wlistData->tx_data.num = num;
    ret = memcpy_s(&(wlistData->tx_data.rdev_info), sizeof(struct rdev), &(rdevInfo), sizeof(struct rdev));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for rdev_info failed, ret(%d) phyId(%u)",
                 ret, rdevInfo.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = memcpy_s(wlistData->tx_data.wlist, sizeof(struct socket_wlist_info_t) * MAX_WLIST_NUM_V1, whiteList,
        sizeof(struct socket_wlist_info_t) * num);
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for wlist failed, ret(%d) phyId(%u)", ret, rdevInfo.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = RaHdcProcessMsg(opcode, rdevInfo.phy_id, (char *)wlistData, sizeof(union op_wlist_data));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]ra hdc process msg failed ret(%d) phy_id(%u)", ret, rdevInfo.phy_id);
        goto out;
    }

out:
    free(wlistData);
    wlistData = NULL;
    return ret;
}

static int RaHdcSocketWhiteListOpV2(unsigned int opcode, struct rdev rdevInfo,
    struct socket_wlist_info_t whiteList[], unsigned int num)
{
    int ret;
    union op_wlist_data_v2 *wlistData = NULL;

    wlistData = (union op_wlist_data_v2 *)calloc(sizeof(union op_wlist_data_v2), sizeof(char));
    CHK_PRT_RETURN(wlistData == NULL, hccp_err("[op][ra_hdc_socket_white_list]calloc wlist data failed! phy_id(%u)",
        rdevInfo.phy_id), -ENOMEM);
    wlistData->tx_data.num = num;
    ret = memcpy_s(&(wlistData->tx_data.rdev_info), sizeof(struct rdev), &(rdevInfo), sizeof(struct rdev));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for rdev_info failed, ret(%d) phyId(%u)",
                 ret, rdevInfo.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = memcpy_s(wlistData->tx_data.wlist, sizeof(struct socket_wlist_info_t) * MAX_WLIST_NUM, whiteList,
        sizeof(struct socket_wlist_info_t) * num);
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]memcpy_s for wlist failed, ret(%d) phyId(%u)", ret, rdevInfo.phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = RaHdcProcessMsg(opcode, rdevInfo.phy_id, (char *)wlistData, sizeof(union op_wlist_data_v2));
    if (ret) {
        hccp_err("[op][ra_hdc_socket_white_list]ra hdc process msg failed ret(%d) phy_id(%u)", ret, rdevInfo.phy_id);
        goto out;
    }

out:
    free(wlistData);
    wlistData = NULL;
    return ret;
}

int RaHdcSocketWhiteListAdd(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[], unsigned int num)
{
    int ret;
    unsigned int interfaceVersion = 0;

    ret = RaHdcGetInterfaceVersion(rdevInfo.phy_id, RA_RS_WLIST_ADD_V2, &interfaceVersion);
    if (ret != 0 || interfaceVersion != RA_RS_WLIST_ADD_V2_VERSION) {
        return RaHdcSocketWhiteListOpV1(RA_RS_WLIST_ADD, rdevInfo, whiteList, num);
    }

    return RaHdcSocketWhiteListOpV2(RA_RS_WLIST_ADD_V2, rdevInfo, whiteList, num);
}

int RaHdcSocketWhiteListDel(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[], unsigned int num)
{
    int ret;
    unsigned int interfaceVersion = 0;

    ret = RaHdcGetInterfaceVersion(rdevInfo.phy_id, RA_RS_WLIST_DEL_V2, &interfaceVersion);
    if (ret != 0 || interfaceVersion != RA_RS_WLIST_DEL_V2_VERSION) {
        return RaHdcSocketWhiteListOpV1(RA_RS_WLIST_DEL, rdevInfo, whiteList, num);
    }

    return RaHdcSocketWhiteListOpV2(RA_RS_WLIST_DEL_V2, rdevInfo, whiteList, num);
}

int RaHdcSocketAcceptCreditAdd(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num,
    unsigned int creditLimit)
{
    union op_accept_credit_data opData = {0};
    int ret;

    opData.tx_data.phy_id = phyId;
    opData.tx_data.credit_limit = creditLimit;
    opData.tx_data.num = num;
    ret = RaGetSocketListenInfo(conn, num, opData.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[set][ra_hdc_socket]ra_get_socket_listen_info failed, ret(%d) phyId(%u)",
        ret, phyId), -EINVAL);

    ret = RaHdcProcessMsg(RA_RS_ACCEPT_CREDIT_ADD, phyId, (char *)&opData, sizeof(union op_accept_credit_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[set][ra_hdc_socket]ra hdc message process failed, ret(%d) phyId(%u)",
        ret, phyId), ret);

    return ret;
}
