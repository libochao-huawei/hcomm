/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rank_consistency_checker_v2.h"
#include "adapter_pub.h"
#include "calc_crc.h"
#include "adapter_error_manager_pub.h"

namespace hccl {

RankConsistencyCheckerV2::~RankConsistencyCheckerV2() = default;

RankConsistencyCheckerV2& RankConsistencyCheckerV2::GetInstance(const s32 &deviceLogicId)
{
    static RankConsistencyCheckerV2 instance[MAX_MODULE_DEVICE_NUM];
    HCCL_INFO("[RankConsistencyCheckerV2][GetInstance] get deviceLogicId[%d]", deviceLogicId);
    CHK_PRT_RET((static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM || deviceLogicId < 0),
        HCCL_WARNING("[RankConsistencyCheckerV2::RankConsistencyCheckerV2]deviceLogicId[%d] is invalid", deviceLogicId), instance[0]);
    return instance[deviceLogicId];
}

HcclResult RankConsistencyCheckerV2::RecordEngineV2(s32 engine)
{
    engine_ = engine;
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordEngineV2] engine_[%d] recorded.", engine_);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordEnvVarCrcV2(u64 buffSize)
{
    std::string buffSizeStr = std::to_string(buffSize);
    u32 crc = 0;
    HcclResult ret = CalcCrc::HcclCalcCrc(buffSizeStr.c_str(), strlen(buffSizeStr.c_str()), crc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RankConsistencyCheckerV2::RecordEnvVarCrcV2] CalcStringCrc failed for HCCL_BUFFSIZE."), ret);
    envVarCrcsV2_.emplace_back(std::string("HCCL_BUFFSIZE"), crc);
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordEnvVarCrcV2] HCCL_BUFFSIZE=[%s], crc[0x%08x] recorded.", buffSizeStr.c_str(), crc);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordRankTableCrcV2(u32 crc)
{
    rankTableCrcsV2_.emplace_back(std::string("ranktable_content"), crc);
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordRankTableCrcV2] ranktable crc[0x%08x] recorded.", crc);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordSubCommParaV2(const std::string &parentIdentifier, uint32_t rankNum,
    const uint32_t *rankIds, uint64_t subCommId)
{
    // 将子通信域四个关键参数计算CRC，带名称存入a5SubCommParaCrcs_
    // 1. 父通信域identifier
    u32 parentCommCrc = 0;
    HcclResult ret = CalcCrc::HcclCalcCrc(parentIdentifier.c_str(), strlen(parentIdentifier.c_str()), parentCommCrc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RankConsistencyCheckerV2::RecordSubCommParaV2] CalcStringCrc failed for parentIdentifier."), ret);
    subCommParaCrcsV2_.emplace_back(std::string("sub_comm_parentIdentifier"), parentCommCrc);
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordSubCommParaV2] parentIdentifier[%s] parentCommCrc[0x%08x] recorded.", parentIdentifier.c_str(), parentCommCrc);

    // 2. rankNum的CRC
    u32 rankNumCrc = 0;
    CHK_RET(CalcRawDataCrc(&rankNum, sizeof(rankNum), rankNumCrc));
    subCommParaCrcsV2_.emplace_back(std::string("sub_comm_rankNum"), rankNumCrc);
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordSubCommParaV2] rankNum[%u], crc[0x%08x] recorded.", rankNum, rankNumCrc);

    // 3. rankIds数组的CRC
    u32 rankIdsCrc = 0;
    CHK_RET(CalcRawDataCrc(rankIds, rankNum * sizeof(uint32_t), rankIdsCrc));
    subCommParaCrcsV2_.emplace_back(std::string("sub_comm_rankIds"), rankIdsCrc);
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordSubCommParaV2] rankIds crc[0x%08x] recorded.", rankIdsCrc);

    // 4. subCommId的CRC
    u32 subCommIdCrc = 0;
    CHK_RET(CalcRawDataCrc(&subCommId, sizeof(subCommId), subCommIdCrc));
    subCommParaCrcsV2_.emplace_back(std::string("sub_comm_subCommId"), subCommIdCrc);
    HCCL_DEBUG("[RankConsistencyCheckerV2::RecordSubCommParaV2] subCommId[%llu], crc[0x%08x] recorded.", subCommId, subCommIdCrc);

    HCCL_INFO("[RankConsistencyCheckerV2::RecordSubCommParaV2] success, SubCommParaCrcs count[%zu].", subCommParaCrcsV2_.size());
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordCannVersionV2(const std::string &version)
{
    u32 strLen = version.length();
    s32 sRet = memset_s(cannVersion_, CANN_VERSION_MAX_LEN + 1, 0, CANN_VERSION_MAX_LEN + 1);
    CHK_PRT_RET(sRet != EOK, HCCL_WARNING("[RankConsistentcyChecker][RecordVerInfo]memory set 0 fail for version str "
        "array. return[%d].", sRet), HCCL_SUCCESS);

    CHK_PRT_RET(strLen == 0, HCCL_WARNING("[Record][CannVersion] version information str is empty."),
        HCCL_SUCCESS);

    CHK_PRT_RET(strLen >= CANN_VERSION_MAX_LEN + 1, HCCL_WARNING("[Record][CannVersion]"
        "length of version information str is too long."), HCCL_SUCCESS);
    sRet = strncpy_s(cannVersion_, CANN_VERSION_MAX_LEN + 1, version.c_str(), strLen);
    CHK_PRT_RET(sRet != EOK, HCCL_WARNING("[Record][CannVersion] call strncpy_s failed, return [%d].", sRet),
        HCCL_SUCCESS);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::GenerateCheckFrameV2(CheckFrameV2 &localFrame)
{
    s32 sRet = memset_s(&localFrame, sizeof(CheckFrameV2), 0, sizeof(CheckFrameV2));
    CHK_PRT_RET(sRet != EOK,
        HCCL_ERROR("[RankConsistencyCheckerV2::GenerateCheckFrameV2] memset failed."), HCCL_E_INTERNAL);

    // 填充引擎类型
    localFrame.engine = engine_;

    // 填充环境变量CRC
    localFrame.crcNum = std::min(static_cast<u32>(envVarCrcsV2_.size()), MAX_CRC_LEN_V2);
    for (u32 i = 0; i < localFrame.crcNum; i++) {
        localFrame.crcArray[i] = envVarCrcsV2_[i].crc;
    }

    // 填充ranktable CRC
    localFrame.rankTableCrcNum = std::min(static_cast<u32>(rankTableCrcsV2_.size()), MAX_CRC_LEN_V2);
    for (u32 i = 0; i < localFrame.rankTableCrcNum; i++) {
        localFrame.rankTableCrcArray[i] = rankTableCrcsV2_[i].crc;
    }

    // 填充子通信域参数CRC
    localFrame.subCommCrcNum = std::min(static_cast<u32>(subCommParaCrcsV2_.size()), MAX_CRC_LEN_V2);
    for (u32 i = 0; i < localFrame.subCommCrcNum; i++) {
        localFrame.subCommCrcArray[i] = subCommParaCrcsV2_[i].crc;
    }

    // 填充CANN版本
    sRet = memcpy_s(localFrame.version, CANN_VERSION_MAX_LEN + 1,
        cannVersion_, CANN_VERSION_MAX_LEN + 1);
    CHK_PRT_RET(sRet != EOK,
        HCCL_ERROR("[RankConsistencyCheckerV2::GenerateCheckFrameV2] memcpy version failed."), HCCL_E_INTERNAL);

    HCCL_INFO("[RankConsistencyCheckerV2::GenerateCheckFrameV2] success, engine[%d], envCrcNum[%u], "
        "rankTableCrcNum[%u], subCommCrcNum[%u], version[%s].",
        localFrame.engine, localFrame.crcNum, localFrame.rankTableCrcNum, localFrame.subCommCrcNum, localFrame.version);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CompareCheckFrameV2(
    const CheckFrameV2 &local, const CheckFrameV2 &remote)
{
    // 默认值是false表示相同，只有不同才会赋值true代表校验参数不一致，后续比对方法不会再赋值false
    bool isDiff = false;
    // dfx日志处理在下面函数的内部，外部不处理日志
    // 比对环境变量CRC
    isDiff |= CompareCrcArrayV2(
        local.crcNum, local.crcArray,
        remote.crcNum, remote.crcArray,
        envVarCrcsV2_, "env var", "env_var_count");

    // 比对ranktable CRC
    isDiff |= CompareCrcArrayV2(
        local.rankTableCrcNum, local.rankTableCrcArray,
        remote.rankTableCrcNum, remote.rankTableCrcArray,
        rankTableCrcsV2_, "ranktable", "ranktable_count");

    // 比对子通信域参数CRC
    isDiff |= CompareCrcArrayV2(
        local.subCommCrcNum, local.subCommCrcArray,
        remote.subCommCrcNum, remote.subCommCrcArray,
        subCommParaCrcsV2_, "sub comm param", "sub_comm_param_count");
        
    CompareVersionV2(local, remote, isDiff);
    CompareEngineV2(local, remote, isDiff);
    return isDiff ? HCCL_E_INTERNAL : HCCL_SUCCESS;
}

u64 RankConsistencyCheckerV2::GetCheckFrameLengthV2()
{
    return sizeof(CheckFrameV2);
}

void RankConsistencyCheckerV2::SetInconsistentCheckFirstDone(bool inconsistentCheckFirstDone)
{
    inconsistentCheckFirstDone_ = inconsistentCheckFirstDone;
}

bool RankConsistencyCheckerV2::GetInconsistentCheckFirstDone()
{
    return inconsistentCheckFirstDone_;
}

// private
bool RankConsistencyCheckerV2::CompareCrcArrayV2(
    u32 localNum, const u32 *localArray,
    u32 remoteNum, const u32 *remoteArray,
    const std::vector<CrcEntryV2> &nameSource,
    const std::string &categoryLabel,
    const std::string &countLabel)
{
    bool isDiff = false;
    if (localNum != remoteNum) {
        HCCL_ERROR("[RankConsistencyCheckerV2::CompareCheckFrameV2] %s crcNum mismatch: local[%u], remote[%u].",
            categoryLabel.c_str(), localNum, remoteNum);
        RPT_INPUT_ERR(true, "EI0005",
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
            std::vector<std::string>({"N/A", "N/A", countLabel,
                std::to_string(localNum), std::to_string(remoteNum)}));
        isDiff = true;
    } else {
        for (u32 i = 0; i < localNum && i < nameSource.size(); i++) {
            if (localArray[i] != remoteArray[i]) {
                const std::string &name = nameSource[i].name;
                HCCL_ERROR("[RankConsistencyCheckerV2::CompareCheckFrameV2] %s mismatch: [%s], local[0x%08x], remote[0x%08x].",
                      categoryLabel.c_str(), name.c_str(), localArray[i], remoteArray[i]);
                RPT_INPUT_ERR(true, "EI0005",
                    std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
                    std::vector<std::string>({"N/A", "N/A", name, std::to_string(localArray[i]), std::to_string(remoteArray[i])}));
                isDiff = true;
            }
        }
    }
    return isDiff;
}

HcclResult RankConsistencyCheckerV2::CompareVersionV2(const CheckFrameV2 &local, const CheckFrameV2 &remote, bool &isDiff)
{
    std::string localVer(local.version);
    std::string remoteVer(remote.version);
    if (localVer.empty() || remoteVer.empty()) {
        HCCL_WARNING("[RankConsistencyCheckerV2::CompareVersionV2] CANN version str is empty. local[%s], remote[%s].",
            localVer.c_str(), remoteVer.c_str());
        return HCCL_SUCCESS;
    } else if (localVer != remoteVer) {
        RPT_INPUT_ERR(true, "EI0008",
            std::vector<std::string>({"local_version", "remote_version"}),
            std::vector<std::string>({localVer, remoteVer}));
        HCCL_ERROR("[RankConsistencyCheckerV2::CompareVersionV2] CANN version mismatch: local[%s], remote[%s].",
            localVer.c_str(), remoteVer.c_str());
        isDiff = true;
    }
    return HCCL_SUCCESS;
}

HcclResult CompareEngineV2(const CheckFrameV2 &local, const CheckFrameV2 &remote, bool &isDiff)
{
    if (local.engine != remote.engine) {
        RPT_INPUT_ERR(true, "EI0008",
            std::vector<std::string>({"local_engine", "remote_engine"}),
            std::vector<std::string>({std::to_string(local.engine), std::to_string(remote.engine)}));
        HCCL_ERROR("[RankConsistencyCheckerV2::CompareEngineV2] engine mismatch: local[%d], remote[%d].",
            local.engine, remote.engine);
        isDiff = true;
    }
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CalcRawDataCrc(const void *ptr, u64 length, u32 &crc)
{
    // 计算内存数据块CRC
    HcclResult ret = CalcCrc::HcclCalcCrc(static_cast<const char*>(ptr), length, crc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RankConsistencyCheckerV2::CalcRawDataCrc] errNo[0x%016llx] calc string crc error",
        HCCL_ERROR_CODE(HCCL_E_INTERNAL)), HCCL_E_INTERNAL);

    HCCL_DEBUG("[RankConsistencyCheckerV2::CalcRawDataCrc] result crc[%u].", crc);
    return HCCL_SUCCESS;
}
}

