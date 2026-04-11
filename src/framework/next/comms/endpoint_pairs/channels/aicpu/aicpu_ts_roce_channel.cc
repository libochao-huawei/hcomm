/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_roce_channel.h"

#include <algorithm>
#include <chrono>
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <securec.h>
#include "hcomm_c_adpt.h"
#include "log.h"
#include "../../../endpoints/endpoint.h"
#include "../../../endpoints/aicpu_ts_roce_endpoint.h"
#include "../../../endpoints/reged_mems/aicpu_ts_roce_mem.h"
#include "adapter_rts_common.h"
#include "channel_param.h"
#include "dispatcher_ctx.h"
#include "exception_handler.h"
#include "adapter_hccp_common.h"
#include "hccl_dispatcher_ctx.h"
#include "hccl_network.h"
#include "mem_device_pub.h"
#include "sal_pub.h"
#include "env_config.h"

namespace hcomm {

namespace {
constexpr uint32_t kDefaultRocePort = 16666;
constexpr uint8_t kHcommTrafficClassConfigNotSet = 0xff;
constexpr uint8_t kHcommServiceLevelConfigNotSet = 0xff;

HcclResult CommAddrToHcclIp(const CommAddr &ca, hccl::HcclIpAddress &out)
{
    if (ca.type == COMM_ADDR_TYPE_IP_V4) {
        out = hccl::HcclIpAddress(ca.addr);
        return HCCL_SUCCESS;
    }
    if (ca.type == COMM_ADDR_TYPE_IP_V6) {
        out = hccl::HcclIpAddress(ca.addr6);
        return HCCL_SUCCESS;
    }
    HCCL_ERROR("[AicpuTsRoceChannel] unsupported CommAddr type[%d]", ca.type);
    return HCCL_E_NOT_SUPPORT;
}

/** Lexicographic compare of GetReadableIP(); smaller string is client. Tie: devPhyId on DEVICE loc. */
HcclResult DecideLocalIsClientByEndpointIps(const EndpointDesc &local, const EndpointDesc &remote, bool &outLocalIsClient)
{
    hccl::HcclIpAddress localIp{};
    hccl::HcclIpAddress remoteIp{};
    CHK_RET(CommAddrToHcclIp(local.commAddr, localIp));
    CHK_RET(CommAddrToHcclIp(remote.commAddr, remoteIp));
    const std::string localStr(localIp.GetReadableIP());
    const std::string remoteStr(remoteIp.GetReadableIP());
    if (localStr < remoteStr) {
        outLocalIsClient = true;
    } else if (localStr > remoteStr) {
        outLocalIsClient = false;
    } else {
        HCCL_ERROR("[AicpuTsRoceChannel] same readable IP but loc not DEVICE; cannot decide socket role");
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

/** Owns res / local+remote RoceMemDetails arrays as separate device allocations for AICPU kernel blob. */
struct RoceAicpuKernelDeviceBlobBundle {
    hccl::DeviceMem resAlloc{};
    hccl::DeviceMem localAlloc{};
    hccl::DeviceMem remoteAlloc{};
};

/** Same semantics as HcclSocketManager::WaitLinkEstablish for a single client socket after Connect(). */
HcclResult WaitClientSocketLinkEstablished(const std::shared_ptr<hccl::HcclSocket> &socket, s32 timeoutSec)
{
    CHK_SMART_PTR_NULL(socket);
    u32 pollCount = 0;
    const auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(timeoutSec > 0 ? timeoutSec : GetExternalInputHcclLinkTimeOut());
    HCCL_DEBUG("[AicpuTsRoceChannel][client][WaitLink] waiting for socket link up...");
    while (true) {
        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[AicpuTsRoceChannel][client][WaitLink] wait socket establish timeout, timeout[%lld s]",
                static_cast<long long>(timeout.count()));
            socket->SetStatus(hccl::HcclSocketStatus::SOCKET_TIMEOUT);
            return HCCL_E_TIMEOUT;
        }
        const hccl::HcclSocketStatus status = socket->GetStatus();
        if (status == hccl::HcclSocketStatus::SOCKET_OK) {
            HCCL_DEBUG("[AicpuTsRoceChannel][client][WaitLink] socket established. localIp[%s], remoteIp[%s]",
                socket->GetLocalIp().GetReadableIP(), socket->GetRemoteIp().GetReadableIP());
            return HCCL_SUCCESS;
        }
        if (status == hccl::HcclSocketStatus::SOCKET_CONNECTING) {
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
            if (pollCount % 50U == 0U) {
                HCCL_DEBUG("[AicpuTsRoceChannel][client][WaitLink] socket is connecting");
            }
            ++pollCount;
            continue;
        }
        if (status == hccl::HcclSocketStatus::SOCKET_TIMEOUT) {
            return HCCL_E_TIMEOUT;
        }
        socket->SetStatus(hccl::HcclSocketStatus::SOCKET_ERROR);
        return HCCL_E_TCP_CONNECT;
    }
}
} // namespace

HcclResult AicpuTsRoceChannel::BuildSocketTagName(std::string &outTag) const
{
    hccl::HcclIpAddress localIp{};
    hccl::HcclIpAddress remoteIp{};
    CHK_RET(CommAddrToHcclIp(localEp_.commAddr, localIp));
    CHK_RET(CommAddrToHcclIp(remoteEp_.commAddr, remoteIp));
    const std::string clientStr(isLocalIpClient_ ? localIp.GetReadableIP() : remoteIp.GetReadableIP());
    const std::string serverStr(isLocalIpClient_ ? remoteIp.GetReadableIP() : localIp.GetReadableIP());
    const uint32_t port = channelDesc_.port != 0 ? channelDesc_.port : kDefaultRocePort;
    outTag = clientStr + "_" + serverStr + ":" + std::to_string(port) + "_" + std::to_string(channelDesc_.common.index);
    if (outTag.size() + 1U > SOCK_CONN_TAG_SIZE) {
        HCCL_ERROR("[AicpuTsRoceChannel] socketTag too long (max %u bytes)", static_cast<unsigned int>(SOCK_CONN_TAG_SIZE - 1U));
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

AicpuTsRoceChannel::AicpuTsRoceChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc)
{}

AicpuTsRoceChannel::~AicpuTsRoceChannel()
{
    transport_.reset();
    if (ownsDispatcherCtx_ && dispatcherCtx_ != nullptr) {
        (void)DestroyDispatcherCtx(dispatcherCtx_, dispatcherCommId_.c_str());
        dispatcherCtx_ = nullptr;
        ownsDispatcherCtx_ = false;
    }
    dataSocket_.reset();
}

HcclResult AicpuTsRoceChannel::ParseInputParam()
{
    auto *localEpPtr = reinterpret_cast<Endpoint *>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    CHK_PTR_NULL(rdmaHandle_);

    remoteEp_ = channelDesc_.remoteEndpoint;
    if (channelDesc_.role == HCOMM_SOCKET_ROLE_CLIENT) {
        isLocalIpClient_ = true;
    } else if (channelDesc_.role == HCOMM_SOCKET_ROLE_SERVER) {
        isLocalIpClient_ = false;
    } else {
        if (channelDesc_.role != HCOMM_SOCKET_ROLE_RESERVED) {
            HCCL_WARNING("[AicpuTsRoceChannel] unexpected channelDesc.role[%d]; "
                "using inner logic to decide socket role based on endpoint IPs",
                static_cast<int>(channelDesc_.role));
        }
        CHK_RET(DecideLocalIsClientByEndpointIps(localEp_, remoteEp_, isLocalIpClient_));
    }
    HCCL_INFO("[AicpuTsRoceChannel][%s] ParseInputParam start", SocketRoleTag());

    notifyNum_ = channelDesc_.notifyNum;
    if (notifyNum_ != 0) {
        HCCL_WARNING("[AicpuTsRoceChannel][%s] channelDesc.notifyNum[%u] ignored; transport uses notifyNum=0 for now.",
            SocketRoleTag(), notifyNum_);
    }

    // Handles from AicpuTsRoceRegedMemMgr: platform hccl::LocalRdmaRmaBuffer.
    if (channelDesc_.exchangeAllMems) {
        HcclBuf *bufArr = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(HcommMemGetAllMemHandles(
            endpointHandle_, reinterpret_cast<void **>(&bufArr), &memHandleNum)));
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            auto *rdmaBuf = static_cast<hccl::LocalRdmaRmaBuffer *>(bufArr[i].handle);
            CHK_PTR_NULL(rdmaBuf);
            regMems_.push_back(AicpuTsRoceMemView{reinterpret_cast<uintptr_t>(rdmaBuf->GetAddr()),
                static_cast<std::size_t>(rdmaBuf->GetSize()), ""});
        }
    } else {
        CHK_PTR_NULL(channelDesc_.memHandles);
        for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
            CHK_PTR_NULL(channelDesc_.memHandles[i]);
            auto *rdmaBuf = reinterpret_cast<hccl::LocalRdmaRmaBuffer *>(channelDesc_.memHandles[i]);
            regMems_.push_back(AicpuTsRoceMemView{reinterpret_cast<uintptr_t>(rdmaBuf->GetAddr()),
                static_cast<std::size_t>(rdmaBuf->GetSize()), ""});
        }
    }
    HCCL_INFO("[AicpuTsRoceChannel][%s] ParseInputParam done, regMemCount[%zu]", SocketRoleTag(), regMems_.size());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildDataSocket()
{
    HCCL_INFO("[AicpuTsRoceChannel][%s] BuildDataSocket start", SocketRoleTag());
    auto *roceEp = dynamic_cast<AicpuTsRoceEndpoint *>(reinterpret_cast<Endpoint *>(endpointHandle_));
    CHK_PTR_NULL(roceEp);

    HcclNetDevCtx netDevCtx = static_cast<HcclNetDevCtx>(roceEp->GetNetDev());
    CHK_PTR_NULL(netDevCtx);

    auto *netDevCtxPtr = static_cast<hccl::NetDevContext *>(netDevCtx);
    machinePara_.localIpAddr = netDevCtxPtr->GetLocalIp();

    hccl::HcclIpAddress remoteIp{};
    CHK_RET(CommAddrToHcclIp(remoteEp_.commAddr, remoteIp));

    uint32_t port = channelDesc_.port != 0 ? channelDesc_.port : kDefaultRocePort;
    std::string socketTag;
    CHK_RET(BuildSocketTagName(socketTag));

    HCCL_INFO("[AicpuTsRoceChannel][%s] BuildDataSocket localIp[%s] remoteIp[%s] port[%u] socketTag[%s]",
        SocketRoleTag(), machinePara_.localIpAddr.GetReadableIP(), remoteIp.GetReadableIP(), port, socketTag.c_str());

    if (isLocalIpClient_) {
        HCCL_INFO("[AicpuTsRoceChannel][client] BuildDataSocket connect to server");
        EXECEPTION_CATCH(dataSocket_ = std::make_shared<hccl::HcclSocket>(socketTag, netDevCtx, remoteIp, port,
                             hccl::HcclSocketRole::SOCKET_ROLE_CLIENT),
            return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(dataSocket_);
        CHK_RET(dataSocket_->Init());
        CHK_RET(dataSocket_->Connect());
        HcclResult waitRet = WaitClientSocketLinkEstablished(dataSocket_, 0);
        if (waitRet != HCCL_SUCCESS) {
            dataSocket_->Close();
            return waitRet;
        }
        HCCL_INFO("[AicpuTsRoceChannel][client] BuildDataSocket TCP link ready");
    } else {
        HCCL_INFO("[AicpuTsRoceChannel][server] BuildDataSocket listen and accept");
        SocketWlistInfo wlistEntry{};
        wlistEntry.connLimit = 1U;
        const auto bin = remoteIp.GetBinaryAddress();
        wlistEntry.remoteIp.addr = bin.addr;
        wlistEntry.remoteIp.addr6 = bin.addr6;
        s32 mw = memcpy_s(wlistEntry.tag, sizeof(wlistEntry.tag), socketTag.c_str(), socketTag.size() + 1U);
        CHK_PRT_RET(mw != EOK, HCCL_ERROR("[AicpuTsRoceChannel][%s] memcpy_s whitelist tag failed", SocketRoleTag()),
            HCCL_E_MEMORY);
        const std::vector<SocketWlistInfo> wlistVec = { wlistEntry };
        CHK_RET(AicpuTsRoceEndpoint::AddListenSocketWhiteList(port, wlistVec));
        CHK_RET(AicpuTsRoceEndpoint::AcceptDataSocket(port, socketTag, dataSocket_, 0));
        CHK_SMART_PTR_NULL(dataSocket_);
        HCCL_INFO("[AicpuTsRoceChannel][server] BuildDataSocket accepted client connection");
    }

    machinePara_.remoteIpAddr = remoteIp;
    machinePara_.localSocketPort = dataSocket_->GetLocalPort();
    machinePara_.remoteSocketPort = dataSocket_->GetRemotePort();
    HCCL_INFO("[AicpuTsRoceChannel][%s] BuildDataSocket done localPort[%u] remotePort[%u]",
        SocketRoleTag(), machinePara_.localSocketPort, machinePara_.remoteSocketPort);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildDispatcherAndTransport()
{
    const u32 devPhyId = static_cast<u32>(localEp_.loc.device.devPhyId);
    char commBuf[160];
    int nc = snprintf_s(commBuf, sizeof(commBuf), sizeof(commBuf) - 1U, "hcomm_roce_ch_%p", static_cast<void *>(this));
    CHK_PRT_RET(nc < 0, HCCL_ERROR("[AicpuTsRoceChannel] snprintf_s commId failed"), HCCL_E_INTERNAL);
    dispatcherCommId_.assign(commBuf);
    HCCL_INFO("[AicpuTsRoceChannel][%s] BuildDispatcherAndTransport commId[%s]", SocketRoleTag(), dispatcherCommId_.c_str());

    DispatcherCtxPtr ctx = nullptr;
    if (!FindDispatcherByCommId(&ctx, dispatcherCommId_.c_str())) {
        CHK_RET(CreateDispatcherCtx(&ctx, devPhyId, dispatcherCommId_.c_str()));
        ownsDispatcherCtx_ = true;
    } else {
        ownsDispatcherCtx_ = false;
    }
    dispatcherCtx_ = ctx;
    CHK_PTR_NULL(dispatcherCtx_);
    auto *dctx = static_cast<hccl::DispatcherCtx *>(dispatcherCtx_);
    const HcclDispatcher dispatcher = dctx->GetDispatcher();
    CHK_PTR_NULL(dispatcher);

    machinePara_.machineType = isLocalIpClient_ ? hccl::MachineType::MACHINE_CLIENT_TYPE
                                                 : hccl::MachineType::MACHINE_SERVER_TYPE;
    machinePara_.linkMode = hccl::LinkMode::LINK_DUPLEX_MODE;
    machinePara_.tag = dispatcherCommId_;
    machinePara_.localDeviceId = localEp_.loc.device.devPhyId;
    machinePara_.remoteDeviceId = remoteEp_.loc.device.devPhyId;
    CHK_RET(hrtGetDevice(&machinePara_.deviceLogicId));
    DevType devType = DevType::DEV_TYPE_COUNT;
    CHK_RET(hrtGetDeviceType(devType));
    machinePara_.deviceType = devType;
    machinePara_.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    machinePara_.userMemEnable = false;
    machinePara_.isIndOp = true;
    machinePara_.isAicpuModeEn = true;
    machinePara_.notifyNum = 0;
    machinePara_.sockets.clear();
    machinePara_.sockets.push_back(dataSocket_);
    if (channelDesc_.roceAttr.tc != kHcommTrafficClassConfigNotSet) {
        machinePara_.tc = channelDesc_.roceAttr.tc;
    } else {
        machinePara_.tc = EnvConfig::HCCL_RDMA_TC_DEFAULT;
    }
    if (channelDesc_.roceAttr.sl != kHcommServiceLevelConfigNotSet) {
        machinePara_.sl = channelDesc_.roceAttr.sl;
    } else {
        machinePara_.sl = EnvConfig::HCCL_RDMA_SL_DEFAULT;
    }

    transportPara_.timeout = std::chrono::milliseconds(120000);
    transportPara_.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    EXECEPTION_CATCH(
        transport_ = std::make_unique<hccl::Transport>(hccl::TransportType::TRANS_TYPE_IBV_EXP, transportPara_, dispatcher,
            notifyPoolHolder_, machinePara_),
        return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(transport_);
    HCCL_INFO("[AicpuTsRoceChannel][%s] Transport Init start", SocketRoleTag());
    HcclResult tr = transport_->Init();
    if (tr != HCCL_SUCCESS) {
        transport_.reset();
        return tr;
    }
    inited_ = true;
    HCCL_INFO("[AicpuTsRoceChannel][%s] BuildDispatcherAndTransport done, transport inited", SocketRoleTag());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::Init()
{
    HCCL_INFO("[AicpuTsRoceChannel] Init start");
    CHK_RET(ParseInputParam());
    CHK_RET(BuildDataSocket());
    CHK_RET(BuildDispatcherAndTransport());
    HCCL_INFO("[AicpuTsRoceChannel][%s] Init success", SocketRoleTag());
    return HCCL_SUCCESS;
}

HcommChannelKind AicpuTsRoceChannel::GetChannelKind() const
{
    return HcommChannelKind::AICPU_TS_ROCE;
}

HcclResult AicpuTsRoceChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(notifyNum);
    *notifyNum = notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memNum);
    CHK_SMART_PTR_NULL(transport_);
    HCCL_DEBUG("[AicpuTsRoceChannel][%s] GetRemoteMem", SocketRoleTag());
    CHK_RET(transport_->GetIndOpRemoteMem(remoteMem, memNum));
    if (memTags != nullptr && *memNum > 0 && *memNum <= regMems_.size()) {
        for (uint32_t i = 0; i < *memNum; ++i) {
            memTags[i] = const_cast<char *>(regMems_[i].tag.c_str());
        }
    }
    return HCCL_SUCCESS;
}

ChannelStatus AicpuTsRoceChannel::GetStatus()
{
    if (inited_) {
        return ChannelStatus::READY;
    }
    return ChannelStatus::INIT;
}

HcclResult AicpuTsRoceChannel::GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    (void)remoteMem;
    (void)memTag;
    (void)memNum;
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AicpuTsRoceChannel::Clean()
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::Resume()
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::PrepareAicpuKernelDeviceBlob(std::shared_ptr<hccl::DeviceMem> &out)
{
    out.reset();
    HCCL_INFO("[AicpuTsRoceChannel][%s] PrepareAicpuKernelDeviceBlob start", SocketRoleTag());
    CHK_PRT_RET(!inited_, HCCL_ERROR("[AicpuTsRoceChannel][%s][PrepareAicpuKernelDeviceBlob] channel not inited",
        SocketRoleTag()),
        HCCL_E_INTERNAL);

    std::vector<RoceMemDetails> localMd;
    std::vector<RoceMemDetails> remoteMd;
    std::vector<HcclQpInfoV2> aiQpInfos;
    u32 qpNum = 0;

    auto *ep = reinterpret_cast<Endpoint *>(endpointHandle_);
    CHK_PTR_NULL(ep);
    auto mgr = std::dynamic_pointer_cast<AicpuTsRoceRegedMemMgr>(ep->GetRegedMemMgr());
    CHK_SMART_PTR_NULL(mgr);
    CHK_RET(mgr->GetAllRoceMemDetails(localMd, remoteMd));
    CHK_RET(transport_->GetAiQpInfo(aiQpInfos));
    qpNum = static_cast<u32>(aiQpInfos.size());
    CHK_PRT_RET(qpNum > RDMA_QP_MAX_NUM || qpNum < 1U,
        HCCL_ERROR("[AicpuTsRoceChannel] bad qpNum[%u]", qpNum), HCCL_E_INTERNAL);

    const size_t nL = localMd.size();
    const size_t nR = remoteMd.size();
    CHK_PRT_RET(nL > 0U && nL > (SIZE_MAX / sizeof(RoceMemDetails)),
        HCCL_ERROR("[AicpuTsRoceChannel][PrepareAicpuKernelDeviceBlob] localMem count overflow"), HCCL_E_PARA);
    CHK_PRT_RET(nR > 0U && nR > (SIZE_MAX / sizeof(RoceMemDetails)),
        HCCL_ERROR("[AicpuTsRoceChannel][PrepareAicpuKernelDeviceBlob] remoteMem count overflow"), HCCL_E_PARA);
    const u64 localBytes = static_cast<u64>(nL * sizeof(RoceMemDetails));
    const u64 remoteBytes = static_cast<u64>(nR * sizeof(RoceMemDetails));
    CHK_PRT_RET(localBytes > static_cast<u64>(UINT32_MAX) || remoteBytes > static_cast<u64>(UINT32_MAX),
        HCCL_ERROR("[AicpuTsRoceChannel][PrepareAicpuKernelDeviceBlob] mem detail blob too large"), HCCL_E_PARA);

    RoceAicpuKernelDeviceBlobBundle bundle;
    EXECEPTION_CATCH(bundle.resAlloc = hccl::DeviceMem::alloc(sizeof(HcommRoceChannelRes)), return HCCL_E_PTR);
    CHK_PTR_NULL(bundle.resAlloc.ptr());
    if (nL > 0U) {
        EXECEPTION_CATCH(bundle.localAlloc = hccl::DeviceMem::alloc(localBytes), return HCCL_E_PTR);
        CHK_PTR_NULL(bundle.localAlloc.ptr());
        CHK_RET(hrtMemSyncCopy(bundle.localAlloc.ptr(),
            localBytes,
            localMd.data(),
            localBytes,
            HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
    }
    if (nR > 0U) {
        EXECEPTION_CATCH(bundle.remoteAlloc = hccl::DeviceMem::alloc(remoteBytes), return HCCL_E_PTR);
        CHK_PTR_NULL(bundle.remoteAlloc.ptr());
        CHK_RET(hrtMemSyncCopy(bundle.remoteAlloc.ptr(),
            remoteBytes,
            remoteMd.data(),
            remoteBytes,
            HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
    }

    HcommRoceChannelRes res{};
    res.localMemCount = static_cast<u32>(nL);
    res.remoteMemCount = static_cast<u32>(nR);
    res.localMem = nL > 0U ? bundle.localAlloc.ptr() : nullptr;
    res.remoteMem = nR > 0U ? bundle.remoteAlloc.ptr() : nullptr;
    res.chipId = LLONG_MAX;
    std::copy_n(aiQpInfos.begin(), static_cast<std::ptrdiff_t>(qpNum), res.QpInfo);
    res.qpsPerConnection = qpNum - static_cast<u32>(qpNum > 1U);

    CHK_RET(hrtMemSyncCopy(bundle.resAlloc.ptr(),
        sizeof(res),
        &res,
        sizeof(res),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    std::shared_ptr<RoceAicpuKernelDeviceBlobBundle> bundleKeep;
    EXECEPTION_CATCH(bundleKeep = std::make_shared<RoceAicpuKernelDeviceBlobBundle>(std::move(bundle)), return HCCL_E_PTR);

    hccl::DeviceMem *viewPtr = nullptr;
    EXECEPTION_CATCH(
        viewPtr = new hccl::DeviceMem(
            hccl::DeviceMem::create(bundleKeep->resAlloc.ptr(), sizeof(HcommRoceChannelRes))),
        return HCCL_E_PTR);

    out = std::shared_ptr<hccl::DeviceMem>(viewPtr, [bundleKeep](hccl::DeviceMem *p) {
        delete p;
    });
    HCCL_INFO("[AicpuTsRoceChannel][%s] PrepareAicpuKernelDeviceBlob done qpNum[%u] localMem[%zu] remoteMem[%zu]",
        SocketRoleTag(), qpNum, nL, nR);
    return HCCL_SUCCESS;
}
} // namespace hcomm
