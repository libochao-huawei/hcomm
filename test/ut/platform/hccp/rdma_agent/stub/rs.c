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
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include "hccp_common.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

struct RsInitConfig {
    u32 port;
    u32 device_id;
    u32 rs_position;
};

struct rs_socket_listen_info_t {
    uint32_t device_id;
    uint32_t ip_addr;
    uint32_t phase;
    uint32_t err;
};

struct rs_socket_connect_info_t {
    uint32_t device_id;
    uint32_t ip_addr;
    char tag[128];
};

struct RsSocketCloseInfoT {
    int fd;
};

struct rs_socket_info_t {
    int fd;
    u32 device_id;
    u32 server_ip_addr;
    u32 client_ip_addr;
    s32 status;
    char tag[128];
};

struct RsQpNorm {
    int flag;
    int qpMode;
    int isExp;
    int isExt;
    int mem_align;
};

struct RsWrlistBaseInfo {
    unsigned int dev_id;
    unsigned int rdevIndex;
    unsigned int qpn;
    unsigned int keyFlag;
};

#define MS_PER_SECOND_F   1000.0
#define MS_PER_SECOND_I   1000

void RsGetCurTime(struct timeval *time)
{
    int ret;

    ret = gettimeofday(time, NULL);
    if (ret) {
        memset(time, 0, sizeof(struct timeval));
    }

    return;
}

void HccpTimeInterval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    /* if low position is sufficient, then borrow one from the high position */
    if (end_time->tv_usec < start_time->tv_usec) {
        end_time->tv_sec -= 1;
        end_time->tv_usec += MS_PER_SECOND_I * MS_PER_SECOND_I;
    }

    *msec = (end_time->tv_sec - start_time->tv_sec) * MS_PER_SECOND_F +
            (end_time->tv_usec - start_time->tv_usec) / MS_PER_SECOND_F;

    return;
}

int RsSocketBatchConnect(struct SocketConnectInfo conn[], uint32_t num)
{
    return 0;
}

int RsSocketSetScopeId(unsigned int dev_id, int scope_id)
{
    return 0;
}

int RsSocketBatchClose(int disuse_linger, struct RsSocketCloseInfoT conn[], u32 num)
{
    return 0;
}

int RsSocketBatchAbort(int disuse_linger, struct RsSocketCloseInfoT conn[], u32 num)
{
    return 0;
}

int RsSocketListenStart(struct SocketListenInfo listen[], uint32_t num)
{
    return 0;
}

int RsSocketListenStop(struct SocketListenInfo listen[], uint32_t num)
{
    return 0;
}

static int fd = 0;
static int dev_id = 0;
int RsGetSockets(uint32_t role, struct SocketFdData conn[], uint32_t num)
{
    int i;

    for (i = 0; i < num; i++) {
        conn[i].phyId = dev_id;
        conn[i].fd = fd;
    }
    return num;
}

int RsGetSslEnable(uint32_t *sslEnable)
{
    return 0;
}

int RsPeerSocketSend(uint32_t sslEnable, int fd, const void *data, uint64_t size)
{
    return size;
}

int RsPeerSocketRecv(uint32_t sslEnable, int fd, void *data, uint64_t size)
{
    return size;
}

int rs_get_socket_num(unsigned int role, struct rdev rdev_info, unsigned int *socket_num)
{
   return 0;
}

int rs_get_all_sockets(unsigned int phyId, uint32_t role, struct SocketFdData *conn,
    uint32_t *socket_num)
{
	conn->fd = 0;
	conn->localIp.addr.s_addr = 0;
	conn->remoteIp.addr.s_addr = 0;
	conn->phyId = 0;
	conn->tag[0] = '0';
	return 0;
}

int RsSocketSend(int fd, void *data, u64 size)
{
    return size;
}

int RsSocketRecv(int fd, void *data, u64 size)
{

    return size;
}

int RsQpCreate(unsigned int phyId, unsigned int rdevIndex, struct RsQpNorm qp_norm,
    struct RsQpResp *qp_resp)
{
	qp_resp->qpn = 0;
	qp_resp->psn = 0;
	qp_resp->gidIdx = 0;
	qp_resp->gid.global.subnet_prefix = 0;
	qp_resp->gid.global.interface_id = 0;

	return 0;
}

