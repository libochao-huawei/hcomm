/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "adapter_pub.h"
#include "calc_crc.h"
#include "rank_consistentcy_checker.h"

namespace hccl {

RankConsistentcyChecker::RankConsistentcyChecker() : cannVersion_{0}, cannVerCheckSwitch_(false),
    cannVerInfoRecordFlag_(false), configFileExist_(false)
{
}

RankConsistentcyChecker::~RankConsistentcyChecker() = default;

RankConsistentcyChecker& RankConsistentcyChecker::GetInstance(s32 deviceLogicId)
{
    static RankConsistentcyChecker instance[MAX_MODULE_DEVICE_NUM];
    if (deviceLogicId == HOST_DEVICE_ID) {
        HCCL_INFO("[GetInstance] deviceLogicId[-1] is HOST_DEVICE_ID");
        return instance[0];
    }
    hrtGetDeviceRefresh(&deviceLogicId);
    HCCL_INFO("[GetInstance] get deviceLogicId[%d]", deviceLogicId);
    CHK_PRT_RET((static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM || deviceLogicId < 0),
        HCCL_WARNING("[R]deviceLogicId[%d] is invalid", deviceLogicId), instance[0]);

    return instance[deviceLogicId];
}

// gather
HcclResult RankConsistentcyChecker::RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count,
    HcclDataType dataType, u32 root, u64 inCclBufferSize, u64 outCclBufferSize, const char *group, u32 crc,
    u32 aivCoreLimit)
{
    return RecordOpPara(opCMD, tag, count, dataType, HCCL_REDUCE_RESERVED, root, 0, 0, 0, inCclBufferSize,
        outCclBufferSize, group, crc, aivCoreLimit);
}

// reduce
HcclResult RankConsistentcyChecker::RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count,
    HcclDataType dataType, HcclReduceOp op, u32 root, u64 inCclBufferSize, u64 outCclBufferSize,
    const char *group, u32 crc, u8 deterministic, u32 aivCoreLimit)
{
    return RecordOpPara(opCMD, tag, count, dataType, op, root, 0, 0, 0, inCclBufferSize, outCclBufferSize, group, crc, aivCoreLimit);
}

// send && receive
HcclResult RankConsistentcyChecker::RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count,
    HcclDataType dataType, u32 rank, u32 srTag, u32 selfRank, u64 inCclBufferSize, u64 outCclBufferSize,
    const char *group, u32 crc)
{
    return RecordOpPara(opCMD, tag, count, dataType, HCCL_REDUCE_RESERVED, 0, rank, srTag, selfRank,
        inCclBufferSize, outCclBufferSize, group, crc);
}

// batchsendrecv
HcclResult RankConsistentcyChecker::RecordOpPara(HcclCMDType opCMD, const std::string &tag,
    u64 inCclBufferSize, u64 outCclBufferSize, const char *group, u32 crc)
{
    return RecordOpPara(opCMD, tag, 0, HCCL_DATA_TYPE_RESERVED, HCCL_REDUCE_RESERVED, 0, 0, 0, 0, inCclBufferSize,
        outCclBufferSize, group, crc);
}

