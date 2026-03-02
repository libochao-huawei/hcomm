/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DETOUR_RULES_H
#define DETOUR_RULES_H

#include <unordered_map>
#include <vector>
#include "topo_common_types.h"

namespace Hccl {

extern const std::unordered_map<LocalId, std::unordered_map<LocalId, std::vector<LocalId>>> DETOUR_2P_TABLE_01;

extern const std::unordered_map<LocalId, std::unordered_map<LocalId, std::vector<LocalId>>> DETOUR_2P_TABLE_04;

extern const std::unordered_map<LocalId, std::unordered_map<LocalId, std::vector<LocalId>>> DETOUR_4P_TABLE_0123;

extern const std::unordered_map<LocalId, std::unordered_map<LocalId, std::vector<LocalId>>> DETOUR_4P_TABLE_4567;

extern const std::unordered_map<LocalId, std::unordered_map<LocalId, std::vector<LocalId>>> DETOUR_4P_TABLE_0246;

extern const std::unordered_map<LocalId, std::unordered_map<LocalId, std::vector<LocalId>>> DETOUR_4P_TABLE_1357;

} // namespace Hccl

#endif // DETOUR_RULES_H
