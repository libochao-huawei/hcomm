/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rank_table.h"

namespace cltm {
RankTable::RankTable(const char *allocatedResource, const unsigned int maxBufSize)
    : allocRes_(allocatedResource), rankToSet_(0), maxBuffSize_(maxBufSize), uniqueInfoPool_(UNIQUE_INFO_NUM),
    serverCnt_(0)
{
}

RankTable::~RankTable()
{
}

/* 判断字符数组是否指定长度以内的有效字符串 */
bool RankTable::IsValidString(const char *strArray, u32 len)
{
    for (u32 loop = 0; loop < len; loop++) {
        if (strArray[loop] == '\0') {
            return true;
        }
    }

    syslog(LOG_ERR, "[CLTM] string is illegal. the length is bigger than %u", len);
    return false;
}

/* 判断string是不是有效的IPv4地址 */
bool RankTable::IsValidIpv4Addr(const std::string &ipAddr)
{
    u32 ipAdd;
    s32 ret = inet_pton(AF_INET, ipAddr.c_str(), &ipAdd);  // IPv4地址字符串转化为u32, 无效地址会返回<=0
    if (ret <= 0) {
        syslog(LOG_ERR, "[CLTM] input IP[%s] is not a valid ipv4 addr", ipAddr.c_str());
        return false;
    }
    return true;
}

cltmResult_t RankTable::CheckUniqueInfo(const uniqueInfoType &type, const std::string &value)
{
    if (type >= UNIQUE_TYPE_DEVICE_IP && type <= UNIQUE_TYPE_HOST_IP) {
        if (!IsValidIpv4Addr(value)) {
            syslog(LOG_ERR, "[CLTM] server id[%s] is not a valid ipv4 address", value.c_str());
            return CLTM_E_PARA;
        }
    } else {
        syslog(LOG_ERR, "[CLTM] invalid unique info type[%d]", type);
        return CLTM_E_PARA;
    }

    /* 信息保存到对应的资源池里面 */
    auto srvIt = uniqueInfoPool_[type].find(value);
    if (srvIt != uniqueInfoPool_[type].end()) {
        syslog(LOG_ERR, "[CLTM] unique type[%d] info[%s] is already exit", type, value.c_str());
        return CLTM_E_PARA;
    }
    uniqueInfoPool_[type].insert(value);
    return CLTM_SUCCESS;
}

/* 获取json中某个属性的值 */
cltmResult_t RankTable::GetJsonProperty(const nlohmann::json &obj, const char *propName, u32 &propValue)
{
    /* 查找json对象中是否有该属性, 不存在的属性不能直接访问 */
    if (obj.find(propName) == obj.end()) {
        syslog(LOG_ERR, "[CLTM] json object has no property called %s", propName);
        return CLTM_E_PARA;
    }

    /* rank_table所有属性值都必须是字符串 */
    if (!obj[propName].is_string()) {
        syslog(LOG_ERR, "[CLTM] property value of Name[%s] is not string!", propName);
        return CLTM_E_PARA;
    }

    /* 将字符串转化为无符号整形 */
    cltmResult_t ret = StrToUint(obj[propName], propValue);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] get json property failed because propName[%s] is not a valid int", propName);
        return CLTM_E_PARA;
    }

    return CLTM_SUCCESS;
}

/* 获取json中某个属性的值 */
cltmResult_t RankTable::GetJsonProperty(const nlohmann::json &obj, const char *propName, std::string &propValue)
{
    /* 查找json对象中是否有该属性, 不存在的属性不能直接访问 */
    if (obj.find(propName) == obj.end()) {
        syslog(LOG_ERR, "[CLTM] json object has no property called %s", propName);
        return CLTM_E_PARA;
    }

    /* rank_table所有属性值都必须是字符串 */
    if (obj[propName].is_string()) {
        propValue = obj[propName];
        return CLTM_SUCCESS;
    } else {
        syslog(LOG_ERR, "[CLTM] property value of Name[%s] is not string!", propName);
        return CLTM_E_PARA;
    }
}

cltmResult_t RankTable::GetJsonProperty(const nlohmann::json &obj, const char *propName, nlohmann::json &propValue)
{
    /* 查找json对象中是否有该属性, 不存在的属性不能直接访问 */
    if (obj.find(propName) == obj.end()) {
        syslog(LOG_ERR, "[CLTM] json object has no property called %s", propName);
        return CLTM_E_PARA;
    }

    propValue = obj[propName];
    return CLTM_SUCCESS;
}

