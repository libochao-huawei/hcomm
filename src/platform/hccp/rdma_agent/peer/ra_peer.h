/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_PEER_H
#define RA_PEER_H

#include <pthread.h>
#include <infiniband/verbs.h>
#include "hccp_common.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

#define HCCP_CLOSE_RETRY_FOR_EINTR(fd) do { \
    int ret_; \
    do { \
        ret_ = close(fd); \
        if (ret_ < 0) { \
            hccp_warn("close filedscp[%d] unsuccessful, errno:%d", fd, errno); \
        } \
    } while ((ret_ < 0) && (errno == EINTR)); \
    fd = -1; \
} while (0)

#define PEER_PTHREAD_MUTEX_LOCK(mutex) do { \
    int ret_lock = pthread_mutex_lock(mutex); \
    if (ret_lock) { \
        hccp_warn("pthread_mutex_lock unsuccessful, ret[%d]", ret_lock); \
    }\
} while (0)

#define PEER_PTHREAD_MUTEX_UNLOCK(mutex) do { \
    int ret_ulock = pthread_mutex_unlock(mutex); \
    if (ret_ulock) { \
        hccp_warn("pthread_mutex_unlock unsuccessful, ret[%d]", ret_ulock); \
    } \
} while (0)

#define RA_SSL_DISABLE 0

struct host_roce_notify_info {
    unsigned int logic_id;
    unsigned long long va;
    unsigned long long sz;
};

#define HOST_DEVICE_NAME     "/dev/host_rdma"
#define RA_NOTIFY_TYPE_TOTAL_SIZE   1

#define HOST_CDEV_IOC_MAGIC  '%'

#define HOST_CDEV_IOC_FREE_NOTIFY _IOWR(HOST_CDEV_IOC_MAGIC, 1, struct host_roce_notify_info)

int RaPeerSocketBatchConnect(unsigned int devId, struct socket_connect_info_t conn[], unsigned int num);

int RaPeerSocketBatchClose(unsigned int devId, struct socket_close_info_t conn[], unsigned int num);

int RaPeerSocketBatchAbort(unsigned int devId, struct socket_connect_info_t conn[], unsigned int num);

int RaPeerSocketListenStart(unsigned int devId, struct socket_listen_info_t conn[], unsigned int num);

int RaPeerSocketListenStop(unsigned int devId, struct socket_listen_info_t conn[], unsigned int num);

int RaPeerGetSockets(unsigned int phyId, unsigned int role, struct socket_info_t conn[], unsigned int num);

int RaPeerSocketSend(unsigned int devId, const void *handle, const void *data, unsigned long long size);

int RaPeerSocketRecv(unsigned int devId, const void *handle, void *data, unsigned long long size);

int RaPeerEpollCtlAdd(const void *fdHandle, enum RaEpollEvent event);

int RaPeerEpollCtlMod(const void *fdHandle, enum RaEpollEvent event);

int RaPeerEpollCtlDel(const void *fdHandle);

void RaPeerSetTcpRecvCallback(unsigned int phyId, const void *callback);

int RaPeerSocketWhiteListAdd(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[], unsigned int num);

int RaPeerSocketWhiteListDel(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[], unsigned int num);

int RaPeerSocketDeinit(struct rdev rdevInfo);

int RaPeerQpCreate(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, void **qpHandle);

int RaPeerQpCreateWithAttrs(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs, void **qpHandle);

int RaPeerLoopbackQpCreate(struct ra_rdma_handle *rdmaHandle, struct loopback_qp_pair *qpPair, void **qpHandle);

int RaPeerQpDestroy(struct ra_qp_handle *qpPeer);

int RaPeerTypicalQpModify(struct ra_qp_handle *qpPeer, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo);

int RaPeerQpConnectAsync(struct ra_qp_handle *qpPeer, const void *sockHandle);

int RaPeerGetQpStatus(struct ra_qp_handle *qpPeer, int *status);

