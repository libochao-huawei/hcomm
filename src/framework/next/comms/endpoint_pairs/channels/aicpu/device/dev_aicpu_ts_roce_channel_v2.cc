/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dev_aicpu_ts_roce_channel_v2.h"
#include "adapter_rts_common.h"
#include "log.h"
#include "aicpu_res_package_helper.h"

 namespace Hccl {

DevAicpuTsRoceChannelV2::DevAicpuTsRoceChannelV2(std::vector<char> &uniqueId)
{
    (void)Init(uniqueId);
}

DevAicpuTsRoceChannelV2::~DevAicpuTsRoceChannelV2()
{
}

HcclResult DevAicpuTsRoceChannelV2::Init(std::vector<char> &uniqueId)
{
    BinaryStream binaryStream(uniqueId);
    u32 type;
    binaryStream >> type;
    binaryStream >> notifyNum_;
    binaryStream >> bufferNum_;
    binaryStream >> connNum_;
 
    std::vector<char> locNotifyUniqueIds;
    binaryStream >> locNotifyUniqueIds;
    ParseLocNotifyVec(locNotifyUniqueIds);

    std::vector<char> rmtNotifyUniqueIds;
    binaryStream >> rmtNotifyUniqueIds;
    ParseRmtNotifyVec(rmtNotifyUniqueIds);

    std::vector<char> notifyValueBufferUniqueIds;
    binaryStream >> notifyValueBufferUniqueIds;
    ParseNotifyValueBuffer(notifyValueBufferUniqueIds);
 
    std::vector<char> locBufferUniqueIds;
    binaryStream >> locBufferUniqueIds;
    ParseLocBufferVec(locBufferUniqueIds);
 
    std::vector<char> rmtBufferUniqueIds;
    binaryStream >> rmtBufferUniqueIds;
    ParseRmtBufferVec(rmtBufferUniqueIds);

    std::vector<char> connUniqueIds;
    binaryStream >> connUniqueIds;
    ParseConnVec(connUniqueIds);

    return HCCL_SUCCESS;
}

void DevAicpuTsRoceChannelV2::ParseLocNotifyVec(std::vector<char> &data)
{
    if (notifyNum_ == 0) {
        HCCL_WARNING("[DevAicpuTsRoceChannelV2::%s] notifyNum is 0", __func__);
        return;
    }

    u32 notifySizePerDto = data.size() / notifyNum_;

    for (u32 idx = 0; idx < notifyNum_; idx++) {
        auto              start = data.begin() + idx * notifySizePerDto;
        auto              end   = start + notifySizePerDto;
        std::vector<char> dto(start, end);
        localNotifies_.push_back(std::make_unique<NotifyLite>(dto));
        HCCL_INFO("locNotify idx=%u, %s", idx, localNotifies_.back()->Describe().c_str());
    }
}

void DevAicpuTsRoceChannelV2::ParseRmtNotifyVec(std::vector<char> &data)
{
    if (notifyNum_ == 0) {
        HCCL_WARNING("[DevAicpuTsRoceChannelV2::%s] notifyNum is 0", __func__);
        return;
    }

    u32 rmtBufferSizePerDto = data.size() / notifyNum_;
    HCCL_INFO("[DevAicpuTsRoceChannelV2::%s] Parse remote notify num=%u, sizePerDto=%u",
        __func__, notifyNum_, rmtBufferSizePerDto);

    BinaryStream binaryStream(data);
    remoteNotifies_.clear();
    u64 addr;
    u64 size;
    u32 rkey;
    for (u32 idx = 0; idx < notifyNum_; idx++) {
        binaryStream >> addr;
        binaryStream >> size;
        binaryStream >> rkey;
        RmtRmaBufferLite rdmaBufLite(addr, size, rkey);
        HCCL_INFO("idx=%u, %s", idx, rdmaBufLite.Describe().c_str());
        remoteNotifies_.emplace_back(rdmaBufLite);
    }
}

void DevAicpuTsRoceChannelV2::ParseNotifyValueBuffer(std::vector<char> &data)
{
    HCCL_INFO("[DevAicpuTsRoceChannelV2::%s] Parse notify value buffer",  __func__);

    BinaryStream binaryStream(data);
    u64 addr;
    u64 size;
    u32 lkey;
    binaryStream >> addr;
    binaryStream >> size;
    binaryStream >> lkey;
    notifyValueBuffer_ = std::make_unique<RmaBufferLite>(addr, size, lkey);
}

void DevAicpuTsRoceChannelV2::ParseLocBufferVec(std::vector<char> &data)
{
    if (bufferNum_ == 0) {
        HCCL_WARNING("[DevAicpuTsRoceChannelV2::%s] bufferNum is 0", __func__);
        return;
    }

    u32 locBufferSizePerDto = data.size() / bufferNum_;
    HCCL_INFO("[DevAicpuTsRoceChannelV2::%s] Parse local buffer num=%u, sizePerDto=%u",
        __func__, bufferNum_, locBufferSizePerDto);

    BinaryStream binaryStream(data);
    locBufferVec_.clear();
    u64 addr;
    u64 size;
    u32 lkey;
    for (u32 idx = 0; idx < bufferNum_; idx++) {
        binaryStream >> addr;
        binaryStream >> size;
        binaryStream >> lkey;
        RmaBufferLite rdmaBufLite(addr, size, lkey);
        HCCL_INFO("idx=%u, %s", idx, rdmaBufLite.Describe().c_str());
        locBufferVec_.emplace_back(rdmaBufLite);
    }
}

void DevAicpuTsRoceChannelV2::ParseRmtBufferVec(std::vector<char> &data)
{
    if (bufferNum_ == 0) {
        HCCL_WARNING("[DevAicpuTsRoceChannelV2::%s] bufferNum is 0", __func__);
        return;
    }

    u32 rmtBufferSizePerDto = data.size() / bufferNum_;
    HCCL_INFO("[DevAicpuTsRoceChannelV2::%s] Parse remote buffer num=%u, sizePerDto=%u",
        __func__, bufferNum_, rmtBufferSizePerDto);

    BinaryStream binaryStream(data);
    rmtBufferVec_.clear();
    u64 addr;
    u64 size;
    u32 rkey;
    for (u32 idx = 0; idx < bufferNum_; idx++) {
        binaryStream >> addr;
        binaryStream >> size;
        binaryStream >> rkey;
        RmtRmaBufferLite rdmaBufLite(addr, size, rkey);
        HCCL_INFO("idx=%u, %s", idx, rdmaBufLite.Describe().c_str());
        rmtBufferVec_.emplace_back(rdmaBufLite);
    }
}

void DevAicpuTsRoceChannelV2::ParseConnVec(std::vector<char> &data)
{
    if (connNum_ == 0) {
        HCCL_WARNING("[DevAicpuTsRoceChannelV2::%s] connNum is 0", __func__);
        return;
    }

    u32 connSizePerDto = data.size() / connNum_;
    HCCL_INFO("[DevAicpuTsRoceChannelV2::%s] Parse conn num=%u, sizePerDto=%u",
        __func__, connNum_, connSizePerDto);
    for (u32 idx = 0; idx < connNum_; idx++) {
        auto              start = data.begin() + idx * connSizePerDto;
        auto              end   = start + connSizePerDto;
        std::vector<char> connUniqueId(start, end);
        connUniqueIdVec_.emplace_back(connUniqueId);
        std::unique_ptr<RdmaConnLite> connLite;
        connLite = std::make_unique<RdmaConnLite>(connUniqueId);
        HCCL_INFO("[DevAicpuTsRoceChannelV2::%s] idx=%u, %s", __func__, idx, connLite->Describe().c_str());
        connVec_.emplace_back(std::move(connLite));
    }
}

RmaBufSliceLite DevAicpuTsRoceChannelV2::GetRmaBufSlicelite(const RmaBufferLite &lite) const
{
    return RmaBufSliceLite(lite.GetAddr(), lite.GetSize(), lite.GetLkey(), 0);
}

RmtRmaBufSliceLite DevAicpuTsRoceChannelV2::GetRmtRmaBufSliceLite(const RmtRmaBufferLite &lite) const
{
    return RmtRmaBufSliceLite(lite.GetAddr(), lite.GetSize(), lite.GetRkey(), 0, 0);
}

RmtRmaBufSliceLite DevAicpuTsRoceChannelV2::GetRmtNotifySliceLite(u32 index) const
{
    auto &lite = remoteNotifies_[index];
    return RmtRmaBufSliceLite(lite.GetAddr(), lite.GetSize(), lite.GetRkey(), 0, 0);
}

std::string DevAicpuTsRoceChannelV2::Describe() const
{
    std::string desc = "DevAicpuTsRoceChannelV2[";

    u32 idx = 0;
    desc += "localNotifies=[";
    for (auto &it : localNotifies_) {
        desc += StringFormat("idx=%u, %u;", idx, it->Describe().c_str());
        idx++;
    }

    idx = 0;
    desc += "], remoteNotifies=[";
    for (auto &it : remoteNotifies_) {
        desc += StringFormat("idx=%u, %u;", idx, it.Describe().c_str());
        idx++;
    }

    idx = 0;
    desc += "], locBufferVec=[";
    for (auto &it : locBufferVec_) {
        desc += StringFormat("idx=%u, %s;", idx, it.Describe().c_str());
        idx++;
    }

    idx = 0;
    desc += "], rmtBufferVec=[";
    for (auto &it : rmtBufferVec_) {
        desc += StringFormat("idx=%u, %s;", idx, it.Describe().c_str());
        idx++;
    }

    idx = 0;
    desc += "], connVec=[";
    for (auto &it : connVec_) {
        desc += StringFormat("idx=%u, %s;", idx, it->Describe().c_str());
        idx++;
    }

    desc += "]]";
    return desc;
}

HcclResult DevAicpuTsRoceChannelV2::Create(const void *blob, u64 blobBytes,
    const HcommDeviceInfo &deviceInfo, ChannelHandle &outHandle)
{
    CHK_PTR_NULL(blob);
    CHK_PRT_RET(blobBytes == 0,
        HCCL_ERROR("[DevAicpuTsRoceChannelV2][Create] blobBytes is 0"), HCCL_E_PARA);

    std::vector<char> data(blobBytes);
    CHK_SAFETY_FUNC_RET(memcpy_s(data.data(), data.size(), blob, blobBytes));

    Hccl::AicpuResPackageHelper helper;
    auto dataVec = helper.ParsePackedData(data);

    Hccl::AicpuResMgrType resType = Hccl::AicpuResMgrType::STREAM;
    if (static_cast<u32>(resType) >= dataVec.size()) {
        HCCL_ERROR("[DevAicpuTsRoceChannelV2][%s] fail, resType[%d], dataVec size[%u]", __func__, resType, dataVec.size());
        return HCCL_E_PARA;
    }

    auto devChannel = std::make_unique<DevAicpuTsRoceChannelV2>();
    CHK_PTR_NULL(devChannel);
    CHK_RET(devChannel->Init(dataVec[resType].data));

    outHandle = reinterpret_cast<ChannelHandle>(devChannel.release());

    HCCL_INFO("[DevAicpuTsRoceChannelV2][Create] success blobBytes[%llu] handle[0x%llx]",
        static_cast<unsigned long long>(blobBytes),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(outHandle)));
    return HCCL_SUCCESS;
}

bool DevAicpuTsRoceChannelV2::Destroy(ChannelHandle handle)
{
    auto *devChannel = reinterpret_cast<DevAicpuTsRoceChannelV2 *>(handle);
    delete devChannel;
    HCCL_DEBUG("[DevAicpuTsRoceChannelV2][Destroy] destroyed handle[0x%llx]", handle);
    return true;
}

}
