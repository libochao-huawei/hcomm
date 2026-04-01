/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_channel_transport_process.h"
#include "aicpu_res_package_helper.h"
#include "aicpu_channel_hccs_process.h"
#include "dispatcher_ctx.h"

namespace hccl {
std::mutex AicpuChannelHccsProcess::mutex_;
std::unordered_map<u32, DispatcherCtxPtr> AicpuChannelHccsProcess::dispatcherCtxMap_;

AicpuChannelHccsProcess::~AicpuChannelHccsProcess()
{
    dispatcherCtxMap_.clear();
}

HcclResult AicpuChannelHccsProcess::GetDispatcherCtxPtr(u32 localDevicePhyId, DispatcherCtxPtr &dispatcherCtx)
{
    std::lock_guard<std::mutex> addLock(mutex_);
    auto handleIt = dispatcherCtxMap_.find(localDevicePhyId);
    if (handleIt != dispatcherCtxMap_.end()) {
        dispatcherCtx = handleIt->second;
        return HCCL_SUCCESS;
    }

    std::string dispatcherCtxTag = std::to_string(localDevicePhyId);
    DispatcherCtxPtr dispatcherCtxTmp;
    CHK_RET(CreateDispatcherCtx(&dispatcherCtxTmp, localDevicePhyId, dispatcherCtxTag.c_str()));
    dispatcherCtx = dispatcherCtxTmp;
    dispatcherCtxMap_.insert({localDevicePhyId, dispatcherCtx});
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelHccsProcess::CheckNotifyOrQPMaxNum(u64 &existNum, const u64 &MaxNum, const bool &isNotifyRes)
{
    std::string resType = isNotifyRes ? "Notify" : "QP";
    if (existNum + 1 > MaxNum) {
        HCCL_ERROR("[%s]%s resources are insufficient, existNum[%llu], MaxNum is [%llu]",
            __func__, resType.c_str(), existNum, MaxNum);
        return HCCL_E_INTERNAL;
    }
    HCCL_DEBUG("[%s]%s resources are sufficient, existNum[%llu], MaxNum is [%llu]",
            __func__, resType.c_str(), existNum, MaxNum);
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelHccsProcess::SetTransportMachinePara(hccl::MachinePara &machinePara,
    HcclChannelHccsRes &channelHccsRes)
{
    machinePara.linkAttribute = 0x03; /* 0x03同时支持目的端和源端发起 */

    machinePara.remoteDeviceId = channelHccsRes.remoteDevicePhyId;
    machinePara.localDeviceId = channelHccsRes.localDevicePhyId;
    machinePara.deviceLogicId = channelHccsRes.localDeviceLogicId;

    machinePara.deviceType = static_cast<DevType>(channelHccsRes.deviceType);
    machinePara.tag = "";

    // 非910_93 2die sio与hccs并发场景，specifyLink设置为RESERVED_LINK_TYPE，平台层将按实际链路类型建链
    machinePara.specifyLink = LinkTypeInServer::RESERVED_LINK_TYPE;
    machinePara.isSkipExchangeIndMem = true;

    HCCL_INFO("%s success, linkAttribute[%x], localUserRank[%u], remoteWorldRank[%u], "
        "remoteUserrank[%u], deviceLogicId[%d], localDeviceId[%d], deviceType[%d], newTag[%s], specifyLink[%d]",
        __func__, machinePara.linkAttribute, machinePara.localUserrank,
        machinePara.remoteWorldRank, machinePara.remoteUserrank, machinePara.deviceLogicId, machinePara.localDeviceId,
        machinePara.deviceType, machinePara.tag.c_str(), machinePara.specifyLink);
    return HCCL_SUCCESS;
}

template <typename T>
HcclResult AicpuChannelHccsProcess::InitAndVerifySingleSignal(const HcclSignalInfo &signalInfo, std::shared_ptr<T> &notify)
{
    if (signalInfo.resId == INVALID_U64) {
        // 无效值不做校验
        HCCL_DEBUG("[%s]resId[%llu] is invalid, need not check", __func__, signalInfo.resId);
        return HCCL_SUCCESS;
    }
    HcclSignalInfo tmpSignalInfo;

    EXECEPTION_CATCH((notify = std::make_shared<T>()), return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(notify);
    CHK_RET(notify->Init(signalInfo, hccl::NotifyLoadType::DEVICE_NOTIFY));
    CHK_RET(notify->GetNotifyData(tmpSignalInfo));
    HCCL_DEBUG("[%s] success resId[%llu], tsId:%d, devId[%u]", __func__,
        tmpSignalInfo.resId, tmpSignalInfo.tsId, tmpSignalInfo.devId);

    return HCCL_SUCCESS;
}

HcclResult AicpuChannelHccsProcess::SetChannelP2pNotify(TransportDeviceP2pData &transDevP2pData,
    u64 &p2pNotifyNum, HcclChannelP2p &channelP2p)
{
    u64 actualNotifyNum = 0;

    // 获取Ipc notify信息
    CHK_RET(CheckNotifyOrQPMaxNum(actualNotifyNum, LINK_P2P_MAX_NUM, true));
    std::shared_ptr<hccl::LocalNotify> ipcPreWaitNotify = std::make_shared<hccl::LocalNotify>();
    CHK_RET(InitAndVerifySingleSignal(channelP2p.localIpcSignal[actualNotifyNum], ipcPreWaitNotify));
    transDevP2pData.ipcPreWaitNotify = ipcPreWaitNotify;

    std::shared_ptr<hccl::RemoteNotify> ipcPreRecordNotify = std::make_shared<hccl::RemoteNotify>();
    CHK_RET(InitAndVerifySingleSignal(channelP2p.remoteIpcSignal[actualNotifyNum], ipcPreRecordNotify));
    transDevP2pData.ipcPreRecordNotify = ipcPreRecordNotify;
    actualNotifyNum++;

    CHK_RET(CheckNotifyOrQPMaxNum(actualNotifyNum, LINK_P2P_MAX_NUM, true));
    std::shared_ptr<hccl::LocalNotify> ipcPostWaitNotify = std::make_shared<hccl::LocalNotify>();
    CHK_RET(InitAndVerifySingleSignal(channelP2p.localIpcSignal[actualNotifyNum], ipcPostWaitNotify));
    transDevP2pData.ipcPostWaitNotify = ipcPostWaitNotify;

    std::shared_ptr<hccl::RemoteNotify> ipcPostRecordNotify = std::make_shared<hccl::RemoteNotify>();
    CHK_RET(InitAndVerifySingleSignal(channelP2p.remoteIpcSignal[actualNotifyNum], ipcPostRecordNotify));
    transDevP2pData.ipcPostRecordNotify = ipcPostRecordNotify;
    actualNotifyNum++;

    transDevP2pData.userLocalNotify.resize(p2pNotifyNum, nullptr);
    transDevP2pData.userRemoteNotify.resize(p2pNotifyNum, nullptr);

    for (u32 idx = 0; idx < p2pNotifyNum; idx++) {
        CHK_RET(CheckNotifyOrQPMaxNum(actualNotifyNum, LINK_P2P_MAX_NUM, true));
        std::shared_ptr<hccl::LocalNotify> ipcWaitNotify = std::make_shared<hccl::LocalNotify>();
        CHK_RET(InitAndVerifySingleSignal(channelP2p.localIpcSignal[actualNotifyNum], ipcWaitNotify));
        transDevP2pData.userLocalNotify[idx] = ipcWaitNotify;

        std::shared_ptr<hccl::RemoteNotify> ipcRecordNotify = std::make_shared<hccl::RemoteNotify>();
        CHK_RET(InitAndVerifySingleSignal(channelP2p.remoteIpcSignal[actualNotifyNum], ipcRecordNotify));
        transDevP2pData.userRemoteNotify[idx] = ipcRecordNotify;

        actualNotifyNum++;
    }

    HCCL_DEBUG("%s get p2pNotify success, p2pNotifyNum[%llu], actualNotifyNum[%llu]",
        __func__, p2pNotifyNum, actualNotifyNum);
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelHccsProcess::InitP2pChannel(HcclChannelTransportResSet *transportResSet, uint32_t channelIndex,
    std::unique_ptr<hccl::Transport> &transport)
{
    HcclChannelHccsRes &channelHccsRes = transportResSet->channelHccsRes[channelIndex];
    HcclChannelP2p &channelP2p = channelHccsRes.channelP2p;
    if (channelP2p.localIpcSignal[0].resId == INVALID_U64) {
        HCCL_ERROR("[%s]the Channel is invalid",__func__);
        return HCCL_E_INTERNAL;
    }

    // 创建Transport对象
    MachinePara machinePara;
    CHK_RET(SetTransportMachinePara(machinePara, channelHccsRes));
    machinePara.notifyNum = channelHccsRes.p2pNotifyNum;

    // 获取localMem & remoteMem
    TransportDeviceP2pData transDevP2pData;
    transDevP2pData.inputBufferPtr = reinterpret_cast<void *>(channelP2p.remoteHcclbuffer.addr);
    transDevP2pData.outputBufferPtr = reinterpret_cast<void *>(channelP2p.remoteHcclbuffer.addr);
    if (transDevP2pData.inputBufferPtr == nullptr || transDevP2pData.outputBufferPtr == nullptr) {
        HCCL_ERROR("[%s]input ptr[%p] or output ptr[%p] is null.", __func__,
            transDevP2pData.inputBufferPtr, transDevP2pData.outputBufferPtr);
        return HCCL_E_PARA;
    }
    // 获取Notify资源
    CHK_RET(SetChannelP2pNotify(transDevP2pData, channelHccsRes.p2pNotifyNum, channelP2p)); // 待确认notify是否

    //  获取transportAttr信息
    transDevP2pData.transportAttr = channelP2p.transportAttr;
 
    //  创建Transport对象
    TransportPara para{};
    const std::unique_ptr<hccl::NotifyPool> notifyPool;
    DispatcherCtxPtr dispatcherCtx;
    CHK_RET(GetDispatcherCtxPtr(channelHccsRes.localDevicePhyId, dispatcherCtx));
    DispatcherCtx *ctx = static_cast<DispatcherCtx *>(dispatcherCtx);
    CHK_PRT(ctx->SetDispatcherHcclQos(channelHccsRes.channelP2p.qos)); // 调度器添加hcclQos
    CHK_PTR_NULL(ctx);

    transport.reset(new (std::nothrow) Transport(
        TransportType::TRANS_TYPE_DEVICE_P2P, para, ctx->GetDispatcher(), notifyPool, machinePara, transDevP2pData));
    CHK_SMART_PTR_NULL(transport);
    CHK_RET(transport->Init()); // 初始化需要增加远端用户注册内存

    return HCCL_SUCCESS;
}
}