int RaPeerMrReg(struct ra_qp_handle *qpPeer, struct mr_info *info);

int RaPeerMrDereg(struct ra_qp_handle *qpPeer, struct mr_info *info);

int RaPeerRegisterMr(struct ra_rdma_handle *rdmaPeer, struct mr_info *info, void **mrHandle);

int RaPeerDeregisterMr(struct ra_rdma_handle *rdmaPeer, void *mrHandle);

int RaPeerSendWr(struct ra_qp_handle *qpPeer, struct send_wr *wr, struct send_wr_rsp *opRsp);

int RaPeerSendWrlist(struct ra_qp_handle *qpHandle, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum);

int RaPeerRecvWrlist(struct ra_qp_handle *qpHandle, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum);

int RaPeerGetNotifyBaseAddr(struct ra_rdma_handle *handle, unsigned long long *va, unsigned long long *size);

int RaPeerInit(struct ra_init_config *cfg, unsigned int whiteListStatus);

int RaPeerGetTlsEnable(unsigned int phyId, bool *tlsEnable);

int RaPeerGetSecRandom(unsigned int *value);

int RaPeerDeinit(struct ra_init_config *cfg);

int RaPeerGetIfnum(unsigned int phyId, unsigned int *num);

int RaPeerGetIfaddrs(unsigned int phyId, struct interface_info interfaceInfos[], unsigned int *num);

int RaPeerRdevInit(
    struct ra_rdma_handle *rdmaHandle, unsigned int notifyType, struct rdev rdevInfo, unsigned int *rdevIndex);

int RaPeerRdevDeinit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType);

int HostNotifyBaseAddrInit(unsigned int phyId);

int RaPeerNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId);

int HostNotifyBaseAddrUninit(unsigned int phyId);

int NotifyBaseAddrUninit(unsigned int notifyType, unsigned int phyId);

int RaPeerSetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int tempDepth, unsigned int *qpNum);

int RaPeerGetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int *tempDepth, unsigned int *qpNum);

int RaPeerGetQpContext(struct ra_qp_handle *qpPeer, void** qp, void** sendCq, void** recvCq);

int RaPeerNormalQpCreate(struct ra_rdma_handle *rdmaHandle, struct ibv_qp_init_attr *qpInitAttr,
    void **qpHandle, void** qp);

int RaPeerNormalQpDestroy(struct ra_qp_handle *qpPeer);

int RaPeerCqCreate(struct ra_rdma_handle *rdmaHandle, struct cq_attr *attr);

int RaPeerCqDestroy(struct ra_rdma_handle *rdmaHandle, struct cq_attr *attr);

int RaPeerSetQpAttrQos(struct ra_qp_handle *qpPeer, struct qos_attr *attr);

int RaPeerSetQpAttrTimeout(struct ra_qp_handle *qpPeer, unsigned int *timeout);

int RaPeerSetQpAttrRetryCnt(struct ra_qp_handle *qpPeer, unsigned int *retryCnt);

int RaPeerCreateCompChannel(struct ra_rdma_handle *rdmaHandle, void** compChannel);

int RaPeerDestroyCompChannel(void* compChannel);

int RaPeerCreateSrq(struct ra_rdma_handle *rdmaHandle, struct srq_attr *attr);

int RaPeerDestroySrq(struct ra_rdma_handle *rdmaHandle, struct srq_attr *attr);

int RaPeerCreateEventHandle(int *eventHandle);

int RaPeerCtlEventHandle(int eventHandle, const void *fdHandle, int opcode, enum RaEpollEvent event);

int RaPeerWaitEventHandle(int eventHandle, struct socket_event_info *eventInfos, int timeout,
    unsigned int maxevents, unsigned int *eventsNum);

int RaPeerDestroyEventHandle(int *eventHandle);

void RaPeerMutexLock(unsigned int phyId);

void RaPeerMutexUnlock(unsigned int phyId);
#endif // RA_PEER_H
