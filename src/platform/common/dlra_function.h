/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_SRC_DLRAFUNCTION_H
#define HCCL_SRC_DLRAFUNCTION_H

#include <functional>
#include <atomic>
#include <mutex>
#include <hccl/hccl_types.h>
#include <infiniband/verbs.h>
#include "hccl/base.h"
#include "network/hccp.h"
#include "network/hccp_ping.h"
#include "private_types.h"
#include "hccl_common.h"

namespace hccl {
class DlRaFunction {
public:
    class Init;
    virtual ~DlRaFunction();
    static DlRaFunction &GetInstance();
    HcclResult DlRaFunctionInit();
    std::function<int(RdmaHandle rdmaHandle, unsigned int *tempDepth, unsigned int *qpNum)> dlRaGetQpDepth;
    std::function<int(RdmaHandle rdmaHandle, unsigned int tempDepth, unsigned int *qpNum)> dlRaSetQpDepth;
    std::function<int(RdmaHandle rdmaHandle, int flag, int qpMode, QpHandle *qpHandle)> dlRaQpCreate;
    std::function<int(void* qpHandle, void** qp, void** sendCq, void** recvCq)> dlRaGetQpContext;
    std::function<int(QpHandle handle)> dlRaQpDestroy;
    std::function<int(QpHandle handle, const SocketHandle sockHandle)> dlRaQpConnectAsync;
    std::function<int(QpHandle handle, int *status)> dlRaGetQpStatus;
    std::function<int(QpHandle handle, struct mr_info *info)> dlRaMrReg;
    std::function<int(RdmaHandle rdmaHandle, struct mr_info *info)> dlRaGetNotifyMrInfo;
    std::function<int(QpHandle handle, struct mr_info *info)> dlRaMrDereg;
    std::function<int(QpHandle handle, struct send_wr *wr, struct send_wr_rsp *opRsp)> dlRaSendWr;
    std::function<int(QpHandle handle, struct send_wr_v2 *wr, struct send_wr_rsp *opRsp)> dlRaSendWrV2;
    std::function<int(QpHandle handle, bool is_send_cq, unsigned int num, void *wc)> dlRaPollCq;
    std::function<int(QpHandle handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
        unsigned int sendNum, unsigned int *completeNum)> dlRaSendWrlist;
    std::function<int(QpHandle handle, struct send_wrlist_data_ext wr[], struct send_wr_rsp op_rsp[],
        unsigned int sendNum, unsigned int *completeNum)> dlRaSendWrlistExt;
    std::function<int(QpHandle handle, struct wr_info wr[], struct send_wr_rsp opRsp[],
        unsigned int sendNum, unsigned int *completeNum)> dlRaSendNormalWrlist;
    std::function<int(RdmaHandle handle, unsigned long long *va, unsigned long long *size)> dlRaGetNotifyBaseAddr;
    std::function<int(struct ra_init_config *config)> dlRaInit;
    std::function<int(struct ra_init_config *config)> dlRaDeInit;
    std::function<int(int mode, u32 notifyType, struct rdev rdevInfo, RdmaHandle *rdmaHandle)> dlRaRdmaInit;
    std::function<int(struct rdev_init_info init_info, struct rdev rdevInfo, RdmaHandle *rdmaHandle)> \
        dlRaRdmaInitWithAttr;
    std::function<int(struct rdev_init_info *init_info, struct rdev *rdevInfo, struct rdev *backupRdevInfo,
        RdmaHandle *rdmaHandle)> dlRaRdmaInitWithBackupAttr;
    std::function<int(unsigned int phy_id, RdmaHandle *rdmaHandle)> dlRaRdmaGetHandle;
    std::function<int(RdmaHandle handle, u32 notifyType)> dlRaRdmaDeInit;
    std::function<int(int mode, struct rdev rdevInfo, SocketHandle *socket_handle)> dlRaSocketInit;
    std::function<int(int mode, struct socket_init_info_t rdevInfo, SocketHandle *socket_handle)> dlRaSocketInitV1;
    std::function<int(SocketHandle handle)> dlRaSocketDeInit;
    std::function<int(struct socket_listen_info_t conn[], unsigned int num)> dlRaSocketListenStart;
    std::function<int(struct socket_listen_info_t conn[], unsigned int num, unsigned int creditLimit)> dlRaSocketAcceptCreditAdd;
    std::function<int(struct socket_listen_info_t conn[], unsigned int num)> dlRaSocketListenStop;
    std::function<int(struct socket_connect_info_t conn[], unsigned int num)> dlRaSocketBatchConnect;
    std::function<int(struct socket_connect_info_t conn[], unsigned int num)> dlRaSocketBatchAbort;
    std::function<int(struct socket_close_info_t conn[], unsigned int num)> dlRaSocketBatchClose;
    std::function<int(unsigned int role, struct socket_info_t conn[], unsigned int num, unsigned int *connected_num)> \
        dlRaGetSockets;
    std::function<int(const FdHandle, const void*, unsigned long long, unsigned long long *sent_size)> \
        dlRaSocketSend;
    std::function<int(const FdHandle fdHandle, void *data, unsigned long long size,
        unsigned long long *received_size)> dlRaSocketRecv;
    std::function<int(unsigned int enable)> dlRaSocketSetWhiteListStatus;
    std::function<int(unsigned int *enable)> dlRaSocketGetWhiteListStatus;
    std::function<int(SocketHandle handle, struct socket_wlist_info_t whiteList[], unsigned int num)> \
        dlRaSocketWhiteListAdd;
    std::function<int(SocketHandle handle, struct socket_wlist_info_t whiteList[], unsigned int num)> \
        dlRaSocketWhiteListDel;
    std::function<int(struct ra_get_ifattr *config, unsigned int *num)> \
        dlRaGetIfNum;
    std::function<int(struct ra_get_ifattr *config, struct interface_info interface_infos[], unsigned int *num)> \
        dlRaGetIfAddress;
    std::function<int(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version)> \
        dlRaGetInterfaceVersion;
    std::function<int(const RdmaHandle rdmaHandle, struct mr_info *mrInfo, MrHandle* mrHandle)> dlRaRegGlobalMr;
    std::function<int(const RdmaHandle rdmaHandle, MrHandle mrHandle)> dlRaDeRegGlobalMr;
    std::function<int(const FdHandle fd_handle, RaEpollEvent event)> dlRaEpollCtlAdd;
    std::function<int(const FdHandle fd_handle, RaEpollEvent event)> dlRaEpollCtlMod;
    std::function<int(const FdHandle fd_handle)> dlRaEpollCtlDel;
    std::function<int(const SocketHandle socketHandle, const void *callback)> dlRaSetRecvDataCallback;
    std::function<int(RdmaHandle handle, struct cq_attr *attr)> dlRaCreateCq;
    std::function<int(RdmaHandle handle, struct cq_attr *attr)> dlRaDestroyCq;
    std::function<int(RdmaHandle handle, struct ibv_qp_init_attr *initAttr, QpHandle *qpHandle, void** qp)> \
        dlRaNormalQpCreate;
    std::function<int(QpHandle handle)> dlRaNormalQpDestroy;
    std::function<int(QpHandle qpHandle, struct qos_attr *attr)> dlRaSetQpAttrQos;
    std::function<int(QpHandle qpHandle, unsigned int *timeout)> dlRaSetQpAttrTimeOut;
    std::function<int(QpHandle qpHandle, unsigned int *retry_cnt)> dlRaSetQpAttrRetryCnt;
    std::function<int(RdmaHandle rdmaHandle, void **compChannel)> dlRaCreateCompChannel;
    std::function<int(RdmaHandle rdmaHandle, void *compChannel)> dlRaDestroyCompChannel;
    std::function<int(unsigned int phy_id, struct cqe_err_info *info)> dlRaGetCqeErrInfo;
    std::function<int(RdmaHandle rdmaHandle, struct cqe_err_info *info_list, u32 *number)> dlRaGetCqeErrInfoList;
    std::function<int(QpHandle qpHandle, struct qp_attr *attr)> dlRaGetQpAttr;
    std::function<int(RdmaHandle rdmaHandle, struct srq_attr *attr)> dlRaCreateSrq;
    std::function<int(RdmaHandle rdmaHandle, struct srq_attr *attr)> dlRaDestroyeSrq;
    std::function<int(int *event_handle)> dlRaCreateEventHandle;
    std::function<int(int event_handle, const void *fd_handle, int opcode, RaEpollEvent event)> dlRaCtlEventHandle;

