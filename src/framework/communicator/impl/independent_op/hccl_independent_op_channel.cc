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
#include "independent_op_channel_manager.h"
#include "log.h"
#include "hccl_comm_pub.h"

using namespace hccl;

HcclResult CommChannelCreate(HcclComm comm, const char *channelTag,
    CommEngine engine, const ChannelDesc *channelDescList, uint32_t listNum, ChannelHandle *channelList)
{
    // 入参校验
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(channelTag);
    CHK_PTR_NULL(channelDescList);
    CHK_PTR_NULL(channelList);
    CHK_PRT_RET(listNum == 0, HCCL_ERROR("[%s]Invalid listNum, listNum[%u]",
        __func__, listNum), HCCL_E_PARA);
    // Todo: listNum最大值校验
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->ChannelCommCreate(std::string(channelTag), engine, channelDescList, listNum, channelList));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}

HcclResult CommChannelGetNotifyNum(HcclComm comm, ChannelHandle channel, uint32_t *notifyNum)
{
    CHK_PTR_NULL(notifyNum);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->ChannelCommGetNotifyNum(channel, notifyNum));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}

HcclResult CommChannelDestroy(HcclComm comm, ChannelHandle *channelList, uint32_t channelNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(channelList);
    CHK_PRT_RET(channelNum == 0, HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u]",
        __func__, channelNum), HCCL_E_PARA);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->ChannelCommDestroy(channelList, channelNum));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}

HcclResult CommChannelGetHcclBuffer(HcclComm comm, ChannelHandle channel, CommBuffer *buffer)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(buffer);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->ChannelCommGetHcclBuffer(channel, buffer));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}

HcclResult CommChannelGetRemoteMem(HcclComm comm, ChannelHandle channel, HcclMem **remoteMem, char **memTag, uint32_t *memNum)
{
    // memTag 目前未使用
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memNum);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->ChannelCommGetRemoteMem(channel, remoteMem, memNum));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}