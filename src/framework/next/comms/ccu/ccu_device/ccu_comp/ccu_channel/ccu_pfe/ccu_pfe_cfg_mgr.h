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

constexpr uint8_t MAX_CCU_INNER_FE_IDX = 7; // 框内最大FE Index
constexpr uint8_t INNER_MAX_CCU_PFE_NUM = 6; // 2D fullmesh组网框内x/y轴最大FE个数
constexpr uint8_t ONE_CCU_PFE_USE_JETTY_NUM = 23;  // 框内每个FE使用的Jetty个数
 // 优先分配框内FE，每个FE按最大数量分配，剩余Jetty分配个出框，每个die仅支持1个出框FE
constexpr uint8_t INNER_MAX_TA_JETTY_ID_OFFSET =
    (INNER_MAX_CCU_PFE_NUM - 2) * ONE_CCU_PFE_USE_JETTY_NUM;

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