/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_CCU_KERNEL_NEW_H
#define HCOMM_CCU_KERNEL_NEW_H

#include <cstdint>
#include <functional>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ccu_task_arg_v1.h"
#include "ccu_task_param_v1.h"

#include "ccu_kernel_resource.h"
#include "ccu_instr_info_v1.h"
#include "ccu_rep_context_v1.h"

#include "ccu_funccall_v1.h"
#include "ccu_loopcall_v1.h"

#include "hccl_types.h"
#include "hcomm_primitives.h"
#include "ccu_datatype_v1.h"
#include "ccu_interface_assist_v1.h"

#include "ccu_res_repo.h"

// 暂时引用方便算法开发
#include "ccu_repeat_v1.h"
#include "ccu_condition_v1.h"
#include "ccu_loopblock_v1.h"
#include "ccu_loopcall_v1.h"
#include "ccu_loopgroupcall_v1.h"
#include "ccu_assist_pub.h"

#ifndef CCU_PROFILING // 和hccl仓兼容性使用
#define CCU_PROFILING
#endif
#include "ccu_types.h"

using CcuKernelHandle = uint64_t;

namespace hcomm {

struct GroupInfo {
    uint16_t loopParamId;
    uint16_t parallelParamId;
    uint16_t residualId;
};

struct GroupOpConfig {
    uint32_t msInterleave;
    uint32_t loopCount;
    uint64_t memSlice;
};

class CcuKernel : public CcuRep::CcuRepContext {
public:
    CcuKernel() = default;
    ~CcuKernel() override;
    HcclResult Init();

    CcuResReq          GetResourceRequest();
    CcuResRepository  &GetResRepository();
    CcuRepResource    &GetResource();
    CcuSharedResource &GetExportedRes();
    CcuSharedResource &GetImportedRes();

    void        SetResRepository(const CcuResRepository &resRepo);
    void        SetInstrId(uint32_t instrId);
    uint32_t    GetInstrId() const;
    uint32_t    GetInstrCount();
    void        SetCcuInstrInfo(const CcuRep::CcuInstrInfo &instrInfo);

    HcclResult GeneTaskParam(const CcuTaskArg &arg, std::vector<CcuTaskParam> &taskParams);

    // 该友元函数用于在context类外创建Variable并被context内的资源管理器管理
    friend CcuRep::Variable CcuRep::CreateVariable(CcuRep::CcuRepContext *context);

    HcclResult AddProfilingInfo(const ChannelHandle *channels, uint32_t channelNum, HcclDataType dataType,
                                HcclDataType outputDataType, HcclReduceOp opType, const std::string& opName);

    HcclResult AddCcuProfiling(GroupInfo groupInfo, const std::vector<ChannelHandle> channelHandle, HcclDataType dataType,
                                 HcclDataType outputDataType, HcclReduceOp opType, const std::string& opName);
    HcclResult AddCcuProfiling(const ChannelHandle *channels, uint32_t channelNum, HcclDataType dataType,
                                HcclDataType outputDataType, HcclReduceOp opType, const std::string& opName);
    HcclResult GetCcuProfilingInfo(const CcuTaskArg &arg, std::vector<CcuProfilingInfo> &allCcuProfilingInfo);

    const std::vector<CcuProfilingInfo> &GetAllCcuProfilingInfo() { return allCcuProfilingInfos_; };
public:
    CcuResult VariableCreate(CcuVariableHandle *var);
    CcuResult VariableAssign(CcuVariableHandle var, uint64_t immediate);
    CcuResult VariableAssignVar(CcuVariableHandle var, CcuVariableHandle varA);
    CcuResult VariableAddVarToVar(CcuVariableHandle resVar,
        CcuVariableHandle varA, CcuVariableHandle varB);
    CcuResult IfBegin(CcuVariableHandle var, uint64_t immediate,
        CcuConditionType condType, const char *label);
    CcuResult IfElse(const char *label);
    CcuResult IfEnd(const char *label);
    CcuResult WhileBegin(CcuVariableHandle var, uint64_t immediate,
        CcuConditionType condType, const char *label);
    CcuResult WhileEnd(const char *label);
    CcuResult DoWhileBegin(const char *label);
    CcuResult DoWhileEnd(CcuVariableHandle var, uint64_t immediate,
        CcuConditionType condType, const char *label);

