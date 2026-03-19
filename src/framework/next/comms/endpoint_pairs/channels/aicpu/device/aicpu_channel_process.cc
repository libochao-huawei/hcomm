/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_channel_process.h"
#include "aicpu_res_package_helper.h"
#include "aicpu_channel_transport_process.h"

using namespace hccl;

std::mutex AicpuChannelProcess::mutex_;
std::unordered_map<ChannelHandle, std::unique_ptr<Hccl::UbTransportLiteImpl>> 
    AicpuChannelProcess::ubTransportMap_;
std::unordered_map<ChannelHandle, bool> AicpuChannelProcess::handleMap_;
std::unordered_map<ChannelHandle, std::unique_ptr<hccl::Transport>> AicpuChannelProcess::transportMap_;

HcclResult AicpuChannelProcess::ParseUrmaPackData(std::vector<char> &data, ChannelHandle &handle)
{
    HCCL_DEBUG("[HcclCommAicpu][%s] data: ptr[%p], size[%u]", __func__, data.data(), data.size());
    Hccl::BinaryStream binaryStream(data);

    std::vector<char> transpUniqueId;
    binaryStream >> transpUniqueId;

    std::unique_ptr<Hccl::UbTransportLiteImpl> ubTransportLiteImpl;
    EXECEPTION_CATCH((ubTransportLiteImpl = std::make_unique<Hccl::UbTransportLiteImpl>(transpUniqueId)),
        return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(ubTransportLiteImpl);

    handle = reinterpret_cast<uint64_t>(ubTransportLiteImpl.get());
    ubTransportMap_.insert({handle, std::move(ubTransportLiteImpl)});

    return HCCL_SUCCESS;
}

HcclResult AicpuChannelProcess::InitUrmaChannel(HcclChannelRes *commParam)
{
    HCCL_INFO("[HcclCommAicpu][%s] commParam->uniqueIdAddr[%p], commParam->uniqueIdSize[%u]",
        __func__, commParam->uniqueIdAddr, commParam->uniqueIdSize);

    u8* currentSrcAddr = reinterpret_cast<u8*>(commParam->uniqueIdAddr);
    u32* addSize = reinterpret_cast<u32*>(commParam->channelSizeAddr);
    for (u32 index = 0; index < commParam->listNum; index++) {
        std::vector<char> data(*addSize);

        // 计算地址块的偏移
        CHK_SAFETY_FUNC_RET(memcpy_s(data.data(), data.size(), currentSrcAddr, *addSize));
        currentSrcAddr += *addSize;
        addSize++;
        // 反序列化得到device侧transport对象
        Hccl::AicpuResPackageHelper helper;
        auto dataVec = helper.ParsePackedData(data);

        Hccl::AicpuResMgrType resType = Hccl::AicpuResMgrType::STREAM; // todo 待修改
        if (static_cast<u32>(resType) >= dataVec.size()) {
            HCCL_ERROR("[HcclCommAicpu][%s] fail, resType[%d], dataVec size[%u]", __func__, resType, dataVec.size());
            return HCCL_E_PARA;
        }
        ChannelHandle channelHandle;
        CHK_RET(ParseUrmaPackData(dataVec[resType].data, channelHandle));

        // 恢复出的channelHandle回填到commParam中
        ChannelHandle* channelList = reinterpret_cast<ChannelHandle*>(commParam->channelList);
        channelList[index] = channelHandle;
        HCCL_INFO("[HcclCommAicpu][%s] index[%u], currentSrcAddr[%p], channelSizeAddr[%p], channelHandle[0x%llx]",
            __func__, index, currentSrcAddr, commParam->channelSizeAddr, channelHandle);
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuChannelProcess::InitTransportChannel(HcclChannelRes *commParam)
{
    CHK_PTR_NULL(commParam->uniqueIdAddr);
    HCCL_INFO("[InitHccsChannel][%s] commParam->uniqueIdAddr[%p], commParam->uniqueIdSize[%u]",
        __func__, commParam->uniqueIdAddr, commParam->uniqueIdSize);

    // get host handle info from commParam->uniqueIdAddr
    HcclChannelTransportResSet *transportResSet = reinterpret_cast<HcclChannelTransportResSet *>(commParam->uniqueIdAddr);;
    HCCL_INFO("[InitHccsChannel][%s] listNum:%u", __func__, transportResSet->listNum);

    for (u32 index = 0; index < transportResSet->listNum; index++) {
        // 反序列化得到device侧transport对象
        ChannelHandle channelHandle;
        std::unique_ptr<hccl::Transport> transport;

        CHK_RET(AicpuChannelTransportProcess::InitTransportChannel(transportResSet, index, transport));

        channelHandle = reinterpret_cast<uint64_t>(transport.get());
        transportMap_.insert({channelHandle, std::move(transport)});
        handleMap_.insert({channelHandle, true});

        // 恢复出的channelHandle回填到commParam中
        ChannelHandle* channelList = reinterpret_cast<ChannelHandle*>(commParam->channelList);
        channelList[index] = channelHandle;
        HCCL_INFO("[HcclCommAicpu][%s] index[%u], singleUniqueIdSize[%u], channelHandle[0x%llx]",
            __func__, index, commParam->singleUniqueIdSize, channelHandle);
    }
    HCCL_INFO("[InitHccsChannel][%s] commParam->uniqueIdAddr[%p], commParam->uniqueIdSize[%u] done",
        __func__, commParam->uniqueIdAddr, commParam->uniqueIdSize);
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelProcess::AicpuChannelInit(HcclChannelRes *commParam)
{
    HCCL_INFO("[AicpuChannelProcess][%s] channelList[%p], listNum[%u], uniqueIdAddr[%p], "
        "uniqueIdSize[%u] isTransport [%u]",
        __func__, commParam->channelList, commParam->listNum, commParam->uniqueIdAddr,
        commParam->uniqueIdSize, static_cast<u32>(commParam->isTransport));

    CHK_RET(hrtSetWorkModeAicpu(true));
    CHK_RET(hrtSetlocalDevice(commParam->deviceLogicId));
    CHK_RET(hrtSetlocalDeviceType(static_cast<DevType>(commParam->deviceType)));

    std::lock_guard<std::mutex> addLock(mutex_);

    HcclResult ret = HCCL_SUCCESS;
    if (commParam->isTransport) {
        ret = InitTransportChannel(commParam);
    } else {
        ret = InitUrmaChannel(commParam);
    }

    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[AicpuChannelProcess][AicpuChannelInit]errNo[0x%016llx] Failed to init channels",
        HCCL_ERROR_CODE(ret)), ret);

    HCCL_INFO("[AicpuChannelProcess][%s] aicpuTask End.", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelProcess::AicpuChannelDestroy(HcclChannelRes *commParam)
{
    HCCL_INFO("[AicpuChannelProcess][%s] commParam->channelList[%p], commParam->listNum[%u]",
              __func__, commParam->channelList, commParam->listNum);

    // 加锁保护 xxxMap_
    std::lock_guard<std::mutex> addLock(mutex_);

    ChannelHandle* channelList = reinterpret_cast<ChannelHandle*>(commParam->channelList);
    for (u32 index = 0; index < commParam->listNum; ++index) {
        ChannelHandle handle = channelList[index];

        // can not get from commParam->isTransport when destroy
        bool isTransport = false;
        auto handleIt = handleMap_.find(handle);
        if (handleIt != handleMap_.end()) {
            isTransport = handleIt->second;
            handleMap_.erase(handleIt);
        }

        // 3. 从 map 中移除条目，unique_ptr 会自动释放对象
        if (isTransport) {
            auto transportIt = transportMap_.find(handle);
            if (transportIt == transportMap_.end()) {
                // 理论上每个 handle 都应该存在，若不存在可能是重复销毁或逻辑错误
                HCCL_WARNING("[AicpuChannelProcess][%s] handle[0x%llx] not found in transportMap_, maybe already destroyed?",
                        __func__, handle);
                continue; // 容错处理：继续销毁其他 channel，不中断流程
            }
            // 销毁底层 transport 资源
            transportMap_.erase(transportIt);
        // default
        } else {
            auto it = ubTransportMap_.find(handle);
            if (it == ubTransportMap_.end()) {
                // 理论上每个 handle 都应该存在，若不存在可能是重复销毁或逻辑错误
                HCCL_WARNING("[AicpuChannelProcess][%s] handle[0x%llx] not found in ubTransportMap_, maybe already destroyed?",
                        __func__, handle);
                continue; // 容错处理：继续销毁其他 channel，不中断流程
            }
            // 销毁底层 UbTransportLiteImpl 资源
            ubTransportMap_.erase(it);
        }

        HCCL_DEBUG("[AicpuChannelProcess][%s] destroyed handle[0x%llx]", __func__, handle);
    }

    HCCL_INFO("[AicpuChannelProcess][%s] aicpuTask End.", __func__);
    return HCCL_SUCCESS;
}
