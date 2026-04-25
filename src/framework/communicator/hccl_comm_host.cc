/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <atomic>
#include <algorithm>
#include <arpa/inet.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <hccl/hccl_types.h>
#include "device_capacity.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "task_abort_handler_pub.h"
#include "coll_alg_utils.h"
#include "env_config.h"
#include "i_hccl_one_sided_service.h"
#include "comm_configer.h"
#include "launch_aicpu.h"
#include "launch_device.h"
#include "sal_pub.h"

namespace hccl
{
    HcclResult hcclComm::AllReduce(const std::string &tag, void *inputPtr, void *outputPtr, u64 count,
                                   HcclDataType dataType, HcclReduceOp op, HcclRtStream stream, SyncMode syncMode)
    {
        /* 增加输出日志关键字 */
        HCCL_INFO("HCCL_KEY_INFO: tag[%s], input_ptr[%p], output_ptr[%p], count[%llu], data_type[%s], op[%s]",
                   tag.c_str(), inputPtr, outputPtr, count, GetDataTypeEnumStr(dataType).c_str(),
                   GetReduceOpEnumStr(op).c_str());

        /* * 入参检查 */
        CHK_PTR_NULL(stream);
        CHK_PTR_NULL(inputPtr);
        CHK_PTR_NULL(outputPtr);

        CHK_PRT_RET(tag.empty(), HCCL_ERROR("[HcclComm][AllReduce]errNo[0x%016llx] AllReduce tag length is 0", HCCL_ERROR_CODE(HCCL_E_PARA)), HCCL_E_PARA);

        CHK_RET(communicator_->CheckCount(count));
        CHK_RET(communicator_->CheckDataType(dataType, true));
        CHK_RET(communicator_->CheckReduceDataType(dataType, op));
        CHK_RET(communicator_->CheckReductionOp(op));
        HcclResult ret = communicator_->AllReduce(tag, inputPtr, outputPtr, count, dataType, op, stream, syncMode);
        if (ret != HCCL_SUCCESS)
        {
            PrintSubmittedOpCnt(tag, ret);
            return ret;
        }

        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::AllReduceOutPlace(const std::string &tag, void *inputPtr, void *outputPtr, u64 count,
                                           HcclDataType dataType, HcclReduceOp op, HcclRtStream stream, SyncMode syncMode)
    {
        /* 增加输出日志关键字 */
        HCCL_INFO("HCCL_KEY_INFO: tag[%s], input_ptr[%p], output_ptr[%p], count[%llu], data_type[%s], op[%s]", tag.c_str(),
                   inputPtr, outputPtr, count, GetDataTypeEnumStr(dataType).c_str(), GetReduceOpEnumStr(op).c_str());

        /* * 入参检查 */
        CHK_RET(communicator_->CheckDataType(dataType, true));
        CHK_RET(communicator_->CheckReduceDataType(dataType, op));
        HcclResult ret = communicator_->AllReduceOutPlace(tag, inputPtr, outputPtr, count, dataType, op, stream, syncMode);
        if (ret != HCCL_SUCCESS)
        {
            PrintSubmittedOpCnt(tag, ret);
            return ret;
        }

        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::GetOneSidedService(IHcclOneSidedService **service)
    {
        CHK_RET(communicator_->GetOneSidedService(service));

        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::InitOneSidedServiceNetDevCtx(u32 remoteRankId)
    {
        CHK_RET(communicator_->InitOneSidedServiceNetDevCtx(remoteRankId));
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::OneSidedServiceStartListen(NicType nicType, HcclNetDevCtx netDevCtx)
    {
        CHK_SMART_PTR_NULL(communicator_);
        CHK_RET(communicator_->OneSidedServiceStartListen(nicType, netDevCtx));
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::GetOneSidedServiceDevIpAndPort(NicType nicType, HcclIpAddress& ipAddress, u32& port)
    {
        CHK_SMART_PTR_NULL(communicator_);
        CHK_RET(communicator_->GetOneSidedServiceDevIpAndPort(nicType, ipAddress, port));
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::DeinitOneSidedService()
    {
        CHK_SMART_PTR_NULL(communicator_);
        CHK_RET(communicator_->DeinitOneSidedService());
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::RegistTaskAbortHandler() const
    {
        HCCL_RUN_INFO("RegistTaskAbortHandler begin, group[%s]", identifier_.c_str());
        CHK_RET(TaskAbortHandler::Init(communicator_.get()));
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::UnRegistTaskAbortHandler() const
    {
        HCCL_RUN_INFO("UnRegistTaskAbortHandler begin, group[%s]", identifier_.c_str());
        CHK_RET(TaskAbortHandler::DeInit(communicator_.get()));
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::RegisterCommUserMem(void* addr, u64 size, void **handle)
    {
        CHK_SMART_PTR_NULL(communicator_);
        CHK_RET(communicator_->RegisterCommUserMem(addr, size, handle));
        return HCCL_SUCCESS;
    }
 
    HcclResult hcclComm::DeregisterCommUserMem(void* handle)
    {
        CHK_SMART_PTR_NULL(communicator_);
        CHK_RET(communicator_->DeregisterCommUserMem(handle));
        return HCCL_SUCCESS;
    }
 
    HcclResult hcclComm::ExchangeCommUserMem(void* handle, std::vector<u32>& peerRanks)
    {
        CHK_SMART_PTR_NULL(communicator_);
        return communicator_->ExchangeCommUserMem(handle, peerRanks);
    }

    HcclResult hcclComm::SetIndependentOpConfig(const CommConfig &commConfig, const RankTable_t &rankTable)
    {
        CHK_SMART_PTR_NULL(communicator_);
        HcclTopoAttr topoAttr = communicator_->GetTopoAttr();
        aclrtBinHandle binHandle = communicator_->GetBinHandle();
        HDCommunicateParams kfcControlTransferH2DParams;
        HDCommunicateParams kfcStatusTransferD2HParams;
        std::function<bool()> getAicpuCommState = [this]() { return this->GetIndependentOp().GetAicpuCommState(); };
        CHK_RET(communicator_->GetHDCommunicate(kfcControlTransferH2DParams, kfcStatusTransferD2HParams));
        CHK_RET(communicator_->SetGetAicpuCommState(getAicpuCommState));
        CHK_RET(GetIndependentOp().SetIndependentOpConfig(commConfig, rankTable, topoAttr, binHandle,
            kfcControlTransferH2DParams, kfcStatusTransferD2HParams, communicator_->GetCCLbufferManager()));
        return HCCL_SUCCESS;
    }

    HcclResult hcclComm::ReleaseChannel()
    {
        return independentOp_.GetChannelManager().ReleaseChannel();
    }

    HcclResult hcclComm::InitIndependentOp()
    {
        if (communicator_ != nullptr) {
            communicator_->SetReleaseChannel([this]() -> HcclResult { return this->ReleaseChannel(); });
        }
        ChannelManagerCallbacks channelCallbacks;
        channelCallbacks.indOpTransportAlloc = [this](const std::string &tag, OpCommTransport &opCommTransport, 
            bool isAicpuModeEn) -> HcclResult {
            return this->IndOpTransportAlloc(tag, opCommTransport, isAicpuModeEn);
        };
        channelCallbacks.getRankLists = [this]() -> std::vector<RankInfo> { return this->GetRankLists(); };
        return independentOp_.SetChannelCallbacks(channelCallbacks);
    }

    IndependentOp& hcclComm::GetIndependentOp() {
        return independentOp_;
    }
    HcclResult hcclComm::PrepareChannelMem(const std::string &tag, TransportIOMem &transMem)
    {
        // 获取本地cclbuffer
        CommBuffer commBuffer;
        CHK_RET(GetIndependentOp().GetCommMemMgr().GetHcclBuffer(&commBuffer));
        DeviceMem cclbuffer =  DeviceMem::create(commBuffer.addr, commBuffer.size);
        CHK_PTR_NULL(cclbuffer.ptr());

        // 获取通信域内存
        IndOpMem indOpMem{};
        std::vector<HcclMem> localMemVec{};
        CHK_RET(GetIndependentOp().GetCommMemMgr().CommGetLocalRegMemByTag(tag, localMemVec));
        for (const HcclMem& mem : localMemVec) {
            if (mem.type == HCCL_MEM_TYPE_HOST) {
                indOpMem.userHostMem.push_back(HostMem::create(mem.addr, mem.size));
                CHK_PTR_NULL(indOpMem.userHostMem.back().ptr());
            } else if (mem.type == HCCL_MEM_TYPE_DEVICE) {
                indOpMem.userDeviceMem.push_back(DeviceMem::create(mem.addr, mem.size));
                CHK_PTR_NULL(indOpMem.userDeviceMem.back().ptr());
            }
        }
        transMem.indOpMem = indOpMem;
        transMem.cclInputMem = cclbuffer;
        transMem.cclOutputMem = cclbuffer;
        return HCCL_SUCCESS;
    }
    HcclResult hcclComm::IndOpTransportAlloc(const std::string &tag, OpCommTransport &opCommTransport, bool isAicpuModeEn)
    {
        CHK_SMART_PTR_NULL(communicator_);
        TransportIOMem transMem;
        CHK_RET(PrepareChannelMem(tag, transMem));
        std::string commId = GetIdentifier();
        return communicator_->IndOpTransportAlloc(tag, opCommTransport, transMem, isAicpuModeEn);
    }
    HcclResult hcclComm::CommGetNetLayers(uint32_t **netLayers, uint32_t *netLayerNum)
    {
        return communicator_->CommGetNetLayers(netLayers, netLayerNum);
    }
    
    HcclResult hcclComm::CommGetInstSizeByNetLayer(uint32_t netLayer, uint32_t *rankNum)
    {
        return communicator_->CommGetInstSizeByNetLayer(netLayer, rankNum);
    }
    
    HcclResult hcclComm::CommGetInstTopoTypeByNetLayer(uint32_t netLayer, u32 *topoType)
    {
        return communicator_->CommGetInstTopoTypeByNetLayer(netLayer, topoType);
    }
    HcclResult hcclComm::GetNetLayers(uint32_t **netLayers, uint32_t *netLayerNum)
    {
        return communicator_->GetNetLayers(netLayers, netLayerNum);
    }
    
    HcclResult hcclComm::GetInstSizeByNetLayer(uint32_t netLayer, uint32_t *rankNum)
    {
        return communicator_->GetInstSizeByNetLayer(netLayer, rankNum);
    }
    
    HcclResult hcclComm::GetInstTopoTypeByNetLayer(uint32_t netLayer, CommTopo *topoType)
    {
        return communicator_->GetInstTopoTypeByNetLayer(netLayer, topoType);
    }

    HcclResult hcclComm::GetInstRanksByNetLayer(uint32_t netLayer, uint32_t **rankList, uint32_t *rankNum)
    {
        return communicator_->GetInstRanksByNetLayer(netLayer, rankList, rankNum);
    }
    
    HcclResult hcclComm::GetInstSizeListByNetLayer(uint32_t netLayer, uint32_t **instSizeList, uint32_t *listSize)
    {
        return communicator_->GetInstSizeListByNetLayer(netLayer, instSizeList, listSize);
    }

    HcclResult hcclComm::GetRankGraph(GraphType type, void **graph, uint32_t *len)
    {
        return communicator_->GetRankGraph(type, graph, len);
    }

    HcclResult hcclComm::GetLinks(uint32_t netLayer, uint32_t srcRank, uint32_t dstRank,
        CommLink **linkList, uint32_t *listSize)
    {
        return communicator_->GetLinks(netLayer, srcRank, dstRank, linkList, listSize);
    }

    HcclResult hcclComm::GetHeterogMode(HcclHeterogMode *mode)
    {
        return communicator_->GetHeterogMode(mode);
    }

    inline uint32_t GetCollCommOpExpansionMode(CollComm *collComm)
    {
        auto *myRank = collComm->GetMyRank();
        CHK_PTR_NULL(myRank);
        return myRank->GetOpExpansionMode();
    }

    HcclResult hcclComm::InitCollComm(void* commV2, void* rankGraph, uint32_t userRank,
        HcclMem cclBuffer, const std::string &commName, HcclCommConfig *config) {
        // 不校验config，为空时配置默认加速模式

        // aicpu侧初始化状态的回调函数
        ManagerCallbacks callbacks;
        callbacks.getAicpuCommState = [this]() {
            return this->GetAicpuCommState();
        };
        callbacks.setAicpuCommState = [this](bool state) {
            this->SetAicpuCommState(state);
        };
        callbacks.kernelLaunchAicpuCommInit = [this]() {
            return this->KernelLaunchAicpuCommInit();
        };

        // Aicpu通信域初始化参数
        auto ret = snprintf_s(commAicpuParam_.hcomId, HCOMID_MAX_SIZE, HCOMID_MAX_SIZE - 1, "%s", commName.c_str());
        if (ret < 0) {
            HCCL_ERROR("[InitCollComm]comm id snprintf_s fail, commId: %s, commId maxSize: %u", commName.c_str(),
                    HCOMID_MAX_SIZE);
            return HCCL_E_PARA;
        }
  
        CHK_RET(hrtGetDevice(&(commAicpuParam_.deviceLogicId)));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(commAicpuParam_.deviceLogicId), commAicpuParam_.devicePhyId));
        CHK_RET(hrtGetDeviceType(devType_));
        commAicpuParam_.deviceType = static_cast<u32>(devType_);
        std::string jsonPath;
        CHK_RET(GetKernelFilePath(jsonPath));
        jsonPath += "ccl_kernel.json";
   
        HcclResult retCode = LoadBinaryFromFile(jsonPath.c_str(), ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE, 0, binHandle_);
        CHK_PRT_RET(retCode != HCCL_SUCCESS,
            HCCL_ERROR("[InitCollComm]errNo[0x%016llx]load aicpu file fail, path[%s] optionType[%u]"
                       "cpuKernelMode[%u].", retCode, jsonPath.c_str(), ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE, 0),
            retCode);
        CHK_PTR_NULL(commV2);

        EXECEPTION_CATCH(collComm_ = std::make_unique<CollComm>(commV2, userRank, commName, callbacks),
        return HCCL_E_PTR);

        CHK_RET(collComm_->Init(rankGraph, binHandle_, cclBuffer, config));
        CHK_RET(collComm_->GetHDCommunicate(commAicpuParam_.kfcControlTransferH2DParams,
            commAicpuParam_.kfcStatusTransferD2HParams));
        commAicpuParam_.userRank = collComm_->GetMyRankId();
        commAicpuParam_.userRankSize = collComm_->GetRankSize();
        const auto opExpansionMode = GetCollCommOpExpansionMode(collComm_.get());
        HCCL_RUN_INFO("[%s]success, commId[%s], deviceLogicId[%u], devicePhyId[%u], devType[%u], "
            "userRank[%u], userRankSize[%u], opExpansionMode[%u].",
            __func__, collComm_->GetCommId().c_str(), commAicpuParam_.deviceLogicId, commAicpuParam_.devicePhyId,
            commAicpuParam_.deviceType, commAicpuParam_.userRank, commAicpuParam_.userRankSize, opExpansionMode);
        
        const char *opModeEnv = getenv("HCCL_CCU_CUSTOM_OP_MODE");
        if (opModeEnv != nullptr && strcmp(opModeEnv, "1") == 0) {
            // 当前需要支持coll comm与legacy comm混跑，coll comm确定加速模式后，需要设置comm加速模式
            auto *commImplV2 = static_cast<Hccl::HcclCommunicator *>(commV2);
            constexpr bool isCcuMsAvailable = false; // 禁止legacy通信域使用ms模式，避免抢占过多coll comm ccu可用资源
            CHK_RET(commImplV2->SetAccelerator(static_cast<int32_t>(opExpansionMode), isCcuMsAvailable));
        }

        return HCCL_SUCCESS;
    }

    void hcclComm::BinaryUnLoad()
    {
        if (binHandle_ != nullptr){
            HCCL_INFO("[BinaryUnLoad]aclrtBinaryUnLoad binHandle");
            aclError ret = aclrtBinaryUnLoad(binHandle_);
            if (ret != 0) {
                HCCL_RUN_WARNING("[BinaryUnLoad]aclrtBinaryUnLoad binHandle faild");
            }
            binHandle_ = nullptr;
        }
    }

    bool hcclComm::GetAicpuCommState()
    {
        return isAicpuCommInit_;
    }

    void hcclComm::SetAicpuCommState(bool aicpuCommState)
    {
        isAicpuCommInit_ = aicpuCommState;
        return;
    }

    HcclResult hcclComm::KernelLaunchAicpuCommInit()
    {
        // 创建局部流
        u64 beginTime = Hccl::DlProfFunction::GetInstance().dlMsprofSysCycleTime();
        Stream localStream(StreamType::STREAM_TYPE_ONLINE);
        constexpr u32 aicpuStreamMode = 1;
        CHK_RET(hrtStreamSetMode(localStream.ptr(), aicpuStreamMode));

        // 下kernel进行自定义算子aicpu侧通信域的公共初始化
        std::string kernelName = "RunAicpuIndOpCommInit";
        HCCL_INFO("AicpuAclKernelLaunch start");
        u16 timeOut = NOTIFY_DEFAULT_WAIT_TIME > std::numeric_limits<uint16_t>::max() ? 
                        std::numeric_limits<uint16_t>::max() : NOTIFY_DEFAULT_WAIT_TIME;
        CHK_RET(AicpuAclKernelLaunch(localStream.ptr(), reinterpret_cast<void *>(&commAicpuParam_),
            sizeof(commAicpuParam_), binHandle_, kernelName, true, timeOut));
        HCCL_INFO("AicpuAclKernelLaunch end, hcclStreamSynchronize start");
        CHK_RET(hcclStreamSynchronize(localStream.ptr(), CommConfiger::GetInstance().GetCommConfigExecTimeOut("")));
        HCCL_INFO("[KernelLaunchAicpuCommInit] ReportAicpuCommKernel begin");
        CHK_PTR_NULL(collComm_);
        HcclCommDfx* hcclComDfx = collComm_->GetHcclCommDfx();
        CHK_PTR_NULL(hcclComDfx);
        CHK_RET(hcclComDfx->ReportKernel(beginTime, identifier_, kernelName, SalGetTid()));
        HCCL_INFO("[KernelLaunchAicpuCommInit] ReportAicpuCommKernel end");
        // 打印增加初始化对应的参数
        HCCL_RUN_INFO("[%s] KernelLaunchAicpuCommInit Success", __func__);
        return HCCL_SUCCESS;
    }

    HcclComm hcclComm::GetCommunicatorV2()
    {
        if (collComm_ == nullptr) {
            HCCL_ERROR("[HcclComm][GetCommunicatorV2]collComm_ is nullptr");
            return nullptr;
        }
        return collComm_->GetCommunicatorV2();
    }

    CollComm* hcclComm::GetCollComm() 
    {
        return collComm_!= nullptr ? collComm_.get() : nullptr;
    }

    HcclResult hcclComm::Resume()
    {
        if (IsCommunicatorV2()) {
            CHK_RET(collComm_->Resume());
        } else {
            CHK_RET(communicator_->Resume());
        }
        
        return HCCL_SUCCESS;
    }
    HcclResult hcclComm::GetCommStatus(HcclCommStatus &status)
    {
        if (IsCommunicatorV2()) {
            status = collComm_->GetCommStatus();
        } else {
            status = HcclCommStatus::HCCL_COMM_STATUS_READY;
        }
        return HCCL_SUCCESS;
    }

} // namespace hccl