    CcuResult LoopCreate(CcuLoopHandle *loop);
    CcuResult LoopBodyEnter(CcuLoopHandle loop);
    CcuResult LoopBodyExit(CcuLoopHandle loop);
    CcuResult LoopSetParam(CcuLoopHandle loop,
        CcuVariableHandle formalParam, CcuVariableHandle actualParam);
    CcuResult LoopGroupCreate(CcuLoopGroupHandle *group,
        const CcuLoopGroupConfig *config);
    CcuResult LoopGroupCreateFromVar(CcuLoopGroupHandle *group,
        CcuVariableHandle parallelVar, CcuVariableHandle offsetVar);
    CcuResult LoopGroupAddLoop(CcuLoopGroupHandle group,
        CcuLoopHandle loop, const CcuLoopConfig *config, bool isUnroll);
    CcuResult LoopGroupAddLoopFromVar(CcuLoopGroupHandle group,
        CcuLoopHandle loop, CcuVariableHandle loopParamVar, bool isUnroll);

    CcuResult AddressCreate(CcuAddressHandle *addrHandle);
    CcuResult AddressAssignImm(CcuAddressHandle addr, uint64_t immediate);
    CcuResult AddressAssignVar(CcuAddressHandle addr, CcuVariableHandle var);
    CcuResult AddressAssignAddr(CcuAddressHandle dstAddrHandle, CcuAddressHandle srcAddrHandle);
    CcuResult AddressAddVarToAddr(CcuAddressHandle resAddr, CcuAddressHandle lhsAddr, CcuVariableHandle rhsVar);
    CcuResult AddressAddAddrToAddr(CcuAddressHandle resAddr, CcuAddressHandle addrA, CcuAddressHandle addrB);
    CcuResult AddressAddAssignVar(CcuAddressHandle addr, CcuVariableHandle var);
    CcuResult AddressAddAssignAddr(CcuAddressHandle addr, CcuAddressHandle otherAddr);

    CcuResult LoadArg(CcuVariableHandle varHandle);
    CcuResult ContinuousVariableCreate(CcuVariableHandle* varHandle);
    CcuResult LoadVariable(uint64_t addr, CcuVariableHandle varHandle, uint32_t num);
    CcuResult LoadVariable(uint64_t addr, CcuVariableHandle varHandle);
    CcuResult CompletedEventCreate(CcuEventHandle *eventHandle);
    CcuResult ContinuousEventCreate(CcuEventHandle *eventHandle, uint32_t num);
    CcuResult SetEventMask(CcuEventHandle eventHandle, uint32_t mask);
    CcuResult CompletedEventRecord(CcuEventHandle eventHandle);
    CcuResult CompletedEventWait(CcuEventHandle eventHandle);
    CcuResult BufferCreate(CcuBufferHandle *bufHandle);
    CcuResult BlockBufferCreate(CcuBufferHandle *bufferHandles, uint32_t count);
    CcuResult LocalCopyToBuffer(CcuBufferHandle dstBuffer, CcuLocalAddrHandle src,
        CcuVariableHandle len, CcuEventHandle event);
    CcuResult LocalCopyFromBuffer(CcuLocalAddrHandle dst, CcuBufferHandle srcBuffer,
            CcuVariableHandle len, CcuEventHandle event);
    CcuResult LocalCopy(CcuLocalAddrHandle dst, CcuLocalAddrHandle src,
        CcuVariableHandle len, CcuEventHandle event);
    CcuResult LocalAddrReduce(CcuLocalAddrHandle dst, CcuLocalAddrHandle src,
        CcuVariableHandle len, HcclDataType dataType,
        HcclReduceOp opType, CcuEventHandle event);
    CcuResult LocalBufferReduce(CcuBufferHandle* buffers, uint32_t count,
        HcclDataType dataType, HcclDataType outputDataType,
        HcclReduceOp opType, CcuVariableHandle len, CcuEventHandle event);
        
