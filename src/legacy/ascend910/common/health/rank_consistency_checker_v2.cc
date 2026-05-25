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
#include "calc_crc.h"
#include "adapter_error_manager_pub.h"

namespace hccl {

RankConsistencyCheckerV2::~RankConsistencyCheckerV2() = default;

RankConsistencyCheckerV2& RankConsistencyCheckerV2::GetInstance()
{
    static RankConsistencyCheckerV2 instance;
    return instance;
}

HcclResult RankConsistencyCheckerV2::RecordEnvVarCrcV2(u64 buffSize)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::string buffSizeStr = std::to_string(buffSize);
    u32 crc = 0;
    HcclResult ret = CalcCrc::HcclCalcCrc(buffSizeStr.c_str(), strlen(buffSizeStr.c_str()), crc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RecordEnvVarCrcV2] CalcStringCrc failed for HCCL_BUFFSIZE."), ret);
    envVarCrcsV2_.push_back({"HCCL_BUFFSIZE", crc});
    HCCL_DEBUG("[RecordEnvVarCrcV2] HCCL_BUFFSIZE=[%s], crc[0x%08x] recorded.", buffSizeStr.c_str(), crc);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordRankTableCrcV2(u32 crc)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rankTableCrcsV2_.push_back({"ranktable_content", crc});
    HCCL_DEBUG("[RecordA5RankTableCrc] ranktable crc[0x%08x] recorded.",crc);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordSubCommParaV2(const std::string &parentIdentifier, uint32_t rankNum,
    const uint32_t *rankIds, uint64_t subCommId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 将子通信域四个关键参数计算CRC，带名称存入a5SubCommParaCrcs_
    // 1. 父通信域identifier的CRC（直接使用传入的parentCommCrc）
    u32 parentCommCrc = 0;
    HcclResult ret = CalcCrc::HcclCalcCrc(parentIdentifier.c_str(), strlen(parentIdentifier.c_str()), parentCommCrc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RecordSubCommParaV2] CalcStringCrc failed for parentIdentifier."), ret);
    subCommParaCrcsV2_.push_back({"sub_comm_parentIdentifier", parentCommCrc});
    HCCL_DEBUG("[RecordSubCommParaV2] parentCommCrc[0x%08x] recorded.", parentCommCrc);

    // 2. rankNum的CRC
    u32 rankNumCrc = 0;
    CHK_RET(CalcRawDataCrc(&rankNum, sizeof(rankNum), rankNumCrc));
    subCommParaCrcsV2_.push_back({"sub_comm_rankNum", rankNumCrc});
    HCCL_DEBUG("[RecordSubCommParaV2] rankNum[%u], crc[0x%08x] recorded.", rankNum, rankNumCrc);

    // 3. rankIds数组的CRC
    u32 rankIdsCrc = 0;
    CHK_RET(CalcRawDataCrc(rankIds, rankNum * sizeof(uint32_t), rankIdsCrc));
    subCommParaCrcsV2_.push_back({"sub_comm_rankIds", rankIdsCrc});
    HCCL_DEBUG("[RecordSubCommParaV2] rankIds crc[0x%08x] recorded.", rankIdsCrc);

    // 4. subCommId的CRC
    u32 subCommIdCrc = 0;
    CHK_RET(CalcRawDataCrc(&subCommId, sizeof(subCommId), subCommIdCrc));
    subCommParaCrcsV2_.push_back({"sub_comm_subCommId", subCommIdCrc});
    HCCL_DEBUG("[RecordSubCommParaV2] subCommId[%llu], crc[0x%08x] recorded.", subCommId, subCommIdCrc);

    HCCL_INFO("[RecordSubCommParaV2] success, SubCommParaCrcs count[%zu].", subCommParaCrcsV2_.size());
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::RecordCannVersionV2(const std::string &version)
{
    std::lock_guard<std::mutex> lock(mutex_);
    u32 strLen = version.length();
    s32 sRet = memset_s(cannVersion_, CANN_VERSION_MAX_LEN + 1, 0, CANN_VERSION_MAX_LEN + 1);
    CHK_PRT_RET(sRet != EOK, HCCL_WARNING("[RankConsistentcyChecker][RecordVerInfo]memory set 0 fail for version str "
        "array. return[%d].", sRet), HCCL_SUCCESS);

    CHK_PRT_RET(strLen == 0, HCCL_WARNING("[Record][CannVersion] version information str is empty."),
        HCCL_SUCCESS);

    CHK_PRT_RET(strLen >= CANN_VERSION_MAX_LEN, HCCL_WARNING("[Record][CannVersion]"
        "length of version information str is too long."), HCCL_SUCCESS);
    sRet = strncpy_s(cannVersion_, CANN_VERSION_MAX_LEN + 1, version.c_str(), strLen);
    CHK_PRT_RET(sRet != EOK, HCCL_WARNING("[Record][CannVersion] call strncpy_s failed, return [%d].", sRet),
        HCCL_SUCCESS);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::GenerateCheckFrameV2(CheckFrameV2 &frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    s32 sRet = memset_s(&frame, sizeof(CheckFrameV2), 0, sizeof(CheckFrameV2));
    CHK_PRT_RET(sRet != EOK,
        HCCL_ERROR("[GenerateCheckFrameV2] memset failed."), HCCL_E_INTERNAL);

    // 填充环境变量CRC
    frame.crcNum = std::min(static_cast<u32>(envVarCrcsV2_.size()), MAX_CRC_LEN_V2);
    for (u32 i = 0; i < frame.crcNum; i++) {
        frame.crcArray[i] = envVarCrcsV2_[i].crc;
    }

    // 填充ranktable CRC
    frame.rankTableCrcNum = std::min(static_cast<u32>(rankTableCrcsV2_.size()), MAX_CRC_LEN_V2);
    for (u32 i = 0; i < frame.rankTableCrcNum; i++) {
        frame.rankTableCrcArray[i] = rankTableCrcsV2_[i].crc;
    }

    // 填充子通信域参数CRC
    frame.subCommCrcNum = std::min(static_cast<u32>(subCommParaCrcsV2_.size()), MAX_CRC_LEN_V2);
    for (u32 i = 0; i < frame.subCommCrcNum; i++) {
        frame.subCommCrcArray[i] = subCommParaCrcsV2_[i].crc;
    }

    // 填充CANN版本
    sRet = memcpy_s(frame.version, CANN_VERSION_MAX_LEN + 1,
        cannVersion_, CANN_VERSION_MAX_LEN + 1);
    CHK_PRT_RET(sRet != EOK,
        HCCL_ERROR("[GenerateCheckFrameV2] memcpy version failed."), HCCL_E_INTERNAL);

    HCCL_INFO("[GenerateCheckFrameV2] success, envCrcNum[%u], rankTableCrcNum[%u], "
        "subCommCrcNum[%u], version[%s].",
        frame.crcNum, frame.rankTableCrcNum, frame.subCommCrcNum, frame.version);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CompareCheckFrameV2(
    const CheckFrameV2 &local, const CheckFrameV2 &remote)
{
    bool isDiff = false;
    CompareEnvV2(local, remote, isDiff);
    CompareRankTableV2(local, remote, isDiff);
    CompareSubCommV2(local, remote, isDiff);
    CompareVersionV2(local, remote, isDiff);
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
HcclResult RankConsistencyCheckerV2::CalcRawDataCrc(const void *ptr, u64 length, u32 &crc)
{
    // 计算内存数据块CRC
    HcclResult ret = CalcCrc::HcclCalcCrc(static_cast<const char*>(ptr), length, crc);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RankConsistencyCheckerV2][CalcRawDataCrc] errNo[0x%016llx] calc string crc error",
        HCCL_ERROR_CODE(HCCL_E_INTERNAL)), HCCL_E_INTERNAL);

    HCCL_DEBUG("[RankConsistencyCheckerV2][CalcRawDataCrc] result crc[%u].", crc);
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CompareEnvV2(const CheckFrameV2 &local, const CheckFrameV2 &remote, bool &isDiff)
{
    if (local.crcNum != remote.crcNum) {
        HCCL_ERROR("[CompareEnvV2] env var crcNum mismatch: local[%u], remote[%u].",
            local.crcNum, remote.crcNum);
        RPT_INPUT_ERR(true, "EI0005",
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
            std::vector<std::string>({"N/A", "N/A", "env_var_count",
                std::to_string(local.crcNum), std::to_string(remote.crcNum)}));
        isDiff = true;
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        for (u32 i = 0; i < local.crcNum && i < envVarCrcsV2_.size(); i++) {
            if (local.crcArray[i] != remote.crcArray[i]) {
                const std::string &varName = envVarCrcsV2_[i].name;
                HCCL_ERROR("[CompareEnvV2] env var mismatch: [%s], local[0x%08x], remote[0x%08x].",
                    varName.c_str(), local.crcArray[i], remote.crcArray[i]);
                RPT_INPUT_ERR(true, "EI0005",
                    std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
                    std::vector<std::string>({"N/A", "N/A", varName,
                        std::to_string(local.crcArray[i]), std::to_string(remote.crcArray[i])}));
                isDiff = true;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CompareRankTableV2(const CheckFrameV2 &local, const CheckFrameV2 &remote, bool &isDiff)
{
    if (local.rankTableCrcNum != remote.rankTableCrcNum) {
        HCCL_ERROR("[CompareRankTableV2] ranktable crcNum mismatch: local[%u], remote[%u].",
            local.rankTableCrcNum, remote.rankTableCrcNum);
        RPT_INPUT_ERR(true, "EI0005",
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
            std::vector<std::string>({"N/A", "N/A", "ranktable_count",
                std::to_string(local.rankTableCrcNum), std::to_string(remote.rankTableCrcNum)}));
        isDiff = true;
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        for (u32 i = 0; i < local.rankTableCrcNum && i < rankTableCrcsV2_.size(); i++) {
            if (local.rankTableCrcArray[i] != remote.rankTableCrcArray[i]) {
                const std::string &rtName = rankTableCrcsV2_[i].name;
                HCCL_ERROR("[CompareRankTableV2] ranktable mismatch: [%s], local[0x%08x], remote[0x%08x].",
                    rtName.c_str(), local.rankTableCrcArray[i], remote.rankTableCrcArray[i]);
                RPT_INPUT_ERR(true, "EI0005",
                    std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
                    std::vector<std::string>({"N/A", "N/A", rtName,
                        std::to_string(local.rankTableCrcArray[i]), std::to_string(remote.rankTableCrcArray[i])}));
                isDiff = true;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CompareSubCommV2(const CheckFrameV2 &local, const CheckFrameV2 &remote, bool &isDiff)
{
    if (local.subCommCrcNum != remote.subCommCrcNum) {
        HCCL_ERROR("[CompareSubCommV2] sub comm crcNum mismatch: local[%u], remote[%u].",
            local.subCommCrcNum, remote.subCommCrcNum);
        RPT_INPUT_ERR(true, "EI0005",
            std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
            std::vector<std::string>({"N/A", "N/A", "sub_comm_param_count",
                std::to_string(local.subCommCrcNum), std::to_string(remote.subCommCrcNum)}));
        isDiff = true;
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        for (u32 i = 0; i < local.subCommCrcNum && i < subCommParaCrcsV2_.size(); i++) {
            if (local.subCommCrcArray[i] != remote.subCommCrcArray[i]) {
                const std::string &paraName = subCommParaCrcsV2_[i].name;
                HCCL_ERROR("[CompareSubCommV2] sub comm param mismatch: [%s], local[0x%08x], remote[0x%08x].",
                    paraName.c_str(), local.subCommCrcArray[i], remote.subCommCrcArray[i]);
                RPT_INPUT_ERR(true, "EI0005",
                    std::vector<std::string>({"ccl_op", "group", "para_name", "local_para", "remote_para"}),
                    std::vector<std::string>({"N/A", "N/A", paraName,
                        std::to_string(local.subCommCrcArray[i]), std::to_string(remote.subCommCrcArray[i])}));
                isDiff = true;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RankConsistencyCheckerV2::CompareVersionV2(const CheckFrameV2 &local, const CheckFrameV2 &remote, bool &isDiff)
{
    std::string localVer(local.version);
    std::string remoteVer(remote.version);
    if (localVer.empty() || remoteVer.empty()) {
        HCCL_WARNING("[CompareVersionV2] CANN version str is empty. local[%s], remote[%s].",
            localVer.c_str(), remoteVer.c_str());
    } else if (localVer != remoteVer) {
        RPT_INPUT_ERR(true, "EI0008",
            std::vector<std::string>({"local_version", "remote_version"}),
            std::vector<std::string>({localVer, remoteVer}));
        HCCL_ERROR("[CompareVersionV2] CANN version mismatch: local[%s], remote[%s].",
            localVer.c_str(), remoteVer.c_str());
        isDiff = true;
    }
    return HCCL_SUCCESS;
}
}

