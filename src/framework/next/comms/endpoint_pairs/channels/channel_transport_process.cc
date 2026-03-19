/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "channel_process.h"
#include "exception_handler.h"
#include "channel_param.h"
#include "aicpu_ts_hccs_channel.h"
#include "launch_aicpu.h"
#include "hcclCommDfx.h"
#include "env_config/env_config.h"
#include "channel_transport_process.h"

namespace hcomm {

HcclResult ChannelTransportProcess::DeepCopyH2DChannelP2p(const HcclChannelP2p &hostChannelP2p, 
    HcclChannelP2p &deviceChannelP2p, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u]", __func__);

    // 复制基本成员
    deviceChannelP2p = hostChannelP2p;
    // 处理remoteUserMem
    if (hostChannelP2p.remoteUserMem != nullptr && hostChannelP2p.remoteUserMemCount > 0) {
        size_t remoteUserMemSize = hostChannelP2p.remoteUserMemCount * sizeof(HcclMem);
        std::shared_ptr<hccl::DeviceMem> deviceMem;
        EXECEPTION_CATCH((deviceMem = std::make_shared<hccl::DeviceMem>(hccl::DeviceMem::alloc(remoteUserMemSize))),
                         return HCCL_E_PTR);
        CHK_RET(hrtMemSyncCopy(deviceMem.get()->ptr(), remoteUserMemSize, hostChannelP2p.remoteUserMem,
            remoteUserMemSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
        deviceChannelP2p.remoteUserMem = reinterpret_cast<HcclMem*>(deviceMem.get()->ptr());
        channelParamMemVector.push_back(std::move(deviceMem));
    } else {
        deviceChannelP2p.remoteUserMem = nullptr;
    }
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::DeepCopyH2DChannelHccsRes(const HcclChannelHccsRes &hostChannelHccsRes, 
    HcclChannelHccsRes &deviceChannelHccsRes, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u]", __func__);
    // 复制基本成员
    deviceChannelHccsRes = hostChannelHccsRes;
    // 处理P2P通道
    CHK_RET(DeepCopyH2DChannelP2p(hostChannelHccsRes.channelP2p, deviceChannelHccsRes.channelP2p, channelParamMemVector));
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::DeepCopyH2DChannelParam(const HcclChannelTransportResSet &hostTransportResSet, 
    HcclChannelTransportResSet &deviceTransportResSet, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u]", __func__);

    deviceTransportResSet = hostTransportResSet;

    // 拷贝remoteResV2
    if (hostTransportResSet.channelHccsRes != nullptr && hostTransportResSet.listNum > 0) {
        // 为设备端的 channelHccsRes 数组分配内存（注意：这个数组存放的是 HcclChannelHccsRes 结构体）
        size_t remoteResV2ArraySize = sizeof(HcclChannelHccsRes) * hostTransportResSet.listNum;
        std::shared_ptr<hccl::DeviceMem> deviceChannelHccsResArray;
        EXECEPTION_CATCH(
            (deviceChannelHccsResArray = std::make_shared<hccl::DeviceMem>(hccl::DeviceMem::alloc(remoteResV2ArraySize))),
            return HCCL_E_PTR);

        // 为每个数组元素进行深度拷贝，并保存设备内存和主机结构体（指针已调整）
        std::vector<hccl::DeviceMem> elementMemories; // 保存每个元素分配的设备内存（包括内部指针数据）
        std::vector<HcclChannelHccsRes> hostRemoteResV2Array(hostTransportResSet.listNum);

        for (uint32_t i = 0; i < hostTransportResSet.listNum; ++i) {
            HcclChannelHccsRes hostElement = hostTransportResSet.channelHccsRes[i];
            HcclChannelHccsRes deviceElement;
            // 深度拷贝一个元素到设备内存，并返回设备内存中的结构体布局（host端）
            CHK_RET(DeepCopyH2DChannelHccsRes(hostElement, deviceElement, channelParamMemVector));
            // 保存调整后的主机端结构体（其指针指向设备内存）
            hostRemoteResV2Array[i] = deviceElement;
        }

        // 将主机端的结构体数组（指针已调整）拷贝到设备内存数组
        CHK_RET(hrtMemSyncCopy(deviceChannelHccsResArray.get()->ptr(), remoteResV2ArraySize, hostRemoteResV2Array.data(),
                remoteResV2ArraySize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

        // 更新设备端参数中的 channelHccsRes 指针
        deviceTransportResSet.channelHccsRes = reinterpret_cast<HcclChannelHccsRes*>(deviceChannelHccsResArray.get()->ptr());
        channelParamMemVector.push_back(std::move(deviceChannelHccsResArray));
    } else {
        HCCL_ERROR("[%s]invalid hostTransportResSet", __func__);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::ChannelPackTransportHccsRes(ChannelHandle *hostChannelHandles, uint32_t listNum,
    HcclChannelTransportResSet &channelTransportResSetDevice, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u]", __func__, listNum);
    std::vector<HcclChannelHccsRes> channelHccsRes(listNum);
    HcclChannelTransportResSet channelTransportResSet;
    channelTransportResSet.listNum = listNum;
    channelTransportResSet.channelHccsRes = std::move(channelHccsRes.data());
 
    // 获取host侧序列化的地址
    for (uint32_t index = 0; index < listNum; index++) {
        auto aicpuTsHccsChannel = reinterpret_cast<AicpuTsHccsChannel *>(hostChannelHandles[index]);
        CHK_PRT(aicpuTsHccsChannel->H2DResPack(channelTransportResSet.channelHccsRes[index]));
    }

    channelTransportResSetDevice = channelTransportResSet;
    CHK_RET(DeepCopyH2DChannelParam(channelTransportResSetDevice, channelTransportResSet, channelParamMemVector));

    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::LaunchChannelKernelForTransportBase(ChannelHandle *channelHandles, ChannelHandle *hostChannelHandles,
    uint32_t listNum, aclrtBinHandle binHandle, const std::string &kernelName)
{
    HCCL_RUN_INFO("[%s] listNum[%u]", __func__, listNum);

    std::vector<std::shared_ptr<hccl::DeviceMem>> channelParamMemVector;
    HcclChannelRes channelParam{};
    CHK_SAFETY_FUNC_RET(memset_s(&channelParam, sizeof(channelParam), 0, sizeof(channelParam)));

    auto channel = reinterpret_cast<Channel *>(hostChannelHandles[0]);
    channelParam.isTransport = true;

    HcclChannelTransportResSet channelTransportResSetDevice;
    channelTransportResSetDevice.engine = channel->GetCommEngine();
    channelTransportResSetDevice.protocol = channel->GetCommProtocol();

    uint32_t resSetSize = sizeof(HcclChannelTransportResSet);

    // 分配连续的host内存，将序列化的地址放入其中
    if (channelTransportResSetDevice.protocol == COMM_PROTOCOL_HCCS) {
        CHK_RET(ChannelPackTransportHccsRes(hostChannelHandles, listNum, channelTransportResSetDevice, channelParamMemVector));
    } else {
        HCCL_ERROR("[%s]invalid channel protocol:%d", __func__, channelTransportResSetDevice.protocol);
        return HCCL_E_INTERNAL;
    }

    hccl::DeviceMem devicePackBuf = hccl::DeviceMem::alloc(sizeof(HcclChannelTransportResSet));
    CHK_PTR_NULL(devicePackBuf.ptr());

    // 将host侧序列化内容拷贝到device侧内存中
    CHK_RET(hrtMemSyncCopy(devicePackBuf.ptr(), resSetSize, &channelTransportResSetDevice, resSetSize,
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    // 为device侧的channelList分配内存
    hccl::DeviceMem deviceChannelList = hccl::DeviceMem::alloc(listNum * sizeof(ChannelHandle));
    CHK_PTR_NULL(deviceChannelList.ptr());

    // 填充channelParam参数
    CHK_RET(ChannelProcess::FillChannelParam(channelParam, "", deviceChannelList, devicePackBuf, listNum, resSetSize, 1));

    // 调用抽离的通用内核启动函数
    CHK_RET(ChannelProcess::LaunchKernel(channelParam, binHandle, kernelName));

    // 将device侧的channelList拷贝回host侧的channelList
    CHK_RET(hrtMemSyncCopy(channelHandles,
        listNum * sizeof(ChannelHandle),
        deviceChannelList.ptr(),
        listNum * sizeof(ChannelHandle),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST));

    CHK_RET(ChannelProcess::FillChannelD2HMap(channelHandles, hostChannelHandles, listNum));

    channelParamMemVector.clear();

    HCCL_INFO("[%s] channel kernel launch success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::ChannelKernelLaunchForTransport(ChannelHandle *channelHandles, 
    ChannelHandle *hostChannelHandles, uint32_t listNum, aclrtBinHandle binHandle)
{
    return LaunchChannelKernelForTransportBase(channelHandles, hostChannelHandles, listNum, 
        binHandle, "RunAicpuChannelInitV2");
}
}