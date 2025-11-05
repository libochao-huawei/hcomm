/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dlra_function.h"

#include <string>
#include <map>
#include "hccl_dl.h"
#include "log.h"

namespace hccl {

std::atomic<unsigned> DlRaFunction::Init::initCount(0);
DlRaFunction* DlRaFunction::hcclDlRaFunction = nullptr;

DlRaFunction::Init::Init()
{
    if (initCount.fetch_add(1) == 0) {
        DlRaFunction::hcclDlRaFunction = new DlRaFunction;
    }
}

DlRaFunction::Init::~Init()
{
    if (initCount.fetch_sub(1) == 0) {
        if (DlRaFunction::GetInstance().handle_ != nullptr) {
            (void)HcclDlclose(DlRaFunction::GetInstance().handle_);
            DlRaFunction::GetInstance().handle_ = nullptr;
        }
        delete DlRaFunction::hcclDlRaFunction;
    }
}

DlRaFunction &DlRaFunction::GetInstance()
{
    return *hcclDlRaFunction;
}

DlRaFunction::DlRaFunction() : handle_(nullptr)
{
}

DlRaFunction::~DlRaFunction()
{
}

HcclResult DlRaFunction::DlRaFunctionRdmaInit()
{
    dlRaGetQpDepth = (int(*)(RdmaHandle, unsigned int*, unsigned int*))HcclDlsym(handle_, "ra_get_tsqp_depth");
    CHK_SMART_PTR_NULL(dlRaGetQpDepth);
    dlRaSetQpDepth = (int(*)(RdmaHandle, unsigned int, unsigned int*))HcclDlsym(handle_, "ra_set_tsqp_depth");
    CHK_SMART_PTR_NULL(dlRaSetQpDepth);
    dlRaQpCreate = (int(*)(RdmaHandle, int, int, QpHandle*))HcclDlsym(handle_, "ra_qp_create");
    CHK_SMART_PTR_NULL(dlRaQpCreate);
    dlRaQpDestroy = (int(*)(QpHandle))HcclDlsym(handle_, "ra_qp_destroy");
    CHK_SMART_PTR_NULL(dlRaQpDestroy);
    dlRaGetQpContext = (int(*)(void*, void**, void**, void**))HcclDlsym(handle_, "ra_get_qp_context");
    CHK_SMART_PTR_NULL(dlRaGetQpContext);
    dlRaQpConnectAsync = (int(*)(QpHandle, const SocketHandle))HcclDlsym(handle_, "ra_qp_connect_async");
    CHK_SMART_PTR_NULL(dlRaQpConnectAsync);
    dlRaGetQpStatus = (int(*)(QpHandle, int*))HcclDlsym(handle_, "ra_get_qp_status");
    CHK_SMART_PTR_NULL(dlRaGetQpStatus);
    dlRaMrDereg = (int(*)(QpHandle, struct mr_info*))HcclDlsym(handle_, "ra_mr_dereg");
    CHK_SMART_PTR_NULL(dlRaMrDereg);
    dlRaMrReg = (int(*)(QpHandle, struct mr_info*))HcclDlsym(handle_, "ra_mr_reg");
    CHK_SMART_PTR_NULL(dlRaMrReg);
    dlRaGetNotifyMrInfo = (int(*)(RdmaHandle, struct mr_info*))HcclDlsym(handle_, "ra_get_notify_mr_info");
    CHK_SMART_PTR_NULL(dlRaGetNotifyMrInfo);
    dlRaRdmaDeInit = (int(*)(RdmaHandle, u32))HcclDlsym(handle_, "ra_rdev_deinit");
    CHK_SMART_PTR_NULL(dlRaRdmaDeInit);
    dlRaRdmaInitWithAttr = (int(*)(struct rdev_init_info, struct rdev, RdmaHandle*))\
        HcclDlsym(handle_, "ra_rdev_init_v2");
    CHK_SMART_PTR_NULL(dlRaRdmaInitWithAttr);
    dlRaRdmaInitWithBackupAttr = (int(*)(struct rdev_init_info*, struct rdev*, struct rdev*, RdmaHandle*))\
        HcclDlsym(handle_, "ra_rdev_init_with_backup");
    CHK_SMART_PTR_NULL(dlRaRdmaInitWithBackupAttr);
    dlRaRdmaGetHandle = (int(*)(unsigned int, RdmaHandle*))HcclDlsym(handle_, "ra_rdev_get_handle");
    dlRaRdmaInit = (int(*)(int, u32, struct rdev, RdmaHandle*))HcclDlsym(handle_, "ra_rdev_init");
    CHK_SMART_PTR_NULL(dlRaRdmaInit);
    dlRaSendWr = (int(*)(QpHandle, struct send_wr*, struct send_wr_rsp*))HcclDlsym(handle_, "ra_send_wr");
    CHK_SMART_PTR_NULL(dlRaSendWr);
    dlRaSendWrV2 = (int(*)(QpHandle, struct send_wr_v2*, struct send_wr_rsp*))HcclDlsym(handle_, "ra_send_wr_v2");
    CHK_SMART_PTR_NULL(dlRaSendWrV2);
    dlRaPollCq = (int(*)(QpHandle, bool, unsigned int, void *))HcclDlsym(handle_, "ra_poll_cq");
    CHK_SMART_PTR_NULL(dlRaPollCq);
    dlRaSendWrlist = (int(*)(QpHandle handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
        unsigned int sendNum, unsigned int *completeNum))HcclDlsym(handle_, "ra_send_wrlist");
    if (dlRaSendWrlist == nullptr) {
        HCCL_WARNING("dlRaSendWrlist is nullptr, can not use ra_send_wrlist");
    }
    dlRaSendWrlistExt = (int(*)(QpHandle handle, struct send_wrlist_data_ext wr[], struct send_wr_rsp op_rsp[],
        unsigned int sendNum, unsigned int *completeNum))HcclDlsym(handle_, "ra_send_wrlist_ext");
    if (dlRaSendWrlistExt == nullptr) {
        HCCL_WARNING("dlRaSendWrlistExt is nullptr, can not use ra_send_wrlist_ext");
    }
    dlRaSendNormalWrlist = (int(*)(QpHandle handle, struct wr_info wr[], struct send_wr_rsp opRsp[],
        unsigned int sendNum, unsigned int *completeNum))HcclDlsym(handle_, "ra_send_normal_wrlist");
    CHK_SMART_PTR_NULL(dlRaSendNormalWrlist);
    dlRaRegGlobalMr = (int(*)(const RdmaHandle, struct mr_info *info, MrHandle *mrHandle))HcclDlsym(handle_,
        "ra_register_mr");
    CHK_SMART_PTR_NULL(dlRaRegGlobalMr);
    dlRaDeRegGlobalMr = (int(*)(const RdmaHandle, MrHandle mrHandle))HcclDlsym(handle_, "ra_deregister_mr");
    CHK_SMART_PTR_NULL(dlRaDeRegGlobalMr);
    dlRaCreateCq = (int(*)(RdmaHandle, struct cq_attr *))HcclDlsym(handle_, "ra_cq_create");
    CHK_SMART_PTR_NULL(dlRaCreateCq);
    dlRaDestroyCq = (int(*)(RdmaHandle, struct cq_attr *))HcclDlsym(handle_, "ra_cq_destroy");
    CHK_SMART_PTR_NULL(dlRaDestroyCq);
    dlRaNormalQpCreate = (int(*)(RdmaHandle, struct ibv_qp_init_attr *, void **, void **))HcclDlsym(handle_,
        "ra_normal_qp_create");
    CHK_SMART_PTR_NULL(dlRaNormalQpCreate);
    dlRaNormalQpDestroy = (int(*)(QpHandle))HcclDlsym(handle_, "ra_normal_qp_destroy");
    CHK_SMART_PTR_NULL(dlRaNormalQpDestroy);
    dlRaSetQpAttrQos = (int(*)(QpHandle, struct qos_attr *))HcclDlsym(handle_, "ra_set_qp_attr_qos");
    CHK_SMART_PTR_NULL(dlRaSetQpAttrQos);
    dlRaSetQpAttrTimeOut = (int(*)(QpHandle, u32 *))HcclDlsym(handle_, "ra_set_qp_attr_timeout");
    CHK_SMART_PTR_NULL(dlRaSetQpAttrTimeOut);
    dlRaSetQpAttrRetryCnt = (int(*)(QpHandle, u32 *))HcclDlsym(handle_, "ra_set_qp_attr_retry_cnt");
    CHK_SMART_PTR_NULL(dlRaSetQpAttrRetryCnt);
    dlRaCreateCompChannel = (int(*)(const void*, void **))HcclDlsym(handle_, "ra_create_comp_channel");
    CHK_SMART_PTR_NULL(dlRaCreateCompChannel);
    dlRaDestroyCompChannel = (int(*)(const void*, void *))HcclDlsym(handle_, "ra_destroy_comp_channel");
    CHK_SMART_PTR_NULL(dlRaDestroyCompChannel);
    dlRaGetCqeErrInfo = (int(*)(unsigned int phy_id, struct cqe_err_info *))HcclDlsym(handle_, "ra_get_cqe_err_info");
    CHK_SMART_PTR_NULL(dlRaGetCqeErrInfo);
    dlRaGetCqeErrInfoList = 
            (int (*)(RdmaHandle, struct cqe_err_info *, u32 *))HcclDlsym(handle_, "ra_rdev_get_cqe_err_info_list");
    CHK_SMART_PTR_NULL(dlRaGetCqeErrInfoList);
    dlRaGetQpAttr = (int(*)(QpHandle, struct qp_attr *))HcclDlsym(handle_, "ra_get_qp_attr");
    CHK_SMART_PTR_NULL(dlRaGetQpAttr);
    dlRaCreateSrq = (int(*)(const void*, struct srq_attr *))HcclDlsym(handle_, "ra_create_srq");
    CHK_SMART_PTR_NULL(dlRaCreateSrq);
    dlRaDestroyeSrq = (int(*)(const void*, struct srq_attr *))HcclDlsym(handle_, "ra_destroy_srq");
    CHK_SMART_PTR_NULL(dlRaDestroyeSrq);
    dlRaQpCreateWithAttrs =
        (int (*)(RdmaHandle, struct qp_ext_attrs *, QpHandle *))HcclDlsym(handle_, "ra_qp_create_with_attrs");
    CHK_SMART_PTR_NULL(dlRaQpCreateWithAttrs);
    dlRaTypicalQpCreate =
        (int(*)(RdmaHandle, int, int, struct typical_qp*, QpHandle*))HcclDlsym(handle_, "ra_typical_qp_create");
    CHK_SMART_PTR_NULL(dlRaTypicalQpCreate);
    dlRaTypicalQpModify =
        (int(*)(QpHandle, struct typical_qp*, struct typical_qp*))HcclDlsym(handle_, "ra_typical_qp_modify");
    CHK_SMART_PTR_NULL(dlRaTypicalQpModify);
    dlRaTypicalSendWr =
        (int(*)(QpHandle, struct send_wr*, struct send_wr_rsp*))HcclDlsym(handle_, "ra_typical_send_wr");
    CHK_SMART_PTR_NULL(dlRaTypicalSendWr);
    dlRaAiQpCreate = (int (*)(RdmaHandle, struct qp_ext_attrs *, struct ai_qp_info *, QpHandle *))HcclDlsym(handle_,
        "ra_ai_qp_create");
    CHK_SMART_PTR_NULL(dlRaAiQpCreate);
    dlRaRecvWrlist = (int(*)(QpHandle handle, struct recv_wrlist_data *wr, unsigned int recvNum,
        unsigned int *completeNum))HcclDlsym(handle_, "ra_recv_wrlist");
    if (dlRaRecvWrlist == nullptr) {
        HCCL_WARNING("dlRaRecvWrlist is nullptr, can not use ra_recv_wrlist");
    }
    dlRaGetRdmaLiteStatus = (int (*)(RdmaHandle, int *))HcclDlsym(handle_, "ra_rdev_get_support_lite");
    CHK_SMART_PTR_NULL(dlRaGetRdmaLiteStatus);

    dlRaQpBatchModify =
        (int (*)(RdmaHandle rdmaHandle, void **qpHandle, unsigned int num, int expectStatus))HcclDlsym(handle_,
        "ra_qp_batch_modify");
    if (dlRaQpBatchModify == nullptr) {
        HCCL_ERROR("dlRaQpBatchModify is nullptr, can not use ra_qp_batch_modify");
    }
    CHK_SMART_PTR_NULL(dlRaQpBatchModify);

    dlRaPingInit =
        (int (*)(struct ping_init_attr *, struct ping_init_info *, void **))HcclDlsym(handle_, "ra_ping_init");
    if (dlRaPingInit == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingInit, please check!");
    }
    dlRaPingDeinit = (int(*)(void *))HcclDlsym(handle_, "ra_ping_deinit");
    if (dlRaPingDeinit == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingDeinit, please check!");
    }
    dlRaPingTargetAdd =
        (int(*)(void *, struct ping_target_info target[], uint32_t))HcclDlsym(handle_, "ra_ping_target_add");
    if (dlRaPingTargetAdd == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingTargetAdd, please check!");
    }
    dlRaPingTargetDel =
        (int(*)(void *, struct ping_target_comm_info target[], uint32_t))HcclDlsym(handle_, "ra_ping_target_del");
    if (dlRaPingTargetDel == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingTargetDel, please check!");
    }
    dlRaPingTaskStart = (int(*)(void *, struct ping_task_attr *))HcclDlsym(handle_, "ra_ping_task_start");
    if (dlRaPingTaskStart == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingTaskStart, please check!");
    }
    dlRaPingTaskStop = (int(*)(void *))HcclDlsym(handle_, "ra_ping_task_stop");
    if (dlRaPingTaskStop == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingTaskStop, please check!");
    }
    dlRaPingGetResults =
        (int(*)(void *, struct ping_target_result target[], uint32_t *))HcclDlsym(handle_, "ra_ping_get_results");
    if (dlRaPingGetResults == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaPingGetResults, please check!");
    }

    dlRaIsFirstUsed = (int(*)(int))HcclDlsym(handle_, "ra_is_first_used");
    CHK_SMART_PTR_NULL(dlRaIsFirstUsed);

    dlRaIsLastUsed = (int(*)(int))HcclDlsym(handle_, "ra_is_last_used");
    CHK_SMART_PTR_NULL(dlRaIsLastUsed);

    dlRaRdevGetPortStatus = (int(*)(RdmaHandle, enum port_status *))HcclDlsym(handle_, "ra_rdev_get_port_status");
    CHK_SMART_PTR_NULL(dlRaRdevGetPortStatus);

    dlRaRemapMr = (int(*)(RdmaHandle, struct mem_remap_info info[], unsigned int num))HcclDlsym(handle_, "ra_remap_mr");
    if (dlRaRemapMr == nullptr) {
        HCCL_WARNING("Current package doesn't have dlRaRemapMr, please check!");
    }

    dlH2DTlvInit = (int(*)(struct tlv_init_info *, uint32_t, uint32_t *, void**))HcclDlsym(handle_, "ra_tlv_init");
    if (dlH2DTlvInit == nullptr) {
        HCCL_WARNING("Current package doesn't have dlH2DTlvInit, please check!");
    }
    dlH2DTlvDeinit = (int(*)(void*))HcclDlsym(handle_, "ra_tlv_deinit");
    if (dlH2DTlvDeinit == nullptr) {
        HCCL_WARNING("Current package doesn't have dlH2DTlvDeinit, please check!");
    }
    dlH2DTlvRequest = (int(*)(void *, struct tlv_msg[],  struct tlv_msg[]))HcclDlsym(handle_, "ra_tlv_request");
    if (dlH2DTlvRequest == nullptr) {
        HCCL_WARNING("Current package doesn't have H2DTlvRequest, please check!");
    }
    return HCCL_SUCCESS;
}

HcclResult DlRaFunction::DlRaFunctionSocketInit()
{
    dlRaGetNotifyBaseAddr =
        (int(*)(RdmaHandle, unsigned long long*, unsigned long long*))HcclDlsym(handle_, "ra_get_notify_base_addr");
    CHK_SMART_PTR_NULL(dlRaGetNotifyBaseAddr);
    dlRaGetSockets = (int(*)(unsigned int, struct socket_info_t[], unsigned int, unsigned int*))\
        HcclDlsym(handle_, "ra_get_sockets");
    CHK_SMART_PTR_NULL(dlRaGetSockets);
    dlRaSocketBatchClose =
        (int(*)(struct socket_close_info_t[], unsigned int))HcclDlsym(handle_, "ra_socket_batch_close");
    CHK_SMART_PTR_NULL(dlRaSocketBatchClose);
    dlRaSocketBatchConnect =
        (int(*)(struct socket_connect_info_t[], unsigned int num))HcclDlsym(handle_, "ra_socket_batch_connect");
    CHK_SMART_PTR_NULL(dlRaSocketBatchConnect);
    dlRaSocketBatchAbort =
        (int(*)(struct socket_connect_info_t[], unsigned int num))HcclDlsym(handle_, "ra_socket_batch_abort");
    CHK_SMART_PTR_NULL(dlRaSocketBatchAbort);
    dlRaSocketDeInit = (int(*)(SocketHandle))HcclDlsym(handle_, "ra_socket_deinit");
    CHK_SMART_PTR_NULL(dlRaSocketDeInit);
    dlRaSocketInit = (int(*)(int, struct rdev, SocketHandle*))HcclDlsym(handle_, "ra_socket_init");
    CHK_SMART_PTR_NULL(dlRaSocketInit);
    dlRaSocketInitV1 = (int(*)(int, struct socket_init_info_t, SocketHandle*))HcclDlsym(handle_, "ra_socket_init_v1");
    CHK_SMART_PTR_NULL(dlRaSocketInitV1);
    dlRaSocketListenStart =
        (int(*)(struct socket_listen_info_t[], unsigned int))HcclDlsym(handle_, "ra_socket_listen_start");
    CHK_SMART_PTR_NULL(dlRaSocketListenStart);
    dlRaSocketAcceptCreditAdd =
        (int(*)(struct socket_listen_info_t[], unsigned int, unsigned int))HcclDlsym(handle_, "ra_socket_accept_credit_add");
    CHK_SMART_PTR_NULL(dlRaSocketAcceptCreditAdd);
    dlRaSocketListenStop =
        (int(*)(struct socket_listen_info_t[], unsigned int))HcclDlsym(handle_, "ra_socket_listen_stop");
    CHK_SMART_PTR_NULL(dlRaSocketListenStop);
    dlRaSocketRecv = (int(*)(const FdHandle, const void*, unsigned long long, unsigned long long*))\
            HcclDlsym(handle_, "ra_socket_recv");
    CHK_SMART_PTR_NULL(dlRaSocketRecv);
    dlRaSocketSend = (int(*)(const FdHandle, const void*, unsigned long long, unsigned long long*))\
            HcclDlsym(handle_, "ra_socket_send");
    CHK_SMART_PTR_NULL(dlRaSocketSend);
    dlRaSocketSetWhiteListStatus =
        (int(*)(unsigned int))HcclDlsym(handle_, "ra_socket_set_white_list_status");
    CHK_SMART_PTR_NULL(dlRaSocketSetWhiteListStatus);
    dlRaSocketGetWhiteListStatus =
        (int(*)(unsigned int*))HcclDlsym(handle_, "ra_socket_get_white_list_status");
    CHK_SMART_PTR_NULL(dlRaSocketGetWhiteListStatus);
    dlRaSocketWhiteListAdd =
        (int(*)(SocketHandle, struct socket_wlist_info_t[], unsigned int))HcclDlsym(handle_,
        "ra_socket_white_list_add");
    CHK_SMART_PTR_NULL(dlRaSocketWhiteListAdd);
    dlRaSocketWhiteListDel =
        (int(*)(SocketHandle, struct socket_wlist_info_t[], unsigned int))HcclDlsym(handle_,
        "ra_socket_white_list_del");
    CHK_SMART_PTR_NULL(dlRaSocketWhiteListDel);
    /* 考虑兼容性问题，这里不校验dlRaGetIfNum是否为空，在使用处校验 */
    dlRaGetIfNum = (int(*)(struct ra_get_ifattr *config, unsigned int *num))HcclDlsym(handle_, "ra_get_ifnum");
    if (dlRaGetIfNum == nullptr) {
        HCCL_WARNING("dlRaGetIfNum is nullptr, can not use ra_get_ifnum");
    }

    dlRaGetIfAddress =
        (int(*)(struct ra_get_ifattr *config, struct interface_info interface_infos[], unsigned int *num))\
        HcclDlsym(handle_, "ra_get_ifaddrs");
    CHK_SMART_PTR_NULL(dlRaGetIfAddress);
    dlRaGetInterfaceVersion =
        (int(*)(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version))\
        HcclDlsym(handle_, "ra_get_interface_version");
    if (dlRaGetInterfaceVersion == nullptr) {
        HCCL_WARNING("dlRaGetInterfaceVersion is nullptr, can not use ra_get_interface_version");
    }
    dlRaEpollCtlAdd = (int(*)(const FdHandle fd_handle, RaEpollEvent event))HcclDlsym(handle_, "ra_epoll_ctl_add");
    CHK_SMART_PTR_NULL(dlRaEpollCtlAdd);
    dlRaEpollCtlMod = (int(*)(const FdHandle fd_handle, RaEpollEvent event))HcclDlsym(handle_, "ra_epoll_ctl_mod");
    CHK_SMART_PTR_NULL(dlRaEpollCtlMod);
    dlRaEpollCtlDel = (int(*)(const FdHandle fd_handle))HcclDlsym(handle_, "ra_epoll_ctl_del");
    CHK_SMART_PTR_NULL(dlRaEpollCtlDel);
    dlRaSetRecvDataCallback = (int(*)(const SocketHandle socketHandle, const void *callback))
        HcclDlsym(handle_, "ra_set_tcp_recv_callback");
    CHK_SMART_PTR_NULL(dlRaSetRecvDataCallback);

    dlRaCreateEventHandle = (int(*)(int *event_handle))HcclDlsym(handle_, "ra_create_event_handle");
    if (dlRaCreateEventHandle == nullptr) {
        HCCL_WARNING("dlRaCreateEventHandle is nullptr, can not use ra_create_event_handle");
    }
    dlRaCtlEventHandle =
        (int(*)(int event_handle, const void *fd_handle, int opcode, RaEpollEvent event))
        HcclDlsym(handle_, "ra_ctl_event_handle");
    if (dlRaCtlEventHandle == nullptr) {
        HCCL_WARNING("dlRaCtlEventHandle is nullptr, can not use ra_ctl_event_handle");
    }
    dlRaWaitEventHandle =
        (int(*)(int event_handle, struct socket_event_info *event_infos, int timeout, unsigned int maxevents,
        unsigned int *events_num))HcclDlsym(handle_, "ra_wait_event_handle");
    if (dlRaWaitEventHandle == nullptr) {
        HCCL_WARNING("dlRaWaitEventHandle is nullptr, can not use ra_wait_event_handle");
    }
    dlRaDestroyEventHandle = (int(*)(int *event_handle))HcclDlsym(handle_, "ra_destroy_event_handle");
    if (dlRaDestroyEventHandle == nullptr) {
        HCCL_WARNING("dlRaDestroyEventHandle is nullptr, can not use ra_destroy_event_handle");
    }

    dlRaGetSocketVnicIpInfos = (int (*)(unsigned int, enum id_type, unsigned int *, unsigned int,
        struct ip_info infos[]))HcclDlsym(handle_, "ra_socket_get_vnic_ip_infos");
    CHK_SMART_PTR_NULL(dlRaGetSocketVnicIpInfos);

    dlRaRaGetTlsEnable = (int(*)(struct ra_info*, bool *))HcclDlsym(handle_, "ra_get_tls_enable");
    if (dlRaRaGetTlsEnable == nullptr) {
        HCCL_WARNING("dlRaRaGetTlsEnable is nullptr, can not use ra_get_tls_enable");
    }

    dlRaSaveSnapShot = (int(*)(struct ra_info*, enum save_snapshot_action))HcclDlsym(handle_, "ra_save_snapshot");
    CHK_SMART_PTR_NULL(dlRaSaveSnapShot);

    dlRaRestoreSnapShot = (int(*)(struct ra_info*))HcclDlsym(handle_, "ra_restore_snapshot");
    CHK_SMART_PTR_NULL(dlRaRestoreSnapShot);
    return HCCL_SUCCESS;
}

HcclResult DlRaFunction::DlRaFunctionInit()
{
    std::lock_guard<std::mutex> lock(handleMutex_);
    if (handle_ == nullptr) {
        handle_ = HcclDlopen("libra.so", RTLD_NOW);
        const char* errMsg = dlerror();
        CHK_PRT_RET(handle_ == nullptr, HCCL_ERROR("dlopen [%s] failed, %s", "libra.so",\
            (errMsg == nullptr) ? "please check the file exist or permission denied." : errMsg),\
            HCCL_E_OPEN_FILE_FAILURE);
    }
    dlRaInit = (int(*)(struct ra_init_config*))HcclDlsym(handle_, "ra_init");
    CHK_SMART_PTR_NULL(dlRaInit);
    dlRaDeInit = (int(*)(struct ra_init_config*))HcclDlsym(handle_, "ra_deinit");
    CHK_SMART_PTR_NULL(dlRaDeInit);
    CHK_RET(DlRaFunctionRdmaInit());
    CHK_RET(DlRaFunctionSocketInit());

    return HCCL_SUCCESS;
}
}
