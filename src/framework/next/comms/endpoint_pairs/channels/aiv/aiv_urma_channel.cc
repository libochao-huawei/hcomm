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
#include "../../../endpoints/endpoint.h"
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

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace hcomm {
constexpr uint32_t QUEUE_NUM_MAX = 20;
constexpr uintptr_t CHANNEL_ENTITY_ALIGN_SIZE = 8;

namespace {
static inline uintptr_t AlignUp(uintptr_t addr, uintptr_t align)
{
    return (addr + align - 1) & ~(align - 1);
}

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

HcclResult GetHostBufRemain(const uint8_t *hostBuf, size_t totalSize, const uint8_t *cursor, size_t &offset,
    size_t &remain, const char *fieldName)
{
    if (hostBuf == nullptr || cursor == nullptr) {
        HCCL_ERROR("[GetHostBufRemain] nullptr is invalid, field[%s]", fieldName);
        return HCCL_E_PTR;
    }

    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(hostBuf);
    uintptr_t cursorAddr = reinterpret_cast<uintptr_t>(cursor);
    if (cursorAddr < baseAddr) {
        HCCL_ERROR("[GetHostBufRemain] cursor is before hostBuf, field[%s], hostBuf[%p], cursor[%p]",
            fieldName, hostBuf, cursor);
        return HCCL_E_PARA;
    }

    offset = static_cast<size_t>(cursorAddr - baseAddr);
    if (offset > totalSize) {
        HCCL_ERROR("[GetHostBufRemain] cursor overflow, field[%s], offset[%zu], totalSize[%zu]",
            fieldName, offset, totalSize);
        return HCCL_E_PARA;
    }

    remain = totalSize - offset;
    return HCCL_SUCCESS;
}

HcclResult SecureMemcpyToHostBuf(uint8_t *hostBuf, size_t totalSize, uint8_t *cursor, const void *src,
    size_t copySize, size_t &offset, const char *fieldName)
{
    if (src == nullptr || cursor == nullptr) {
        HCCL_ERROR("[SecureMemcpyToHostBuf] nullptr is invalid, field[%s], src[%p], cursor[%p]",
            fieldName, src, cursor);
        return HCCL_E_PTR;
    }

    size_t remain = 0;
    CHK_RET(GetHostBufRemain(hostBuf, totalSize, cursor, offset, remain, fieldName));
    if (copySize > remain) {
        HCCL_ERROR("[SecureMemcpyToHostBuf] insufficient host buffer, field[%s], copySize[%zu], remain[%zu], "
                   "offset[%zu], totalSize[%zu]",
            fieldName, copySize, remain, offset, totalSize);
        return HCCL_E_PARA;
    }

    errno_t ret = memcpy_s(cursor, remain, src, copySize);
    if (ret != EOK) {
        HCCL_ERROR("[SecureMemcpyToHostBuf] memcpy_s failed, field[%s], ret[%d], copySize[%zu], remain[%zu]",
            fieldName, ret, copySize, remain);
        return HCCL_E_MEMORY;
    }

    return HCCL_SUCCESS;
}

HcclResult MoveCursorWithAlign(uint8_t *hostBuf, size_t totalSize, uint8_t *&cursor, size_t copySize,
    const char *fieldName)
{
    size_t offset = 0;
    size_t remain = 0;
    CHK_RET(GetHostBufRemain(hostBuf, totalSize, cursor, offset, remain, fieldName));
    if (copySize > remain) {
        HCCL_ERROR("[MoveCursorWithAlign] copySize exceeds remain, field[%s], copySize[%zu], remain[%zu]",
            fieldName, copySize, remain);
        return HCCL_E_PARA;
    }

    uintptr_t nextAddr = reinterpret_cast<uintptr_t>(cursor) + copySize;
    uintptr_t alignedAddr = AlignUp(nextAddr, CHANNEL_ENTITY_ALIGN_SIZE);
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(hostBuf);
    if (alignedAddr < nextAddr || alignedAddr - baseAddr > totalSize) {
        HCCL_ERROR("[MoveCursorWithAlign] aligned cursor overflow, field[%s], alignedOffset[%zu], totalSize[%zu]",
            fieldName, static_cast<size_t>(alignedAddr - baseAddr), totalSize);
        return HCCL_E_PARA;
    }

    cursor = reinterpret_cast<uint8_t *>(alignedAddr);
    return HCCL_SUCCESS;
}

template <typename T>
HcclResult PackArrayToHostBuf(uint8_t *hostBuf, size_t totalSize, uint8_t *&cursor, const T *src, uint32_t num,
    T **dstField, const char *fieldName)
{
    if (dstField == nullptr) {
        HCCL_ERROR("[PackArrayToHostBuf] dstField is nullptr, field[%s]", fieldName);
        return HCCL_E_PTR;
    }

    if (num == 0 || src == nullptr) {
        *dstField = nullptr;
        return HCCL_SUCCESS;
    }

    if (num > SIZE_MAX / sizeof(T)) {
        HCCL_ERROR("[PackArrayToHostBuf] copy size overflow, field[%s], num[%u], elemSize[%zu]",
            fieldName, num, sizeof(T));
        return HCCL_E_PARA;
    }

    size_t copySize = static_cast<size_t>(num) * sizeof(T);
    size_t offset = 0;
    CHK_RET(SecureMemcpyToHostBuf(hostBuf, totalSize, cursor, src, copySize, offset, fieldName));
    *dstField = reinterpret_cast<T *>(offset);
    CHK_RET(MoveCursorWithAlign(hostBuf, totalSize, cursor, copySize, fieldName));
    return HCCL_SUCCESS;
}

HcclResult MemCopyHostToDev(void *devPtr, const void *hostPtr, size_t size)
{
    if (devPtr == nullptr || hostPtr == nullptr) {
        HCCL_ERROR("[memCopyHostToDev] nullptr is invalid");
        return HCCL_E_PTR;
    }
    if (size == 0) {
        HCCL_ERROR("[memCopyHostToDev] size is 0");
        return HCCL_E_PARA;
    }

    aclError aclRet = aclrtMemcpy(devPtr, size, hostPtr, size, ACL_MEMCPY_HOST_TO_DEVICE);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[memCopyHostToDev] aclrtMemcpy failed, error=%d", aclRet);
        return HCCL_E_MEMORY;
    }

    return HCCL_SUCCESS;
}

void FreeChannelEntityBuildBuf(uint8_t *hostBuf, uint8_t *devBase)
{
    if (hostBuf != nullptr) {
        free(hostBuf);
    }
    if (devBase != nullptr) {
        (void)aclrtFree(devBase);
    }
}

size_t CalcSingleChannelSize(const ChannelEntity &ch)
{
    size_t totalSize = sizeof(ChannelEntity);
    totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));

    if (ch.localNotifyNum > 0 && ch.localNotifyAddr != nullptr) {
        totalSize += ch.localNotifyNum * sizeof(RegedNotifyEntity);
        totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));
    }

    if (ch.remoteNotifyNum > 0 && ch.remoteNotifyAddr != nullptr) {
        totalSize += ch.remoteNotifyNum * sizeof(RegedNotifyEntity);
        totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));
    }

    if (ch.localBufferNum > 0 && ch.localBufferAddr != nullptr) {
        totalSize += ch.localBufferNum * sizeof(RegedBufferEntity);
        totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));
    }

    if (ch.remoteBufferNum > 0 && ch.remoteBufferAddr != nullptr) {
        totalSize += ch.remoteBufferNum * sizeof(RegedBufferEntity);
        totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));
    }

    if (ch.sqNum > 0 && ch.sqContextAddr != nullptr) {
        totalSize += ch.sqNum * sizeof(SqContext);
        totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));
    }

    if (ch.cqNum > 0 && ch.cqContextAddr != nullptr) {
        totalSize += ch.cqNum * sizeof(CqContext);
        totalSize = static_cast<size_t>(AlignUp(totalSize, CHANNEL_ENTITY_ALIGN_SIZE));
    }

    return totalSize;
}

