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
#include "urma_api.h"
#include "../../../endpoints/endpoint.h"
#include "orion_adpt_utils.h"

// Orion
#include "coll_alg_param.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "aicpu_res_package_helper.h"

namespace hcomm {
constexpr u32 FENCE_TIMEOUT_MS = 30 * 1000; // 定义最大等待30秒

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

HcclResult HostCpuUrmaChannel::BuildAttr()
{
    attr_.opMode      = Hccl::OpMode::OPBASE;
    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::BuildConnection()
{
    Hccl::OpMode        opMode = Hccl::OpMode::OPBASE;
    bool                devUsed  = true;  // aicpu 为 true
    Hccl::LinkProtocol  protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));
    
    Hccl::IpAddress     locAddr;
    Hccl::IpAddress     rmtAddr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtAddr));

    std::unique_ptr<Hccl::HostUbConnection> ubConn = nullptr;
    switch (protocol) {
        case Hccl::LinkProtocol::UB_TP:
            EXECEPTION_CATCH(
                ubConn = std::make_unique<Hccl::HostUbTpConnection>(rdmaHandle_, locAddr, rmtAddr, opMode, devUsed),
                return HCCL_E_PTR
            );
            break;
        case Hccl::LinkProtocol::UB_CTP:
            EXECEPTION_CATCH(
                ubConn = std::make_unique<Hccl::HostUbCtpConnection>(rdmaHandle_, locAddr, rmtAddr, opMode, devUsed),
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
    localNotifies_.clear();
    commonRes_.notifyVec.clear();
    bool devUsed = false;
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

    // make_unique / make_shared / release 包一层抛异常的宏
    EXECEPTION_CATCH(
        memTransport_ = std::make_unique<Hccl::UbMemTransport>(
            commonRes_, attr_, linkData, socket, rdmaHandle_, locCntNotifyRes
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
    CHK_RET(BuildConnection());
    CHK_RET(BuildNotify());
    CHK_RET(BuildBuffer());
    CHK_RET(BuildUbMemTransport());
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
            HCCL_ERROR("[HostCpuUrmaChannel][%s] Invalid TransportStatus[%d]", __func__, transportStatus);
            out = ChannelStatus::INVALID;
            break;
    }
    return out;
}

HcclResult HostCpuUrmaChannel::SetModuleDataName(Hccl::ModuleData &module, const std::string &name)
{
    int ret = strcpy_s(module.name, sizeof(module.name), name.c_str());
    if (ret != 0) {
        HCCL_ERROR("[SetModuleDataName] strcpy_s name %s failed", name.c_str());
        return HCCL_E_INTERNAL;
    }

    return HCCL_SUCCESS;
}

HcclResult HostCpuUrmaChannel::PackOpData(std::vector<char> &data)
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

HcclResult HostCpuUrmaChannel::H2DResPack(std::vector<char>& buffer)
{
    CHK_RET(PackOpData(buffer));
    HCCL_INFO("[HostCpuUrmaChannelImpl][%s] Pack Buffer data[%p], Pack Buffer size[%zu].",
        __func__, buffer.data(), buffer.size());
    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::PrepareNotifyWrResource(const uint32_t remoteNotifyIdx, urma_jfs_wr_t &notifyRecordWr)
{
    // 构造 urma_jfs_wr_t
    notifyRecordWr.opcode = URMA_OPC_WRITE_IMM;
    notifyRecordWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);//需要一个标志位，正常情况下是relax_order就是place_order是relax_order，除非加了fence,在调用fence后的第一个post_wr需要设置成strong order,随后又是relax_order.
    notifyRecordWr.flag.bs.comp_order = 1; // comp_order要一直保持为1,
    notifyRecordWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    notifyRecordWr.flag.bs.solicited_enable = 1;
    notifyRecordWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa()); // 控制面建链时放到channel里，直接从channel里拿到(唯一的对端信息，如果创建的模式rc的话是不用填target_jetty的),在控制面urma_import_jetty时会拿到这个target_jetty
    notifyRecordWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    // notifyRecordWr.rw.src.sge->addr; 不关心
    notifyRecordWr.rw.src.sge->len = 0;
    notifyRecordWr.rw.src.num_sge = 1;
    // notifyRecordWr.rw.dst.sge->addr ; 不关心
    notifyRecordWr.rw.dst.sge->len = 0;

    notifyRecordWr.rw.target_hint = 0;
    notifyRecordWr.rw.notify_data = remoteNotifyIdx;
    notifyRecordWr.send.src.sge->len = 0;
    notifyRecordWr.send.src.num_sge = 1;
    notifyRecordWr.send.imm_data = remoteNotifyIdx;
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
    writeWithNotifyWr.rw.src.num_sge = 1;
    writeWithNotifyWr.rw.dst.sge->addr = reinterpret_cast<uint64_t>(dst); // 远端地址
    writeWithNotifyWr.rw.dst.sge->len = len;

    writeWithNotifyWr.rw.target_hint = 0;
    writeWithNotifyWr.rw.notify_data = remoteNotifyIdx;
    writeWithNotifyWr.send.src.sge->addr = reinterpret_cast<uint64_t>(src); // 源地址
    writeWithNotifyWr.send.src.sge->len = len;
    writeWithNotifyWr.send.src.num_sge = 1;
    writeWithNotifyWr.send.target_hint = 0;
    writeWithNotifyWr.send.imm_data = remoteNotifyIdx;
    writeWithNotifyWr.next = nullptr;
    
    fenceFlag_ = false;

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::NotifyRecord(const uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start, remoteNotifyIdx[%u]", __func__, remoteNotifyIdx);
    // 补充jfr中消耗的rqe
    // 1. 准备jfr_wr
    CHK_RET(UrmaPostJfr());

    // 2.构造jfs_wr
    urma_jfs_wr_t  notifyRecordWr {};
    urma_jfs_wr_t *badWr = nullptr;

    CHK_RET(PrepareNotifyWrResource(remoteNotifyIdx, notifyRecordWr));

    // 3.调用urma_post_jetty_send_wr	 
    HCCL_INFO("[HostCpuUrmaChannel::%s] call urma_post_jetty_send_wr.", __func__);	 
    auto ret = urma_post_jetty_send_wr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &notifyRecordWr, &badWr);
    if (ret != 0 && badWr == nullptr) {
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_send_wr failed while badWr is nullptr", __func__);
        return HCCL_E_INTERNAL;
    }

    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_send_wr failed. ret:%d, "
        "badWr->rw.src.sge->addr[%llu], badWr->rw.dst.sge->addr[%llu]",
        __func__, ret, badWr->rw.src.sge->addr, badWr->rw.dst.sge->addr));
    
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
        
        auto actualNum = urma_poll_jfc(reinterpret_cast<urma_jfc_t*>(connections_[0]->GetCqVa()), 1, &wc);
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

    // 1.补充jfr中消耗的rqe
    CHK_RET(UrmaPostJfr());

    // 2.构造jfs_wr
    urma_jfs_wr_t writeWithNotifyWr {};
    urma_jfs_wr_t *badWr = nullptr;

    CHK_RET(PrepareWriteWrResource(dst, src, len, remoteNotifyIdx, writeWithNotifyWr));

    // 3.调用urma_post_jetty_send_wr	 
    auto ret = urma_post_jetty_send_wr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &writeWithNotifyWr, &badWr);
    if (ret != 0 && badWr == nullptr) {
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_send_wr failed while badWr is nullptr", __func__);
        return HCCL_E_INTERNAL;
    }

    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_send_wr failed. ret:%d, "
        "badWr->rw.src.sge->addr[%llu], badWr->rw.dst.sge->addr[%llu]",
        __func__, ret, badWr->rw.src.sge->addr, badWr->rw.dst.sge->addr));

    HCCL_INFO("[HostCpuUrmaChannel::%s] end.", __func__);
    wqeNum_++;

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::Write(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] start. dst[%p], src[%p], len[%llu].", __func__, dst, src, len);

    // 1.补充jfr中消耗的rqe	 
    CHK_RET(UrmaPostJfr());

    // 2.构造urma的wr
    urma_jfs_wr_t urmaWriteWr{};
    urma_jfs_wr_t *badWr = nullptr;
    urmaWriteWr.opcode = URMA_OPC_WRITE;
    urmaWriteWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);//需要一个标志位，正常情况下是relax_order就是place_order是relax_order，除非加了fence,在调用fence后的第一个post_wr需要设置成strong order,随后又是relax_order.
    // comp_order要一直保持为1,
    urmaWriteWr.flag.bs.comp_order = 1;
    urmaWriteWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    urmaWriteWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa()); // 控制面建链时放到channel里，直接从channel里拿到(唯一的对端信息，如果创建的模式rc的话是不用填target_jetty的),在控制面urma_import_jetty时会拿到这个target_jetty
    //华为云那边用1825的uboe,集合通信（host ub);单边通信也会用1650的UDie(host ub)
    urmaWriteWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    urmaWriteWr.rw.src.sge->addr = reinterpret_cast<uint64_t>(src); // 源地址
    urmaWriteWr.rw.src.sge->len = len;
    urmaWriteWr.rw.src.num_sge = 1;
    urmaWriteWr.rw.dst.sge->addr = reinterpret_cast<uint64_t>(dst); // 远端地址
    urmaWriteWr.rw.dst.sge->len = len;

    urmaWriteWr.rw.target_hint = 0;
    urmaWriteWr.rw.notify_data = 0; // 在write下不需要填，给个初值即可。在writewithnotify中该值和imm_data值一致
    urmaWriteWr.next = nullptr;
    
    // 3.调用 urma_post_jetty_send_wr	 
    auto ret = urma_post_jetty_send_wr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &urmaWriteWr, &badWr);
    
    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_send_wr failed. ret:%d, "
        "badWr->rw.src.sge->addr[%llu], badWr->rw.dst.sge->addr[%llu]",
        __func__, ret, badWr->rw.src.sge->addr, badWr->rw.dst.sge->addr));
    
    HCCL_INFO("[HostCpuUrmaChannel::%s] SUCCESS.", __func__);
    fenceFlag_ = false;
    wqeNum_++;

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::Read(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[HostCpuUrmaChannel::%s] START. dst[%p], src[%p], len[%llu].", __func__, dst, src, len);

    // 1.补充jfr中消耗的rqe	 
    CHK_RET(UrmaPostJfr());

    // 2.构造 WR
    urma_jfs_wr_t urmaReadWr{};
    urma_jfs_wr_t *badWr = nullptr;
    urmaReadWr.opcode = URMA_OPC_READ;
    urmaReadWr.flag.bs.place_order = (fenceFlag_ == true ? 2 : 1);//需要一个标志位，正常情况下是relax_order就是place_order是relax_order，除非加了fence,在调用fence后的第一个post_wr需要设置成strong order,随后又是relax_order.
    // comp_order要一直保持为1,
    urmaReadWr.flag.bs.comp_order = 1;
    urmaReadWr.flag.bs.fence = (fenceFlag_ == true ? 1 : 0);
    urmaReadWr.tjetty = reinterpret_cast<urma_target_jetty_t*>(connections_[0]->GetTJettyVa()); // 控制面建链时放到channel里，直接从channel里拿到(唯一的对端信息，如果创建的模式rc的话是不用填target_jetty的),在控制面urma_import_jetty时会拿到这个target_jetty
    //华为云那边用1825的uboe,集合通信（host ub);单边通信也会用1650的UDie(host ub)
    urmaReadWr.user_ctx = 0; // 跟ibvs中的wr_id对应
    urmaReadWr.rw.src.sge->addr = reinterpret_cast<uint64_t>(src); // 源地址
    urmaReadWr.rw.src.sge->len = len;
    urmaReadWr.rw.src.num_sge = 1;
    urmaReadWr.rw.dst.sge->addr = reinterpret_cast<uint64_t>(dst); // 远端地址
    urmaReadWr.rw.dst.sge->len = len;

    urmaReadWr.rw.target_hint = 0;
    urmaReadWr.rw.notify_data = 0; // 在write下不需要填，给个初值即可。在writewithnotify中该值和imm_data值一致
    urmaReadWr.next = nullptr;

    // 3.调用 urma_post_jetty_send_wr	 
    auto ret = urma_post_jetty_send_wr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &urmaReadWr, &badWr);
    
    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_send_wr. ret:%d, "
        "badWr->rw.src.sge->addr[%llu], badWr->rw.dst.sge->addr[%llu]",
        __func__, ret, badWr->rw.src.sge->addr, badWr->rw.dst.sge->addr));
    
    HCCL_INFO("[HostCpuUrmaChannel::%s] SUCCESS.", __func__);
    fenceFlag_ = false;
    wqeNum_++;

    return HCCL_SUCCESS;
}

