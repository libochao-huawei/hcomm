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

HcclResult ChannelTransportProcess::DeepCopyH2DChannelHccsRes(const HcclChannelHccsRes &hostChannelHccsRes, 
    HcclChannelHccsRes &deviceChannelHccsRes, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s]", __func__);
    // 复制基本成员
    deviceChannelHccsRes = hostChannelHccsRes;

    std::shared_ptr<hccl::DeviceMem> deviceMem = nullptr;
    if (hostChannelHccsRes.localBufSize > 0 && hostChannelHccsRes.localBufMem != nullptr) {
        size_t localBufSize = hostChannelHccsRes.localBufSize * sizeof(HcclMemEx);
        EXECEPTION_CATCH((deviceMem = std::make_shared<hccl::DeviceMem>(hccl::DeviceMem::alloc(localBufSize))),
                            return HCCL_E_PTR);
        CHK_RET(hrtMemSyncCopy(deviceMem.get()->ptr(), localBufSize, hostChannelHccsRes.localBufMem,
            localBufSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
        deviceChannelHccsRes.localBufMem = reinterpret_cast<HcclMemEx*>(deviceMem.get()->ptr());
        channelParamMemVector.push_back(std::move(deviceMem));
    }

    if (hostChannelHccsRes.remoteBufSize > 0 && hostChannelHccsRes.remoteBufMem != nullptr) {
        size_t remoteBufSize = hostChannelHccsRes.remoteBufSize * sizeof(HcclMemEx);
        EXECEPTION_CATCH((deviceMem = std::make_shared<hccl::DeviceMem>(hccl::DeviceMem::alloc(remoteBufSize))),
                            return HCCL_E_PTR);
        CHK_RET(hrtMemSyncCopy(deviceMem.get()->ptr(), remoteBufSize, hostChannelHccsRes.remoteBufMem,
            remoteBufSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
        deviceChannelHccsRes.remoteBufMem = reinterpret_cast<HcclMemEx*>(deviceMem.get()->ptr());
        channelParamMemVector.push_back(std::move(deviceMem));
    }
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::DeepCopyH2DChannelParam(HcclChannelTransportResSet &channelTransportResSetHost,
    HcclChannelTransportResSet &channelTransportResSetDevice,
    std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] enter", __func__);

    channelTransportResSetDevice = channelTransportResSetHost;

    // 拷贝remoteResV2
    if (channelTransportResSetHost.channelHccsRes != nullptr && channelTransportResSetHost.listNum > 0) {
        // 为设备端的 channelHccsRes 数组分配内存（注意：这个数组存放的是 HcclChannelHccsRes 结构体）
        size_t remoteResV2ArraySize = sizeof(HcclChannelHccsRes) * channelTransportResSetHost.listNum;
        std::shared_ptr<hccl::DeviceMem> deviceChannelHccsResArray;
        EXECEPTION_CATCH(
            (deviceChannelHccsResArray = std::make_shared<hccl::DeviceMem>(hccl::DeviceMem::alloc(remoteResV2ArraySize))),
            return HCCL_E_PTR);

        // 为每个数组元素进行深度拷贝，并保存设备内存和主机结构体（指针已调整）
        std::vector<hccl::DeviceMem> elementMemories; // 保存每个元素分配的设备内存（包括内部指针数据）
        std::vector<HcclChannelHccsRes> channelHccsResArray(channelTransportResSetHost.listNum);

        for (uint32_t i = 0; i < channelTransportResSetHost.listNum; ++i) {
            HcclChannelHccsRes channelHccsResHost = channelTransportResSetHost.channelHccsRes[i];
            HcclChannelHccsRes channelHccsResDevice;
            // 深度拷贝一个元素到设备内存，并返回设备内存中的结构体布局（host端）
            CHK_RET(DeepCopyH2DChannelHccsRes(channelHccsResHost, channelHccsResDevice, channelParamMemVector));
            // 保存调整后的主机端结构体（其指针指向设备内存）
            channelHccsResArray[i] = channelHccsResDevice;
        }

        // 将主机端的结构体数组（指针已调整）拷贝到设备内存数组
        CHK_RET(hrtMemSyncCopy(deviceChannelHccsResArray.get()->ptr(), remoteResV2ArraySize, channelHccsResArray.data(),
                remoteResV2ArraySize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

        // 更新设备端参数中的 channelHccsRes 指针
        channelTransportResSetDevice.channelHccsRes =
            reinterpret_cast<HcclChannelHccsRes*>(deviceChannelHccsResArray.get()->ptr());
        channelParamMemVector.push_back(std::move(deviceChannelHccsResArray));
    } else {
        HCCL_ERROR("[%s]invalid channelTransportResSetHost", __func__);
        return HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO("[%s] done", __func__);
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::ChannelPackTransportHccsResToDev(ChannelHandle *hostChannelHandles,
    uint32_t listNum, HcclChannelTransportResSet &channelTransportResSetDevice,
    std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u] enter", __func__, listNum);

    auto channel = reinterpret_cast<Channel *>(hostChannelHandles[0]);
    CHK_PTR_NULL(channel);

    HcclChannelTransportResSet channelTransportResSetHost;
    channelTransportResSetHost.protocol = channel->GetCommProtocol();
    channelTransportResSetHost.engine = channel->GetCommEngine();
    channelTransportResSetHost.listNum = listNum;

    std::vector<char> transportResSetData(sizeof(HcclChannelHccsRes) * listNum);
    channelTransportResSetHost.channelHccsRes = reinterpret_cast<HcclChannelHccsRes *>(transportResSetData.data());

    // 获取host侧序列化的地址
    for (uint32_t index = 0; index < channelTransportResSetHost.listNum; index++) {
        auto aicpuTsHccsChannel = reinterpret_cast<AicpuTsHccsChannel *>(hostChannelHandles[index]);
        CHK_PRT(aicpuTsHccsChannel->PackToHccsRes(channelTransportResSetHost.channelHccsRes[index]));
    }

    CHK_RET(DeepCopyH2DChannelParam(channelTransportResSetHost, channelTransportResSetDevice, channelParamMemVector));

    HCCL_RUN_INFO("[%s] listNum[%u] done", __func__, listNum);
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::ChannelPackTransportResSetToDev(ChannelHandle *hostChannelHandles, uint32_t listNum,
    hccl::DeviceMem &devicePackBuf, uint32_t &resSetSize,
    std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u] resSetSize[%u] enter", __func__, listNum, resSetSize);
    auto channel = reinterpret_cast<Channel *>(hostChannelHandles[0]);
    CHK_PTR_NULL(channel);

    HcclChannelTransportResSet channelTransportResSetDevice;

    CommProtocol protocol = channel->GetCommProtocol();
    // 分配连续的host内存，将序列化的地址放入其中
    if (protocol == COMM_PROTOCOL_HCCS) {
        CHK_RET(ChannelPackTransportHccsResToDev(hostChannelHandles, listNum, channelTransportResSetDevice,
            channelParamMemVector));
    } else {
        HCCL_ERROR("[%s]invalid channel protocol:%d", __func__, protocol);
        return HCCL_E_INTERNAL;
    }

    HCCL_INFO("[ChannelTransportProcess][%s] engine[%u] protocol[%u].", __func__,
        static_cast<u32>(channelTransportResSetDevice.engine), static_cast<u32>(channelTransportResSetDevice.protocol));

    resSetSize = sizeof(HcclChannelTransportResSet);
    // 将host侧序列化内容拷贝到device侧内存中
    CHK_RET(hrtMemSyncCopy(devicePackBuf.ptr(), resSetSize, &channelTransportResSetDevice, resSetSize,
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    HCCL_RUN_INFO("[%s] listNum[%u] resSetSize[%u] done", __func__, listNum, resSetSize);
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::LaunchChannelKernelForTransportBase(ChannelHandle *channelHandles,
    ChannelHandle *hostChannelHandles, uint32_t listNum, aclrtBinHandle binHandle,
    std::vector<std::shared_ptr<hccl::DeviceMem>> channelParamMemVector)
{
    HCCL_RUN_INFO("[%s] listNum[%u]", __func__, listNum);

    hccl::DeviceMem devicePackBuf = hccl::DeviceMem::alloc(sizeof(HcclChannelTransportResSet));
    CHK_PTR_NULL(devicePackBuf.ptr());

    uint32_t resSetSize = 0;
    CHK_RET(ChannelPackTransportResSetToDev(hostChannelHandles, listNum, devicePackBuf, resSetSize, channelParamMemVector));

    // 为device侧的channelList分配内存
    hccl::DeviceMem deviceChannelList = hccl::DeviceMem::alloc(listNum * sizeof(ChannelHandle));
    CHK_PTR_NULL(deviceChannelList.ptr());

    // 填充channelParam参数
    HcclChannelRes channelParam{};
    CHK_SAFETY_FUNC_RET(memset_s(&channelParam, sizeof(channelParam), 0, sizeof(channelParam)));
    channelParam.isTransport = true;
    
    hccl::DeviceMem deviceChannelList();
    CHK_RET(ChannelProcess::FillChannelParam(channelParam, "", deviceChannelList, devicePackBuf,
        listNum, resSetSize, channelSizeAddr));

    // 调用抽离的通用内核启动函数
    CHK_RET(ChannelProcess::LaunchKernel(channelParam, binHandle, "RunAicpuChannelInitV2"));

    // 将device侧的channelList拷贝回host侧的channelList
    CHK_RET(hrtMemSyncCopy(channelHandles,
        listNum * sizeof(ChannelHandle),
        deviceChannelList.ptr(),
        listNum * sizeof(ChannelHandle),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST));

    CHK_RET(ChannelProcess::FillChannelD2HMap(channelHandles, hostChannelHandles, listNum));

    HCCL_INFO("[%s] channel kernel launch success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult ChannelTransportProcess::ChannelKernelLaunchForTransport(ChannelHandle *channelHandles, 
    ChannelHandle *hostChannelHandles, uint32_t listNum, aclrtBinHandle binHandle)
{
    std::vector<std::shared_ptr<hccl::DeviceMem>> channelParamMemVector;
    HcclResult ret = LaunchChannelKernelForTransportBase(channelHandles, hostChannelHandles, listNum, 
        binHandle, channelParamMemVector);
    channelParamMemVector.clear();
    return ret;
}
}