    CcuResult LocalAddrCreate(CcuLocalAddrHandle *handle,
        CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle);
    CcuResult RemoteAddrCreate(CcuRemoteAddrHandle *handle,
        CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle);
    
    CcuResult Read(ChannelHandle channel, CcuLocalAddrHandle localHandle, CcuRemoteAddrHandle remoteHandle,
        CcuVariableHandle lenHandle, CcuEventHandle eventHandle);
    CcuResult ReadBuffer(ChannelHandle channel, CcuBufferHandle localHandle, CcuRemoteAddrHandle remoteHandle,
        CcuVariableHandle lenHandle, CcuEventHandle eventHandle);
    CcuResult Write(ChannelHandle channel, CcuLocalAddrHandle localHandle, CcuRemoteAddrHandle remoteHandle,
        CcuVariableHandle lenHandle, CcuEventHandle eventHandle);
    CcuResult WriteBuffer(ChannelHandle channel, CcuBufferHandle localHandle, CcuRemoteAddrHandle remoteHandle,
        CcuVariableHandle lenHandle, CcuEventHandle eventHandle);
    CcuResult ReadReduce(ChannelHandle channel, CcuLocalAddrHandle localHandle, CcuRemoteAddrHandle remoteHandle,
        CcuVariableHandle lenHandle, HcclDataType dataType,
        HcclReduceOp opType, CcuEventHandle eventHandle);
    CcuResult WriteReduce(ChannelHandle channel, CcuRemoteAddrHandle remoteHandle, CcuLocalAddrHandle localHandle,
        CcuVariableHandle lenHandle, HcclDataType dataType,
        HcclReduceOp opType, CcuEventHandle eventHandle);
    /*========== 远端同步操作 ==========*/
    CcuResult WriteVariableWithSignal(const ChannelHandle channel, CcuVariableHandle varHandle,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask);
    CcuResult NotifyWait(const ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask);
private:
    CcuResult GetVariableByHandle(CcuVariableHandle varHandle, CcuRep::Variable **variable);
    CcuResult GetCompletedEventByHandle(CcuEventHandle eventHandle, CcuRep::CompletedEvent **event);

    struct PendingIfContext {
        std::shared_ptr<CcuRep::CcuRepJumpLabel> elseLabel;
        std::shared_ptr<CcuRep::CcuRepJumpLabel> endLabel;
        bool hasElse{false};
    };

    struct PendingWhileContext {
        std::shared_ptr<CcuRep::CcuRepJumpLabel> beginLabel;
        std::shared_ptr<CcuRep::CcuRepJumpLabel> endLabel;
        CcuVariableHandle varHandle;
        uint64_t immediate;
        CcuConditionType condType;
    };

    struct PendingDoWhileContext {
        std::shared_ptr<CcuRep::CcuRepJumpLabel> beginLabel;
    };

    std::unordered_map<CcuVariableHandle, CcuRep::Variable> ccuVarMap_{};

    std::unordered_map<std::string, PendingIfContext> pendingIfCtx_{};
    std::unordered_map<std::string, PendingWhileContext> pendingWhileCtx_{};
    std::unordered_map<std::string, PendingDoWhileContext> pendingDoWhileCtx_{};

    std::unordered_map<CcuEventHandle, CcuRep::CompletedEvent> ccuEventMap_{};

    CcuResult GetBufferByHandle(CcuBufferHandle bufferHandle, CcuRep::CcuBuf **buffer);
    std::unordered_map<CcuBufferHandle, CcuRep::CcuBuf> ccuBufferMap_{};

    CcuResult GetAddressByHandle(CcuAddressHandle addrHandle, CcuRep::Address **address);
    std::unordered_map<CcuAddressHandle, CcuRep::Address> ccuAddrMap_{};

