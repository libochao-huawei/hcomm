/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_roce_channel.h"

#include "../../../endpoints/endpoint.h"
#include "../../../endpoints/aicpu_ts_roce_endpoint.h"
#include "../../../endpoints/reged_mems/aicpu_ts_roce_mem.h"
#include "adapter_rts_common.h"
#include "channel_param.h"
#include "dispatcher_ctx.h"
#include "exception_handler.h"
#include "hcomm_c_adpt.h"
#include "hccl_dispatcher_ctx.h"
#include "hccl_network.h"
#include "log.h"
#include "mem_device_pub.h"

#include <algorithm>
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <securec.h>

namespace hcomm {

namespace {
constexpr uint32_t kDefaultRocePort = 16666;

/** Owns res / local+remote RoceMemDetails arrays as separate device allocations for AICPU kernel blob. */
struct RoceAicpuKernelDeviceBlobBundle {
    hccl::DeviceMem resAlloc{};
    hccl::DeviceMem localAlloc{};
    hccl::DeviceMem remoteAlloc{};
};

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
} // namespace

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
    notifyNum_ = channelDesc_.notifyNum;
    if (notifyNum_ != 0) {
        HCCL_WARNING("[AicpuTsRoceChannel] channelDesc.notifyNum[%u] ignored; transport uses notifyNum=0 for now.",
            notifyNum_);
    }

    // Handles from AicpuTsRoceRegedMemMgr: platform hccl::LocalRdmaRmaBuffer.
    if (channelDesc_.exchangeAllMems) {
        HcclBuf *bufArr = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void **>(&bufArr), &memHandleNum));
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
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildDataSocket()
{
    auto *roceEp = dynamic_cast<AicpuTsRoceEndpoint *>(reinterpret_cast<Endpoint *>(endpointHandle_));
    CHK_PTR_NULL(roceEp);

    HcclNetDevCtx netDevCtx = static_cast<HcclNetDevCtx>(roceEp->GetNetDev());
    CHK_PTR_NULL(netDevCtx);

    auto *netDevCtxPtr = static_cast<hccl::NetDevContext *>(netDevCtx);
    machinePara_.localIpAddr = netDevCtxPtr->GetLocalIp();

    hccl::HcclIpAddress remoteIp{};
    CHK_RET(CommAddrToHcclIp(remoteEp_.commAddr, remoteIp));

    uint32_t port = channelDesc_.port != 0 ? channelDesc_.port : kDefaultRocePort;
    char tagBuf[128];
    int n = snprintf_s(tagBuf, sizeof(tagBuf), sizeof(tagBuf) - 1U, "hcomm_roce_%p", static_cast<void *>(this));
    CHK_PRT_RET(n < 0, HCCL_ERROR("[AicpuTsRoceChannel] snprintf_s tag failed"), HCCL_E_INTERNAL);
    const std::string socketTag(tagBuf);

    if (channelDesc_.role == HcommSocketRole::HCOMM_SOCKET_ROLE_CLIENT) {
        EXECEPTION_CATCH(dataSocket_ = std::make_shared<hccl::HcclSocket>(socketTag, netDevCtx, remoteIp, port,
                             hccl::HcclSocketRole::SOCKET_ROLE_CLIENT),
            return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(dataSocket_);
        CHK_RET(dataSocket_->Init());
        CHK_RET(dataSocket_->Connect());
    } else {
        CHK_RET(AicpuTsRoceEndpoint::AcceptDataSocket(port, socketTag, dataSocket_, 0));
        CHK_SMART_PTR_NULL(dataSocket_);
    }

    machinePara_.remoteIpAddr = remoteIp;
    machinePara_.localSocketPort = dataSocket_->GetLocalPort();
    machinePara_.remoteSocketPort = dataSocket_->GetRemotePort();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildDispatcherAndTransport()
{
    const u32 devPhyId = static_cast<u32>(localEp_.loc.device.devPhyId);
    char commBuf[160];
    int nc = snprintf_s(commBuf, sizeof(commBuf), sizeof(commBuf) - 1U, "hcomm_roce_ch_%p", static_cast<void *>(this));
    CHK_PRT_RET(nc < 0, HCCL_ERROR("[AicpuTsRoceChannel] snprintf_s commId failed"), HCCL_E_INTERNAL);
    dispatcherCommId_.assign(commBuf);

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

    machinePara_.machineType = (channelDesc_.role == HcommSocketRole::HCOMM_SOCKET_ROLE_CLIENT)
        ? hccl::MachineType::MACHINE_CLIENT_TYPE
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
    if (channelDesc_.roceAttr.tc != HCCL_COMM_TRAFFIC_CLASS_CONFIG_NOT_SET) {
        machinePara_.tc = channelDesc_.roceAttr.tc;
    }
    if (channelDesc_.roceAttr.sl != HCCL_COMM_SERVICE_LEVEL_CONFIG_NOT_SET) {
        machinePara_.sl = channelDesc_.roceAttr.sl;
    }

    transportPara_.timeout = std::chrono::milliseconds(120000);
    transportPara_.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    EXECEPTION_CATCH(
        transport_ = std::make_unique<hccl::Transport>(hccl::TransportType::TRANS_TYPE_IBV_EXP, transportPara_, dispatcher,
            notifyPoolHolder_, machinePara_),
        return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(transport_);
    HcclResult tr = transport_->Init();
    if (tr != HCCL_SUCCESS) {
        transport_.reset();
        return tr;
    }
    inited_ = true;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::Init()
{
    CHK_RET(ParseInputParam());
    CHK_RET(BuildDataSocket());
    CHK_RET(BuildDispatcherAndTransport());
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

HcclResult AicpuTsRoceChannel::PrepareAicpuKernelDeviceBlob(std::shared_ptr<hccl::DeviceMem> &out)
{
    out.reset();
    CHK_PRT_RET(!inited_, HCCL_ERROR("[AicpuTsRoceChannel][PrepareAicpuKernelDeviceBlob] channel not inited"),
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
    return HCCL_SUCCESS;
}
} // namespace hcomm
