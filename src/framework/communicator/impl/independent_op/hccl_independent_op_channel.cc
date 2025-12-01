/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api.h"
#include "channel_manager.h"
#include "log.h"
#include "hccl_comm_pub.h"
#include "independent_op.h"

using namespace hccl;

HcclResult HcclChannelCreate(HcclComm comm, const char *channelTag,
    CommEngine engine, const ChannelDesc *channelDescList, uint32_t listNum, ChannelHandle *channelList)
{
    // 入参校验
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(channelTag);
    CHK_PTR_NULL(channelDescList);
    CHK_PTR_NULL(channelList);
    CHK_PRT_RET(listNum == 0, HCCL_ERROR("[%s]Invalid listNum, listNum[%u]",
        __func__, listNum), HCCL_E_PARA);

    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
    HcclResult ret = channelMgr.ChannelCommCreate(hcclComm->GetIdentifier(), std::string(channelTag), engine, channelDescList, listNum, channelList);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to create channel, group[%s], channelTag[%s], engine[%d], channelNum[%llu], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), channelTag, engine, listNum, ret);
        return ret;
    }

    HCCL_RUN_INFO("[%s] create channel success, group[%s], channelTag[%s], engine[%d], channelNum[%llu], ret[%d]", 
        __func__, hcclComm->GetIdentifier().c_str(), channelTag, engine, listNum, ret);
    return HCCL_SUCCESS;
}

HcclResult HcclChannelGetNotifyNum(HcclComm comm, ChannelHandle channel, uint32_t *notifyNum)
{
    CHK_PTR_NULL(notifyNum);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
    HcclResult ret = channelMgr.ChannelCommGetNotifyNum(channel, notifyNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to get channel notifyNum, group[%s], channel[%llu], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), channel, ret);
        return ret;
    }

    HCCL_RUN_INFO("[%s] get channel notifyNum success, group[%s], channel[%llu], notifyNum[%lu], ret[%d]", 
        __func__, hcclComm->GetIdentifier().c_str(), channel, *notifyNum, ret);
    return HCCL_SUCCESS;
}

HcclResult CommChannelDestroy(HcclComm comm, ChannelHandle *channelList, uint32_t channelNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(channelList);
    CHK_PRT_RET(channelNum == 0, HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u]",
        __func__, channelNum), HCCL_E_PARA);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
    HcclResult ret = channelMgr.ChannelCommDestroy(channelList, channelNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to destroy channel, group[%s], channelList[%p], channelNum[%lu], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), channelList, channelNum, ret);
        return ret;
    }

    HCCL_RUN_INFO("[%s] destroy channel success, group[%s], channelList[%p], channelNum[%lu], ret[%d]", 
        __func__, hcclComm->GetIdentifier().c_str(), channelList, channelNum, ret);
    return HCCL_SUCCESS;
}

HcclResult HcclChannelGetHcclBuffer(HcclComm comm, ChannelHandle channel, CommBuffer *buffer)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(buffer);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
    HcclResult ret = channelMgr.ChannelCommGetHcclBuffer(channel, buffer);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to get channel hccl buffer, group[%s], channel[%llu], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), channel, ret);
        return ret;
    }

    HCCL_RUN_INFO("[%s] get channel hccl buffer success, group[%s], channel[%llu], " 
        "buffer[type:%d, addr:%p, size:%llu], ret[%d]", __func__, hcclComm->GetIdentifier().c_str(), 
        channel, buffer->type, buffer->addr, buffer->size, ret);
    return HCCL_SUCCESS;
}

HcclResult HcclChannelGetRemoteMem(HcclComm comm, ChannelHandle channel, HcclMem **remoteMem, char **memTag, uint32_t *memNum)
{
    // memTag 目前未使用
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memNum);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
    HcclResult ret = channelMgr.ChannelCommGetRemoteMem(channel, remoteMem, memNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to get channel remote mem, group[%s], channel[%p], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), channel, ret);
        return ret;
    }

    HCCL_RUN_INFO("[%s] get channel remote mem success, group[%s], channel[%p], memNum[%lu], ret[%d]", 
        __func__, hcclComm->GetIdentifier().c_str(), channel, *memNum, ret);
    return HCCL_SUCCESS;
}