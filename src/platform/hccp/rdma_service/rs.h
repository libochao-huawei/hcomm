/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_H
#define RS_H

#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <infiniband/verbs.h>
#include <infiniband/driver.h>
#include "hccp_common.h"
#include "user_log.h"
#include "ra_rs_comm.h"

#define PROCESS_RS_SIGN_LENGTH 49
#define PROCESS_RS_RESV_LENGTH 4
#define EXP_DEVNUM             2

struct rs_mr_reg_info {
    unsigned int phy_id;
    char *addr;
    unsigned long long len;
    int access;
};

struct process_rs_sign {
    int tgid;
    char sign[PROCESS_RS_SIGN_LENGTH];
    char resv[PROCESS_RS_RESV_LENGTH];
};

struct rs_qp_norm {
    int flag;
    int qp_mode;
    int is_exp;
    int is_ext;
    int mem_align; // 0,1:4KB, 2:2MB
};

struct rs_qp_status_info {
    int status;
    unsigned int udp_sport;
};

struct rs_wrlist_base_info {
    unsigned int phy_id;
    unsigned int rdev_index;
    unsigned int qpn;
    unsigned int key_flag;
};

struct rs_linux_version_info {
    int major;
    int minor;
    int patch;
};

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ++++++++++++++++++++++++++++++RDMA API for RA++++++++++++++++++++++++++++++++++ */
/*
 * rs_open
 * flag: bit0: 0 = RC, 1= UD
 */
#define MS_PER_SECOND_F   1000.0
#define US_PER_MS_F   1000.0
#define MS_PER_SECOND_I   1000
#define RS_EXPECT_TIME_MAX 200.0 // ms

#define RS_GID_SEQ_NUM             4
#define RS_GID_SEQ_ZERO            0
#define RS_GID_SEQ_ONE             1
#define RS_GID_SEQ_TWO             2
#define RS_GID_SEQ_THREE           3

#define RS_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

RS_ATTRI_VISI_DEF int RsSetTsqpDepth(unsigned int phyId, unsigned int rdevIndex, unsigned int tempDepth,
    unsigned int *qpNum);
RS_ATTRI_VISI_DEF int RsGetTsqpDepth(unsigned int phyId, unsigned int rdevIndex, unsigned int *tempDepth,
    unsigned int *qpNum);
RS_ATTRI_VISI_DEF int RsQpCreate(unsigned int phyId, unsigned int rdevIndex, struct rs_qp_norm qpNorm,
    struct rs_qp_resp *qpResp);
RS_ATTRI_VISI_DEF int RsQpCreateWithAttrs(unsigned int phyId, unsigned int rdevIndex,
    struct rs_qp_norm_with_attrs *qpNorm, struct rs_qp_resp_with_attrs *qpResp);
RS_ATTRI_VISI_DEF int RsQpDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn);
RS_ATTRI_VISI_DEF int RsTypicalQpModify(unsigned int phyId, unsigned int rdevIndex,
    struct typical_qp localQpInfo, struct typical_qp remoteQpInfo, unsigned int *udpSport);
RS_ATTRI_VISI_DEF int RsQpBatchModify(unsigned int phyId, unsigned int rdevIndex,
    int status, int qpn[], int qpnNum);
RS_ATTRI_VISI_DEF int RsQpConnectAsync(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, int fd);

enum rs_qp_status {
    RS_QP_STATUS_DISCONNECT = 0,
    RS_QP_STATUS_CONNECTED = 1,
    RS_QP_STATUS_TIMEOUT = 2,
    RS_QP_STATUS_CONNECTING = 3,
    RS_QP_STATUS_REM_FD_CLOSE = 4,
    RS_QP_STATUS_PAUSE = 5,
};

#define RS_IS_EXP       0
#define RS_NOT_EXP      1

enum rs_access_flags {
    RS_ACCESS_LOCAL_WRITE  = 1,
    RS_ACCESS_REMOTE_WRITE = (1 << 1),
    RS_ACCESS_REMOTE_READ  = (1 << 2UL),
    RS_ACCESS_REDUCE       = (1 << 8),
};

struct rs_init_config {
    unsigned int chip_id;
    unsigned int hccp_mode;
    unsigned int white_list_status;
};

struct rs_qp_conn_para {
    unsigned int phy_id;
    unsigned int rdev_index;
    uint32_t qpn;
};

struct rs_backup_info {
    bool backup_flag;
    struct rdev rdev_info;
};

