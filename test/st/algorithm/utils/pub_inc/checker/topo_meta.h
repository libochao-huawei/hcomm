#ifndef RANKTABLE_FOR_LLT_H
#define RANKTABLE_FOR_LLT_H

#include "topoinfo_struct.h"
#include "hccl_types.h"
#include <vector>

namespace checker {

using u32 = unsigned int;
using PhyDeviceId = u32;
using ServerMeta = std::vector<PhyDeviceId>;
using SuperPodMeta = std::vector<ServerMeta>;
using TopoMeta = std::vector<SuperPodMeta>;

u32 GetRankNumFormTopoMeta(TopoMeta& topoMeta);
u32 GetServerNumFormTopoMeta(TopoMeta& topoMeta);

class RankTable_For_LLT {
public:
    explicit RankTable_For_LLT() = default;
    ~RankTable_For_LLT() = default;
    static HcclResult GenTopoMeta(TopoMeta &topoMate, int superPodNum, int serverNum, int rankNum);
};
}  // namespace checker

#endif