// reduce scatter v && AllGather v
HcclResult RankConsistentcyChecker::RecordOpPara(HcclCMDType opCMD, const std::string &tag,
    const void *counts, const void *displs, const u32 rankSize,
    HcclDataType dataType, HcclReduceOp op, u64 inCclBufferSize, u64 outCclBufferSize, const char *group, u32 crc, u8 deterministic,
    u32 aivCoreLimit)
{
    CHK_RET(RecordOpPara(opCMD, tag, 0, dataType, op, 0, 0, 0, 0, inCclBufferSize,
        outCclBufferSize, group, crc, aivCoreLimit));
    CHK_RET(RecordVaringOpPara(tag, counts, displs, rankSize));
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::RecordVaringOpPara(const std::string &tag, const void *counts, const void *displs,
    const u32 rankSize)
{
    u32 countsCrc;
    CHK_RET(CalcRawDataCrc(static_cast<const char_t*>(counts), rankSize * sizeof(u64), countsCrc));
    crcRecords_[tag][HcclCrcRecordType::HCCL_CRC_RECORD_VARING_COUNTS] = countsCrc;

    u32 displsCrc;
    CHK_RET(CalcRawDataCrc(static_cast<const char_t*>(displs), rankSize * sizeof(u64), displsCrc));
    crcRecords_[tag][HcclCrcRecordType::HCCL_CRC_RECORD_VARING_DISPLACEMENTS] = displsCrc;
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::DelOpPara(const std::string &tag)
{
    std::lock_guard<std::mutex> lock(mutex_);
    CHK_PRT_RET(!cmdInfoMap_.erase(tag),
        HCCL_ERROR("[RankConsistentcyChecker][DelOpPara]CMD info for tag[%s] does not exist, delete fail.",
        tag.c_str()), HCCL_E_INTERNAL);
    CHK_PRT_RET(!infoFlagCmdMap_.erase(tag),
        HCCL_ERROR("[RankConsistentcyChecker][DelOpPara]CMD info flag cmd for tag[%s] does not exist, delete fail.",
        tag.c_str()), HCCL_E_INTERNAL);
    crcRecords_.erase(tag);
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::RecordVerInfo(const std::string &versionInfo)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Only record once, in case of multiple calls while other process is reading CANN version information 
    // in CompareFrame func and get the intermediate state.
    if (cannVerInfoRecordFlag_) {
        HCCL_INFO("[RankConsistentcyChecker][RecordVerInfo]Cann version information has been recorded.");
        return HCCL_SUCCESS;
    }

    u32 strLen = versionInfo.length();
    s32 sRet = memset_s(cannVersion_, MAX_CANN_VERSION_LEN + 1, 0, MAX_CANN_VERSION_LEN + 1);
    CHK_PRT_RET(sRet != EOK, HCCL_WARNING("[RankConsistentcyChecker][RecordVerInfo]memory set 0 fail for version str "
        "array. return[%d].", sRet), HCCL_SUCCESS);

    CHK_PRT_RET(strLen == 0, HCCL_WARNING("[Record][CannVersion] version information str is empty."),
        HCCL_SUCCESS);

    CHK_PRT_RET(strLen >= MAX_CANN_VERSION_LEN, HCCL_WARNING("[Record][CannVersion]"
        "length of version information str is too long."), HCCL_SUCCESS);
    sRet = strncpy_s(cannVersion_, MAX_CANN_VERSION_LEN + 1, versionInfo.c_str(), strLen);
    CHK_PRT_RET(sRet != EOK, HCCL_WARNING("[Record][CannVersion] call strncpy_s failed, return [%d].", sRet),
        HCCL_SUCCESS);

    cannVerInfoRecordFlag_ = true;
    return HCCL_SUCCESS;
}

u64 RankConsistentcyChecker::GetRankConsistentDataLength()
{
    return sizeof(HcclCheckInfo);
}

void RankConsistentcyChecker::RecordProtocolType(ProtocolType protocolType)
{
    HCCL_INFO("[RankConsistentcyChecker][RecordProtocolType]protocolType set to [%d].",
        static_cast<s32>(protocolType));
    protocolType_ = protocolType;
    return;
}

HcclResult RankConsistentcyChecker::GetCheckFrame(u8 *destBuf, u64 maxDestBuf, const std::string &tag)
{
    CHK_PTR_NULL(destBuf);
    // 要发送的校验帧
    HcclCheckInfo checkInfo;
    u64 checkInfoLen = sizeof(checkInfo);
    HcclResult ret = GenerateCheckFrame(checkInfo, tag);
    checkInfo.cmdInfo.selfRank = 0; // 自身的group rank 不做校验,置0
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[RankConsistentcyChecker][GetCheckFrame]generate check frame fail. "
        "return[%d]", ret), ret);

    s32 sret = memcpy_s(destBuf, maxDestBuf, &checkInfo, checkInfoLen);
    CHK_PRT_RET(sret != EOK, HCCL_ERROR("[RankConsistentcyChecker][GetCheckFrame]frame len[%llu] is bigger than "
        "dest buffer len[%llu].", checkInfoLen, maxDestBuf), HCCL_E_INTERNAL);
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::CheckFrameRecv(const u8 *recvBuf, u32 recvBufLen, const std::string &tag)
{
    CHK_PTR_NULL(recvBuf);
    CHK_PRT_RET(recvBufLen == 0 || recvBufLen > MAX_FRAME_LEN,
        HCCL_ERROR("[RankConsistentcyChecker][CheckFrameRecv] errNo[0x%016llx] recvBufLen is wrong.",
        HCCL_ERROR_CODE(HCCL_E_INTERNAL)), HCCL_E_INTERNAL);

    CHK_PRT_RET(recvBufLen < sizeof(HcclCheckInfo),
        HCCL_ERROR("[RankConsistentcyChecker][CheckFrameRecv] errNo[0x%016llx] recvBufLen[%u]is less than "
        "check info[%zu].", HCCL_ERROR_CODE(HCCL_E_INTERNAL), recvBufLen, sizeof(HcclCheckInfo)), HCCL_E_PARA);
    
    // 校验Hcomm基础信息
    u64 hcommInfoLen = GetHcommInfoLength();
    CHK_PRT_RET(recvBufLen < hcommInfoLen, 
        HCCL_ERROR("[RankConsistentcyChecker][CheckFrameRecv] recvBufLen[%u] < hcommInfoLen[%u]", 
            recvBufLen, hcommInfoLen), HCCL_E_PARA);

    HcclResult ret = CheckHcommInfo(recvBuf, hcommInfoLen, tag);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[RankConsistentcyChecker][CheckFrameRecv] Hcom basic info check failed, tag[%s]", 
            tag.c_str());
        return ret;
    }
    HCCL_INFO("[RankConsistentcyChecker][CheckFrameRecv] Hcomm basic info check passed, tag[%s]", tag.c_str());

    // 校验HCCL算子信息
    // 检查是否包含HCCL算子信息
    if (recvBufLen > hcommInfoLen) {
        u32 hcclOpInfoLen = recvBufLen - hcommInfoLen;
        ret = CheckHcclOpInfo(recvBuf + hcommInfoLen, hcclOpInfoLen, tag);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[RankConsistentcyChecker][CheckFrameRecv] HCCL operator info check failed, tag[%s]", 
                tag.c_str());
            return ret;
        }
        HCCL_INFO("[RankConsistentcyChecker][CheckFrameRecv] HCCL operator info check passed, tag[%s]", tag.c_str());
    }

    // HcclCheckInfo checkInfoRecv;
    // // 对固定长度的全局数组变量，结构体变量进行初始化和拷贝，可以不用检查初始化安全函数返回值
    // (void)memset_s(&checkInfoRecv, sizeof(HcclCheckInfo), 0, sizeof(HcclCheckInfo));
    // (void)memcpy_s(&checkInfoRecv, sizeof(HcclCheckInfo), recvBuf, sizeof(HcclCheckInfo));

    // HcclCheckInfo checkInfo;
    // CHK_RET(GenerateCheckFrame(checkInfo, tag));
    // if (checkInfo.cmdInfo.cmdType == HcclCMDType::HCCL_CMD_SEND) {
    //     checkInfo.cmdInfo.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    //     checkInfo.cmdInfo.rank = checkInfo.cmdInfo.selfRank;
    // } else if (checkInfo.cmdInfo.cmdType == HcclCMDType::HCCL_CMD_RECEIVE) {
    //     checkInfo.cmdInfo.cmdType = HcclCMDType::HCCL_CMD_SEND;
    //     checkInfo.cmdInfo.rank = checkInfo.cmdInfo.selfRank;
    // }

    // checkInfo.cmdInfo.selfRank = 0; // 自身的子group rank 不做校验
    // if (CompareFrame(checkInfo, checkInfoRecv)) {
    //     return HCCL_E_INTERNAL;
    // }

    HCCL_INFO("[RankConsistentcyChecker][CheckFrameRecv] check success, len of frame[%u], tag[%s].",
        recvBufLen, tag.c_str());
    return HCCL_SUCCESS;
}