#define RS_HEARTBEAT_TIME (1000.0 * 60) // ms
#define PTHREAD_NAME_LEN 32

struct rs_pthread_info {
    char pthread_name[PTHREAD_NAME_LEN];
    struct timeval last_check_time;
};

RS_ATTRI_VISI_DEF int RsMrReg(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct rdma_mr_reg_info *mrRegInfo);
RS_ATTRI_VISI_DEF int RsMrDereg(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, char *addr);

RS_ATTRI_VISI_DEF int RsRegisterMr(unsigned int phyId, unsigned int rdevIndex,
    struct rdma_mr_reg_info *mrRegInfo, void **mrHandle);
RS_ATTRI_VISI_DEF int RsTypicalRegisterMrV1(unsigned int phyId, unsigned int rdevIndex,
    struct rdma_mr_reg_info *mrRegInfo, void **mrHandle);
RS_ATTRI_VISI_DEF int RsTypicalRegisterMr(unsigned int phyId, unsigned int rdevIndex,
    struct rdma_mr_reg_info *mrRegInfo, void **mrHandle);
RS_ATTRI_VISI_DEF int RsRemapMr(unsigned int phyId, unsigned int rdevIndex, struct mem_remap_info memList[],
    unsigned int memNum);
RS_ATTRI_VISI_DEF int RsTypicalDeregisterMr(unsigned int phyId, unsigned int devIndex, unsigned long long addr);
RS_ATTRI_VISI_DEF int RsDeregisterMr(void *mrHandle);

enum rs_send_flags {
    RS_SEND_FENCE  = 1 << 0,
    RS_SEND_SIGNALED = 1 << 1,
    RS_SEND_SOLICITED = 1 << 2,
    RS_SEND_INLINE  = 1 << 3,
};
RS_ATTRI_VISI_DEF int RsSendWr(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct send_wr *wr,
    struct send_wr_rsp *wrRsp);
RS_ATTRI_VISI_DEF int RsSendWrlist(struct rs_wrlist_base_info baseInfo, struct wr_info *wrList,
    unsigned int sendNum, struct send_wr_rsp *wrRsp, unsigned int *completeNum);

RS_ATTRI_VISI_DEF int RsRecvWrlist(struct rs_wrlist_base_info baseInfo, struct recv_wrlist_data *wr,
    unsigned int recvNum, unsigned int *completeNum);

RS_ATTRI_VISI_DEF int RsGetNotifyMrInfo(unsigned int phyId, unsigned int rdevIndex, struct mr_info *info);
RS_ATTRI_VISI_DEF int RsSetHostPid(uint32_t phyId, pid_t hostPid, const char *pidSign);

RS_ATTRI_VISI_DEF int RsInit(struct rs_init_config *cfg);
RS_ATTRI_VISI_DEF int RsGetTlsEnable(unsigned int phyId, bool *tlsEnable);
RS_ATTRI_VISI_DEF int RsGetHccnCfg(unsigned int phyId, enum hccn_cfg_key key, char *value,
    unsigned int *valueLen);
RS_ATTRI_VISI_DEF int RsBindHostpid(unsigned int chipId, pid_t pid);
RS_ATTRI_VISI_DEF int RsDeinit(struct rs_init_config *cfg);

RS_ATTRI_VISI_DEF int RsSocketInit(const unsigned int *vnicIp, unsigned int num);
RS_ATTRI_VISI_DEF int RsSocketDeinit(struct rdev rdevInfo);

RS_ATTRI_VISI_DEF int RsRdevInit(struct rdev rdevInfo, unsigned int notifyType, unsigned int *rdevIndex);
RS_ATTRI_VISI_DEF int RsRdevInitWithBackup(struct rdev rdevInfo, struct rdev backupRdevInfo,
    unsigned int notifyType, unsigned int *rdevIndex);
RS_ATTRI_VISI_DEF int RsRdevGetPortStatus(unsigned int phyId, unsigned int rdevIndex, enum port_status *status);
RS_ATTRI_VISI_DEF int RsRdevDeinit(unsigned int phyId, unsigned int notifyType, unsigned int rdevIndex);

