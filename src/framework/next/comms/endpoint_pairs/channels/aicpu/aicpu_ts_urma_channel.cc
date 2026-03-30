/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_urma_channel.h"
#include "../../../endpoints/endpoint.h"
#include "orion_adpt_utils.h"

// Orion
#include "coll_alg_param.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "aicpu_res_package_helper.h"

namespace hcomm {

AicpuTsUrmaChannel::AicpuTsUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc):
    endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HcclResult AicpuTsUrmaChannel::ParseInputParam() 
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    // TODO: 使用 HcommEndpointGet
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();

    HCCL_INFO("[%s] localProtocol[%d]", __func__, localEp_.protocol);

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    notifyNum_ = channelDesc_.notifyNum;

    // 3. 从 channelDesc 的 memHandle，获得 bufs_

    // TODO: memHandle 强转成 Hccl::LocalUbRmaBuffer*, push_back 进去 commLocRes_.bufferVec

    for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
        Hccl::LocalUbRmaBuffer *localUbRmaBuffer = reinterpret_cast<Hccl::LocalUbRmaBuffer *>(channelDesc_.memHandles[i]);
        bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
            reinterpret_cast<uintptr_t>(localUbRmaBuffer->GetAddr()), localUbRmaBuffer->GetSize(), localUbRmaBuffer->GetBuf()->GetMemTag().c_str())
        ));
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::BuildAttr()
{
    attr_.devicePhyId = localEp_.loc.device.devPhyId;
    attr_.opMode      = Hccl::OpMode::OPBASE;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::BuildConnection()
{
    Hccl::OpMode        opMode = Hccl::OpMode::OPBASE;
    bool                devUsed  = true;  // aicpu 为 true
    Hccl::LinkProtocol  protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));
    
    Hccl::IpAddress     locAddr;
    Hccl::IpAddress     rmtAddr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtAddr));

    std::unique_ptr<Hccl::DevUbConnection> ubConn = nullptr;
    switch (protocol) {
        case Hccl::LinkProtocol::UB_TP:
            EXECEPTION_CATCH(
                ubConn = std::make_unique<Hccl::DevUbTpConnection>(rdmaHandle_, locAddr, rmtAddr, opMode, devUsed),
                return HCCL_E_PTR
            );
            break;
        case Hccl::LinkProtocol::UB_CTP:
            EXECEPTION_CATCH(
                ubConn = std::make_unique<Hccl::DevUbCtpConnection>(rdmaHandle_, locAddr, rmtAddr, opMode, devUsed),
                return HCCL_E_PTR
            );
            break;
        default:
            HCCL_ERROR("%s No LinkProtocol to match", __func__);
            break;
    }
    CHK_SMART_PTR_NULL(ubConn);

    commonRes_.connVec.clear();
    commonRes_.connVec.emplace_back(ubConn.get());
    connections_.clear();
    connections_.push_back(std::move(ubConn));

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::BuildNotify()
{
    localNotifies_.clear();
    commonRes_.notifyVec.clear();
    bool devUsed = true;
    for (uint32_t i = 0; i < notifyNum_; ++i) {
        std::unique_ptr<Hccl::UbLocalNotify> notifyPtr = nullptr;
        EXECEPTION_CATCH(
            notifyPtr = std::make_unique<Hccl::UbLocalNotify>(rdmaHandle_, devUsed),
            return HCCL_E_PTR
        );
        commonRes_.notifyVec.push_back(notifyPtr.get());
        localNotifies_.push_back(std::move(notifyPtr));
    }
    return HCCL_SUCCESS;
}