    std::function<int(int event_handle, struct socket_event_info *event_infos, int timeout, unsigned int maxevents,
        unsigned int *events_num)> dlRaWaitEventHandle;

    std::function<int(int *event_handle)> dlRaDestroyEventHandle;
    std::function<int(QpHandle handle, struct recv_wrlist_data *wr, unsigned int recvNum,
        unsigned int *completeNum)> dlRaRecvWrlist;

    std::function<int(RdmaHandle rdmaHandle, struct qp_ext_attrs *ext_attrs, QpHandle *qpHandle)> dlRaQpCreateWithAttrs;
    std::function<int(RdmaHandle rdmaHandle, int flag,
        int qpMode, struct typical_qp *qpInfo, QpHandle* qpHandle)> dlRaTypicalQpCreate;
    std::function<int(RdmaHandle rdmaHandle,
        struct typical_qp *localQpInfo, struct typical_qp *remoteQpInfo)> dlRaTypicalQpModify;
    std::function<int(QpHandle handle, struct send_wr *wr, struct send_wr_rsp *opRsp)> dlRaTypicalSendWr;
 
    std::function<int(RdmaHandle rdmaHandle, struct qp_ext_attrs *ext_attrs, struct ai_qp_info *info, \
    QpHandle *qpHandle)> dlRaAiQpCreate;
    
