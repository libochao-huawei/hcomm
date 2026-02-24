/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_comm_aicpu.h"
#include "aicpu_operator_pub.h"
#include "adapter_hal_pub.h"
#include "aicpu_ts_thread.h"
#include "aicpu_res_package_helper.h"
#include "ub_transport_lite_impl.h"

constexpr u32 NOTIFY_SIZE_EIGHT = 8;

HcclResult CollCommAicpu::InitAicpuIndOp(CommAicpuParam *commAicpuParam)
{
    if (indOpCommInitialized_) {
        HCCL_RUN_INFO("[%s][InitAicpuIndOp]Group[%s] already initialized, skip reinit", __func__,
            identifier_.c_str());
        return HCCL_SUCCESS;
    }
    CHK_PTR_NULL(commAicpuParam);
    topoInfo_.deviceLogicId = commAicpuParam->deviceLogicId;
    topoInfo_.devicePhyId = commAicpuParam->devicePhyId;
    topoInfo_.deviceType = static_cast<DevType>(commAicpuParam->deviceType);
    identifier_ = std::string(commAicpuParam->hcomId);
    topoInfo_.userRankSize = commAicpuParam->userRankSize;
    topoInfo_.userRank = commAicpuParam->userRank; 
    notifys_.reserve(LOCAL_NOTIFY_MAX_NUM);
    notifySize_ = NOTIFY_SIZE_EIGHT;

    CHK_RET(hrtSetWorkModeAicpu(true)); // ??
    CHK_RET(hrtSetlocalDevice(topoInfo_.deviceLogicId)); // ??
    CHK_RET(hrtSetlocalDeviceType(topoInfo_.deviceType)); // ??
    CHK_RET(hrtDrvGetLocalDevIDByHostDevID(topoInfo_.devicePhyId, &devId_)); // ??
    CHK_RET(taskExecption_.Init(devId_, localUserRank_, identifier_)); // ??
    // CHK_RET(RegisterProfCallBack());

    HCCL_INFO("[HcclCommAicpu][InitAicpuIndOp] InitAicpuIndOpV2 start");
    indOpCommInitialized_ = true;
    return HCCL_SUCCESS;
}

HcclResult CollCommAicpu::InitThreads(ThreadMgrAicpuParam *param)
{
    u32 threadNum = param->threadNum;
    std::vector<std::shared_ptr<Thread>> outThreads;
    outThreads.reserve(threadNum);
    std::string hcomId(param->hcomId);
    for (u32 i = 0; i < threadNum; ++i) {
        std::string thdUniqueId(param->threadParam[i], THREAD_UNIQUE_ID_MAX_SIZE);
        if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_INFO))) {
            std::ostringstream oss;
            oss << "threadParam[" << i << "] raw bytes: ";
            for (u32 j = 0; j < THREAD_UNIQUE_ID_MAX_SIZE; ++j) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned int>(static_cast<unsigned char>(param->threadParam[i][j])) << " ";
            }
            HCCL_INFO("[HcclCommAicpu][%s] %s", __func__, oss.str().c_str());
        }
        std::shared_ptr<AicpuTsThread> thread;
        EXECEPTION_CATCH((thread = std::make_shared<AicpuTsThread>(thdUniqueId)), return HCCL_E_PTR);
        HcclResult ret = thread->Init();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[HcclCommAicpu][%s] comm identifier[%s], init threads num[%u] failed at index %u",
                __func__, hcomId.c_str(), param->threadNum, i);
            return ret;
        }
        outThreads.emplace_back(thread);
    }

    ThreadHandle *threadArray = static_cast<ThreadHandle*>(param->deviceHandle);
    // 空指针校验
    CHK_PTR_NULL(threadArray);
    for (size_t i = 0; i < threadNum; ++i) {
        threadArray[i] = reinterpret_cast<ThreadHandle>(outThreads[i].get());  // 拷贝裸指针
        HCCL_INFO("[HcclCommAicpu][%s] threadArray[%u] = [%lu]", __func__, i, threadArray[i]);
    }
    threads_.insert(threads_.end(), std::make_move_iterator(outThreads.begin()),
        std::make_move_iterator(outThreads.end()));
    HCCL_INFO("[HcclCommAicpu][%s] comm identifier[%s], init threads num[%u] success",
        __func__, hcomId.c_str(), threadNum);
    return HCCL_SUCCESS;
}

HcclResult CollCommAicpu::AllocChannelResource(HcclChannelUrmaRes *commParam)
{
    HCCL_INFO("[HcclCommAicpu][%s] deviceLogicId[%d], devicePhyId[%u], deviceType[%d], commParam->channelList[%p], "
              "commParam->listNum[%u], commParam->uniqueIdAddr[%p], commParam->uniqueIdSize[%u]",
              __func__, topoInfo_.deviceLogicId, topoInfo_.devicePhyId, topoInfo_.deviceType, commParam->channelList,
              commParam->listNum, commParam->uniqueIdAddr, commParam->uniqueIdSize);
    CHK_PRT(InitUrmaChannel(commParam));
}

HcclResult CollCommAicpu::InitUrmaChannel(HcclChannelUrmaRes *commParam)
{
    HCCL_INFO("[HcclCommAicpu][%s] commParam->uniqueIdAddr[%p], commParam->uniqueIdSize[%u]",
        __func__, commParam->uniqueIdAddr, commParam->uniqueIdSize);

    for (u32 index = 0; index < commParam->listNum; index++) {
        std::vector<char> data(commParam->singleUniqueIdSize);

        // 计算地址块的偏移
        u8* currentSrcAddr = reinterpret_cast<u8*>(commParam->uniqueIdAddr) + index * commParam->singleUniqueIdSize;
        CHK_SAFETY_FUNC_RET(memcpy_s(data.data(), data.size(), currentSrcAddr, commParam->singleUniqueIdSize));

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
        HCCL_INFO("[HcclCommAicpu][%s] index[%u], currentSrcAddr[%p], singleUniqueIdSize[%u], channelHandle[0x%llx]",
            __func__, index, currentSrcAddr, commParam->singleUniqueIdSize, channelHandle);
    }

    return HCCL_SUCCESS;
}

HcclResult CollCommAicpu::ParsePackData(std::vector<char> &data, ChannelHandle &handle)
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