/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RANK_TABLE_PUB_H
#define RANK_TABLE_PUB_H

#include <algorithm>
#include <functional>
#include <vector>
#include <string>
#include <fstream>
#include <set>
#include <cstring>
#include <nlohmann/json.hpp>
#include <cltm.h>
#include "hccl/base.h"

namespace cltm {
using cltmUniqueInfoType = enum uniqueInfoType {
    UNIQUE_TYPE_DEVICE_IP = 0,
    UNIQUE_TYPE_SERVER_ID = 1,
    UNIQUE_TYPE_HOST_IP   = 2,
    UNIQUE_INFO_NUM
};

/* 打印日志的模块ID */
const u64 LOG_SYSTEM_RESERVED = 0;
const u64 LOG_HCCL_MODUL_ID = 5;
const u64 LOG_SUB_MODULE_ID_CLTM = 2;
#define CLTM_ERROR_CODE(error) ((LOG_SYSTEM_RESERVED << 32) + (LOG_HCCL_MODUL_ID << 24) + \
    (((u64)LOG_SUB_MODULE_ID_CLTM) << 16) + error)
    
class RankTable {
public:
    explicit RankTable(const char *allocatedResource, const unsigned int maxBufSize);

    virtual ~RankTable();

    cltmResult_t init();
    cltmResult_t GenerateRankTable(char *rankTableBuf, unsigned int *usedSize);

protected:
private:
    bool IsValidIpv4Addr(const std::string &ipAddr);
    bool IsValidString(const char *strArray, u32 len);
    cltmResult_t CheckUniqueInfo(const uniqueInfoType &type, const std::string &serverId);
    cltmResult_t StrToUint(const std::string &intStr, u32 &uintVal);
    cltmResult_t GetJsonArrayLen(const nlohmann::json &obj, u32 &len);
    cltmResult_t AssertIntEq(const u32 &a, const u32 &b);
    cltmResult_t AddServer(const nlohmann::json &server);
    cltmResult_t AddServerRankList(const nlohmann::json &devices, const u32 deviceCnt,
                                   nlohmann::json &srvRankList);
    cltmResult_t GetGroupsInfo(nlohmann::json &groups);
    cltmResult_t AddGroup(const nlohmann::json &group);
    cltmResult_t GetJsonProperty(const nlohmann::json &obj, const char *propName, u32 &propValue);
    cltmResult_t GetJsonProperty(const nlohmann::json &obj, const char *propName, std::string &propValue);
    cltmResult_t GetJsonProperty(const nlohmann::json &obj, const char *propName, nlohmann::json &propValue);
    cltmResult_t GetJsonArrayMemberProperty(const nlohmann::json &obj, const u32 index, const char *propName,
                                            u32 &propValue);
    cltmResult_t GetJsonArrayMemberProperty(const nlohmann::json &obj, const u32 index, const char *propName,
                                            std::string &propValue);
    cltmResult_t GetJsonArrayMemberProperty(const nlohmann::json &obj, const u32 index, const char *propName,
                                            nlohmann::json &propValue);

    const char *allocRes_;  // CSM分配资源json字符串
    unsigned int rankToSet_;
    unsigned int serverCnt_;
    nlohmann::json serverLists_;  // 生成的rank_table中一个group的server_list
    unsigned int maxBuffSize_;
    std::vector<std::string> planeNicNames_;
    std::string nicLocation_;
    std::vector<std::set<std::string>> uniqueInfoPool_;
    nlohmann::json resouceJson_;           // CSM分配资源的json信息,输入
    nlohmann::json rankTable_;             // json表示的rank_table数据,输出
};
}  // namespace cltm

#endif