HcclResult AllocChannelEntityBuildBuf(size_t totalSize, uint8_t **hostBuf, uint8_t **devBase)
{
    aclError aclRet = aclrtMalloc(reinterpret_cast<void **>(devBase), totalSize, ACL_MEM_MALLOC_NORMAL_ONLY);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[BuildChannelEntityDev] aclrtMalloc failed, error=%d", aclRet);
        return HCCL_E_MEMORY;
    }

    *hostBuf = static_cast<uint8_t *>(malloc(totalSize));
    if (*hostBuf == nullptr) {
        FreeChannelEntityBuildBuf(nullptr, *devBase);
        *devBase = nullptr;
        HCCL_ERROR("[BuildChannelEntityDev] Host malloc failed");
        return HCCL_E_MEMORY;
    }

    HcclResult ret = SecureMemset(*hostBuf, totalSize, 0, totalSize, "hostBuf");
    if (ret != HCCL_SUCCESS) {
        FreeChannelEntityBuildBuf(*hostBuf, *devBase);
        *hostBuf = nullptr;
        *devBase = nullptr;
    }
    return ret;
}

HcclResult PackChannelEntityHostBuf(ChannelEntity *channelEntity, uint8_t *hostBuf, size_t totalSize)
{
    uint8_t *cursor = hostBuf;
    ChannelEntity *dst = reinterpret_cast<ChannelEntity *>(cursor);
    *dst = *channelEntity;
    cursor += sizeof(ChannelEntity);
    CHK_RET(MoveCursorWithAlign(hostBuf, totalSize, cursor, 0, "ChannelEntity"));

    RegedNotifyEntity *srcLocalNotify = channelEntity->localNotifyAddr;
    RegedNotifyEntity *srcRemoteNotify = channelEntity->remoteNotifyAddr;
    RegedBufferEntity *srcLocalBuffer = channelEntity->localBufferAddr;
    RegedBufferEntity *srcRemoteBuffer = channelEntity->remoteBufferAddr;
    SqContext *srcSq = channelEntity->sqContextAddr;
    CqContext *srcCq = channelEntity->cqContextAddr;

    CHK_RET(PackArrayToHostBuf(hostBuf, totalSize, cursor, srcLocalNotify, channelEntity->localNotifyNum,
        &dst->localNotifyAddr, "localNotify"));
    CHK_RET(PackArrayToHostBuf(hostBuf, totalSize, cursor, srcRemoteNotify, channelEntity->remoteNotifyNum,
        &dst->remoteNotifyAddr, "remoteNotify"));
    CHK_RET(PackArrayToHostBuf(hostBuf, totalSize, cursor, srcLocalBuffer, channelEntity->localBufferNum,
        &dst->localBufferAddr, "localBuffer"));
    CHK_RET(PackArrayToHostBuf(hostBuf, totalSize, cursor, srcRemoteBuffer, channelEntity->remoteBufferNum,
        &dst->remoteBufferAddr, "remoteBuffer"));
    CHK_RET(PackArrayToHostBuf(hostBuf, totalSize, cursor, srcSq, channelEntity->sqNum,
        &dst->sqContextAddr, "sqContext"));
    CHK_RET(PackArrayToHostBuf(hostBuf, totalSize, cursor, srcCq, channelEntity->cqNum,
        &dst->cqContextAddr, "cqContext"));
    return HCCL_SUCCESS;
}

