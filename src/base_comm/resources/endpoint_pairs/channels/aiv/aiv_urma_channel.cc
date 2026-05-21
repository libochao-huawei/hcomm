/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_urma_channel.h"
#include "endpoint.h"
#include "orion_adpt_utils.h"
#include "comm_mems.h"

#include "hcomm_c_adpt.h"
#include "exception_handler.h"

// Orion
#include "coll_alg_param.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "aicpu_res_package_helper.h"
#include "tp_manager.h"
#include "orion_adapter_hccp.h"
#include "acl/acl_rt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace hcomm {
constexpr uint32_t QUEUE_NUM_MAX = 20;

namespace {
constexpr size_t AIV_URMA_ENTITY_ALIGN_SIZE = 64;
constexpr size_t QUEUE_INDEX_MEM_UNIT_SIZE = sizeof(void *);

struct DeviceEntitySection {
    size_t offset{0};
    size_t size{0};
};

class AclDeviceSlabGuard {
public:
    AclDeviceSlabGuard() = default;
    ~AclDeviceSlabGuard()
    {
        if (ptr_ != nullptr) {
            aclError ret = aclrtFree(ptr_);
            if (ret != ACL_SUCCESS) {
                HCCL_WARNING("[AclDeviceSlabGuard] aclrtFree failed, ptr[%p], size[%zu], ret[%d]", ptr_, size_, ret);
            }
        }
    }

    void Reset(void *ptr, size_t size)
    {
        ptr_ = ptr;
        size_ = size;
    }

    void *Release()
    {
        void *ptr = ptr_;
        ptr_ = nullptr;
        size_ = 0;
        return ptr;
    }

private:
    void *ptr_{nullptr};
    size_t size_{0};
};

HcclResult SecureMemset(void *dest, size_t destMax, int value, size_t count, const char *fieldName)
{
    if (dest == nullptr) {
        HCCL_ERROR("[SecureMemset] dest is nullptr, field[%s]", fieldName);
        return HCCL_E_PTR;
    }
    if (count > destMax) {
        HCCL_ERROR("[SecureMemset] invalid size, field[%s], count[%zu], destMax[%zu]",
            fieldName, count, destMax);
        return HCCL_E_PARA;
    }

    errno_t ret = memset_s(dest, destMax, value, count);
    if (ret != EOK) {
        HCCL_ERROR("[SecureMemset] memset_s failed, field[%s], ret[%d], count[%zu], destMax[%zu]",
            fieldName, ret, count, destMax);
        return HCCL_E_MEMORY;
    }
    return HCCL_SUCCESS;
}

size_t AlignUp(size_t value, size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

HcclResult AddDeviceEntitySection(size_t elemSize, uint32_t elemNum, size_t &offset, DeviceEntitySection &section,
    const char *sectionName)
{
    section.offset = AlignUp(offset, AIV_URMA_ENTITY_ALIGN_SIZE);
    if (elemNum == 0) {
        section.size = 0;
        offset = section.offset;
        return HCCL_SUCCESS;
    }
    CHK_PRT_RET(elemSize != 0 && elemNum > (SIZE_MAX / elemSize),
        HCCL_ERROR("[AivUrmaChannel::AddDeviceEntitySection] %s size overflow, elemSize[%zu], elemNum[%u]",
            sectionName, elemSize, elemNum), HCCL_E_PARA);
    section.size = elemSize * static_cast<size_t>(elemNum);
    CHK_PRT_RET(section.offset > (SIZE_MAX - section.size),
        HCCL_ERROR("[AivUrmaChannel::AddDeviceEntitySection] %s offset overflow, offset[%zu], size[%zu]",
            sectionName, section.offset, section.size), HCCL_E_PARA);
    offset = section.offset + section.size;
    return HCCL_SUCCESS;
}

void *GetSlabPtr(void *base, const DeviceEntitySection &section)
{
    if (section.size == 0) {
        return nullptr;
    }
    return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(base) + section.offset);
}

template <typename T>
HcclResult CopyArrayToSlab(void *slabBase, const T *hostArray, uint32_t arrayNum, const DeviceEntitySection &section,
    T **deviceArrayPtr, const char *arrayName)
{
    CHK_PTR_NULL(deviceArrayPtr);
    if (arrayNum == 0 || hostArray == nullptr) {
        CHK_PRT_RET(arrayNum != 0,
            HCCL_ERROR("[AivUrmaChannel::CopyArrayToSlab] %s hostArray is nullptr, num[%u]",
                arrayName, arrayNum), HCCL_E_PTR);
        *deviceArrayPtr = nullptr;
        return HCCL_SUCCESS;
    }
    CHK_PRT_RET(section.size != static_cast<size_t>(arrayNum) * sizeof(T),
        HCCL_ERROR("[AivUrmaChannel::CopyArrayToSlab] %s size mismatch, sectionSize[%zu], expect[%zu]",
            arrayName, section.size, static_cast<size_t>(arrayNum) * sizeof(T)), HCCL_E_PARA);
    void *sectionPtr = GetSlabPtr(slabBase, section);
    CHK_PTR_NULL(sectionPtr);
    Hccl::HrtMemcpy(sectionPtr, section.size, hostArray, section.size,
        Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);
    *deviceArrayPtr = reinterpret_cast<T *>(sectionPtr);
    HCCL_INFO("[AivUrmaChannel::CopyArrayToSlab] %s: host[%p] -> dev[%p], num[%u], size[%zu]",
        arrayName, hostArray, sectionPtr, arrayNum, section.size);
    return HCCL_SUCCESS;
}
} // namespace

