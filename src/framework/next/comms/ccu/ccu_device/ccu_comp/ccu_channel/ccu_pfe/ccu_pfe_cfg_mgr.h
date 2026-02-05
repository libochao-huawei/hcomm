/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: ccu pfe configure generator header file
 * Create: 2025-02-19
 */

#ifndef CCU_PFE_CFG_MGR_H
#define CCU_PFE_CFG_MGR_H

#include <array>
#include <vector>
#include "hccl_types.h"
#include "ccu_dev_mgr_imp.h"

namespace hcomm {

struct PfeJettyCtxCfg {
    uint32_t feId{0};
    uint32_t startJettyCtxId{0};
    uint32_t startTaJettyId{0};
    uint8_t  size{0};

    PfeJettyCtxCfg() = default;
    PfeJettyCtxCfg(uint32_t feId, uint32_t startJettyCtxId, uint32_t startTaJettyId, uint8_t size) :
        feId(feId), startJettyCtxId(startJettyCtxId), startTaJettyId(startTaJettyId), size(size) {}
};

class CcuPfeCfgMgr {
public:
    static CcuPfeCfgMgr &GetInstance(const int32_t deviceLogicId);
    HcclResult Init();
    HcclResult Deinit();

    std::vector<PfeJettyCtxCfg> GetPfeJettyCtxCfg(const uint8_t dieId);

private:
    explicit CcuPfeCfgMgr() = default;
    ~CcuPfeCfgMgr() = default;
    CcuPfeCfgMgr(const CcuPfeCfgMgr &that) = delete;
    CcuPfeCfgMgr &operator=(const CcuPfeCfgMgr &that) = delete;
    
    HcclResult SetPfeJettyCtxCfgMap(const int32_t logicDeviceId);

private:
    bool initFlag_{false};
    int32_t devLogicId_{0};
    uint32_t devPhyId_{0};
    // 每个iodie上PfeJettyCtxCfg的映射关系
    std::array<std::vector<PfeJettyCtxCfg>, CCU_MAX_IODIE_NUM> pfeJettyCtxCfgs_{};

};

}; // Hccl

#endif // CCU_PFE_CFG_MGR_H