HcclResult BuildChannelEntityDev(ChannelEntity *channelEntity, void **outDevPtr)
{
    if (channelEntity == nullptr || outDevPtr == nullptr) {
        HCCL_ERROR("[BuildChannelEntityDev] Invalid nullptr input");
        return HCCL_E_PARA;
    }

    size_t totalSize = CalcSingleChannelSize(*channelEntity);
    if (totalSize == 0) {
        HCCL_ERROR("[BuildChannelEntityDev] totalSize is 0");
        return HCCL_E_PARA;
    }

    uint8_t *hostBuf = nullptr;
    uint8_t *devBase = nullptr;
    HcclResult ret = AllocChannelEntityBuildBuf(totalSize, &hostBuf, &devBase);
    if (ret == HCCL_SUCCESS) {
        ret = PackChannelEntityHostBuf(channelEntity, hostBuf, totalSize);
    }
    if (ret == HCCL_SUCCESS) {
        ret = MemCopyHostToDev(devBase, hostBuf, totalSize);
    }
    free(hostBuf);
    if (ret != HCCL_SUCCESS) {
        if (devBase != nullptr) {
            (void)aclrtFree(devBase);
        }
        HCCL_ERROR("[BuildChannelEntityDev] build channel entity failed, ret=%d", ret);
        return ret;
    }

    *outDevPtr = devBase;
    HCCL_INFO("[BuildChannelEntityDev] Single channel build success, size=%zu", totalSize);
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
    ReleaseDeviceChannelEntity();
}