void RankConsistentcyChecker::ClearCheckInfo()
{
    configFileExist_ = false;
    cannVerInfoRecordFlag_ = false;
    ClearCrcInfo();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cmdInfoMap_.clear();
        infoFlagCmdMap_.clear();
    }
    // 相关规范的例外场景，对固定数组的memset_s可以不判断返回值
    (void)memset_s(cannVersion_, MAX_CANN_VERSION_LEN + 1, 0, MAX_CANN_VERSION_LEN + 1);
    protocolType_ = ProtocolType::RESERVED;
    subCommInfoRecorded_ = false;
    (void)memset_s(&subCommInfo_, sizeof(subCommInfo_), 0, sizeof(subCommInfo_));
    return;
}

HcclResult RankConsistentcyChecker::CalcStringCrc(const char *str, u32 &crc)
{
    // 计算字符串CRC
    HcclResult ret = CalcCrc::HcclCalcCrc(str, strlen(str), crc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RankConsistentcyChecker][CalcStringCrc] errNo[0x%016llx] calc string crc error",
        HCCL_ERROR_CODE(HCCL_E_INTERNAL)), HCCL_E_INTERNAL);

    HCCL_DEBUG("[RankConsistentcyChecker][CalcStringCrc] result crc[%u].", crc);
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::CalcRawDataCrc(const void *ptr, u64 length, u32 &crc)
{
    // 计算内存数据块CRC
    HcclResult ret = CalcCrc::HcclCalcCrc(static_cast<const char*>(ptr), length, crc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RankConsistentcyChecker][CalcRawDataCrc] errNo[0x%016llx] calc string crc error",
        HCCL_ERROR_CODE(HCCL_E_INTERNAL)), HCCL_E_INTERNAL);

    HCCL_DEBUG("[RankConsistentcyChecker][CalcRawDataCrc] result crc[%u].", crc);
    return HCCL_SUCCESS;
}

void RankConsistentcyChecker::SetCheckCannVersionSwitch(const bool cannVerCheckSwitch)
{
    cannVerCheckSwitch_ = cannVerCheckSwitch;
    return;
}

