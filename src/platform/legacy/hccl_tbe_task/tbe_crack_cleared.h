/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TBE_CRACK_CLEARED_H
#define TBE_CRACK_CLEARED_H

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <hccl/hccl_types.h>
#include <mutex>
#include <map>
#include <hccl/base.h>
#include "tbe_vector_reduce.h"
#include "op_tiling.h"
#include "legacy_common.h"

namespace TbeReduce {
constexpr u32 MEMSET_CRACK_TILING_DATA_MAX_SZIE = 64;
constexpr u32 MEMSET_ALIGNMENT_CRACK_LIST_MAX_SZIE = 32;

class TbeCrackCleard : public TbeVectorReduce {
public:
    explicit TbeCrackCleard();
    ~TbeCrackCleard() override;
    HcclResult CrackInit();
    HcclResult CrackDeInit();
    HcclResult Run(const std::vector<std::int64_t> &crackAddr, const std::vector<std::int64_t> &crackSize,\
        HcclRtStream stream);
private:
    void CrackInitOpInfoMap910A();
    void CrackInitOpInfoMap310P3();
    void CrackInitOpInfoMap910B();
    HcclResult GetOpInfo(nlohmann::json &opDescInfo, nlohmann::json &opTilingInfo);
    HcclResult GetTilingRunInfo(nlohmann::json &opDescInfo, nlohmann::json &opTilingInfo, OpRunInfo &runInfo,\
        const std::vector<std::int64_t> &crackSize);
    HcclResult ExecuteKernelLaunch(const std::vector<std::int64_t> &crackAddr, void *stream,
        aclrtBinHandle &binHandle, std::string &kernelName, OpRunInfo &runInfo, void *tilingDataDevPtr);
    HcclResult GetopInfoIndex(std::string &opInfoIndex);

    std::map<std::string, nlohmann::json> opInfoMap_;
    std::mutex crackDataMutex_;
    void *addrListDevPtr_;
    bool initTilingDataHostPtr_;
    std::string binaryName_ = "";
};
} // namespace TbeReduce
#endif