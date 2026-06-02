/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rank_table_crc_bridge.h"
#include "checkcrc.h"
#include "sal_pub.h"

RankTableCrcBridge::~RankTableCrcBridge() = default;

RankTableCrcBridge RankTableCrcBridge::GetInstance()
{
    static RankTableCrcBridge instance;
    return instance;
}

void RankTableCrcBridge::RecordRankTableJsonCrc(s32 deviceLogicId, const std::string &rankTableJson)
{
    Hccl::CheckCrc checkCrc;
    u32 crc = 0;
    HcclResult ret = checkCrc.CalcStringCrc(rankTableJson.c_str(), &crc);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcomRecordRankTableJsonCrc] CalcStringCrc failed, ret[%d]", ret);
        return;
    }
    g_rankTableJsonCrcMap[deviceLogicId] = crc;
    HCCL_INFO("[HcomRecordRankTableJsonCrc] deviceLogicId[%d], crc[0x%08x] recorded.", deviceLogicId, crc);
}

u32 RankTableCrcBridge::ConsumeRankTableJsonCrc(s32 deviceLogicId)
{
    auto it = g_rankTableJsonCrcMap.find(deviceLogicId);
    if (it == g_rankTableJsonCrcMap.end()) {
        return 0;
    }
    u32 crc = it->second;
    g_rankTableJsonCrcMap.erase(it);
    return crc;
}