// TODO: to be deleted
HcclResult AicpuTsUrmaChannel::BuildBuffer()
{
    localRmaBuffers_.clear();
    commonRes_.bufferVec.clear();
    for (size_t i = 0; i < bufs_.size(); i++) {
        std::unique_ptr<Hccl::LocalUbRmaBuffer> bufferPtr = nullptr;
        EXECEPTION_CATCH(
            bufferPtr = std::make_unique<Hccl::LocalUbRmaBuffer>(bufs_[i], rdmaHandle_),
            return HCCL_E_PTR
        );
        commonRes_.bufferVec.push_back(bufferPtr.get());
        localRmaBuffers_.push_back(std::move(bufferPtr));
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::BuildUbMemTransport()
{
    Hccl::BaseMemTransport::LocCntNotifyRes locCntNotifyRes{};
    locCntNotifyRes.vec.clear();
    locCntNotifyRes.desc.clear();
    const Hccl::Socket &socket = *socket_;

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));

    // make_unique / make_shared / release 包一层抛异常的宏
    EXECEPTION_CATCH(
        memTransport_ = std::make_unique<Hccl::UbMemTransport>(
            commonRes_, attr_, linkData, socket, rdmaHandle_, locCntNotifyRes
        ),
        return HCCL_E_PTR
    );
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::Init()
{
    /*
        Argue result: make_unique 配合一场捕获的宏 EXCEPTION CATCH
        Attention: const 和引用
    */
    // TODO: 处理抛异常
    CHK_RET(ParseInputParam());
    CHK_RET(BuildAttr());
    CHK_RET(BuildConnection());
    CHK_RET(BuildNotify());
    CHK_RET(BuildBuffer());
    CHK_RET(BuildUbMemTransport());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    *notifyNum = this->notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    return memTransport_->GetRemoteMem(remoteMem, memNum, memTags);
}

ChannelStatus AicpuTsUrmaChannel::GetStatus()
{
    Hccl::TransportStatus transportStatus = memTransport_->GetStatus();
    ChannelStatus out = ChannelStatus::INIT;
    switch (transportStatus) {
        case Hccl::TransportStatus::INIT:
            out = ChannelStatus::INIT;
            break;
        case Hccl::TransportStatus::SOCKET_OK:
            out = ChannelStatus::SOCKET_OK;
            break;
        case Hccl::TransportStatus::SOCKET_TIMEOUT:
            out = ChannelStatus::SOCKET_TIMEOUT;
            break;
        case Hccl::TransportStatus::READY:
            out = ChannelStatus::READY;
            break;
        default:
            HCCL_ERROR("[AicpuTsUrmaChannel][%s] Invalid TransportStatus[%d]", __func__, transportStatus);
            out = ChannelStatus::INVALID;
            break;
    }
    return out;
}

HcclResult SetModuleDataName(Hccl::ModuleData &module, const std::string &name)
{
    int ret = strcpy_s(module.name, sizeof(module.name), name.c_str());
    if (ret != 0) {
        HCCL_ERROR("[SetModuleDataName] strcpy_s name %s failed", name.c_str());
        return HCCL_E_INTERNAL;
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::PackOpData(std::vector<char> &data)
{
    std::vector<Hccl::ModuleData> dataVec;
    dataVec.resize(Hccl::AicpuResMgrType::__COUNT__);

    Hccl::AicpuResMgrType resType = Hccl::AicpuResMgrType::STREAM;
    CHK_RET(SetModuleDataName(dataVec[resType], "UbMemTransport"));

    std::vector<char> result;
    Hccl::BinaryStream      binaryStream;
    binaryStream << memTransport_->GetUniqueIdV2();

    binaryStream.Dump(result);

    dataVec[resType].data = result;

    Hccl::AicpuResPackageHelper helper;
    data = helper.GetPackedData(dataVec);

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUrmaChannel::H2DResPack(std::vector<char>& buffer)
{
    CHK_RET(PackOpData(buffer));
    HCCL_INFO("[AicpuTsUrmaChannelImpl][%s] Pack Buffer data[%p], Pack Buffer size[%zu].",
        __func__, buffer.data(), buffer.size());
    return HCCL_SUCCESS;
}


} // namespace hcomm