HcclResult hcomm::HostCpuUrmaChannel::ChannelFence()
{
    std::lock_guard<std::mutex> lock(fenceMutex_);
    HCCL_INFO("[HostCpuUrmaChannel::%s] start, wqeNum_ = %u", __func__, wqeNum_);
    CHK_PRT_RET(wqeNum_ == 0, HCCL_INFO("[HostCpuUrmaChannel::%s] no need to fence since no wqeNum[%u].", __func__), HCCL_SUCCESS);
    std::vector<urma_cr_t> wc(wqeNum_);

    auto timeout = std::chrono::milliseconds(FENCE_TIMEOUT_MS);
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto actualNum = urma_poll_jfc(reinterpret_cast<urma_jfc_t*>(connections_[0]->GetCqVa()), wqeNum_, wc.data());

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

HcclResult hcomm::HostCpuUrmaChannel::UrmaPostJfr()
{
    // 1. 准备jfr_wr
    urma_jfr_wr_t jfrWr{};
    urma_jfr_wr_t *badWr = nullptr;
    // TODO: 待修改从控制面拿到segment
    // jfrWr.src.sge->addr = ; // 填希望用于做recv的地址（固定的device侧地址，响应writewithimm的4字节或8字节）【在控制面申请过并注册过segment的内存】
    // jfrWr.src.sge->len = ; // 填希望用于做recv的长度
    // jfrWr.src.sge->tseg->seg = ; // 注册时的segment，从控制面拿到
    // jfrWr.src.sge->tseg ;//整个tseg应该由控制面提供
    jfrWr.src.sge->user_tseg = nullptr;
    jfrWr.src.num_sge = 1;
    jfrWr.user_ctx = 0;
    jfrWr.next = nullptr;

    HCCL_INFO("[HostCpuUrmaChannel::%s] call ibv_post_recv", __func__);

    // 2. 调用 urma_post_jetty_recv_wr	 
    auto ret = urma_post_jetty_recv_wr(reinterpret_cast<urma_jetty_t*>(connections_[0]->GetJettyVa()), &jfrWr, &badWr);

    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuUrmaChannel::%s] urma_post_jetty_recv_wr failed. ret:%d, "
        "badWr->src.sge->addr[%llu]]", __func__, ret, badWr->src.sge->addr));

    return HCCL_SUCCESS;
}


} // namespace hcomm