int RsQpCreateWithAttrs(unsigned int phyId, unsigned int rdevIndex,
    struct RsQpNormWithAttrs  *qp_norm, struct RsQpRespWithAttrs *qp_resp)
{
	qp_resp->qpn = 0;
	qp_resp->aiQpAddr = 0;
	return 0;
}

int RsQpDestroy(unsigned int dev_id, unsigned int rdevIndex, unsigned int qpn)
{
    return 0;
}

int RsTypicalQpModify(unsigned int phyId, unsigned int rdevIndex,
    struct TypicalQp local_qp_info, struct TypicalQp remote_qp_info, unsigned int *udp_sport)
{
	return 0;
}

static struct ibv_mr stub_mr = {0};

int RsTypicalRegisterMrV1(unsigned int phyId, unsigned int rdevIndex, struct RdmaMrRegInfo *mr_reg_info,
    void **mr_handle)
{
	*mr_handle = &stub_mr;
	return 0;
}

int RsTypicalRegisterMr(unsigned int phyId, unsigned int rdevIndex, struct RdmaMrRegInfo *mr_reg_info,
    void **mr_handle)
{
	*mr_handle = &stub_mr;
	return 0;
}

int RsTypicalDeregisterMr(unsigned int phyId, unsigned int dev_index, unsigned long long addr)
{
	return 0;
}

int RsQpConnectAsync(unsigned int dev_id, unsigned int rdevIndex, unsigned int qpn, int fd)
{
    return 0;
}

int RsGetQpStatus(unsigned int dev_id, unsigned int rdevIndex, unsigned int qpn, int *status)
{
    return 0;
}

int RsMrReg(unsigned int dev_id, unsigned int rdevIndex, unsigned int qpn, struct RdmaMrRegInfo *mr_reg_info)
{
    return 0;
}

int RsMrDereg(unsigned int dev_id, unsigned int rdevIndex, unsigned int qpn, char *addr)
{
    return 0;
}

int RsRegisterMr(unsigned int dev_id, unsigned int rdevIndex, struct RdmaMrRegInfo *mr_reg_info)
{
    return 0;
}

int RsDeregisterMr(unsigned int dev_id, unsigned int rdevIndex, char *addr)
{
    return 0;
}

int RsSendWr(unsigned int dev_id, unsigned int rdevIndex, uint32_t qpn, struct SendWr *wr,
    struct SendWrRsp *wr_rsp)
{
    return 0;
}

int RsSendWrlist(struct RsWrlistBaseInfo base_info, struct WrInfo *wr,
    unsigned int send_num, struct SendWrRsp *wr_rsp, unsigned int *complete_num)
{
	return 0;
}

int RsGetNotifyMrInfo(unsigned int phyId, unsigned int rdevIndex, struct mrInfo*info)
{
    return 0;
}

int RsSetHostPid(uint32_t dev_id, uint32_t host_pid)
{
    return 0;
}

int RsRdevGetPortStatus(unsigned int phyId, unsigned int rdevIndex, enum PortStatus *status)
{
	return 0;
}

int RsInit(struct RaInitConfig *cfg)
{
    return 0;
}

int RsBindHostpid(unsigned int chipId, pid_t pid)
{
	return 0;
}

int RsDeinit(struct RaInitConfig *cfg)
{
    return 0;
}

int RsSocketInit(const unsigned int *vnic_ip, unsigned int num)
{
    return 0;
}

int RsSocketDeinit(struct rdev rdev_info)
{
    return 0;
}

int RsRdevInit(struct rdev rdev_info, unsigned int notify_type,  unsigned int *rdevIndex)
{
    return 0;
}

int RsRdevInitWithBackup(struct rdev rdev_info, struct rdev backup_rdev_info,
    unsigned int notify_type, unsigned int *rdevIndex)
{
    return 0;
}

int RsRdevDeinit(unsigned int dev_id, unsigned int notify_type,  unsigned int rdevIndex)
{
    return 0;
}

int RsSocketWhiteListAdd(struct rdev rdev_info, struct SocketWlistInfoT white_list[],
                                        u32 num)
{
    return 0;
}

int RsSocketWhiteListDel(struct rdev rdev_info, struct SocketWlistInfoT white_list[],
                                        u32 num)
{
    return 0;
}