HcclResult RankConsistentcyChecker::RecordEnvVarCrc()
{
    static const std::vector<std::string> ENV_VAR_NAMES = {
        "HCCL_ALGO",
        "HCCL_BUFFSIZE",
        "HCCL_OP_EXPANSION_MODE",
        "HCCL_RDMA_QPS_PER_CONNECTION",
        "HCCL_MULTI_QP_THRESHOLD"
    };

    crcTable_.clear();
    for (const auto &envVar : ENV_VAR_NAMES) {
        const char *value = getenv(envVar.c_str());
        std::string envStr = envVar + "=" + (value != nullptr ? value : "");
        u32 crc = 0;
        CHK_RET(CalcStringCrc(envStr.c_str(), crc));
        crcTable_.push_back(crc);
        HCCL_INFO("[RankConsistentcyChecker][RecordEnvVarCrc] %s, crc[0x%x]", envStr.c_str(), crc);
    }

    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::RecordSubCommPara(uint32_t rankNum, const uint32_t *rankIds, uint64_t subCommId, const char *parentCommIdentifier)
{
    std::lock_guard<std::mutex> lock(mutex_);
    CHK_PRT_RET(rankNum == 0, HCCL_ERROR("[RankConsistentcyChecker][RecordSubCommPara] rankNum is 0."), HCCL_E_PARA);
    CHK_PRT_RET(rankNum > MAX_SUB_COMM_RANK_NUM, HCCL_ERROR("[RankConsistentcyChecker][RecordSubCommPara] rankNum exceeds max[%u].", 
        MAX_SUB_COMM_RANK_NUM), HCCL_E_PARA);
    CHK_PRT_RET(rankIds);

    subCommInfo_.rankNum = rankNum;
    subCommInfo_.subCommId = subCommId;
    subCommInfo_.valid = 1;

    // 拷贝父通信域标识符
    if (parentCommIdentifier != nullptr) {
        s32 sRet = memcpy_s(subCommInfo_.parentCommIdentifier, MAX_COMM_IDENTIFIER_LEN + 1, parentCommIdentifier, strlen(parentCommIdentifier));
        CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[RankConsistentcyChecker][RecordSubCommPara] memcpy_s parentCommIdentifier failed, ret[%d]", sRet), HCCL_E_MEMORY);
    }

    s32 sRet = memcpy_s(subCommInfo_.rankIds, sizeof(subCommInfo_.rankIds), rankIds, rankNum * sizeof(u32));
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[RankConsistentcyChecker][RecordSubCommPara] memcpy_s rankIds failed, ret[%d]", sRet), HCCL_E_MEMORY);

    subCommInfoRecorded_ = true;
    return HCCL_SUCCESS;
}

u64 RankConsistentcyChecker::GetHcommInfoLength()
{
    // Hcomm基础信息 不包含ccrInfoOp和cmdInfo（HCCL算子信息）
    return sizeof(HcclCRCInfo) + sizeof(ProtocolType) + MAX_CANN_VERSION_LEN + 1;
}

HcclResult RankConsistentcyChecker::CheckHcommInfo(const u8 *recvBuf, u32 recvBufLen, const std::string &tag)
{
    CHK_PTR_NULL(recvBuf);

    u64 hcommInfoLen = GetHcommInfoLength();
    CHK_PRT_RET(recvBufLen < hcommInfoLen, HCCL_ERROR("[RankConsistentcyChecker][CheckHcommInfo] recvBufLen[%u] < hcommInfoLen[%llu]", 
        recvBufLen, hcommInfoLen), HCCL_E_PARA);
    
    // 解析接收到的Hcomm基础信息
    HcclCRCInfo recvCrcInfoGlobal;
    ProtocolType recvProtocolType;
    char recvVersion[MAX_CANN_VERSION_LEN + 1] = {0};
    u32 offset = 0;
    (void)memcpy_s(&recvCrcInfoGlobal, sizeof(HcclCRCInfo), recvBuf + offset, sizeof(HcclCRCInfo));
    offset += sizeof(HcclCRCInfo);
    (void)memcpy_s(&recvProtocolType, sizeof(recvProtocolType), recvBuf + offset, sizeof(recvProtocolType));
    offset += sizeof(recvProtocolType);
    (void)memcpy_s(recvVersion, MAX_CANN_VERSION_LEN + 1, recvBuf + offset, MAX_CANN_VERSION_LEN + 1);

    // 生成本地Hcomm基础信息
    HcclCRCInfo localCrcInfoGlobal;
    localCrcInfoGlobal.configFileExist_ = configFileExist_;
    localCrcInfoGlobal.envCrcNum = crcTable_.size();
    for (u32 i = 0; i < localCrcInfoGlobal.envCrcNum; i++) {
        localCrcInfoGlobal.envCrcArray[i] = crcTable_[i];
    }

    // 对比环境变量CRC
    bool isDiff = false;
    static const std::vector<std::string> ENV_VAR_NAMES = {
        "HCCL_ALGO",
        "HCCL_BUFFSIZE",
        "HCCL_OP_EXPANSION_MODE",
        "HCCL_RDMA_QPS_PER_CONNECTION",
        "HCCL_MULTI_QP_THRESHOLD"
    };
    if (localCrcInfoGlobal.envCrcNum != recvCrcInfoGlobal.envCrcNum) {
        HCCL_ERROR("[RankConsistentcyChecker][CheckHcommInfo] CRC count mismatch: local[%u], remote[%u]", 
            localCrcInfoGlobal.envCrcNum, recvCrcInfoGlobal.envCrcNum);
        isDiff = true;
    } else {
        for (u32 i = 0; i < localCrcInfoGlobal.envCrcNum; i++) {
            if (localCrcInfoGlobal.envCrcArray[i] != recvCrcInfoGlobal.crcArray[i]) {
                std::string envVarName = ENV_VAR_NAMES[i];
                HCCL_ERROR("[RankConsistentcyChecker][CheckHcommInfo] Env var Crc mismatch:%s local[0x%x], remote[0x%x]", 
                    envVarName, localCrcInfoGlobal.envCrcArray[i], recvCrcInfoGlobal.crcArray[i]);
                RPT_INPUT_ERR(true, "EI0005", 
                    std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}), 
                    std::vector<std::string>({"N/A", tag, "envVarName", 
                        std::to_string(localCrcInfoGlobal.envCrcArray[i]), std::to_string(recvCrcInfoGlobal.crcArray[i])}));
                isDiff = true;
            }
        }
    }

    // 对比协议类型
    if (protocolType_ != recvProtocolType) {
        HCCL_ERROR("[RankConsistentcyChecker][CheckHcommInfo] ProtocolType count mismatch: local[%d], remote[%d]", 
            static_cast<s32>(protocolType_), static_cast<s32>(recvProtocolType));
        RPT_INPUT_ERR(true, "EI0005", 
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}), 
            std::vector<std::string>({"N/A", tag, "protocolType", 
                std::to_string(static_cast<s32>(protocolType_)), std::to_string(static_cast<s32>(recvProtocolType))}));
        isDiff = true;
    }

    // 对比CANN版本（仅在版本校验开关打开时）
    if (cannVerCheckSwitch_) {
        std::string localVersion(cannVersion_);
        std::string remoteVersion(recvVersion);
        if (localVersion.empty() || remoteVersion.empty()) {
            HCCL_WARNING("[RankConsistentcyChecker][CheckHcommInfo] CANN version is empty. local[%s], remote[%s]",  
                localVersion.c_str(), remoteVersion.c_str());
        } else if (localVersion != remoteVersion) {
            HCCL_ERROR("[RankConsistentcyChecker][CheckHcommInfo] CANN version mismatch: local[%s], remote[%s]", 
                localVersion.c_str(), remoteVersion.c_str());
            RPT_INPUT_ERR(true, "EI0008", 
                std::vector<std::string>({"local_version", "remote_version"}), 
                std::vector<std::string>({localVersion, remoteVersion}));
            isDiff = true;
        }
    }
    return isDiff ? HCCL_E_PARA : HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::CheckHcclOpInfo(const u8 *recvBuf, u32 recvBufLen, const std::string &tag)
{
    CHK_PTR_NULL(recvBuf);

    // 生成本地HCCL算子校验信息
    HcclCheckInfo localCheckInfo;
    CHK_PRT(GenerateCheckFrame(localCheckInfo, tag));

    // 解析接收到的HCCL算子信息
    HcclCRCInfo recvCrcInfoOp;
    HcclCMDInfo recvCmdInfo;

    u32 offset = 0;
    (void)memcpy_s(&recvCrcInfoOp, sizeof(HcclCRCInfo), recvBuf + offset, sizeof(HcclCRCInfo));
    offset += sizeof(HcclCRCInfo);
    (void)memcpy_s(&recvCmdInfo, sizeof(HcclCMDInfo), recvBuf + offset, sizeof(HcclCMDInfo));
    
    // 比对算子CRC信息
    bool isDiff = CompareCrcInfo(localCheckInfo.cmdInfo, localCheckInfo.crcInfoOp, recvCrcInfoOp);
    if (isDiff) {
        HCCL_ERROR("[RankConsistentcyChecker][CheckHcclOpInfo] Op CRC check fail");
        RPT_INPUT_ERR(true, "EI0005", 
            std::vector<std::string>({"reason"}), 
            std::vector<std::string>({"Op CRC check fail"}));
    }

    // 处理Send/Recv的cmdType交换
    if (localCheckInfo.cmdInfo.cmdType == HcclCMDType::HCCL_CMD_SEND) {
        localCheckInfo.cmdInfo.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
        localCheckInfo.cmdInfo.rank = localCheckInfo.cmdInfo.selfRank;
    } else if (localCheckInfo.cmdInfo.cmdType == HcclCMDType::HCCL_CMD_RECEIVE) {
        localCheckInfo.cmdInfo.cmdType = HcclCMDType::HCCL_CMD_SEND;
        localCheckInfo.cmdInfo.rank = localCheckInfo.cmdInfo.selfRank;
    }
    localCheckInfo.cmdInfo.selfRank = 0;

    // 比对CMD参数信息
    if (!CompareSection(reinterpret_cast<char_t *>(&localCheckInfo.cmdInfo), 
        reinterpret_cast<char_t *>(&recvCmdInfo), sizeof(HcclCMDInfo))) {
        HCCL_ERROR("[RankConsistentcyChecker][CheckHcclOpInfo] CMD check fail");
        RPT_INPUT_ERR(true, "EI0005", 
            std::vector<std::string>({"reason"}), 
            std::vector<std::string>({"CMD check fail"}));
        isDiff = true;
    }

    return isDiff ? HCCL_E_PARA : HCCL_SUCCESS;
}

