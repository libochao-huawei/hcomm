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
#include "adapter_hal_pub.h"

namespace hccl {
// HcclDispatcher   AicpuChannelHccsProcess::dispatcher_; // dispatcher放到最后析构
DispatcherCtxPtr AicpuChannelHccsProcess::dispatcherCtx_;
AicpuChannelHccsProcess::~AicpuChannelHccsProcess()
{
}

HcclResult AicpuChannelHccsProcess::SetTransportMachinePara(hccl::MachinePara &machinePara,
    HcclChannelHccsRes &channelHccsRes)
{
    machinePara.linkAttribute = 0x03; /* 0x03同时支持目的端和源端发起 */

    machinePara.remoteDeviceId = channelHccsRes.remoteDevicePhyId;
    machinePara.localDeviceId = channelHccsRes.localDevicePhyId;
    machinePara.deviceLogicId = channelHccsRes.localDeviceLogicId;

    machinePara.deviceType = static_cast<DevType>(channelHccsRes.deviceType);
    machinePara.tag = channelHccsRes.channelTag;
    machinePara.machineType = channelHccsRes.machineType;

    // 非910_93 2die sio与hccs并发场景，specifyLink设置为RESERVED_LINK_TYPE，平台层将按实际链路类型建链
    machinePara.specifyLink = LinkTypeInServer::RESERVED_LINK_TYPE;
    machinePara.isNewOndeSide = true;

    machinePara.localBufSize = channelHccsRes.localBufSize;
    machinePara.remoteBufSize = channelHccsRes.remoteBufSize;
    machinePara.localBufMem = channelHccsRes.localBufMem;
    machinePara.remoteBufMem = channelHccsRes.remoteBufMem;

    HCCL_INFO("%s success, linkAttribute[%x], localUserRank[%u], remoteWorldRank[%u], "
        "remoteUserrank[%u], deviceLogicId[%d], localDeviceId[%d], deviceType[%d], newTag[%s], specifyLink[%d], machineType[%u]"
        "localBufSize[%u],remoteBufSize[%u], localBufMem[%p], remoteBufMem[%p]",
        __func__, machinePara.linkAttribute, machinePara.localUserrank,
        machinePara.remoteWorldRank, machinePara.remoteUserrank, machinePara.deviceLogicId, machinePara.localDeviceId,
        machinePara.deviceType, machinePara.tag.c_str(), machinePara.specifyLink, static_cast<u32>(machinePara.machineType),
        machinePara.localBufSize, machinePara.remoteBufSize, machinePara.localBufMem, machinePara.remoteBufMem);
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelHccsProcess::InitP2pChannel(HcclChannelTransportResSet *transportResSet, uint32_t channelIndex,
    std::unique_ptr<hccl::Transport> &transport)
{
    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] transportResSet->channelHccsRes[%p] channelIndex[%u] start",
        __func__, static_cast<u32>(transportResSet->protocol), transportResSet->channelHccsRes, channelIndex);

    HcclChannelHccsRes &channelHccsRes = transportResSet->channelHccsRes[channelIndex];
    HcclChannelP2p &channelP2p = channelHccsRes.channelP2p;

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] 0", __func__, static_cast<u32>(transportResSet->protocol));
    // 创建Transport对象
    MachinePara machinePara;
    CHK_RET(SetTransportMachinePara(machinePara, channelHccsRes));
    machinePara.notifyNum = channelHccsRes.p2pNotifyNum;

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] 1", __func__, static_cast<u32>(transportResSet->protocol));
    TransportDeviceP2pData transDevP2pData;
    transDevP2pData.inputBufferPtr = nullptr;
    transDevP2pData.outputBufferPtr = nullptr;

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] 2", __func__, static_cast<u32>(transportResSet->protocol));
    //  获取transportAttr信息
    transDevP2pData.transportAttr = channelP2p.transportAttr;
 
    //  创建Transport对象
    TransportPara para{};
    const std::unique_ptr<hccl::NotifyPool> notifyPool;

    u32 devId = 0;
    CHK_RET(hrtDrvGetLocalDevIDByHostDevID(channelHccsRes.localDevicePhyId, &devId));
    // for data dispatcher read/write with DEFAULT_DISPATCH_NAME
    if (!FindDispatcherByCommId(&dispatcherCtx_, DEFAULT_DISPATCH_NAME)) {
        CHK_RET(CreateDispatcherCtx(&dispatcherCtx_, devId, DEFAULT_DISPATCH_NAME));
    }
    CHK_PTR_NULL(dispatcherCtx_);

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] 3", __func__, static_cast<u32>(transportResSet->protocol));
    DispatcherCtx *ctx = static_cast<DispatcherCtx *>(dispatcherCtx_);
    CHK_PRT(ctx->SetDispatcherHcclQos(channelHccsRes.channelP2p.qos)); // 调度器添加hcclQos
    CHK_PTR_NULL(ctx);

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] 4", __func__, static_cast<u32>(transportResSet->protocol));
    EXECEPTION_CATCH((transport = std::make_unique<Transport>(
        TransportType::TRANS_TYPE_DEVICE_P2P, para, ctx->GetDispatcher(), notifyPool, machinePara, transDevP2pData)),
        return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(transport);

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] 5", __func__, static_cast<u32>(transportResSet->protocol));
    CHK_RET(transport->Init()); // 初始化需要增加远端用户注册内存

    HCCL_INFO("[AicpuChannelHccsProcess][%s] protocol[%u] transport[%p] done", __func__,
        static_cast<u32>(transportResSet->protocol), transport.get());
    return HCCL_SUCCESS;
}
}