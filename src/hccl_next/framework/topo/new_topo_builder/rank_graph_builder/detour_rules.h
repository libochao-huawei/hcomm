/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: DetourService rules file
 * Create: 2024-12-16
 */
#ifndef DETOUR_RULES_H
#define DETOUR_RULES_H

#include <unordered_map>
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