// private
bool RankConsistentcyChecker::CompareSubCommInfo(HcclSubCommInfo &localInfo, HcclSubCommInfo &remoteInfo)
{
    bool bIsDiff = false;

    if(!localInfo.valid && !remoteInfo.valid) {
        return false;
    } 

    if(localInfo.valid != remoteInfo.valid) {
        HCCL_ERROR("[RankConsistentcyChecker][CompareSubCommInfo] subCommInfo valid flag mismatch, "
            "local[%u], remote[%u]", localInfo.valid, remoteInfo.valid);
        bIsDiff = true;
    } 

    // 父通信域标识符对比
    if(strcmp(localInfo.parentCommIdentifier, remoteInfo.parentCommIdentifier) != 0) {
        RPT_INPUT_ERR(true, "EI0005", 
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}), 
            std::vector<std::string>({"HcclCreateSubCommConfig", "", "parentCommIdentifier", 
                localInfo.parentCommIdentifier, remoteInfo.parentCommIdentifier}));
        HCCL_ERROR("[RankConsistentcyChecker][CompareSubCommInfo][%s][%s] Parent comm identifier check fail, local[%s], remote[%s]", 
            LOG_KEYWORDS_INIT_CHANNEL.c_str(), LOG_KEYWORDS_PARAMETER_CONFLICT.c_str(), 
            localInfo.parentCommIdentifier, remoteInfo.parentCommIdentifier);
        bIsDiff = true;
    }

    // rankNum比对
    if(localInfo.rankNum != remoteInfo.rankNum) {
        RPT_INPUT_ERR(true, "EI0005", 
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}), 
            std::vector<std::string>({"HcclCreateSubCommConfig", "", "rankNum", 
                std::to_string(localInfo.rankNum), std::to_string(remoteInfo.rankNum)}));
        HCCL_ERROR("[RankConsistentcyChecker][CompareSubCommInfo][%s][%s] SubComm rankNum check fail, local[%u], remote[%u]", 
            LOG_KEYWORDS_INIT_CHANNEL.c_str(), LOG_KEYWORDS_PARAMETER_CONFLICT.c_str(), 
            localInfo.rankNum, remoteInfo.rankNum);
        bIsDiff = true;
    } else {
        // rankNum一致， 逐项比较rankIds
        for (u32 i = 0; i < localInfo.rankNum; i++) {
            if (localInfo.rankIds[i] != remoteInfo.rankIds[i]) {
                RPT_INPUT_ERR(true, "EI0005", 
                    std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}), 
                    std::vector<std::string>({"HcclCreateSubCommConfig", "", "rankIds[" + std::to_string(i) + "]", 
                        std::to_string(localInfo.rankIds[i]), std::to_string(remoteInfo.rankIds[i])}));
                HCCL_ERROR("[RankConsistentcyChecker][CompareSubCommInfo][%s][%s] SubComm rankIds[%u] check fail, local[%u], remote[%u]", 
                    LOG_KEYWORDS_INIT_CHANNEL.c_str(), LOG_KEYWORDS_PARAMETER_CONFLICT.c_str(), 
                    localInfo.rankIds[i], remoteInfo.rankIds[i]);
                bIsDiff = true;
            }
        }
    }

    // subCommId对比
    if(localInfo.subCommId != remoteInfo.subCommId) {
        RPT_INPUT_ERR(true, "EI0005", 
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}), 
            std::vector<std::string>({"HcclCreateSubCommConfig", "", "subCommId", 
                std::to_string(localInfo.subCommId), std::to_string(remoteInfo.subCommId)}));
        HCCL_ERROR("[RankConsistentcyChecker][CompareSubCommInfo][%s][%s] SubComm subCommId check fail, local[%llu], remote[%llu]", 
            LOG_KEYWORDS_INIT_CHANNEL.c_str(), LOG_KEYWORDS_PARAMETER_CONFLICT.c_str(), 
            localInfo.rankNum, remoteInfo.rankNum);
        bIsDiff = true;
    }

    return bIsDiff;
}

