/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_CCU_KERNEL_H
#define HCOMM_CCU_KERNEL_H

#include <functional>
#include <vector>

#include "ccu_types.h"
#include "ccu_datatype.h"
#include "ccu_resource.h"
#include "ccu_control_flow.h"
#include "internal/ccu_internal.h"

#include "hccl_types.h"
#include "hcomm_primitives.h"

using CcuKernelHandle = uint64_t;

namespace hcomm {

class CcuKernelArg {
public:
    explicit CcuKernelArg() = default;
    virtual ~CcuKernelArg() = default;
    virtual CcuKernelSignature GetKernelSignature() const = 0;
    std::vector<ChannelHandle> channels{};
};

class CcuKernelSignature {
public:
    CcuKernelSignature() = default;
    ~CcuKernelSignature() = default;
    CcuKernelSignature(const CcuKernelSignature &other) { data << other.data.str(); }

    void operator=(const CcuKernelSignature &other) { data << other.data.str(); }
    bool operator==(const CcuKernelSignature &rhs) const { return this == &rhs || data.str() == rhs.data.str(); }

    template <typename T>
    void Append(T t) { data << t; }

    void Append(const CcuKernelSignature &other) { data << other.data.str(); }

    std::string Describe() const { return "CcuKernelSignature[data=" + data.str() + "]"; }
    std::string GetData() const { return data.str(); }

    friend class std::hash<hcomm::CcuKernelSignature>;

private:
    std::ostringstream data;
};

class CcuKernel : public CcuRep::CcuRepContext {
public:
    explicit CcuKernel(const CcuKernelArg &arg);
    CcuKernel() = default;
    ~CcuKernel() override;
    HcclResult Init();

    CcuResReq GetResourceRequest();
    CcuResRepository &GetResRepository();
    CcuRepResource &GetResource();
    CcuSharedResource &GetExportedRes();
    CcuSharedResource &GetImportedRes();

    void SetResRepository(const CcuResRepository &resRepo);
    void SetInstrId(uint32_t instrId);
    uint32_t GetInstrId() const;
    uint32_t GetInstrCount();
    void SetCcuInstrInfo(const CcuRep::CcuInstrInfo &instrInfo);

    HcclResult GeneTaskParam(const CcuTaskArg &arg, std::vector<CcuTaskParam> &taskParams);

    friend CcuRep::Variable CcuRep::CreateVariable(CcuRep::CcuRepContext *context);

protected:
    virtual HcclResult Algorithm() = 0;
    virtual std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) = 0;

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

    HcclResult NotifyRecord(const ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask = 1);
    HcclResult NotifyRecord(const ChannelHandle channel, uint32_t remoteNotifyIdx,
                            uint32_t remoteVarIdx, const CcuRep::Variable &var, uint32_t mask = 1);
    HcclResult NotifyWait(const ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask = 1);

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
                           CcuRep::CompletedEvent event);
    HcclResult LocalCopyNb(const CcuRep::CcuBuf &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                           CcuRep::CompletedEvent event);
    HcclResult LocalCopyNb(const CcuRep::LocalAddr &dst, const CcuRep::CcuBuf &src, const CcuRep::Variable &len,
                           CcuRep::CompletedEvent event);

    HcclResult LocalReduceNb(const CcuRep::LocalAddr &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                             HcclDataType dataType, HcclReduceOp opType, CcuRep::CompletedEvent event);
    HcclResult LocalReduceNb(const CcuRep::CcuBuf *bufs, uint32_t count, HcclDataType dataType,
                             HcclDataType outputDataType, HcclReduceOp opType,
                             const CcuRep::Variable &len, CcuRep::CompletedEvent event);

    void Load(const CcuRep::Variable &var);
    void LoadVariable(const CcuRep::Variable &src, const CcuRep::Variable &var);
    void LoadVariable(uint64_t addr, const CcuRep::Variable &var);
    void StoreVariable(const CcuRep::Variable &var, uint64_t addr);
    void LoadVariable(uint64_t addr, const CcuRep::Variable &var, uint32_t num);

    CcuRep::FuncCall Func(const std::string &label);
    CcuRep::FuncCall Func(const CcuRep::Variable &funcAddr);
    CcuRep::LoopCall Loop(const std::string &label);

private:
    CcuRep::Address CreateAddress();
    CcuRep::LocalAddr CreateLocalAddr(const CcuRep::Variable &token);

protected:
    std::vector<ChannelHandle> channels_;

private:
    template <typename T>
    T CreateResAssist(std::array<std::vector<T>, CCU_MAX_IODIE_NUM> &resRecord);
    template <typename T>
    std::vector<T> CreateBlockResAssist(const uint32_t count,
                                        std::array<std::vector<T>, CCU_MAX_IODIE_NUM> &resRecord);

private:
    CcuRepResource res_{};
    CcuResRepository resRepo_{};
    CcuRep::CcuInstrInfo instrInfo_{};
    uint32_t loadArgIndex_{0};
    CcuSharedResource exportedRes_{};
    CcuSharedResource importedRes_{};
};

using KernelCreator = std::function<std::unique_ptr<hcomm::CcuKernel>(const CcuKernelArg &)>;

} // namespace hcomm

namespace std {
template <>
class hash<hcomm::CcuKernelSignature> {
public:
    size_t operator()(const hcomm::CcuKernelSignature &signature) const {
        auto dataHash = hash<string>{}(signature.GetData());
        constexpr size_t res = 17;
        constexpr size_t padding = 31;
        constexpr size_t preHash = res * padding;
        return preHash + dataHash;
    }
};
} // namespace std

#endif // HCOMM_CCU_KERNEL_H