void AivUrmaChannel::ReleaseDeviceChannelEntity()
{
    if (devChannelEntity_ == nullptr) {
        return;
    }

    aclError aclRet = aclrtFree(devChannelEntity_);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[AivUrmaChannel][%s] aclrtFree devChannelEntity failed, ptr[%p], ret[%d]",
            __func__, devChannelEntity_, aclRet);
    }
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
    // TODO: 使用 HcommEndpointGet
    Endpoint *localEpPtr = reinterpret_cast<Endpoint *>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();

    socket_ = reinterpret_cast<Hccl::Socket *>(channelDesc_.socket);
    remoteEp_ = channelDesc_.remoteEndpoint;
    notifyNum_ = channelDesc_.notifyNum;
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    // 3. 从 channelDesc 的 memHandle，获得 bufs_
    HCCL_INFO("[AivUrmaChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
    CHK_RET(Makebufs(channelDesc_.memHandles, channelDesc_.memHandleNum, bufs_));

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AivUrmaChannel][%s] socket ptr is NULL, rebuildSocket", __func__);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AivUrmaChannel][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    bool noRankId = true;
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, socketTag, noRankId);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));

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

    size_t connectNum = channelDesc_.roceAttr.queueNum;
    if (connectNum >= QUEUE_NUM_MAX) {
        HCCL_WARNING("[AivUrmaChannel] connectNum[%u] over queueNumMax[%u]", connectNum, QUEUE_NUM_MAX);
        connectNum = 1;
    }

    for (size_t i = 0; i < connectNum; ++i) {
        commonRes_.connVec.emplace_back(ubConn.get());
        connections_.push_back(std::move(ubConn));
    }

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

    transport_->GetHostChannelEntity(&hostChannel);
    hostChannel.abiHeader = channelDesc_.header;
    hostChannel.engine = COMM_ENGINE_AIV;
    hostChannel.protocol = channelDesc_.remoteEndpoint.protocol;
    void *newDevChannelEntity = nullptr;
    HcclResult ret = BuildChannelEntityDev(&hostChannel, &newDevChannelEntity);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AivUrmaChannel] BuildChannelEntityDev failed, ret=%d", ret);
        return ret;
    }
    if (newDevChannelEntity == nullptr) {
        HCCL_ERROR("[AivUrmaChannel] BuildChannelEntityDev returned nullptr");
        return HCCL_E_PTR;
    }

    ReleaseDeviceChannelEntity();
    devChannelEntity_ = newDevChannelEntity;
    *devChannelPtr = devChannelEntity_;
    HCCL_INFO("[AivUrmaChannel] Build channel entity to device success");
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
    // TODO: 处理抛异常
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
    return out;
}

} // namespace hcomm