HcclResult RankConsistentcyChecker::RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count,
    HcclDataType dataType, HcclReduceOp op, u32 root, u32 rank, u32 srTag, u32 selfRank, u64 inCclBufferSize,
    u64 outCclBufferSize, const char *group, u32 crc, u8 deterministic, u32 aivCoreLimit)
{
    HcclCMDInfo cmdInfo;
    // 相关规范的例外场景，对固定数组的memset_s可以不判断返回值
    (void)memset_s(&cmdInfo, sizeof(cmdInfo), 0, sizeof(cmdInfo));

    cmdInfo.cmdType = opCMD;
    s32 sRet = strncpy_s(cmdInfo.tag, TAG_MAX_LEN + 1, tag.c_str(), tag.length());
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[RankConsistentcyChecker][RecordOpPara]errNo[0x%016llx] strlen[%u] of tag is "
        "longer than buffer[%u].", HCCL_ERROR_CODE(HCCL_E_PARA), tag.length(), TAG_MAX_LEN), HCCL_E_PARA);

    cmdInfo.count = count;
    cmdInfo.dataType = dataType;

    std::string strGroup = (group == nullptr) ? HCCL_WORLD_GROUP : group;
    sRet = strncpy_s(cmdInfo.group, GROUP_NAME_MAX_LEN + 1, strGroup.c_str(), strGroup.length());
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[RankConsistentcyChecker][RecordOpPara]errNo[0x%016llx] strlen[%u] group is "
        "longer than buffer[%u].", HCCL_ERROR_CODE(HCCL_E_PARA), strGroup.length(), GROUP_NAME_MAX_LEN), HCCL_E_PARA);

    cmdInfo.op = op;
    cmdInfo.root = root;
    cmdInfo.rank = rank;
    cmdInfo.srTag = srTag;
    cmdInfo.selfRank = selfRank;
    cmdInfo.inCclBufferSize = inCclBufferSize;
    cmdInfo.outCclBufferSize = outCclBufferSize;
    cmdInfo.aivCoreLimit = aivCoreLimit;
    cmdInfo.deterministic = deterministic;

    std::lock_guard<std::mutex> lock(mutex_);
    cmdInfoMap_[tag] = cmdInfo;
    infoFlagCmdMap_[tag] = true;
    crcRecords_[tag][HcclCrcRecordType::HCCL_CRC_RECORD_RANKTABLE] = crc;

    return HCCL_SUCCESS;
}


