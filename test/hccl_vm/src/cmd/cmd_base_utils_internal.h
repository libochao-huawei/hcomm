/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_CMD_BASE_UTILS_INTERNAL_H
#define HCCL_CMD_BASE_UTILS_INTERNAL_H

#include <cstdint>
#include <string>
#include <vector>

#include "sim_common_defs.h"
#include "sim_models.h"

bool IsAivExpansionModeEnabled();
bool IsBinDumpDisabled();
bool StartsWith(const std::string &value, const std::string &prefix);
bool EndsWith(const std::string &value, const std::string &suffix);
bool TryParseAivRankIdFromTaskFileName(const std::string &fileName, uint32_t &rankId);
HcclVmResult ValidateAivTaskJsonByRank(const std::vector<sim::Rank> &allRank);

#endif
