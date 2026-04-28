/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#include "host_cpu_urma_channel.h"
#include "../../../endpoints/endpoint.h"
#include "orion_adpt_utils.h"
#include "hcomm_res.h"
#include "hcomm_adapter_urma.h"
#include "urma_api.h"

// Orion
#include "coll_alg_param.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "aicpu_res_package_helper.h"

namespace hcomm {
constexpr u32 FENCE_TIMEOUT_MS = 30 * 1000; // 定义最大等待30秒
constexpr u32 MEMORY_BLOCK_SIZE = 128;
constexpr uint16_t DEFAULT_LISTENING_PORT = 60001;

HostCpuUrmaChannel::HostCpuUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc):
    endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HcclResult HostCpuUrmaChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    // TODO: 使用 HcommEndpointGet
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();

    HCCL_INFO("[AicpuTsUrmaChannel][%s] localProtocol[%d]", __func__, localEp_.protocol);

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    notifyNum_ = channelDesc_.notifyNum;

    if (channelDesc_.exchangeAllMems) {
        // 3. Get memHandles from endpoint
        HCCL_INFO("[AicpuTsUrmaChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalUbRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(HcommMemGetAllMemHandles(
            endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum)));
        HCCL_INFO("[AicpuTsUrmaChannel][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalUbRmaBuffer> &localUbRmaBuffer = memHandles[i];
            HCCL_INFO("[AicpuTsUrmaChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(), localUbRmaBuffer->GetBuf()->GetMemTag().c_str());
            bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
                localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(), localUbRmaBuffer->GetBuf()->GetMemTag().c_str())
            ));
        }
    } else {
        // 3. 从 channelDesc 的 memHandle，获得 bufs_
        HCCL_WARNING("[AicpuTsUrmaChannel][%s] exchangeAllMems is false.", __func__);
    }

    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::BuildAttr()
{
    attr_.opMode      = Hccl::OpMode::OPBASE;
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::StartListen()
{
    uint16_t port = channelDesc_.port;
    HCCL_INFO("[HostCpuUrmaChannel::%s] Start. EndpointHandle[0x%llx], port[%u]", __func__, reinterpret_cast<uint64_t>(endpointHandle_), port);
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuUrmaChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    CHK_RET(static_cast<HcclResult>(HcommEndpointStartListen(endpointHandle_, port, nullptr)));
    HCCL_INFO("[HostCpuUrmaChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[HostCpuUrmaChannel::%s] socket ptr is NULL, rebuild Socket", __func__);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(HostUrmaEndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[HostCpuUrmaChannel::%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuUrmaChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    bool noRankId = true;
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, socketTag, noRankId);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    HCCL_INFO("[HostCpuUrmaChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::BuildConnection()
{
    Hccl::OpMode        opMode = Hccl::OpMode::OPBASE;
    Hccl::LinkProtocol  protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));

    Hccl::IpAddress     locAddr;
    Hccl::IpAddress     rmtAddr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtAddr));

    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    Hccl::TpManager::GetInstance(deviceLogicId).Init();

    std::unique_ptr<Hccl::HostUbConnection> ubConn = nullptr;
    switch (protocol) {
        case Hccl::LinkProtocol::UB_TP:
            EXECEPTION_CATCH(
                ubConn = std::make_unique<Hccl::HostUbTpConnection>(rdmaHandle_, locAddr, rmtAddr, opMode),
                return HCCL_E_PTR
            );
            break;
        case Hccl::LinkProtocol::UB_CTP:
            EXECEPTION_CATCH(
                ubConn = std::make_unique<Hccl::HostUbCtpConnection>(rdmaHandle_, locAddr, rmtAddr, opMode),
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

HcclResult HostCpuUrmaChannel::BuildNotify()
{
    // TODO: 软件管理notify，仿造hostRoce
    return HCCL_SUCCESS;
}

// TODO: to be deleted
HcclResult HostCpuUrmaChannel::BuildBuffer()
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

HcclResult HostCpuUrmaChannel::BuildUbMemTransport()
{
    Hccl::BaseMemTransport::LocCntNotifyRes locCntNotifyRes{};
    locCntNotifyRes.vec.clear();
    locCntNotifyRes.desc.clear();
    const Hccl::Socket &socket = *socket_;

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));

    bool isRecvFirst = socket.GetRole() == Hccl::SocketRole::CLIENT ? true : false;

    // make_unique / make_shared / release 包一层抛异常的宏
    EXECEPTION_CATCH(
        memTransport_ = std::make_unique<Hccl::UbMemTransport>(
            commonRes_, attr_, linkData, socket, rdmaHandle_, locCntNotifyRes, isRecvFirst
        ),
        return HCCL_E_PTR
    );
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::Init()
{
    /*
        Argue result: make_unique 配合一场捕获的宏 EXCEPTION CATCH
        Attention: const 和引用
    */
    // TODO: 处理抛异常
    CHK_RET(ParseInputParam());
    CHK_RET(BuildAttr());
    CHK_RET(StartListen());
    CHK_RET(BuildSocket());
    CHK_RET(BuildConnection());
    CHK_RET(BuildNotify());
    CHK_RET(BuildBuffer());
    CHK_RET(BuildUbMemTransport());
    // urma函数初始化
    CHK_RET(DlUrmaFunction::GetInstance().DlUrmaFunctionInit());
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    *notifyNum = this->notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    return memTransport_->GetRemoteMem(remoteMem, memNum, memTags);
}

ChannelStatus HostCpuUrmaChannel::GetStatus()
{
    Hccl::TransportStatus transportStatus = memTransport_->GetSyncStatus();
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
            HCCL_ERROR("[HostCpuUrmaChannel][%s] Invalid TransportStatus[%d]", __func__, transportStatus);
            out = ChannelStatus::INVALID;
            break;
    }
    return out;
}

HcclResult hcomm::HostCpuUrmaChannel::PrepareNotifyWrResource(const uint32_t remoteNotifyIdx, urma_jfs_wr_t &notifyRecordWr)
{
    // 构造 urma_jfs_wr_t
    notifyRecordWr.opcode = URMA_OPC_WRITE_IMM;
    notifyRecordWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);
    notifyRecordWr.flag.bs.comp_order = 1; // comp_order要一直保持为1,
    notifyRecordWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    notifyRecordWr.flag.bs.solicited_enable = 1;
    notifyRecordWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa());
    notifyRecordWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    notifyRecordWr.rw.src.sge->len = 0;
    notifyRecordWr.rw.src.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    notifyRecordWr.rw.src.sge->user_tseg = nullptr;
    notifyRecordWr.rw.src.num_sge = 1;
    notifyRecordWr.rw.dst.sge->len = 0;
    notifyRecordWr.rw.dst.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    notifyRecordWr.rw.dst.sge->user_tseg = nullptr;

    notifyRecordWr.rw.target_hint = 0;
    notifyRecordWr.rw.notify_data = remoteNotifyIdx;
    notifyRecordWr.send.src.sge->len = 0;
    notifyRecordWr.send.src.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    notifyRecordWr.send.src.sge->user_tseg = nullptr;
    notifyRecordWr.send.src.num_sge = 1;
    notifyRecordWr.send.imm_data = remoteNotifyIdx;
    notifyRecordWr.send.target_hint = 0;
    notifyRecordWr.send.tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    notifyRecordWr.next = nullptr;

    fenceFlag_ = false;

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx, urma_jfs_wr_t &writeWithNotifyWr)
{
    CHK_PRT_RET(len > UINT32_MAX, HCCL_ERROR("[HostCpuUrmaChannel][%s] the len[%llu] exceeds the size of u32.",
        __func__, len), HCCL_E_PARA);

    // 构造 urma_jfs_wr_t
    writeWithNotifyWr.opcode = URMA_OPC_WRITE_IMM;
    writeWithNotifyWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);//需要一个标志位，正常情况下是relax_order就是place_order是relax_order，除非加了fence,在调用fence后的第一个post_wr需要设置成strong order,随后又是relax_order.
    // comp_order要一直保持为1,
    writeWithNotifyWr.flag.bs.comp_order = 1;
    writeWithNotifyWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    writeWithNotifyWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa()); // 控制面建链时放到channel里，直接从channel里拿到(唯一的对端信息，如果创建的模式rc的话是不用填target_jetty的),在控制面urma_import_jetty时会拿到这个target_jetty
    writeWithNotifyWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    writeWithNotifyWr.rw.src.sge->addr = reinterpret_cast<uint64_t>(src); // 源地址
    writeWithNotifyWr.rw.src.sge->len = len;
    writeWithNotifyWr.rw.src.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    writeWithNotifyWr.rw.src.sge->user_tseg = nullptr;
    writeWithNotifyWr.rw.src.num_sge = 1;
    writeWithNotifyWr.rw.dst.sge->addr = reinterpret_cast<uint64_t>(dst); // 远端地址
    writeWithNotifyWr.rw.dst.sge->len = len;
    writeWithNotifyWr.rw.dst.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    writeWithNotifyWr.rw.dst.sge->user_tseg = nullptr;

    writeWithNotifyWr.rw.target_hint = 0;
    writeWithNotifyWr.rw.notify_data = remoteNotifyIdx;
    writeWithNotifyWr.send.src.sge->addr = reinterpret_cast<uint64_t>(src); // 源地址
    writeWithNotifyWr.send.src.sge->len = len;
    writeWithNotifyWr.send.src.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    writeWithNotifyWr.send.src.sge->user_tseg = nullptr;
    writeWithNotifyWr.send.src.num_sge = 1;
    writeWithNotifyWr.send.target_hint = 0;
    writeWithNotifyWr.send.imm_data = remoteNotifyIdx;
    writeWithNotifyWr.send.tseg = reinterpret_cast<urma_target_seg_t*>(localRmaBuffers_[0]->GetTargetSeg());
    writeWithNotifyWr.next = nullptr;

    fenceFlag_ = false;

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::NotifyRecord(const uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start, remoteNotifyIdx[%u]", __func__, remoteNotifyIdx);
    // 1.构造jfs_wr
    urma_jfs_wr_t  notifyRecordWr {};

    CHK_RET(PrepareNotifyWrResource(remoteNotifyIdx, notifyRecordWr));

    wqeNum_++;
    HCCL_INFO("[HostCpuUrmaChannel::%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start. localNotifyIdx[%u], timeout[%u]", __func__, localNotifyIdx, timeout);

    // 1. 准备WR
    urma_cr_t wc{};
    std::lock_guard<std::mutex> lock(jfcMutex_);

    // 2.轮询jfr对应的jfc
    auto startTime = std::chrono::steady_clock::now();
    auto waitTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(timeout));
    while (true) {
        HCCL_INFO("[HostCpuUrmaChannel::%s] start to poll jfc", __func__);

        uint64_t actualNum = 10;
        CHK_PRT_RET(wc.status != URMA_CR_SUCCESS,
            HCCL_ERROR("[HostCpuUrmaChannel][%s] urma_poll_jfc return wc.status is [%d].",
            __func__, wc.status), HCCL_E_NETWORK);

        HCCL_INFO("[HostCpuUrmaChannel::%s] actualNum = %d; imm_data = %u", actualNum, wc.imm_data);

        if (actualNum > 0 && wc.imm_data == localNotifyIdx) {
            HCCL_INFO("[HostCpuUrmaChannel::%s] poll jfc success.", __func__);
            break;
        }

        if ((std::chrono::steady_clock::now() - startTime) >= waitTime) {
            HCCL_ERROR("[HostCpuUrmaChannel][%s] call urma_poll_jfc timeout.", __func__);
            return HCCL_E_TIMEOUT;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start.", __func__);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);

    // 1.构造jfs_wr
    urma_jfs_wr_t writeWithNotifyWr {};

    CHK_RET(PrepareWriteWrResource(dst, src, len, remoteNotifyIdx, writeWithNotifyWr));

    HCCL_INFO("[HostCpuUrmaChannel::%s] end.", __func__);
    wqeNum_++;

    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::GetLocSeg(const void *addr, const size_t size, u64 *seg)
{
    if (localRmaBuffers_.empty()) {
        HCCL_ERROR("[HostCpuUrmaChannel::%s] localRmaBuffers is empty.", __func__);
        return HCCL_E_INTERNAL;
    }

    bool isAddrInRange = false;
    for (auto &it : localRmaBuffers_) {
        Hccl::Buffer iterBuf(it->GetBufferInfo().first, it->GetBufferInfo().second);
        if (iterBuf.Contains(reinterpret_cast<uintptr_t>(addr), size)) {
            *seg = it->GetTargetSeg();
            isAddrInRange = true;
            break;
        }
    }

    if (!isAddrInRange) {
        HCCL_ERROR("GetLocSeg addr[%p] size[%llu] is not in localRmaBuffers_", addr, size);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::Write(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start. dst[%p], src[%p], len[%llu] fenceFlag_[%u].",
        __func__, dst, src, len, fenceFlag_);

    // 构造urma的wr
    urma_jfs_wr_t urmaWriteWr{};

    urmaWriteWr.opcode = URMA_OPC_WRITE;
    // 需要一个标志位，正常情况下是relax_order就是place_order是relax_order，除非加了fence
    // 在调用fence后的第一个post_wr需要设置成strong order,随后又是relax_order.
    urmaWriteWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);
    urmaWriteWr.flag.bs.comp_order = 1;     // comp_order要一直保持为1,
    urmaWriteWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    urmaWriteWr.flag.bs.complete_enable = 0;
    urmaWriteWr.flag.bs.inline_flag = 0;
    // 控制面建链时放到channel里，直接从channel里拿到(唯一的对端信息
    // 如果创建的模式rc的话是不用填target_jetty的),在控制面urma_import_jetty时会拿到这个target_jetty
    urmaWriteWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa());
    urmaWriteWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    urmaWriteWr.next = nullptr;

    u64 splitNum = 0;
    if ((len % MAX_JETTY_WR_DATA_LEN) == 0) {
        splitNum = len / MAX_JETTY_WR_DATA_LEN;
    } else {
        splitNum = (len / MAX_JETTY_WR_DATA_LEN) + 1;
    }

    u64 localSeg;
    u64 remoteSeg;
    CHK_RET(GetLocSeg(src, len, &localSeg));
    CHK_RET(memTransport_->GetRemoteSeg(dst, len, &remoteSeg));
    uint64_t offset = 0;
    for (u64 i = 0; i < splitNum; i++) {
        urma_jfs_wr_t *badWr = nullptr;
        uint64_t chunkLen = std::min(len - offset, MAX_JETTY_WR_DATA_LEN);
        // 源地址 数据长度 tseg
        urma_sge_t srclist = {0};
        urmaWriteWr.rw.src.sge = &srclist;
        urmaWriteWr.rw.src.sge->addr = reinterpret_cast<uint64_t>(static_cast<char *>(const_cast<void *>(src)) + offset);
        urmaWriteWr.rw.src.sge->len = chunkLen;
        urmaWriteWr.rw.src.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localSeg);
        urmaWriteWr.rw.src.num_sge = 1;

        // 目的地址 数据长度 tseg
        urma_sge_t dstlist = {0};
        urmaWriteWr.rw.dst.sge = &dstlist;
        urmaWriteWr.rw.dst.sge->addr = reinterpret_cast<uint64_t>(static_cast<const char *>(dst) + offset); // 远端地址
        urmaWriteWr.rw.dst.sge->len = chunkLen;
        urmaWriteWr.rw.dst.sge->tseg = reinterpret_cast<urma_target_seg_t*>(remoteSeg);
        urmaWriteWr.rw.dst.num_sge = 1;

        offset += chunkLen;
        // 只有最后一个wr上报cqe
        if (i == splitNum - 1) {
            urmaWriteWr.flag.bs.complete_enable = 1;
            urmaWriteWr.flag.bs.place_order = 2; // 最后一个wr设置为strong order
        }
        CHK_RET(HrtUrmaPostJettySendWr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &urmaWriteWr, &badWr));
    }

    HCCL_INFO("[HostCpuUrmaChannel::%s] remoteJettyVa[%llu] jettyVa[%llu] localSeg[%llu] remoteSeg[%llu] splitNum[%llu]",
        __func__, connections_[0]->GetTJettyVa(), connections_[0]->GetJettyVa(), localSeg, remoteSeg, splitNum);
    fenceFlag_ = false;
    wqeNum_++;
    HCCL_INFO("[HostCpuUrmaChannel::%s] fenceFlag_[%u] wqeNum_[%u] SUCCESS.", __func__, fenceFlag_, wqeNum_);

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::Read(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start. dst[%p], src[%p], len[%llu] fenceFlag_[%u].",
        __func__, dst, src, len, fenceFlag_);

    // 2.构造urma的wr
    urma_jfs_wr_t urmaWriteWr{};

    urmaWriteWr.opcode = URMA_OPC_READ;

    // 需要一个标志位，正常情况下是relax_order就是place_order是relax_order，除非加了fence
    // 在调用fence后的第一个post_wr需要设置成strong order,随后又是relax_order.
    urmaWriteWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);
    urmaWriteWr.flag.bs.comp_order = 1;     // comp_order要一直保持为1,
    urmaWriteWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    urmaWriteWr.flag.bs.complete_enable = 0;
    urmaWriteWr.flag.bs.inline_flag = 0;

    urmaWriteWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa()); // 控制面建链时放到channel里，直接从channel里拿到(唯一的对端信息，如果创建的模式rc的话是不用填target_jetty的),在控制面urma_import_jetty时会拿到这个target_jetty
    //华为云那边用1825的uboe,集合通信（host ub);单边通信也会用1650的UDie(host ub)
    urmaWriteWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    urmaWriteWr.next = nullptr;

    u64 splitNum = 0;
    if ((len % MAX_JETTY_WR_DATA_LEN) == 0) {
        splitNum = len / MAX_JETTY_WR_DATA_LEN;
    } else {
        splitNum = (len / MAX_JETTY_WR_DATA_LEN) + 1;
    }

    u64 localSeg;
    u64 remoteSeg;
    // read的dst其实就是本端,跟write操作相反
    CHK_RET(GetLocSeg(dst, len, &localSeg));
    CHK_RET(memTransport_->GetRemoteSeg(src, len, &remoteSeg));
    uint64_t offset = 0;
    for (u64 i = 0; i < splitNum; i++) {
        urma_jfs_wr_t *badWr = nullptr;
        uint64_t chunkLen = std::min(len - offset, MAX_JETTY_WR_DATA_LEN);

        // 源地址 数据长度 tseg
        urma_sge_t srclist = {0};
        urmaWriteWr.rw.src.sge = &srclist;
        urmaWriteWr.rw.src.sge->addr = reinterpret_cast<uint64_t>(static_cast<char *>(const_cast<void *>(src)) + offset);
        urmaWriteWr.rw.src.sge->len = chunkLen;
        urmaWriteWr.rw.src.sge->tseg = reinterpret_cast<urma_target_seg_t*>(remoteSeg);
        urmaWriteWr.rw.src.num_sge = 1;

        // 目的地址 数据长度 tseg
        urma_sge_t dstlist = {0};
        urmaWriteWr.rw.dst.sge = &dstlist;
        urmaWriteWr.rw.dst.sge->addr = reinterpret_cast<uint64_t>(static_cast<const char *>(dst) + offset); // 远端地址
        urmaWriteWr.rw.dst.sge->len = chunkLen;
        urmaWriteWr.rw.dst.sge->tseg = reinterpret_cast<urma_target_seg_t*>(localSeg);
        urmaWriteWr.rw.dst.num_sge = 1;

        // 只有最后一个wr上报cqe
        if (i == splitNum - 1) {
            urmaWriteWr.flag.bs.complete_enable = 1;
            urmaWriteWr.flag.bs.place_order = 2; // 最后一个wr设置为strong order
        }
        CHK_RET(HrtUrmaPostJettySendWr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &urmaWriteWr, &badWr));
        offset += chunkLen;
    }

    HCCL_INFO("[HostCpuUrmaChannel::%s] remoteJettyVa[%llu] jettyVa[%llu] localSeg[%llu] remoteSeg[%llu]",
        __func__, connections_[0]->GetTJettyVa(), connections_[0]->GetJettyVa(), localSeg, remoteSeg);
    fenceFlag_ = false;
    wqeNum_++;
    HCCL_INFO("[HostCpuUrmaChannel::%s] fenceFlag_[%u] wqeNum_[%u] SUCCESS.", __func__, fenceFlag_, wqeNum_);
    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::ChannelFence()
{
    std::lock_guard<std::mutex> lock(fenceMutex_);
    HCCL_INFO("[HostCpuUrmaChannel::%s] start, wqeNum_ = %u va[%llu]", __func__, wqeNum_, connections_[0]->GetCqVa());
    CHK_PRT_RET(wqeNum_ == 0, HCCL_INFO("[HostCpuUrmaChannel::%s] no need to fence since no wqeNum[%u].", __func__), HCCL_SUCCESS);
    std::vector<urma_cr_t> wc(wqeNum_);

    auto timeout = std::chrono::milliseconds(FENCE_TIMEOUT_MS);
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto actualNum = HrtUrmaPollJfc(reinterpret_cast<urma_jfc_t*>(connections_[0]->GetCqVa()), wqeNum_, wc.data());
        if (actualNum < 0) {
            HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_poll_jfc failed. actualNum=%d", __func__, actualNum);
            return HCCL_E_NETWORK;
        }

        uint32_t actualNum32 = static_cast<uint32_t>(actualNum);
        if (actualNum32 > wqeNum_) {
            HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_poll_jfc polled more completions (%u) than expected (%u).",
                __func__, actualNum32, wqeNum_);
            return HCCL_E_INTERNAL;
        } else if (actualNum32 > 0) {
            for (uint32_t i = 0; i < actualNum32; i++) {
                if (wc[i].status != URMA_CR_SUCCESS) {
                    HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_poll_jfc error. wc[%u] status:%d", __func__, i, wc[i].status);
                    return HCCL_E_NETWORK;
                }
            }
            wqeNum_ -= actualNum32; // 减去已完成的数量，继续等待剩余的完成
            if(wqeNum_ == 0) {
                break; // 所有的wqe都已完成，退出循环
            }
        }

        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[HostCpuUrmaChannel::%s] call urma_poll_jfc timeout.", __func__);
            return HCCL_E_TIMEOUT;
        }
    }

    wqeNum_ = 0; // 所有wqe都已完成，重置计算器
    fenceFlag_ = true;
    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::Clean()
{
    // 该模式当前不支持N秒快恢
    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::Resume()
{
    // 该模式当前不支持N秒快恢
    return HCCL_SUCCESS;
}

} // namespace hcomm