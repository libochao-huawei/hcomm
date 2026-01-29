/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: DetourService header file
 * Create: 2024-12-16
 */

#ifndef DETOUR_SERVICE_H
#define DETOUR_SERVICE_H

#include <unordered_map>
#include "phy_topo.h"
#include "rank_gph.h"

namespace Hccl {

class DetourService {
public:
    static DetourService &GetInstance();

    // 插入detourLinks
    void InsertDetourLinks(RankGraph *rankGraph, const RankTableInfo *rankTable);

private:
    explicit DetourService(const PhyTopo *phyTopo);
    ~DetourService()                                             = default;
    DetourService(const DetourService &detourService)            = delete;
    DetourService &operator=(const DetourService &detourService) = delete;
 
    const PhyTopo *phyTopo{nullptr};
};

void SetDetourTable4P(const std::set<u32>                                             &tableIdSet,
                      unordered_map<LocalId, unordered_map<LocalId, vector<LocalId>>> &detourTable);
} // namespace Hccl

#endif // DETOUR_SERVICE_H