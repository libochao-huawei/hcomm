/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu context header file
 * Create: 2025-02-18
 */

#ifndef CCU_REP_CTX_H
#define CCU_REP_CTX_H

#include <set>
#include <string>

#include "ccu_rep_base_v1.h"
#include "ccu_rep_block_v1.h"
#include "ccu_kernel_arg.h"

#include "ccu_common.h"

namespace hcomm {
namespace CcuRep {

class CcuRepContext {
public:
    explicit CcuRepContext();
    virtual ~CcuRepContext();

    // 平台层内部使用
    std::shared_ptr<CcuRep::CcuRepBlock> CurrentBlock();
    void                                 SetCurrentBlock(std::shared_ptr<CcuRep::CcuRepBlock> repBlock);
    void                                 Append(std::shared_ptr<CcuRep::CcuRepBase> rep);
    const std::vector<std::shared_ptr<CcuRep::CcuRepBase>> &GetRepSequence();
    std::shared_ptr<CcuRep::CcuRepBase> GetRepByInstrId(uint16_t instrId);
    void DumpReprestation();

    void     SetDieId(uint32_t dieId);
    uint32_t GetDieId() const;
    void     SetMissionId(uint32_t missionId);
    uint32_t GetMissionId() const;
    void     SetMissionKey(uint32_t missionKey);
    uint32_t GetMissionKey() const;
    /**
        @details 获取返回的profiling信息
    */
    std::vector<CcuProfilingInfo> & GetProfilingInfo();
    /**
        @details 获取返回的loopGroup的profiling信息
    */
    LoopGroupProfilingInfo &GetLGProfilingInfo();

    /**
        @details 获取返回的waitcke的profiling信息
    */
    const std::vector<std::shared_ptr<CcuRepBase>> &GetWaiteCkeProfilingReps() const;
    /**
        @details:保存rep信息，后续有arg后配合补全profiling信息
    */
    void CollectProfilingReps(std::shared_ptr<CcuRep::CcuRepBase> rep);
    /**
        @details:添加sqe粒度的profiling信息，在ctx构造时调用
    */
    void AddSqeProfiling(const CcuCtxArg &arg);

    void AddProfiling(const std::string &name, uint32_t mask);

    void AddProfiling(const CcuTransport &transport, const std::string &name, uint32_t signalIndex, uint32_t mask);
    void AddProfiling(const CcuTransportGroup &transportGroup, const std::string &name, uint32_t signalIndex, uint32_t mask);
    void AddProfiling(const std::vector<CcuTransport*> &transports);
    void AddProfiling(const std::vector<CcuTransport *> &transports, DataType dataType, DataType outputDataType,
                      ReduceOp opType);

protected:
    std::set<std::string> registeredLoop;

private:
    std::shared_ptr<CcuRep::CcuRepBlock> activeBlock{nullptr};
    std::shared_ptr<CcuRep::CcuRepBlock> mainBlock{nullptr};

    uint32_t             dieId{CCU_MAX_IODIE_NUM};
    uint32_t             missionId{UINT32_MAX};
    uint32_t             missionKey{0};
    // CCU Profiling相关数据
    CcuProfilingInfo ccuProfilingInfoCache;
    std::vector<std::shared_ptr<CcuRepBase>> allLgProfilingReps;  // 当前所有的loopGroup Rep
    LoopGroupProfilingInfo lgProfilingInfo; // LoopGroup相关profiling缓存信息
    std::vector<std::shared_ptr<CcuRepBase>> waitCkeProfilingReps; // waitCKE相关REP缓存
    std::vector<CcuProfilingInfo> profilingInfo; // context全部profiling缓存信息
};

}; // namespace CcuRep
}; // namespace hcomm

#endif // _CCU_REP_CTX_H