HcclResult RankConsistentcyChecker::GetOpParaByTag(const std::string &tag, HcclCMDInfo &CMDInfoOutput)
{
    auto getResult = cmdInfoMap_.find(tag);
    CHK_PRT_RET(getResult == cmdInfoMap_.end(),
        HCCL_ERROR("[RankConsistentcyChecker][GetOpParaByTag]There is not any CMD information for tag[%s]",
        tag.c_str()), HCCL_E_INTERNAL);
    CMDInfoOutput = getResult->second;
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::GetCrcByTag(const std::string &tag, HcclCRCInfo &crcInfo)
{
    const auto recordsIter = crcRecords_.find(tag);
    CHK_PRT_RET(recordsIter == crcRecords_.end(),
        HCCL_ERROR("[RankConsistentcyChecker][GetCrcByTag]There is not any CRC information for tag[%s]",
        tag.c_str()), HCCL_E_INTERNAL);
    crcInfo.crcNum = 0;
    const auto &tagRecords = recordsIter->second;
    for (const auto &record : tagRecords) {
        crcInfo.crcArray[crcInfo.crcNum++] = record.second;
        HCCL_DEBUG("[RankConsistentcyChecker][GetCrcByTag]Append crc[%u] for tag[%s].", record.second, tag.c_str());
    }

    HCCL_INFO("[RankConsistentcyChecker][GetCrcByTag]After adding crc for tag[%s], crcNum set to [%u].",
        tag.c_str(), crcInfo.crcNum);
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::GenerateCheckFrame(HcclCheckInfo &checkInfo, const std::string &tag)
{
    // 初始化用于发送的BUFFER
    // 对入参的结构体变量指向的内存进行初始化时，使用了变量的结构体类型大小进行初始化，
    // 如果指针不为空，可以不检查初始化安全函数的返回值
    u64 checkInfoLen = sizeof(HcclCheckInfo);
    (void)memset_s(&checkInfo, checkInfoLen, 0, checkInfoLen);

    // 添加CRC字段到校验帧
    checkInfo.crcInfoGlobal.configFileExist_ = configFileExist_;
    checkInfo.crcInfoGlobal.envCrcNum = crcTable_.size();
    if (u32 i = 0; i < checkInfo.crcInfoGlobal.envCrcNum; i++) {
        checkInfo.crcInfoGlobal.envCrcArray[i] = crcTable_[i];
    }
    // 添加CMD参数信息到校验帧
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto getResult = infoFlagCmdMap_.find(tag);
        if (getResult != infoFlagCmdMap_.end()) {
            CHK_PRT_RET(GetOpParaByTag(tag, checkInfo.cmdInfo) != HCCL_SUCCESS,
                HCCL_ERROR("[RankConsistentcyChecker][GenerateCheckFrame]get Op para by tag[%s] fail.", tag.c_str()),
                HCCL_E_INTERNAL);
            checkInfo.crcInfoOp.configFileExist_ = configFileExist_;
            // 添加CRC字段到校验帧
            CHK_PRT_RET(GetCrcByTag(tag, checkInfo.crcInfoOp) != HCCL_SUCCESS,
                HCCL_ERROR("[RankConsistentcyChecker][GenerateCheckFrame]get ranktable crc by tag[%s] fail.", tag.c_str()),
                HCCL_E_INTERNAL);
        }
    }
    // 添加HCCL版本信息到校验帧
    if (cannVerInfoRecordFlag_) {
        HCCL_DEBUG("[RankConsistentcyChecker][GenerateCheckFrame] CANN version information is [%s].", cannVersion_);
        s32 sret = memcpy_s(checkInfo.version, MAX_CANN_VERSION_LEN + 1, cannVersion_, strlen(cannVersion_));
        CHK_PRT_RET(sret != EOK,
            HCCL_ERROR("[RankConsistentcyChecker][GenerateCheckFrame] memcpy CANN version information failed, "
                "errorno [%d].", sret), HCCL_E_MEMORY);
    }
    // 添加拉远通信传输类型校验
    // 910* 不会配置isTcpMode，因此910*在此处的待校验值是一致的
    checkInfo.protocolType = protocolType_;
    HCCL_INFO("[RankConsistentcyChecker][GenerateCheckFrame] loc protocolType is [%d].", checkInfo.protocolType);

    // 添加子通信域参数校验信息
    if (subCommInfoRecorded_) {
        s32 sRet = memcpy_s(&checkInfo.subCommInfo, sizeof(HcclSubCommInfo), &subCommInfo_, sizeof(HcclSubCommInfo));
        CHK_PRT_RET(sRet != EOK, 
            HCCL_ERROR("[RankConsistentcyChecker][GenerateCheckFrame] memcpy_s subCommInfo failed, ret[%d]", sRet), HCCL_E_MEMORY);
    }

    return HCCL_SUCCESS;
}

bool RankConsistentcyChecker::CompareSection(const char *pRawData, const char *recvBuf, u32 len)
{
    for (u32 i = 0; i < len; i++) {
        if (*(pRawData + i) != *(recvBuf + i)) {
            return false;
        }
    }
    return true;
}

bool RankConsistentcyChecker::CompareCrcInfo(const  HcclCMDInfo &hcclCMDInfo, HcclCRCInfo &crcInfo, HcclCRCInfo &crcInfoRecv)
{
    bool bIsDiff = false;
    // 检校验整体是否一致
    if (!CompareSection(reinterpret_cast<char_t *>(&crcInfo), reinterpret_cast<char_t *>(&crcInfoRecv), sizeof(crcInfo))) {
        bIsDiff = true;
        // 检查每种CRC类型是否一致
        for (auto i = 0U; i < crcInfo.crcNum; ++i) {
            if (crcInfo.crcArray[i] != crcInfoRecv.crcArray[i]) {
                ReportCrcCheckFailed(hcclCMDInfo, static_cast<HcclCrcRecordType>(i), crcInfo.crcArray[i],
                    crcInfoRecv.crcArray[i]);
            }
        }
    }
    return bIsDiff;
}

void RankConsistentcyChecker::ReportCmdInfoCheckFailed(const HcclCMDInfo &hcclCMDInfo, const std::string &paraName,
    const std::string &localPara, const std::string &remotePara)
{
    ReportCommonError(hcclCMDInfo, paraName, localPara, remotePara, "CMD information" );
}

void RankConsistentcyChecker::ReportCmdInfoCheckFailed(const HcclCMDInfo &hcclCMDInfo, const std::string &paraName,
    uint32_t localPara, uint32_t remotePara)
{
    ReportCommonError(hcclCMDInfo, paraName, std::to_string(localPara), std::to_string(remotePara), "CMD information");
}

void RankConsistentcyChecker::ReportCrcCheckFailed(const HcclCMDInfo &hcclCMDInfo, HcclCrcRecordType crcType,
    const uint32_t localCrc, const uint32_t remoteCrc)
{
    const auto crcTypeStr = GetCRCTypeEnumStr(crcType);
    ReportCommonError(hcclCMDInfo, crcTypeStr, std::to_string(localCrc), std::to_string(remoteCrc), "CRC check");
}

void RankConsistentcyChecker::ReportCommonError(const HcclCMDInfo &hcclCMDInfo, const std::string &paraName,
    const std::string &localParaStr, const std::string &remoteParaStr, const std::string &errorMsg) const {
    std::string opInfo = "Unknown";
    for (const auto& pair : HCCL_OPTYPE_NAME_MAP) {
        if (pair.second == hcclCMDInfo.cmdType) {
            opInfo = std::string(pair.first);
            break;
        }
    }
    RPT_INPUT_ERR(true,
        "EI0005",
        std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
        std::vector<std::string>({opInfo, hcclCMDInfo.group, paraName, localParaStr, remoteParaStr}));
    HCCL_ERROR("[%s][%s]%s %s check fail. local[%s], remote[%s]",
        LOG_KEYWORDS_INIT_CHANNEL.c_str(),
        LOG_KEYWORDS_PARAMETER_CONFLICT.c_str(),
        errorMsg.c_str(),
        paraName.c_str(),
        localParaStr.c_str(),
        remoteParaStr.c_str());
}

void RankConsistentcyChecker::CompareCmdInfo(HcclCheckInfo &checkInfo, HcclCheckInfo &checkInfoRecv)
{
    auto localInfo = &checkInfo.cmdInfo;
    auto remoteInfo = &checkInfoRecv.cmdInfo;

    if (!CompareSection(localInfo->tag, remoteInfo->tag, TAG_MAX_LEN + 1)) {
        ReportCmdInfoCheckFailed(*localInfo, "tag", localInfo->tag, remoteInfo->tag);
    }

    if (localInfo->cmdType != remoteInfo->cmdType) {
        ReportCmdInfoCheckFailed(*localInfo, "cmdType",
            static_cast<uint32_t>(localInfo->cmdType), static_cast<uint32_t>(remoteInfo->cmdType));
    }

    if (localInfo->count != remoteInfo->count) {
        ReportCmdInfoCheckFailed(*localInfo, "count", localInfo->count, remoteInfo->count);
    }

    if (localInfo->dataType != remoteInfo->dataType) {
        ReportCmdInfoCheckFailed(*localInfo, "dataType",
            static_cast<uint32_t>(localInfo->dataType), static_cast<uint32_t>(remoteInfo->dataType));
    }

    if (localInfo->op != remoteInfo->op) {
        ReportCmdInfoCheckFailed(*localInfo, "op", static_cast<uint32_t>(localInfo->op), static_cast<uint32_t>(remoteInfo->op));
    }

    if (!CompareSection(localInfo->group, remoteInfo->group, GROUP_NAME_MAX_LEN + 1)) {
        ReportCmdInfoCheckFailed(*localInfo, "group", localInfo->group, remoteInfo->group);
    }

    if (localInfo->root != remoteInfo->root) {
        ReportCmdInfoCheckFailed(*localInfo, "root", localInfo->root, remoteInfo->root);
    }

    if (localInfo->rank != remoteInfo->rank) {
        ReportCmdInfoCheckFailed(*localInfo, "rank", localInfo->rank, remoteInfo->rank);
    }

    if (localInfo->srTag != remoteInfo->srTag) {
        ReportCmdInfoCheckFailed(*localInfo, "srTag", localInfo->srTag, remoteInfo->srTag);
    }

    if (localInfo->inCclBufferSize != remoteInfo->inCclBufferSize) {
        ReportCmdInfoCheckFailed(*localInfo, "inCclBufferSize", localInfo->inCclBufferSize,
            remoteInfo->inCclBufferSize);
    }

    if (localInfo->outCclBufferSize != remoteInfo->outCclBufferSize) {
        ReportCmdInfoCheckFailed(*localInfo, "outCclBufferSize", localInfo->outCclBufferSize,
            remoteInfo->outCclBufferSize);
    }

    if (localInfo->aivCoreLimit != remoteInfo->aivCoreLimit) {
        ReportCmdInfoCheckFailed(*localInfo, "aivCoreLimit", localInfo->aivCoreLimit,
            remoteInfo->aivCoreLimit);
    }

    if (localInfo->deterministic != remoteInfo->deterministic) {
        ReportCmdInfoCheckFailed(*localInfo, "deterministic", localInfo->deterministic,
            remoteInfo->deterministic);
    }

    return;
}

bool RankConsistentcyChecker::CompareFrame(HcclCheckInfo &checkInfo, HcclCheckInfo &checkInfoRecv)
{
    bool bIsDiff = false;
    if (CompareCrcInfo(checkInfo.cmdInfo, checkInfo.crcInfoGlobal, checkInfoRecv.crcInfoGlobal)) {
        HCCL_ERROR("[RankConsistentcyChecker][CompareFrame]errNo[0x%016llx] CRC check fail, please check the "
            "rankTable file and hccl_config file.", HCCL_ERROR_CODE(HCCL_E_INTERNAL));
        bIsDiff = true;
    }
    if (CompareCrcInfo(checkInfo.cmdInfo, checkInfo.crcInfoOp, checkInfoRecv.crcInfoOp)) {
        HCCL_ERROR("[RankConsistentcyChecker][CompareFrame]errNo[0x%016llx] Op CRC check fail, please check the op"
            " parameters, rankTable file and hccl_config file.", HCCL_ERROR_CODE(HCCL_E_INTERNAL));
        bIsDiff = true;
    }
    if (!CompareSection(reinterpret_cast<char_t *>(&checkInfo.cmdInfo),
        reinterpret_cast<char_t *>(&checkInfoRecv.cmdInfo), sizeof(checkInfo.cmdInfo))) {
        CompareCmdInfo(checkInfo, checkInfoRecv);
        HCCL_ERROR("[RankConsistentcyChecker][CompareFrame]errNo[0x%016llx] CMD check fail",
            HCCL_ERROR_CODE(HCCL_E_INTERNAL));
        bIsDiff = true;
    }
    HCCL_INFO("loc protocolType is [%d], rem protocolType is [%d].",
        checkInfo.protocolType, checkInfoRecv.protocolType);
    if (checkInfo.protocolType != checkInfoRecv.protocolType) {
        HCCL_ERROR("[RankConsistentcyChecker][CompareFrame]errNo[0x%016llx] ProtocolType check fail",
            HCCL_ERROR_CODE(HCCL_E_INTERNAL));
        bIsDiff = true;
    }

    // Cann版本校验，只在集合通信场景校验CANN版本
    if (cannVerCheckSwitch_) {
        std::string localCannVersion = checkInfo.version;
        std::string remoteCannVersion = checkInfoRecv.version;
        if (localCannVersion.empty() || remoteCannVersion.empty()) { // cann版本信息读取失败，返回告警
            HCCL_WARNING("[RankConsistentcyChecker][CompareFrame] CANN version str is empty. local_version %s, "
                "remote_version %s.", checkInfo.version, checkInfoRecv.version);
        } else if (localCannVersion != remoteCannVersion) { // cann版本信息读取成功，且版本不一致
            RPT_INPUT_ERR(true, "EI0008", std::vector<std::string>({"local_version", "remote_version"}),
                std::vector<std::string>({localCannVersion, remoteCannVersion}));
            HCCL_ERROR("[%s][%s] errNo[0x%016llx] Inconsistent HCCL Versions. local_version %s, remote_version %s.",
                LOG_KEYWORDS_INIT_CHANNEL.c_str(), LOG_KEYWORDS_VERSION_CONFLICT.c_str(),
                HCCL_ERROR_CODE(HCCL_E_INTERNAL), checkInfo.version, checkInfoRecv.version);
            bIsDiff = true;
        }
    }

    // 子通信域参数一致性校验
    if (CompareSubCommInfo(checkInfo.subCommInfo, checkInfoRecv.subCommInfo)) {
        HCCL_ERROR("[RankConsistentcyChecker][CompareFrame]errNo[0x%016llx] SubCommInfo check fail", HCCL_ERROR_CODE(HCCL_E_INTERNAL));
        bIsDiff = true;
    }

    return bIsDiff;
}

HcclResult RankConsistentcyChecker::AddCrc(const u32 crcValue)
{
    HCCL_DEBUG("crcValue[%u].", crcValue);
    crcTable_.push_back(crcValue);
    HCCL_DEBUG("num[%llu].", crcTable_.size());
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::ClearCrcInfo(void)
{
    this->crcTable_.clear();
    if (this->crcTable_.size() != 0) {
        HCCL_ERROR("[Clear][CrcInfo]errNo[0x%016llx] clear crcTable_ is failed", HCCL_ERROR_CODE(HCCL_E_INTERNAL));
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult RankConsistentcyChecker::GetCrc(u32 num, u32 *crcAddr)
{
    CHK_PTR_NULL(crcAddr);
    HCCL_DEBUG("num[%u], crc[%u].", num, *crcAddr);

    if (num == 0) {
        HCCL_ERROR("[Get][Crc]errNo[0x%016llx] In get crc the value of num is 0", HCCL_ERROR_CODE(HCCL_E_PARA));
        return HCCL_E_PARA;
    }

    if (num != crcTable_.size()) {
        HCCL_ERROR("[Get][Crc]errNo[0x%016llx] num error inputNum[%u], localNum[%llu]",
            HCCL_ERROR_CODE(HCCL_E_INTERNAL), num, crcTable_.size());
        return HCCL_E_INTERNAL;
    }

    for (u32 i = 0; i < num; i++) {
        crcAddr[i] = crcTable_[i];
    }
    return HCCL_SUCCESS;
}
}