cltmResult_t RankTable::GetJsonArrayMemberProperty(const nlohmann::json &obj, const u32 index, const char *propName,
                                                   u32 &propValue)
{
    /* 索引必须在数组内有效 */
    if (!obj.is_array() || index >= obj.size()) {
        syslog(LOG_ERR, "[CLTM] index[%u] is not array or out of json object range", index);
        return CLTM_E_PARA;
    }

    /* 字段必须存在、有效 */
    nlohmann::json subObj = obj.at(index);
    if (subObj.find(propName) == subObj.end()) {
        syslog(LOG_ERR, "[CLTM] json object index[%u] has no property called %s", index, propName);
        return CLTM_E_PARA;
    }

    /* rank_table所有属性值都必须是字符串 */
    if (!subObj[propName].is_string()) {
        syslog(LOG_ERR, "[CLTM] property value of Name[%s] is not string!", propName);
        return CLTM_E_PARA;
    }

    /* 将字符串转化为无符号整形 */
    cltmResult_t ret = StrToUint(subObj[propName], propValue);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] get json property failed because index[%u] propName[%s] is invalid int",
               index, propName);
        return CLTM_E_PARA;
    }

    return CLTM_SUCCESS;
}

cltmResult_t RankTable::GetJsonArrayMemberProperty(const nlohmann::json &obj, const u32 index, const char *propName,
                                                   std::string &propValue)
{
    /* 索引必须在数组内有效 */
    if (!obj.is_array() || index >= obj.size()) {
        syslog(LOG_ERR, "[CLTM] index[%u] is not array or out of json object range", index);
        return CLTM_E_PARA;
    }

    nlohmann::json subObj = obj.at(index);
    /* 字段必须存在、有效 */
    if (subObj.find(propName) == subObj.end()) {
        syslog(LOG_ERR, "[CLTM] json object index[%u] has no property called %s", index, propName);
        return CLTM_E_PARA;
    }

    /* rank_table所有属性值都必须是字符串 */
    if (subObj[propName].is_string()) {
        propValue = subObj[propName];
        return CLTM_SUCCESS;
    } else {
        syslog(LOG_ERR, "[CLTM] property value of Name[%s] is not string!", propName);
        return CLTM_E_PARA;
    }
}

cltmResult_t RankTable::GetJsonArrayMemberProperty(const nlohmann::json &obj, const u32 index, const char *propName,
                                                   nlohmann::json &propValue)
{
    /* 索引必须在数组内有效 */
    if (!obj.is_array() || index >= obj.size()) {
        syslog(LOG_ERR, "[CLTM] index[%u] is not array or out of json object range", index);
        return CLTM_E_PARA;
    }

    nlohmann::json subObj = obj.at(index);
    /* 字段必须存在、有效 */
    if (subObj.find(propName) == subObj.end()) {
        syslog(LOG_ERR, "[CLTM] json object index[%u] has no property called %s", index, propName);
        return CLTM_E_PARA;
    }
    propValue = subObj[propName];
    return CLTM_SUCCESS;
}

cltmResult_t RankTable::GetJsonArrayLen(const nlohmann::json &obj, u32 &len)
{
    /* 必须是数组字段 */
    if (!obj.is_array()) {
        syslog(LOG_ERR, "[CLTM] json obj is not array or out of json object range");
        return CLTM_E_PARA;
    }

    len = obj.size();
    return CLTM_SUCCESS;
}

cltmResult_t RankTable::AssertIntEq(const u32 &a, const u32 &b)
{
    if (a != b) {
        syslog(LOG_ERR, "[CLTM] first[%u] is not equal to second[%u]", a, b);
        return CLTM_E_PARA;
    }

    return CLTM_SUCCESS;
}

/* 将字符串转换位正整数，如果不为正整数则报错 */
cltmResult_t  RankTable::StrToUint(const std::string &intStr, u32 &uintVal)
{
    try {
        uintVal = std::stoul(intStr, nullptr, 10); // 原数据为10进制
    }
    catch (std::invalid_argument&) {
        syslog(LOG_ERR, "stoul invalid argument,  str[%s] val[%u]", intStr.c_str(), uintVal);
        return CLTM_E_PARA;
    }
    catch (std::out_of_range&) {
        syslog(LOG_ERR, "stoul out of range,  str[%s] val[%u]", intStr.c_str(), uintVal);
        return CLTM_E_PARA;
    }
    catch (...) {
        syslog(LOG_ERR, "stoul catch error,  str[%s] val[%u]", intStr.c_str(), uintVal);
        return CLTM_E_PARA;
    }
    return CLTM_SUCCESS;
}

