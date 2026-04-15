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
#include "aicpu_channel_res_handler.h"
#include "aicpu_res_package_helper.h"

#include "adapter_rts_common.h"
#include "exception_handler.h"
#include "log.h"

#include <securec.h>

#include <cstdint>
#include <vector>

std::mutex AicpuChannelProcess::mutex_;
std::unordered_map<ChannelHandle, std::unique_ptr<Hccl::UbTransportLiteImpl>> AicpuChannelProcess::ubTransportMap_;

HcclResult AicpuChannelProcess::ParsePackData(std::vector<char> &data, ChannelHandle &handle)
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

HcclResult AicpuChannelProcess::InitUrmaChannel(HcclChannelUrmaRes *commParam)
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
        CHK_RET(ParsePackData(dataVec[resType].data, channelHandle));

        // 恢复出的channelHandle回填到commParam中
        ChannelHandle* channelList = reinterpret_cast<ChannelHandle*>(commParam->channelList);
        channelList[index] = channelHandle;
        HCCL_INFO("[HcclCommAicpu][%s] index[%u], currentSrcAddr[%p], channelSizeAddr[%p], channelHandle[0x%llx]",
            __func__, index, currentSrcAddr, commParam->channelSizeAddr, channelHandle);
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuChannelProcess::AicpuChannelInit(HcclChannelUrmaRes *commParam)
{
    HCCL_INFO("[AicpuChannelProcess][%s] commParam->channelList[%p], commParam->listNum[%u], commParam->uniqueIdAddr[%p], "
        "commParam->uniqueIdSize[%u]", __func__, commParam->channelList, commParam->listNum, commParam->uniqueIdAddr,
        commParam->uniqueIdSize);

    CHK_RET(hrtSetWorkModeAicpu(true));
    CHK_RET(hrtSetlocalDevice(commParam->deviceLogicId));
    CHK_RET(hrtSetlocalDeviceType(static_cast<DevType>(commParam->deviceType)));

    std::lock_guard<std::mutex> addLock(mutex_);

    HcclResult ret = InitUrmaChannel(commParam);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[AicpuChannelProcess][AicpuChannelInit]errNo[0x%016llx] Failed to init channels",
        HCCL_ERROR_CODE(ret)), ret);

    HCCL_INFO("[AicpuChannelProcess][%s] aicpuTask End.", __func__);
    return HCCL_SUCCESS;
}

namespace {

struct AicpuChannelResRollbackEntry {
    ChannelHandle handle;
    uint32_t kind;
};

void AicpuChannelResRollback(const std::vector<AicpuChannelResRollbackEntry> &rollback)
{
    const auto &handlers = GetAicpuChannelResHandlers();
    for (const auto &e : rollback) {
        auto hit = handlers.find(e.kind);
        if (hit != handlers.end()) {
            (void)hit->second->Destroy(e.handle);
        }
    }
}

} // namespace

HcclResult AicpuChannelProcess::InitHcommChannelRes(HcommChannelRes *commParam)
{
    CHK_PTR_NULL(commParam);
    HCCL_INFO("[AicpuChannelProcess][%s] channelList[%p], listNum[%u]", __func__, commParam->channelList,
        commParam->listNum);

    CHK_PTR_NULL(commParam->channelList);
    CHK_PTR_NULL(commParam->channelDataListAddr);
    CHK_PTR_NULL(commParam->channelDataSizeListAddr);
    CHK_PTR_NULL(commParam->channelTypeListAddr);

    CHK_RET(hrtSetWorkModeAicpu(true));
    CHK_RET(hrtSetlocalDevice(commParam->deviceInfo.deviceLogicId));
    CHK_RET(hrtSetlocalDeviceType(static_cast<DevType>(commParam->deviceInfo.deviceType)));

    void **dataList = reinterpret_cast<void **>(commParam->channelDataListAddr);
    auto *sizeList = reinterpret_cast<u64 *>(commParam->channelDataSizeListAddr);
    auto *typeList = reinterpret_cast<u32 *>(commParam->channelTypeListAddr);
    auto *channelList = reinterpret_cast<ChannelHandle *>(commParam->channelList);

    const auto &handlers = GetAicpuChannelResHandlers();
    std::vector<AicpuChannelResRollbackEntry> rollback;
    rollback.reserve(commParam->listNum);

    std::lock_guard<std::mutex> addLock(mutex_);
    for (u32 index = 0; index < commParam->listNum; ++index) {
        const u32 kind = typeList[index];
        auto hit = handlers.find(kind);
        if (hit == handlers.end()) {
            HCCL_ERROR("[AicpuChannelProcess][%s] index[%u] unsupported kind[%u]", __func__, index, kind);
            AicpuChannelResRollback(rollback);
            return HCCL_E_NOT_SUPPORT;
        }
        void *dp = dataList[index];
        CHK_PTR_NULL(dp);
        const u64 sz = sizeList[index];
        ChannelHandle h{};
        HcclResult pret = hit->second->Parse(dp, sz, commParam->deviceInfo, h);
        if (pret != HCCL_SUCCESS) {
            HCCL_ERROR("[AicpuChannelProcess][InitHcommChannelRes] parse fail at index[%u]", index);
            AicpuChannelResRollback(rollback);
            return pret;
        }
        channelList[index] = h;
        rollback.push_back(AicpuChannelResRollbackEntry{h, kind});
        HCCL_INFO("[AicpuChannelProcess][%s] index[%u] channelHandle[0x%llx]", __func__, index, h);
    }

    HCCL_INFO("[AicpuChannelProcess][%s] aicpuTask End.", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuChannelProcess::AicpuChannelDestroy(HcclChannelUrmaRes *commParam)
{
    HCCL_INFO("[AicpuChannelProcess][%s] commParam->channelList[%p], commParam->listNum[%u]",
              __func__, commParam->channelList, commParam->listNum);

    std::lock_guard<std::mutex> addLock(mutex_);

    ChannelHandle* channelList = reinterpret_cast<ChannelHandle*>(commParam->channelList);
    for (u32 index = 0; index < commParam->listNum; ++index) {
        ChannelHandle handle = channelList[index];

        auto it = ubTransportMap_.find(handle);
        if (it != ubTransportMap_.end()) {
            ubTransportMap_.erase(it);
            HCCL_DEBUG("[AicpuChannelProcess][%s] destroyed ub handle[0x%llx]", __func__, handle);
            continue;
        }

        if (AicpuChannelResDestroyForHandle(handle)) {
            HCCL_DEBUG("[AicpuChannelProcess][%s] destroyed hcomm res handle[0x%llx]", __func__, handle);
            continue;
        }

        HCCL_WARNING("[AicpuChannelProcess][%s] handle[0x%llx] not found in ub/hcomm maps, maybe already destroyed?",
            __func__, handle);
    }

    HCCL_INFO("[AicpuChannelProcess][%s] aicpuTask End.", __func__);
    return HCCL_SUCCESS;
}