AivUrmaChannel::AivUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc)
    : endpointHandle_(endpointHandle),
      channelDesc_(channelDesc)
{
    channelKind_ = HcommChannelKind::AIV_URMA;
}

AivUrmaChannel::~AivUrmaChannel()
{
    PutSocketIfNeeded();
    ReleaseDeviceChannelEntity();
}

void AivUrmaChannel::PutSocketIfNeeded()
{
    if (socket_ == nullptr) {
        return;
    }
    if (socketConfig_ == nullptr) {
        socket_ = nullptr;
        return;
    }
    (void)SocketMgr::GetInstance(devicePhyId_).PutSocket(socketConfig_, socket_);
    socket_ = nullptr;
}

void AivUrmaChannel::ReleaseDeviceChannelEntity()
{
    if (devChannelEntitySlab_ != nullptr) {
        aclError ret = aclrtFree(devChannelEntitySlab_);
        if (ret != ACL_SUCCESS) {
            HCCL_WARNING("[AivUrmaChannel::%s] aclrtFree devChannelEntitySlab failed, ptr[%p], size[%zu], ret[%d]",
                __func__, devChannelEntitySlab_, devChannelEntitySlabSize_, ret);
        }
        devChannelEntitySlab_ = nullptr;
        devChannelEntitySlabSize_ = 0;
    }
    deviceMemories_.clear();
    devChannelEntity_ = nullptr;
}