/* ++++++++++++++++++++++++++++++Epoll API start++++++++++++++++++++++++++++++++++ */
RS_ATTRI_VISI_DEF int RsEpollCtlAdd(const void *fdHandle, enum RaEpollEvent event);
RS_ATTRI_VISI_DEF int RsEpollCtlMod(const void *fdHandle, enum RaEpollEvent event);
RS_ATTRI_VISI_DEF int RsEpollCtlDel(int fd);
RS_ATTRI_VISI_DEF void RsSetTcpRecvCallback(const void *callback);
RS_ATTRI_VISI_DEF int RsCreateEventHandle(int *eventHandle);
RS_ATTRI_VISI_DEF int RsCtlEventHandle(int eventHandle, const void *fdHandle, int opcode,
    enum RaEpollEvent event);
RS_ATTRI_VISI_DEF int RsWaitEventHandle(int eventHandle, struct socket_event_info *eventInfos,
    int timeout, unsigned int maxevents, unsigned int *eventsNum);
RS_ATTRI_VISI_DEF int RsDestroyEventHandle(int *eventHandle);
/* ++++++++++++++++++++++++++++++Epoll API end++++++++++++++++++++++++++++++++++ */

/* ++++++++++++++++++++++++++++++Socket API++++++++++++++++++++++++++++++++++ */
#define RS_SOCK_PORT_DEF 16666
RS_ATTRI_VISI_DEF int RsSocketListenStart(struct socket_listen_info conn[], uint32_t num);
RS_ATTRI_VISI_DEF int RsSocketListenStop(struct socket_listen_info conn[], uint32_t num);

RS_ATTRI_VISI_DEF int RsSocketBatchConnect(struct socket_connect_info conn[], uint32_t num);
RS_ATTRI_VISI_DEF int RsSocketBatchAbort(struct socket_connect_info conn[], uint32_t num);

RS_ATTRI_VISI_DEF int RsSocketGetClientSocketErrInfo(struct socket_connect_info conn[],
    struct socket_err_info err[], unsigned int num);
RS_ATTRI_VISI_DEF int RsSocketGetServerSocketErrInfo(struct socket_listen_info conn[],
    struct server_socket_err_info err[], unsigned int num);

RS_ATTRI_VISI_DEF void RsGetCurTime(struct timeval *time);
RS_ATTRI_VISI_DEF void HccpTimeInterval(struct timeval *endTime, struct timeval *startTime, float *msec);
RS_ATTRI_VISI_DEF int RsSocketWhiteListSwitch(unsigned int phyId, unsigned int enable);
RS_ATTRI_VISI_DEF int RsSocketWhiteListAdd(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[],
    unsigned int num);
RS_ATTRI_VISI_DEF int RsSocketWhiteListDel(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[],
    unsigned int num);
RS_ATTRI_VISI_DEF int RsSocketAcceptCreditAdd(struct socket_listen_info conn[], uint32_t num,
    unsigned int creditLimit);
RS_ATTRI_VISI_DEF int RsGetIfnum(unsigned int phyId, bool isAll, unsigned int *num);
RS_ATTRI_VISI_DEF int RsPeerGetIfnum(unsigned int phyId, unsigned int *num);
RS_ATTRI_VISI_DEF int RsGetIfaddrs(struct ifaddr_info ifaddrInfos[], unsigned int *num, unsigned int phyId);
RS_ATTRI_VISI_DEF int RsGetIfaddrsV2(struct interface_info interfaceInfos[], unsigned int *num,
    unsigned int phyId, bool isAll);
RS_ATTRI_VISI_DEF int RsPeerGetIfaddrs(struct interface_info interfaceInfos[], unsigned int *num,
    unsigned int phyId);
RS_ATTRI_VISI_DEF int RsGetVnicIp(unsigned int phyId, unsigned int *vnicIp);
RS_ATTRI_VISI_DEF int RsGetInterfaceVersion(unsigned int opcode, unsigned int *version);
RS_ATTRI_VISI_DEF int RsGetVnicIpInfos(unsigned int phyId, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[]);
RS_ATTRI_VISI_DEF int RsSocketSetScopeId(unsigned int devId, int scopeId);
struct rs_socket_close_info_t {
    int fd;
};
RS_ATTRI_VISI_DEF int RsSocketBatchClose(int disuseLinger, struct rs_socket_close_info_t conn[], uint32_t num);

enum rs_conn_role {
    RS_CONN_ROLE_SERVER = 0,
    RS_CONN_ROLE_CLIENT = 1,
};
enum rs_socket_status {
    RS_SOCK_STATUS_NA = 0,
    RS_SOCK_STATUS_OK = 1,
    RS_SOCK_STATUS_TIMEOUT = 2,
    RS_SOCK_STATUS_ING = 3,
};

