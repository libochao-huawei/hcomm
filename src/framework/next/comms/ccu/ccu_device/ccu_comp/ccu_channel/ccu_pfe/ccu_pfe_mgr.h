/* 
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: ccu pfe manager header file
 * Create: 2025-02-18
 */

#ifndef CCU_PFE_MGR_H
#define CCU_PFE_MGR_H

#include <vector>
#include <unordered_map>

#include "ccu_dev_mgr_imp.h"
#include "ccu_pfe_cfg_mgr.h"

namespace hcomm {

struct PfeJettyStrategy {
    uint32_t feId;              // FE index.
    uint32_t pfeId;             // PFE ctx index.
    uint32_t startTaJettyId;    // PFE assigned to CCU starting jetty index.
    uint8_t  size;              // PFE assigned to CCU jetty num.
    uint32_t startLocalJettyCtxId; // PFE assigned to CCU starting local jetty index.
};

#pragma pack(push, 1)
struct PfeCtx {
    uint16_t startJettyId; // PFE assigned to CCU starting jetty index.
    /********2 Bytes**********/

    uint16_t jettyNum : 7;             // PFE assigned to CCU total jetty num.
    uint16_t startLocalJettyCtxId : 7; // CCU maintained starting jetty context index.
    uint16_t rsvBit : 2;
    /********4 Bytes**********/

    uint16_t rsv[2];
    /********8 Bytes**********/
};
#pragma pack(pop)

class CcuPfeMgr {
public:
    CcuPfeMgr(const int32_t devLogicId, const uint8_t dieId, const uint32_t devPhyId)
        : devLogicId_(devLogicId), dieId_(dieId), devPhyId_(devPhyId) {};
    CcuPfeMgr() = default;
    ~CcuPfeMgr() = default;

    HcclResult Init();
    HcclResult GetPfeStrategy(uint32_t feId, PfeJettyStrategy &pfeJettyStrategy) const;

private:
    int32_t devLogicId_{0};
    uint8_t dieId_{0};
    uint32_t devPhyId_{0};

    std::unordered_map<uint32_t, struct PfeJettyStrategy> pfeJettyMap_;
};

}; // Hccl

#endif // CCU_PFE_MGR_H