HcclResult AivUrmaChannel::Makebufs(HcommMemHandle *memHandles, uint32_t memHandleNum,
    std::vector<std::shared_ptr<Hccl::Buffer>> &bufs)
{
    bufs.clear();
    for (uint32_t i = 0; i < memHandleNum; ++i) {
        auto locMemInfo = reinterpret_cast<CommMemInfo *>(memHandles[i]);
        HCCL_INFO("[AivUrmaChannel][%s] tag[%s]", __func__, locMemInfo->memTag);
        bufs.emplace_back(std::move(std::make_shared<Hccl::Buffer>(reinterpret_cast<uintptr_t>(locMemInfo->mem.addr),
            locMemInfo->mem.size, hccl::ConvertCommToHcclMemType(locMemInfo->mem.type), locMemInfo->memTag)));
    }
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::ParseInputParam() 
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    Endpoint *localEpPtr = reinterpret_cast<Endpoint *>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    devicePhyId_ = localEp_.loc.device.devPhyId;

    socket_ = reinterpret_cast<Hccl::Socket *>(channelDesc_.socket);
    remoteEp_ = channelDesc_.remoteEndpoint;
    notifyNum_ = channelDesc_.notifyNum;
    // 3. 从 channelDesc 的 memHandle，获得 bufs_
    if (channelDesc_.exchangeAllMems) {
        HCCL_INFO("[AivUrmaChannel][%s] exchangeAllMems == true. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalUbRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(
            HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void **>(&memHandles), &memHandleNum)));
        HCCL_INFO("[AivUrmaChannel][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        bufs_.clear();
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalUbRmaBuffer> &localUbRmaBuffer = memHandles[i];
            HCCL_INFO("[AivUrmaChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(),
                localUbRmaBuffer->GetBuf()->GetMemTag().c_str());
            bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
                localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(),
                localUbRmaBuffer->GetBuf()->GetMemTag().c_str())));
        }
    } else {
        HCCL_INFO("[AivUrmaChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        CHK_RET(Makebufs(channelDesc_.memHandles, channelDesc_.memHandleNum, bufs_));
    }

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AivUrmaChannel][%s] socket ptr is NULL, rebuildSocket", __func__);
    
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, ipAddr));
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(static_cast<Hccl::RankId>(localEp_.loc.device.devPhyId), type, 0, ipAddr);
    Hccl::SocketHandle socketHandle
        = Hccl::SocketHandleManager::GetInstance().Create(localEp_.loc.device.devPhyId, localPort);
    EXECEPTION_CATCH(serverSocket_ = std::make_unique<Hccl::Socket>(socketHandle, ipAddr, 60001, ipAddr, "server",
                         Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE),
        return HCCL_E_PARA);
    HCCL_INFO("[AivUrmaChannel][%s] listen_socket_info[%s]", __func__, serverSocket_->Describe().c_str());
    EXECEPTION_CATCH(serverSocket_->Listen(), return HCCL_E_INTERNAL);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AivUrmaChannel][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    bool noRankId = true;
    EXECEPTION_CATCH(socketConfigHolder_ = std::make_unique<Hccl::SocketConfig>(linkData, socketTag, noRankId),
        return HCCL_E_PTR);
    socketConfig_ = socketConfigHolder_.get();
    CHK_RET(SocketMgr::GetInstance(devicePhyId_).GetSocket(*socketConfigHolder_, socket_));

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildAttr()
{
    attr_.devicePhyId = localEp_.loc.device.devPhyId;
    attr_.opMode = Hccl::OpMode::OPBASE;
    attr_.opAcceState = Hccl::AcceleratorState::AIV;
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildConnection()
{
    Hccl::OpMode opMode = Hccl::OpMode::OPBASE;
    bool devUsed = true;
    Hccl::HrtUbJfcMode jfcMode = Hccl::HrtUbJfcMode::USER_CTL;
    Hccl::LinkProtocol protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));

    Hccl::IpAddress locAddr;
    Hccl::IpAddress rmtAddr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtAddr));

    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    Hccl::TpManager::GetInstance(deviceLogicId).Init();

    std::unique_ptr<Hccl::DevUbConnection> ubConn = nullptr;
    switch (protocol) {
        case Hccl::LinkProtocol::UB_TP:
            EXECEPTION_CATCH(ubConn = std::make_unique<Hccl::DevUbTpConnection>(
                                 rdmaHandle_, locAddr, rmtAddr, opMode, devUsed, jfcMode),
                return HCCL_E_PTR);
            break;
        case Hccl::LinkProtocol::UB_CTP:
            EXECEPTION_CATCH(ubConn = std::make_unique<Hccl::DevUbCtpConnection>(
                                 rdmaHandle_, locAddr, rmtAddr, opMode, devUsed, jfcMode),
                return HCCL_E_PTR);
            break;
        default:
            HCCL_ERROR("%s No LinkProtocol to match", __func__);
            break;
    }
    CHK_SMART_PTR_NULL(ubConn);

    commonRes_.connVec.clear();
    connections_.clear();
    commonRes_.connVec.emplace_back(ubConn.get());
    connections_.push_back(std::move(ubConn));

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs)
{
    HCCL_INFO("AivUrmaChannel BuildBuffer start size[%u].", bufs.size());
    for (size_t i = 0; i < bufs.size(); i++) {
        std::unique_ptr<Hccl::LocalUbRmaBuffer> bufferPtr = nullptr;
        EXECEPTION_CATCH(bufferPtr = std::make_unique<Hccl::LocalUbRmaBuffer>(bufs[i], rdmaHandle_), return HCCL_E_PTR);
        commonRes_.bufferVec.push_back(bufferPtr.get());
        localRmaBuffers_.push_back(std::move(bufferPtr));
    }
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildAivUrmaTransport()
{

    const Hccl::Socket &socket = *socket_;

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));

    // make_unique / make_shared / release 包一层抛异常的宏
    EXECEPTION_CATCH(transport_ = std::make_unique<Hccl::AivUrmaTransport>(
                         commonRes_, attr_, linkData, socket, rdmaHandle_), // 这里区分是否是优先recv
        return HCCL_E_PTR);
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildChannelEntityToDevice(void **devChannelPtr)
{
    if (devChannelPtr == nullptr) {
        HCCL_ERROR("[AivUrmaChannel] BuildChannelEntityToDevice devChannelPtr is nullptr");
        return HCCL_E_PTR;
    }

    CHK_PTR_NULL(transport_.get());

    ChannelEntity hostChannel;
    CHK_RET(SecureMemset(&hostChannel, sizeof(ChannelEntity), 0, sizeof(ChannelEntity), "hostChannel"));

    transport_->PrepareHostChannelEntity(&hostChannel);

    DeviceEntitySection entitySection{0, sizeof(ChannelEntity)};
    DeviceEntitySection localNotifySection;
    DeviceEntitySection remoteNotifySection;
    DeviceEntitySection localBufferSection;
    DeviceEntitySection remoteBufferSection;
    DeviceEntitySection sqContextSection;
    DeviceEntitySection cqContextSection;
    DeviceEntitySection sqPiSection;
    DeviceEntitySection sqCiSection;
    DeviceEntitySection cqPiSection;
    DeviceEntitySection cqCiSection;

    size_t slabSize = AlignUp(sizeof(ChannelEntity), AIV_URMA_ENTITY_ALIGN_SIZE);
    CHK_RET(AddDeviceEntitySection(sizeof(RegedNotifyEntity), hostChannel.localNotifyNum, slabSize,
        localNotifySection, "localNotifyAddr"));
    CHK_RET(AddDeviceEntitySection(sizeof(RegedNotifyEntity), hostChannel.remoteNotifyNum, slabSize,
        remoteNotifySection, "remoteNotifyAddr"));
    CHK_RET(AddDeviceEntitySection(sizeof(RegedBufferEntity), hostChannel.localBufferNum, slabSize,
        localBufferSection, "localBufferAddr"));
    CHK_RET(AddDeviceEntitySection(sizeof(RegedBufferEntity), hostChannel.remoteBufferNum, slabSize,
        remoteBufferSection, "remoteBufferAddr"));
    CHK_RET(AddDeviceEntitySection(sizeof(SqContext), hostChannel.sqNum, slabSize, sqContextSection,
        "sqContextAddr"));
    CHK_RET(AddDeviceEntitySection(sizeof(CqContext), hostChannel.cqNum, slabSize, cqContextSection,
        "cqContextAddr"));
    CHK_RET(AddDeviceEntitySection(QUEUE_INDEX_MEM_UNIT_SIZE, hostChannel.sqNum, slabSize, sqPiSection,
        "sqPiAddr"));
    CHK_RET(AddDeviceEntitySection(QUEUE_INDEX_MEM_UNIT_SIZE, hostChannel.sqNum, slabSize, sqCiSection,
        "sqCiAddr"));
    CHK_RET(AddDeviceEntitySection(QUEUE_INDEX_MEM_UNIT_SIZE, hostChannel.cqNum, slabSize, cqPiSection,
        "cqPiAddr"));
    CHK_RET(AddDeviceEntitySection(QUEUE_INDEX_MEM_UNIT_SIZE, hostChannel.cqNum, slabSize, cqCiSection,
        "cqCiAddr"));
    slabSize = AlignUp(slabSize, AIV_URMA_ENTITY_ALIGN_SIZE);

    void *slabPtr = nullptr;
    aclError aclRet = aclrtMalloc(&slabPtr, slabSize, static_cast<aclrtMemMallocPolicy>(ACL_MEM_MALLOC_HUGE_ONLY));
    CHK_PRT_RET(aclRet != ACL_SUCCESS || slabPtr == nullptr,
        HCCL_ERROR("[AivUrmaChannel::%s] aclrtMalloc huge-only slab failed, ret[%d], size[%zu]",
            __func__, aclRet, slabSize), HCCL_E_MEMORY);
    AclDeviceSlabGuard slabGuard;
    slabGuard.Reset(slabPtr, slabSize);

    std::vector<uint8_t> zeroQueueIndexMem(QUEUE_INDEX_MEM_UNIT_SIZE * std::max(hostChannel.sqNum, hostChannel.cqNum), 0);
    auto zeroQueueIndexSection = [&zeroQueueIndexMem, slabPtr](const DeviceEntitySection &section) -> HcclResult {
        if (section.size == 0) {
            return HCCL_SUCCESS;
        }
        void *sectionPtr = GetSlabPtr(slabPtr, section);
        CHK_PTR_NULL(sectionPtr);
        Hccl::HrtMemcpy(sectionPtr, section.size, zeroQueueIndexMem.data(), section.size,
            Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);
        return HCCL_SUCCESS;
    };
    CHK_RET(zeroQueueIndexSection(sqPiSection));
    CHK_RET(zeroQueueIndexSection(sqCiSection));
    CHK_RET(zeroQueueIndexSection(cqPiSection));
    CHK_RET(zeroQueueIndexSection(cqCiSection));

    transport_->SetQueueIndexDeviceMem(GetSlabPtr(slabPtr, sqPiSection), GetSlabPtr(slabPtr, sqCiSection),
        GetSlabPtr(slabPtr, cqPiSection), GetSlabPtr(slabPtr, cqCiSection),
        std::max(hostChannel.sqNum, hostChannel.cqNum) * QUEUE_INDEX_MEM_UNIT_SIZE);
    CHK_RET(SecureMemset(&hostChannel, sizeof(ChannelEntity), 0, sizeof(ChannelEntity), "hostChannel"));
    transport_->GetHostChannelEntity(&hostChannel);
    hostChannel.abiHeader = channelDesc_.header;
    hostChannel.engine = COMM_ENGINE_AIV;
    hostChannel.protocol = channelDesc_.remoteEndpoint.protocol;

    ChannelEntity devChannel = hostChannel;
    CHK_RET(CopyArrayToSlab(slabPtr, hostChannel.localNotifyAddr, hostChannel.localNotifyNum,
        localNotifySection, &devChannel.localNotifyAddr, "localNotifyAddr"));
    CHK_RET(CopyArrayToSlab(slabPtr, hostChannel.remoteNotifyAddr, hostChannel.remoteNotifyNum,
        remoteNotifySection, &devChannel.remoteNotifyAddr, "remoteNotifyAddr"));
    CHK_RET(CopyArrayToSlab(slabPtr, hostChannel.localBufferAddr, hostChannel.localBufferNum,
        localBufferSection, &devChannel.localBufferAddr, "localBufferAddr"));
    CHK_RET(CopyArrayToSlab(slabPtr, hostChannel.remoteBufferAddr, hostChannel.remoteBufferNum,
        remoteBufferSection, &devChannel.remoteBufferAddr, "remoteBufferAddr"));
    CHK_RET(CopyArrayToSlab(slabPtr, hostChannel.sqContextAddr, hostChannel.sqNum,
        sqContextSection, &devChannel.sqContextAddr, "sqContextAddr"));
    CHK_RET(CopyArrayToSlab(slabPtr, hostChannel.cqContextAddr, hostChannel.cqNum,
        cqContextSection, &devChannel.cqContextAddr, "cqContextAddr"));

    void *entityDevPtr = GetSlabPtr(slabPtr, entitySection);
    CHK_PTR_NULL(entityDevPtr);
    Hccl::HrtMemcpy(entityDevPtr, sizeof(ChannelEntity), &devChannel, sizeof(ChannelEntity),
        Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);

    ReleaseDeviceChannelEntity();
    devChannelEntitySlab_ = slabGuard.Release();
    devChannelEntitySlabSize_ = slabSize;
    devChannelEntity_ = entityDevPtr;
    *devChannelPtr = devChannelEntity_;
    HCCL_INFO("[AivUrmaChannel] Build channel entity to device success, devPtr[%p], slabPtr[%p], slabSize[%zu]",
        devChannelEntity_, devChannelEntitySlab_, devChannelEntitySlabSize_);
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    HCCL_INFO("AivUrmaChannel GetNotifyNum is not supported.");
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    return transport_->GetRemoteMem(remoteMem, memNum, memTags);
}

