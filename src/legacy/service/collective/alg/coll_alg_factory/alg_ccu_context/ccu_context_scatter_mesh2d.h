/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextScatterMesh2D的头文件
 * Author: chenchao
 * Create: 2025-03-20
 */


#ifndef HCCLV2_CCU_CONTEXT_SCATTER_MESH_2D_H_
#define HCCLV2_CCU_CONTEXT_SCATTER_MESH_2D_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextScatterMesh2D : public CcuContext {
public:
    CcuContextScatterMesh2D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextScatterMesh2D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    uint64_t root_{0};
    std::vector<uint64_t> dimSize_;
    uint64_t rankId_{0};
    uint64_t rankSize_{0};
    uint64_t axisId_{0};
    std::vector<uint64_t> dimId_;
    std::vector<uint64_t> rootDimId_;
    uint64_t localId_{0};
    uint64_t localSize_{0};

    CcuRep::Variable input_;
    std::vector<CcuRep::Variable> scratch_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable sliceSize_;
    CcuRep::Variable stride_;
    std::vector<CcuRep::Variable> axisSliceSize_;
    GroupOpSize curGoSize_;

    std::string localAxisSignalName_;
    std::string anotherAxisSignalName_;
    CcuRep::MaskSignal localAxisSignal_;
    CcuRep::MaskSignal anotherAxisSignal_;

    bool SameRowWithRoot();
    bool SameColumnWithRoot();
    void PrepareVariables();
    void LoadArgs();
    void PreSync();
    void Sync(uint32_t ckeId);
    void AxisSync(uint32_t signalIndex);
    void CcuWrite1DMesh(std::vector<CcuRep::Memory> &src, std::vector<CcuRep::Memory> &dst, CcuRep::Variable &size);
    uint64_t CoordinateToGlobalId(uint32_t x, uint32_t y);
    void CcuMultiply(CcuRep::Memory &a, CcuRep::Variable &b, uint64_t i) const;

    void PrepareAndTransferRootRelayInfo(std::vector<CcuRep::Memory> &relaySrc, std::vector<CcuRep::Memory> &relayDst);
    void RelaySendFor1D(std::vector<CcuRep::Memory> &relaySrc, std::vector<CcuRep::Memory> &relayDst, uint64_t j);
    void PrepareAndTransferRootDirectInfo(std::vector<CcuRep::Memory> &directSrc, std::vector<CcuRep::Memory> &directDst);
    void RelaySend(std::vector<CcuRep::Memory> &relaySrc, std::vector<CcuRep::Memory> &relayDst);
    void LocalTransfer();

    void RootSendAlgorithm();
    void RelaySendAlgorithm();
    void NonDirectRecvAlgorithm();
};
} // namespace Hccl
#endif // HCCLV2_CCU_CONTEXT_SCATTER_MESH_2D_H_