RS_ATTRI_VISI_DEF int RsGetSockets(uint32_t role, struct socket_fd_data conn[], uint32_t num);
RS_ATTRI_VISI_DEF int RsGetSslEnable(uint32_t *sslEnable);
RS_ATTRI_VISI_DEF int RsSocketSend(int fd, const void *data, uint64_t size);
RS_ATTRI_VISI_DEF int RsPeerSocketSend(uint32_t sslEnable, int fd, const void *data, uint64_t size);
RS_ATTRI_VISI_DEF int RsSocketRecv(int fd, void *data, uint64_t size);
RS_ATTRI_VISI_DEF int RsPeerSocketRecv(uint32_t sslEnable, int fd, void *data, uint64_t size);
RS_ATTRI_VISI_DEF int RsNotifyCfgSet(unsigned int phyId, unsigned long long va, unsigned long long size);
RS_ATTRI_VISI_DEF int RsNotifyCfgGet(unsigned int phyId, unsigned long long *va, unsigned long long *size);
RS_ATTRI_VISI_DEF void RsHeartbeatAlivePrint(struct rs_pthread_info *pthreadInfo);

RS_ATTRI_VISI_DEF int RsGetQpContext(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, void** qp,
                                        void** sendCq, void** recvCq);
RS_ATTRI_VISI_DEF int RsGetQpStatus(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct rs_qp_status_info *qpInfo);

int rsGetLocalDevIDByHostDevID(unsigned int phyId, unsigned int *chipId);
int rsGetDevIDByLocalDevID(unsigned int chipId, unsigned int *phyId);

RS_ATTRI_VISI_DEF int RsCqCreate(unsigned int phyId, unsigned int rdevIndex, struct cq_attr *attr);
RS_ATTRI_VISI_DEF int RsCqDestroy(unsigned int phyId, unsigned int rdevIndex, struct cq_attr *attr);
RS_ATTRI_VISI_DEF int RsNormalQpCreate(unsigned int phyId, unsigned int rdevIndex,
    struct ibv_qp_init_attr *qpInitAttr, struct rs_qp_resp *qpResp, void **qp);
RS_ATTRI_VISI_DEF int RsNormalQpDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn);
RS_ATTRI_VISI_DEF int RsSetQpAttrQos(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct qos_attr *attr);
RS_ATTRI_VISI_DEF int RsSetQpAttrTimeout(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    unsigned int *timeout);
RS_ATTRI_VISI_DEF int RsSetQpAttrRetryCnt(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    unsigned int *retryCnt);
RS_ATTRI_VISI_DEF int RsCreateCompChannel(unsigned int phyId, unsigned int rdevIndex, void** compChannel);
RS_ATTRI_VISI_DEF int RsDestroyCompChannel(void* compChannel);
RS_ATTRI_VISI_DEF int RsGetCqeErrInfo(struct cqe_err_info *info);
RS_ATTRI_VISI_DEF int RsCreateSrq(unsigned int phyId, unsigned int rdevIndex, struct srq_attr *attr);
RS_ATTRI_VISI_DEF int RsDestroySrq(unsigned int phyId, unsigned int rdevIndex, struct srq_attr *attr);
RS_ATTRI_VISI_DEF int RsGetLiteSupport(unsigned int phyId, unsigned int rdevIndex, int *supportLite);
RS_ATTRI_VISI_DEF int RsGetLiteRdevCap(
    unsigned int phyId, unsigned int rdevIndex, struct lite_rdev_cap_resp *resp);
RS_ATTRI_VISI_DEF int RsGetLiteQpCqAttr(
    unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_qp_cq_attr_resp *resp);
RS_ATTRI_VISI_DEF int RsGetLiteConnectedInfo(
    unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_connected_info_resp *resp);
RS_ATTRI_VISI_DEF int RsGetLiteMemAttr(
    unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_mem_attr_resp *resp);
RS_ATTRI_VISI_DEF void RsSetCtx(unsigned int phyId);
RS_ATTRI_VISI_DEF int RsGetCqeErrInfoNum(unsigned int phyId, unsigned int rdevIdx, unsigned int *num);
RS_ATTRI_VISI_DEF int RsGetCqeErrInfoList(unsigned int phyId, unsigned int rdevIdx, struct cqe_err_info *info,
    unsigned int *num);
RS_ATTRI_VISI_DEF int RsDrvGetRandomNum(int *randNum);
RS_ATTRI_VISI_DEF int RsGetSecRandom(unsigned int *value);
#ifdef __cplusplus
}
#endif
#endif // RS_H