int RsPeerGetIfaddrs(struct InterfaceInfo interface_infos[], unsigned int *num,
    unsigned int phyId)
{
    return 0;
}

int RsGetIfaddrs(struct IfaddrInfo ifaddr_infos[], unsigned int *num, unsigned int dev_id)
{
	return 0;
}

int RsGetIfaddrsV2(struct InterfaceInfo interface_infos[], unsigned int *num,
    unsigned int phyId, bool is_all)
{
	return 0;
}

int RsGetIfnum(unsigned int phyId, bool is_all, unsigned int *num)
{
    return 0;
}

int RsPeerGetIfnum(unsigned int phyId, unsigned int *num)
{
    return 0;
}

int RsGetVnicIp(unsigned int phyId, unsigned int *vnic_ip)
{
	return 0;
}

int RsGetInterfaceVersion(unsigned int opcode, unsigned int *version)
{
    *version = 1;
    return 0;
}

int RsNotifyCfgSet(unsigned int dev_id, unsigned long long va, unsigned long long size)
{
	return 0;
}

int RsNotifyCfgGet(unsigned int dev_id, unsigned long long *va, unsigned long long *size)
{
	return 0;
}

int RsSetTsqpDepth(unsigned int phyId, unsigned int rdevIndex, unsigned int temp_depth,
    unsigned int *qp_num)
{
	return 0;
}

int RsGetTsqpDepth(unsigned int phyId, unsigned int rdevIndex, unsigned int *temp_depth,
    unsigned int *qp_num)
{
	return 0;
}

void RsHeartbeatAlivePrint(struct RaHdcAsyncInfo *pthread_info)
{
	return;
}

int RsRecvWrlist(struct RsWrlistBaseInfo base_info, struct RecvWrlistData *wr,
    unsigned int recv_num, unsigned int *complete_num)
{
    *complete_num = recv_num;
	return 0;
}

int RsEpollCtlAdd(const void *fd_handle, enum RaEpollEvent event)
{
    return 0;
}

int RsEpollCtlMod(const void *fd_handle, enum RaEpollEvent event)
{
    return 0;
}

int RsEpollCtlDel(int fd)
{
    return 0;
}

void RsSetTcpRecvCallback(const void *callback)
{
    return;
}

int RsGetQpContext(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, void** qp,
    void** send_cq, void** recv_cq)
{
    return 0;
}

int RsCqCreate(unsigned int phyId, unsigned int rdevIndex, struct CqAttr *attr)
{
    return 0;
}

int RsCqDestroy(unsigned int phyId, unsigned int rdevIndex, struct CqAttr *attr)
{
    return 0;
}

int RsNormalQpCreate(unsigned int phyId, unsigned int rdevIndex,
    struct ibv_qp_init_attr *qp_init_attr, struct RsQpResp *qp_resp, void** qp)
{
    return 0;
}

int RsNormalQpDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn)
{
    return 0;
}

int RsSetQpAttrQos(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct QosAttr *attr)
{
    return 0;
}

int RsSetQpAttrTimeout(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    unsigned int *timeout)
{
    return 0;
}

int RsSetQpAttrRetryCnt(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    unsigned int *retry_cnt)
{
    return 0;
}

int RsGetCqeErrInfo(struct CqeErrInfo *info)
{
	return 0;
}

int RsGetCqeErrInfoNum(unsigned int phyId, unsigned int rdev_idx, unsigned int *num)
{
    return 0;
}

int RsGetCqeErrInfoList(unsigned int phyId, unsigned int rdev_idx, struct CqeErrInfo *info,
    unsigned int *num)
{
    return 0;
}

int tls_get_user_config(unsigned int save_mode, unsigned int chipId, const char *name,
    unsigned char *buf, unsigned int *buf_size)
{
    return 0;
}

void tls_get_enable_info(unsigned int save_mode, unsigned int chipId, unsigned char *buf, unsigned int buf_size)
{
    return 0;
}

struct kmc_enc_info {
    unsigned char *cipher_text;
    unsigned int cipher_text_len;
    unsigned char *work_key;
    unsigned int work_key_len;
};

int enc_with_sdp(unsigned int enc_alg, const unsigned char *inbuf,
    unsigned int size_in, unsigned char *cpr, unsigned int *ret_len)
{
    return 0;
}

