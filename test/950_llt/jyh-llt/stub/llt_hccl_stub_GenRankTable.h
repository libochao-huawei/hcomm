#ifndef RankTable_For_LLT_H
#define RankTable_For_LLT_H

#include "topoinfo_struct.h"
#include "hccl_types.h"
#include <vector>

namespace hccl {

using u32 = unsigned int;
using PhyDeviceId = u32;
using ServerMate = std::vector<PhyDeviceId>;
using SuperPodMeta = std::vector<ServerMate>;
using TopoMeta = std::vector<SuperPodMeta>;

class RankTable_For_LLT {
public:
    explicit RankTable_For_LLT() = default;
    ~RankTable_For_LLT() = default;
    static HcclResult GenRankTable(TopoMeta topoMate, RankTable_t &rankTable);
    static HcclResult GenTopoMeta(TopoMeta &topoMate, int arg1 = 2, int arg2 = 2, int arg3 = 8);
};

}  // namespace hccl

#endif