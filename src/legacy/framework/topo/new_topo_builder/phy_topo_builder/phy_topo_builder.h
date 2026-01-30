/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: PhyTopoBuilder header file
 * Create: 2024-12-16
 */

#ifndef PHY_TOPO_BUILDER_H
#define PHY_TOPO_BUILDER_H

#include <mutex>
#include <memory>
#include <set>
#include <sys/stat.h>
#include "phy_topo.h"
#include "topo_info.h"
#include "edge_info.h"
#include "topo_common_types.h"

namespace Hccl {
constexpr u32 SUPPORT_MAX_TOPOFILE_SIZE = 512 * 1024; // 512k 

struct LinkParams {
    std::shared_ptr<PhyTopo::Node> srcNode;
    std::shared_ptr<PhyTopo::Node> dstNode;
    std::set<std::string>       localAPorts;
    std::set<std::string>       localBPorts;
    AddrPosition                   position;
    TopoType                       topoType;
    u32                            topoInstanceId;
    std::set<LinkProtocol>      protocols;
};


class PhyTopoBuilder {
public:
    static PhyTopoBuilder &GetInstance();
    void Build(const std::string &topoPath);
    std::shared_ptr<TopoInfo> GetTopoInfo() const;
    void RecoverBuild(const TopoInfo &topoInfo);

private:
    std::shared_ptr<TopoInfo>          LoadTopoInfo(const std::string &topoPath);
    std::shared_ptr<Graph<PhyTopo::Node, PhyTopo::Link>> CreateGraph(const std::vector<EdgeInfo> &edges) const;
    std::shared_ptr<TopoInfo> topoInfo_;
    std::mutex phyTopoMutex;
};
} // namespace Hccl

#endif // PHY_TOPO_BUILDER_H