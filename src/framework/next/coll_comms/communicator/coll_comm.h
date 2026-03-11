/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COLL_COMM_H
#define COLL_COMM_H

#include <memory>
#include <string>
#include "my_rank.h"
// #include "rank_graphs/rank_graph.h"
#include "rank_graph.h"
#include "comm_config_pub.h"
#include "comm_engine_res_manager.h"
#include "independent_op_context_manager.h"
#include "comm_mem_manager.h"
#include "channel_manager.h"
#include "hcclCommDfx.h"
#include "rank_graph_v2.h"
#include "error_message_v2.h"

namespace hccl {
/**
 * @note 职责：集合通信通信域上下文管理，包括RankGraph和本rank信息资源等内容。
 * 当前需包含原有的91092/91093的通信域、原有的91095的通信域void
 * *指针、及新独立算子架构的通信域（支持91092/91093/91095...）。
 */
class CollComm {
public:
    CollComm(void *comm, uint32_t rankId, const std::string &commName, const ManagerCallbacks& callbacks);
    ~CollComm();
    
    // 初始化通信域
    HcclResult Init(void * rankGraph, aclrtBinHandle binHandle, HcclMem cclBuffer, HcclCommConfig *config);

    inline RankGraph* GetRankGraph() { return rankgraph_.get(); }
    inline CommEngineResMgr* GetCommEngineResMgr() { return commEngineResMgr_.get(); }
    inline ContextManager* GetContextManager() { return contextMgr_.get(); }
    inline CommMemMgr* GetCommMemMgr() { return commMemMgr_.get(); }
    inline ChannelManager* GetChannelManager() { return channelMgr_.get(); }
    void *GetCommunicatorV2() { return comm_; }
    // 获取MyRank
    MyRank* GetMyRank() const { return myRank_.get(); }
    
    // 获取Rank ID
    uint32_t GetMyRankId() const;
    
    // 获取Rank数量
    uint32_t GetRankSize() {
        if(rankgraph_ == nullptr){
            HCCL_ERROR("[CollComm]get ranksize failed");
            return 0;
        }
        uint32_t rankSize{0};
        HcclResult ret = rankgraph_->GetRankSize(&rankSize);
        if(ret != 0){
            HCCL_ERROR("[CollComm]get ranksize failed");
            return 0;
        }
        return rankSize;
    }

    // 获取HcclCommDfx
    HcclCommDfx* GetHcclCommDfx() { return hcclCommDfx_.get(); }
    std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> GetDfxCallback() {
        if (hcclCommDfx_ == nullptr) {
            HCCL_ERROR("[CollComm]CollComm DfxCallBack failed. hcclCommDfx is nullptr");
            return nullptr;
        }
        return hcclCommDfx_->GetCallback();
    }
    const std::string& GetCommId() const {return commId_;}
    HcclResult GetHDCommunicate(
        HDCommunicateParams &kfcControlTransferH2DParams, HDCommunicateParams &kfcStatusTransferD2HParams);
    void RegisterAicpuTaskExceptionCallback(u32 streamId);
    Hccl::ErrorMessageReport GetAicpuTaskException();
    uint32_t UpdateIndex();

private:
    HcclResult DestroyAicpuComm();
    HcclResult InitHDCommunicate();   
    HcclResult InitTaskExceptionHandler();

    void* comm_{nullptr};
    uint32_t rankId_{};
    std::string commId_;
    CommConfig config_{};
    ManagerCallbacks callbacks_; 
    s32 deviceLogicId_{0};
    uint32_t index_{0};


    std::unique_ptr<RankGraph> rankgraph_{nullptr};
    std::unique_ptr<CommEngineResMgr> commEngineResMgr_{nullptr};
    std::unique_ptr<ContextManager>  contextMgr_{nullptr};
    std::unique_ptr<CommMemMgr> commMemMgr_{nullptr};
    std::unique_ptr<ChannelManager> channelMgr_{nullptr};
    
    //TODO
    std::shared_ptr<MyRank> myRank_{};
    std::unique_ptr<HcclCommDfx> hcclCommDfx_{nullptr};
    uintptr_t   addr_{0};
    std::size_t size_{0};
    HcclMemType memType_{HcclMemType::HCCL_MEM_TYPE_DEVICE};
    HcclCommDfx dfx_;

    std::shared_ptr<HDCommunicate> kfcControlTransferH2D_{nullptr};
    std::shared_ptr<HDCommunicate> kfcStatusTransferD2H_{nullptr};
};
}  // namespace hccl

#endif  // COLL_COMM_H