    CcuResult GetLocalAddrByHandle(CcuLocalAddrHandle handle, CcuRep::LocalAddr **localAddr);
    std::unordered_map<CcuLocalAddrHandle, CcuRep::LocalAddr> ccuLocalAddrMap_{};

    CcuResult GetRemoteAddrByHandle(CcuRemoteAddrHandle handle, CcuRep::RemoteAddr **remoteAddr);
    std::unordered_map<CcuRemoteAddrHandle, CcuRep::RemoteAddr> ccuRemoteAddrMap_{};


protected:
    // 子类实现
    // virtual HcclResult Algorithm() = 0;
    // virtual std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) = 0;

    // 使用channel中的Variable
    HcclResult CreateVariable(const ChannelHandle channel, uint32_t varIndex, CcuRep::Variable *var) const;
    CcuRep::Variable CreateVariable();
    CcuRep::Variable CreateContinuousVariable();
    CcuRep::LocalAddr CreateLocalAddr();
    CcuRep::RemoteAddr CreateRemoteAddr();
    CcuRep::RemoteAddr GetRemoteAddr(const ChannelHandle channel, const uint32_t index);
    CcuRep::LocalNotify CreateLocalNotify();
    CcuRep::CompletedEvent CreateCompletedEvent();
    CcuRep::CcuBuf CreateCcuBuf();
    CcuRep::Executor CreateExecutor();

    HcclResult CreateBlockCcuBuf(const uint32_t count, CcuRep::CcuBuf *ccuBufs);
    HcclResult CreateBlockExecutor(const uint32_t count, CcuRep::Executor *ccuExes);
    HcclResult CreateBlockCompletedEvent(const uint32_t count, CcuRep::CompletedEvent *ccuEvents);

    HcclResult RecordEvent(CcuRep::CompletedEvent event);
    HcclResult WaitEvent(CcuRep::CompletedEvent event);

    HcclResult LocalNotifyRecord(const uint32_t coreId, const uint32_t dstNotifyIdx, const uint32_t mask);
    HcclResult LocalNotifyWait(const uint32_t coreId, const uint32_t notifyIdx, const uint32_t mask);

    HcclResult NotifyRecord(const ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask=1);

    HcclResult NotifyRecord(const ChannelHandle channel, uint32_t remoteNotifyIdx,
                                 uint32_t remoteVarIdx, const CcuRep::Variable &var, uint32_t mask = 1);
    // HcclResult NotifyWait(const ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask = 1);

    // 数据操作
    HcclResult WriteNb(const ChannelHandle channel, const CcuRep::RemoteAddr &rem, const CcuRep::LocalAddr &loc,
                 const CcuRep::Variable &len, CcuRep::CompletedEvent event);   
    HcclResult WriteNb(const ChannelHandle channel, const CcuRep::RemoteAddr &rem, const CcuRep::CcuBuf &loc,
                 const CcuRep::Variable &len, CcuRep::CompletedEvent event);

    HcclResult ReadNb(const ChannelHandle channel, const CcuRep::LocalAddr &loc, const CcuRep::RemoteAddr &rem,
              const CcuRep::Variable &len, CcuRep::CompletedEvent event);
    HcclResult ReadNb(const ChannelHandle channel, const CcuRep::CcuBuf &loc, const CcuRep::RemoteAddr &rem,
              const CcuRep::Variable &len, CcuRep::CompletedEvent event);

    HcclResult WriteReduceNb(const ChannelHandle channel, const CcuRep::RemoteAddr &rem, const CcuRep::LocalAddr &loc,
                     const CcuRep::Variable &len, HcclDataType dataType, HcclReduceOp opType, CcuRep::CompletedEvent event);
    HcclResult ReadReduceNb(const ChannelHandle channel, const CcuRep::LocalAddr &loc, const CcuRep::RemoteAddr &rem,
                    const CcuRep::Variable &len, HcclDataType dataType, HcclReduceOp opType, CcuRep::CompletedEvent event);
    