    std::function<int(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num, \
    struct ip_info infos[])>  dlRaGetSocketVnicIpInfos;
    std::function<int(RdmaHandle rdmaHandle, int *supportlite)> dlRaGetRdmaLiteStatus;
    std::function<int(RdmaHandle rdmaHandle, void **qpHandle, unsigned int num, int expectStatus)> dlRaQpBatchModify;

    std::function<int(struct ping_init_attr *init_attr, struct ping_init_info *init_info, void **pingHandle)> \
        dlRaPingInit;
    std::function<int(void *pingHandle)> dlRaPingDeinit;
    std::function<int(void *pingHandle, struct ping_target_info target[], uint32_t num)> dlRaPingTargetAdd;
    std::function<int(void *pingHandle, struct ping_target_comm_info target[], uint32_t num)> dlRaPingTargetDel;
    std::function<int(void *pingHandle, struct ping_task_attr *attr)> dlRaPingTaskStart;
    std::function<int(void *pingHandle)> dlRaPingTaskStop;
    std::function<int(void *pingHandle, struct ping_target_result target[], uint32_t *num)> dlRaPingGetResults;

    std::function<int(int ins_id)> dlRaIsFirstUsed{};
    std::function<int(int ins_id)> dlRaIsLastUsed{};
    std::function<int(RdmaHandle rdmaHandle, enum port_status *status)> dlRaRdevGetPortStatus{};
    std::function<int(RdmaHandle rdmaHandle, struct mem_remap_info info[], unsigned int num)> dlRaRemapMr{};
    std::function<int(struct tlv_init_info *init_info, uint32_t tlv_handle_id, uint32_t *buffer_size,
        void **tlv_handle)>  dlH2DTlvInit;
    std::function<int(void *tlv_handle)>  dlH2DTlvDeinit;
    std::function<int(void *tlv_handle, struct tlv_msg *send_msg, struct tlv_msg *recv_msg)>  dlH2DTlvRequest;
    std::function<int(struct ra_info *info, bool *tls_enable)> dlRaRaGetTlsEnable;
    std::function<int(struct ra_info *info, enum save_snapshot_action action)> dlRaSaveSnapShot;
    std::function<int(struct ra_info *info)> dlRaRestoreSnapShot;
protected:
private:
    friend Init;
    void *handle_;
    std::mutex handleMutex_;
    static DlRaFunction* hcclDlRaFunction;
    DlRaFunction(const DlRaFunction&);
    DlRaFunction &operator=(const DlRaFunction&);
    DlRaFunction();
    HcclResult DlRaFunctionSocketInit();
    HcclResult DlRaFunctionRdmaInit();
};

class DlRaFunction::Init {
public:
    Init();
    ~Init();
private:
    static std::atomic<unsigned> initCount;
};

static DlRaFunction::Init g_dlraInit; // 关键变量

}  // namespace hccl

#endif  // HCCL_SRC_DLRAFUNCTION_H
