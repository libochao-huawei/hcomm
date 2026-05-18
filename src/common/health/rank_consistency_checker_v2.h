/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RANK_CONSISTENCY_CHECKER_V2_H
#define RANK_CONSISTENCY_CHECKER_V2_H

#include <string>
#include <cstdint>
#include <mutex>
#include <vector>
#include <hccl/hccl_types.h>

namespace hccl {
constexpr u32 MAX_CANN_VERSION_LEN = 50;  // CANN版本校验
static constexpr u32 MAX_CRC_LEN_V2 = 16; // A5最大CRC个数
struct CrcEntryV2 {
    std::string name;  // A5环境变量名（如"HCCL_BUFFSIZE"）或子通信域参数名（如"sub_comm_rankNum"）或ranktable名
    u32 crc = 0;
};

struct CheckFrameV2 {
    u32 crcNum = 0;                                   // 环境变量CRC个数
    u32 crcArray[MAX_CRC_LEN_V2] = {0};               // 环境变量CRC数组
    u32 subCommCrcNum = 0;                            // 子通信域参数CRC个数
    u32 subCommCrcArray[MAX_CRC_LEN_V2] = {0};        // 子通信域参数CRC数组
    u32 rankTableCrcNum = 0;                          // ranktable CRC个数
    u32 rankTableCrcArray[MAX_CRC_LEN_V2] = {0};      // ranktable CRC数组
    char version[MAX_CANN_VERSION_LEN + 1] = {0};
};

class RankConsistencyCheckerV2 {
public:
    ~RankConsistencyCheckerV2();
    static RankConsistencyCheckerV2& GetInstance();
    
    HcclResult RecordEnvVarCrcV2(u64 buffSize);
    HcclResult RecordRankTableCrcV2(const std::string &rankTableContent);
    HcclResult RecordSubCommParaV2(u32 parentCommCrc, uint32_t rankNum,
        const uint32_t *rankIds, uint64_t subCommId);
    HcclResult RecordCannVersionV2(const std::string &version);
    HcclResult GenerateCheckFrameV2(CheckFrameV2 &frame);
    HcclResult CompareCheckFrameV2(const CheckFrameV2 &local, const CheckFrameV2 &remote);
    u64 GetCheckFrameLengthV2();

    void SetInconsistentCheckFirstDone(bool inconsistentCheckFirstDone);
    bool GetInconsistentCheckFirstDone();
private:
    HcclResult CalcRawDataCrc(const void *ptr, u64 length, u32 &crc);
    bool CompareEnvV2(const CheckFrameV2 &local, const CheckFrameV2 &remote);
    bool CompareRankTableV2(const CheckFrameV2 &local, const CheckFrameV2 &remote);
    bool CompareSubCommV2(const CheckFrameV2 &local, const CheckFrameV2 &remote);
    bool CompareVersionV2(const CheckFrameV2 &local, const CheckFrameV2 &remote);
    
    std::mutex mutex_;
    // cann 版本号
    char cannVersion_[MAX_CANN_VERSION_LEN + 1];
    std::vector<CrcEntryV2> envVarCrcsV2_;        // A5环境变量CRC（带名称，用于精确报错）
    std::vector<CrcEntryV2> rankTableCrcsV2_;     // A5 ranktable CRC（带名称，用于精确报错）
    std::vector<CrcEntryV2> subCommParaCrcsV2_;   // A5子通信域参数CRC（带名称，用于精确报错）
    bool inconsistentCheckFirstDone_ = false; // 是否完成首次校验
    
};
}
#endif /* RANK_CONSISTENCY_CHECKER_V2_H */