    HcclResult LocalCopyNb(const CcuRep::LocalAddr &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                   CcuRep::CompletedEvent event);//dst和src是否都是local
    HcclResult LocalCopyNb(const CcuRep::CcuBuf &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                   CcuRep::CompletedEvent event);
    HcclResult LocalCopyNb(const CcuRep::LocalAddr &dst, const CcuRep::CcuBuf &src, const CcuRep::Variable &len,
                   CcuRep::CompletedEvent event);

    HcclResult LocalReduceNb(const CcuRep::LocalAddr &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                     HcclDataType dataType, HcclReduceOp opType, CcuRep::CompletedEvent event);
    HcclResult LocalReduceNb(const CcuRep::CcuBuf *bufs, uint32_t count, HcclDataType dataType,
                     HcclDataType outputDataType, HcclReduceOp opType,
                     const CcuRep::Variable &len, CcuRep::CompletedEvent event);

    // 参数操作
    void Load(const CcuRep::Variable &var);

    // Variable src中存放内存地址，从地址中加载数据到Variable var中
    void LoadVariable(const CcuRep::Variable &src, const CcuRep::Variable &var);

    // void LoadVariable(uint64_t addr, const CcuRep::Variable &var);
    void StoreVariable(const CcuRep::Variable &var, uint64_t addr);
    // void LoadVariable(uint64_t addr, const CcuRep::Variable &var, uint32_t num);
    // 控制逻辑
    // 宏定义IF、WHILE
    CcuRep::FuncCall Func(const std::string &label);
    CcuRep::FuncCall Func(const CcuRep::Variable &funcAddr);
    CcuRep::LoopCall Loop(const std::string &label);

private:
    CcuRep::Address CreateAddress();
    CcuRep::LocalAddr CreateLocalAddr(const CcuRep::Variable &token);

protected:
    GroupOpConfig       moConfig_{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF};

private:
    template <typename T> T CreateResAssist(std::array<std::vector<T>, CCU_MAX_IODIE_NUM> &resRecord);
    template <typename T> std::vector<T> CreateBlockResAssist(const uint32_t count,
                                        std::array<std::vector<T>, CCU_MAX_IODIE_NUM> &resRecord);

private:
    CcuRepResource    res_{};
    CcuResRepository  resRepo_{};

    std::vector<ChannelHandle> channels_;

    CcuRep::CcuInstrInfo instrInfo_{};

    uint32_t loadArgIndex_{0};

    CcuSharedResource exportedRes_{};
    CcuSharedResource importedRes_{};
    std::vector<GroupInfo> groupOpSizeInfo_;
    std::vector<CcuProfilingInfo> allCcuProfilingInfos_;

    struct ParamBinding {
        CcuRep::Variable formal;
        CcuRep::Variable actual;
    };

    struct LoopDescriptor {
        std::string label;
        std::shared_ptr<CcuRep::CcuRepLoopBlock> repLoopBlock;
        std::shared_ptr<CcuRep::CcuRepBlock> prevActiveBlock;
        std::vector<ParamBinding> paramBindings;
        bool bodyDefined{false};
        CcuRep::Executor executor;
        bool executorAssigned{false};
    };

    struct LoopGroupDescriptor {
        CcuLoopGroupConfig config;
        uint64_t repeatLoopIdx{0};
        uint64_t totalLoopNum{0};
        std::vector<CcuLoopHandle> nonUnrollLoops;
        std::vector<CcuLoopHandle> unrollLoops;
        CcuRep::Variable parallelVar;
        CcuRep::Variable offsetVar;
        std::shared_ptr<CcuRep::CcuRepBase> bundleRep;
        bool isVarBased{false};
    };

    std::unordered_map<CcuLoopHandle, LoopDescriptor> loopMap_;
    std::unordered_map<CcuLoopGroupHandle, LoopGroupDescriptor> loopGroupMap_;
    uint32_t loopHandleCounter_{0};
    uint32_t loopGroupHandleCounter_{0};
    uint32_t loopBodyDepth_{0};
};

} // namespace hcomm

#endif // HCOMM_CCU_KERNEL_NEW_H