int dec_with_kmc(unsigned char *cpr, unsigned int cpr_len, unsigned char *outbuf, unsigned int *size_out)
{
    return 0;
}

int RsCreateCompChannel(unsigned int phyId, unsigned int rdevIndex, void** comp_channel)
{
	return 0;
}

int RsDestroyCompChannel(void* comp_channel)
{
	return 0;
}

int RsCreateSrq(unsigned int phyId, unsigned int rdevIndex, struct SrqAttr *attr)
{
	return 0;
}

int RsDestroySrq(unsigned int phyId, unsigned int rdevIndex, struct SrqAttr *attr)
{
	return 0;
}

int RsGetLiteSupport(unsigned int phyId, unsigned int rdevIndex, int *support_lite)
{
	return 0;
}

int RsGetLiteRdevCap(unsigned int phyId, unsigned int rdev_index, struct LiteRdevCapResp *resp)
{
	return 0;
}

int RsGetLiteQpCqAttr(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteQpCqAttrResp *resp)
{
	return 0;
}

int RsGetLiteConnectedInfo(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteConnectedInfoResp *resp)
{
	return 0;
}

int RsGetLiteMemAttr(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteMemAttrResp *resp)
{
    return 0;
}

void RsSetCtx(unsigned int phyId)
{
    return;
}

int RsCreateEventHandle(int *event_handle)
{
    return 0;
}

int RsCtlEventHandle(int event_handle, const void *fd_handle, int opcode,
    enum RaEpollEvent event)
{
    int ret;
    int fd = -1;
    int tmpEvent;

    if (event_handle < 0) {
        hccp_err("[RsCtlEventHandle]event_handle[%d] is invalid", event_handle);
        return -EINVAL;
    }
    if (fd_handle == NULL) {
        hccp_err("[RsCtlEventHandle]fd_handle is NULL");
        return -EINVAL;
    }
    if (opcode != EPOLL_CTL_ADD && opcode != EPOLL_CTL_DEL && opcode != EPOLL_CTL_MOD) {
        hccp_err("[RsCtlEventHandle]opcode[%d] invalid, valid opcode includes {%d, %d, %d}",
            opcode, EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD);
        return -EINVAL;
    }
    if (opcode == EPOLL_CTL_DEL && event != RA_EPOLLIN) {
        hccp_err("[RsCtlEventHandle]param invalid: opcode[%d], event[%d]", opcode, event);
        return -EINVAL;
    }

    if (event == RA_EPOLLONESHOT) {
        tmpEvent = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmpEvent = EPOLLIN;
    } else {
        hccp_err("[RsCtlEventHandle]unkown event[%d]", event);
        return -EINVAL;
    }

    return 0;
}

int RsWaitEventHandle(int event_handle, struct SocketEventInfoT *event_info, int timeout)
{
    return 0;
}

int RsDestroyEventHandle(int *event_handle)
{
    return 0;
}

int RsGetVnicIpInfos(unsigned int phyId, enum IdType type, unsigned int ids[], unsigned int num,
    struct IpInfo infos[])
{
    return 0;
}

int RsQpBatchModify(unsigned int phyId, unsigned int rdevIndex,
    int status, int qpn[], int qpn_num)
{
    return 0;
}

int RsSocketGetClientSocketErrInfo(struct SocketConnectInfo conn[],
    struct SocketErrInfo  err[], unsigned int num)
{
    return 0;
}

int RsSocketGetServerSocketErrInfo(struct SocketListenInfo conn[],
    struct ServerSocketErrInfo err[], unsigned int num)
{
    return 0;
}

int RsSocketAcceptCreditAdd(struct SocketListenInfo conn[], uint32_t num, unsigned int credit_limit)
{
    return 0;
}

int RsRemapMr(unsigned int phyId, unsigned int rdevIndex, struct MemRemapInfo memList[], unsigned int mem_num)
{
    return 0;
}

int RsGetTlsEnable(unsigned int phyId, bool *tls_enable)
{
	return 0;
}

int RsGetSecRandom(unsigned int *value)
{
	return 0;
}

int RsDrvGetRandomNum(int *rand_num)
{
	return 0;
}

int RsGetHccnCfg(unsigned int phyId, enum HccnCfgKey key, char *value, unsigned int *value_len)
{
	return 0;
}