cltmResult_t RankTable::init()
{
    /* 输入的json文件不能过大 */
    if (!IsValidString(allocRes_, MAX_JSON_LEN)) {
        syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] input allocated resource string is invalid. max len is %u", \
               CLTM_ERROR_CODE(CLTM_E_PARA), MAX_JSON_LEN);
        return CLTM_E_PARA;
    }

    try {
        resouceJson_ = nlohmann::json::parse(allocRes_);
    } catch (...) {
        syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] load allocated resource to json fail. please check json input!", \
               CLTM_ERROR_CODE(CLTM_E_PARA));
        return CLTM_E_PARA;
    }

    return CLTM_SUCCESS;
}

/* 将rank信息添加到rank_list尾部 */
cltmResult_t RankTable::AddServerRankList(const nlohmann::json &devices, const u32 deviceCnt,
                                          nlohmann::json &srvRankList)
{
    cltmResult_t ret;
    std::set<std::string> devIdxPool;
    for (u32 loop = 0; loop < deviceCnt; loop++) {
        /* 获取device_index并校验 */
        u32 devIdx;
        ret = GetJsonArrayMemberProperty(devices, loop, "device_index", devIdx);
        if (ret != CLTM_SUCCESS) {
            syslog(LOG_ERR, "[CLTM] Add ranklist failed because of invalid device index. loop[%u]", loop);
            return CLTM_E_PARA;
        }
        /* device_index不能超过8 */
        if (devIdx > MAX_DEV_ID) {
            syslog(LOG_ERR, "[CLTM] devIdx[%u] is illegal. max device id[%u]", devIdx, MAX_DEV_ID);
            return CLTM_E_PARA;
        }

        /* 同一个server内部device_index不能重复 */
        std::string deviceIndex = std::to_string(devIdx);
        auto srvIt = devIdxPool.find(deviceIndex);
        if (srvIt != devIdxPool.end()) {
            syslog(LOG_ERR, "[CLTM] serverId[%s] is already exit", deviceIndex.c_str());
            return CLTM_E_PARA;
        }
        devIdxPool.insert(deviceIndex);

        /* 获取device_ip_addr并校验 */
        std::string deviceIp;
        ret = GetJsonArrayMemberProperty(devices, loop, "device_ip_addr", deviceIp);
        CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] get dev ip from device json loop[%u] failed", loop), ret);
        ret = CheckUniqueInfo(UNIQUE_TYPE_DEVICE_IP, deviceIp);
        CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] check device ip[%s] unique failed", deviceIp.c_str()), ret);

        /* 一个rank对应的所有device IP信息 */
        nlohmann::json deviceDesc;
        deviceDesc[PROP_DEV_ID] = deviceIndex;
        deviceDesc[PROP_DEV_IP] = deviceIp;
        deviceDesc[PROP_RANK_ID] = std::to_string(rankToSet_);
        srvRankList.push_back(deviceDesc);

        /* 下个rank编号+1 */
        rankToSet_++;
    }
    return CLTM_SUCCESS;
}

cltmResult_t RankTable::AddServer(const nlohmann::json &server)
{
    cltmResult_t ret;
    std::string serverId;
    ret = GetJsonProperty(server, PROP_SERVER_ID, serverId);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] get server id from server json fail"), ret);
    ret = CheckUniqueInfo(UNIQUE_TYPE_SERVER_ID, serverId);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] check serverId[%s] unique failed", serverId.c_str()), ret);

    nlohmann::json devices;
    ret = GetJsonProperty(server, PROP_AVAIL_DEV, devices);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] server[%s] get avail dev from server json fail", serverId.c_str()), ret);

    u32 deviceNum, deviceCnt;
    ret = GetJsonProperty(server, PROP_AVAIL_DEV_CNT, deviceCnt);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] server[%s] get device num from json fail", serverId.c_str()), ret);
    ret = GetJsonArrayLen(devices, deviceNum);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] server[%s] get device num from json fail", serverId.c_str()), ret);
    ret = AssertIntEq(deviceCnt, deviceNum);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] server[%s] deviceCnt[%u] deviceNum[%u] is not equal", \
               serverId.c_str(), deviceCnt, deviceNum);
        return CLTM_E_PARA;
    }

    /* 都需要添加device的rank_list信息 */
    nlohmann::json srvRankList;
    ret = AddServerRankList(devices, deviceCnt, srvRankList);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] add server[%s] to ranklist fail", serverId.c_str()), ret);

    nlohmann::json serverOut;
    serverOut[PROP_HOST_IP] = "reserve";
    serverOut[PROP_SERVER_ID] = serverId;
    serverOut[PROP_DEVICE] = srvRankList;
    serverLists_.push_back(serverOut);
    serverCnt_++;

    return CLTM_SUCCESS;
}