HcclResult AivUrmaChannel::GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    return transport_->GetUserRemoteMem(remoteMem, memTag, memNum);
}

HcclResult AivUrmaChannel::Clean()
{
    ReleaseDeviceChannelEntity();
    ReleasePtrArrayDevMems();
    transport_.reset();
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::Resume()
{
    BuildConnection();
    BuildAivUrmaTransport();
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::NotifyRecord(const uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::WriteWithNotify(void *dst, const void *src, const uint64_t len,
    uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::Write(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::Read(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::ChannelFence()
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::Init()
{
    /*
        Argue result: make_unique 配合一场捕获的宏 EXCEPTION CATCH
        Attention: const 和引用
    */
    CHK_RET(ParseInputParam());
    CHK_RET(BuildSocket());
    CHK_RET(BuildAttr());
    CHK_RET(BuildConnection());
    localRmaBuffers_.clear();
    commonRes_.bufferVec.clear();
    CHK_RET(BuildBuffer(bufs_));
    CHK_RET(BuildAivUrmaTransport());
    return HCCL_SUCCESS;
}

ChannelStatus AivUrmaChannel::GetStatus()
{
    Hccl::TransportStatus transportStatus = transport_->GetStatus();
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
            HCCL_ERROR("[AivUrmaChannel][%s] Invalid TransportStatus[%d]", __func__, transportStatus);
            out = ChannelStatus::INVALID;
            break;
    }
    if (out == ChannelStatus::READY) {
        PutSocketIfNeeded();
    }
    return out;
}

} // namespace hcomm