cltmResult_t RankTable::AddGroup(const nlohmann::json &group)
{
    cltmResult_t ret;
    /* 确保当前group中标注的server个数与server数组长度相同 */
    u32 serverCnt;
    ret = GetJsonProperty(group, PROP_SRV_NUM, serverCnt);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] AddGroup failed because get server number failed");
        return CLTM_E_PARA;
    }
    if (serverCnt == 0) {
        syslog(LOG_ERR, "[CLTM] AddGroup failed because server count is zero");
        return CLTM_E_PARA;
    }

    nlohmann::json servers;
    ret = GetJsonProperty(group, "allocated_resource", servers);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] AddGroup failed because get servers from group failed"), ret);
    u32 serverNum;
    ret = GetJsonArrayLen(servers, serverNum);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] AddGroup failed because get serverNum from servers failed"), ret);
    ret = AssertIntEq(serverCnt, serverNum);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] AddGroup failed because serverCnt[%u] vs serverNum[%u]", serverCnt, serverNum);
        return CLTM_E_PARA;
    }

    /* 添加server信息 */
    for (unsigned int index = 0; index < serverCnt; index++) {
        ret = AddServer(servers.at(index));
        CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] AddGroup failed because add server[%u] failed", index), ret);
    }

    return CLTM_SUCCESS;
}

cltmResult_t RankTable::GetGroupsInfo(nlohmann::json &groups)
{
    cltmResult_t ret;
    u32 groupCnt;
    ret = GetJsonProperty(resouceJson_, PROP_GROUP_CNT, groupCnt);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] Generate Rank Table failed because invalid group count", \
               CLTM_ERROR_CODE(CLTM_E_PARA));
        return CLTM_E_PARA;
    }
    if (groupCnt != 1) {      /* 只支持1个group */
        syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] Generate Rank Table. group count[%u] should more than zero", \
               CLTM_ERROR_CODE(CLTM_E_PARA), groupCnt);
        return CLTM_E_PARA;
    }

    /* 确保输入的group_count和json描述的个数一致 */
    ret = GetJsonProperty(resouceJson_, PROP_ALLOC_RES, groups);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] Generate RankTable failed because "\
        "get groups failed", CLTM_ERROR_CODE(CLTM_E_PARA)), ret);
    u32 groupNum;
    ret = GetJsonArrayLen(groups, groupNum);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] Generate RankTable failed because "\
        "get group_num failed", CLTM_ERROR_CODE(CLTM_E_PARA)), ret);
    ret = AssertIntEq(groupCnt, groupNum);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] Generate RankTable failed because groupCnt[%u] "\
            "groupnum[%u]", CLTM_ERROR_CODE(CLTM_E_PARA), groupCnt, groupNum);
        return CLTM_E_PARA;
    }

    return CLTM_SUCCESS;
}

cltmResult_t RankTable::GenerateRankTable(char *rankTableBuf, unsigned int *usedSize)
{
    cltmResult_t ret;
    nlohmann::json groups;
    ret = GetGroupsInfo(groups);
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] Get Groups Info failed"), ret);

    /* 添加分配的group资源到rank_table */
    ret = AddGroup(groups.at(0));
    CLTM_CHECK(ret, syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] GenerateRankTable failed because \
        AddGroup failed", CLTM_ERROR_CODE(CLTM_E_PARA)), ret);

    /* 填写rank_table一级信息 */
    rankTable_[PROP_SERVER_LIST] = serverLists_;
    rankTable_[PROP_SRV_CNT] = std::to_string(serverCnt_);
    rankTable_[PROP_VERSION] = "1.0";
    rankTable_[PROP_STATUS] = "completed";

    /* 导出rank table的字符串信息 */
    std::string outStr = rankTable_.dump();
    unsigned int length = outStr.length() + 1;
    if (length > maxBuffSize_) {
        syslog(LOG_ERR, "[CLTM] errNo[0x%016llx] output buff is too small. needed size(%u)", \
               CLTM_ERROR_CODE(CLTM_E_NO_RESOURCE), length);
        *usedSize = 0;
        return CLTM_E_NO_RESOURCE;
    }

    for (unsigned int index = 0; index < length; index++) {
        rankTableBuf[index] = outStr[index];
    }

    *usedSize = length;
    syslog(LOG_INFO, "[CLTM] generate rankTable length[%u]", length);
    syslog(LOG_INFO, "[CLTM] rank_table: %s", rankTable_.dump().c_str());

    return CLTM_SUCCESS;
}
}  // namespace cltm
