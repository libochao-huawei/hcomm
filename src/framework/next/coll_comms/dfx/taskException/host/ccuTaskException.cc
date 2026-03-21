/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcclCommTaskException.h"
#include <memory>
#include "log.h"
#include "coll_comm.h"
#include "acl/acl_rt.h"
#include "orion_adapter_hccp.h"
#include <adapter_error_manager_pub.h>
#include "op_type.h"
#include "task_exception_handler.h"
#include "task_param.h"
#include "ccu_rep_type.h"
#include "ccu_kernel_mgr.h"
#include "hcomm_c_adpt.h"
#include "ccu_urma_channel.h"
#include "orion_adpt_utils.h"
#include "ccuTaskException.h"

namespace hcomm {

using namespace std;
constexpr int BYTE = 8;
constexpr uint64_t CCU_MSG_256MB_LEN = 256 * 1024 * 1024; // CCU消息长度不能大于256MB

struct ccum_dfx_info {
    unsigned int query_result; // 0:success, 1:fail
    unsigned int ccum_sqe_recv_cnt;
    unsigned int ccum_sqe_send_cnt;
    unsigned int ccum_mission_dfx;
    unsigned int ccum_sqe_drop_cnt;
    unsigned int ccum_sqe_addr_len_err_drop_cnt;
    unsigned int lqc_ccu_sec_reg0;
    unsigned int ccum_tif_sqe_cnt;
    unsigned int ccum_tif_cqe_cnt;
    unsigned int ccum_cif_sqe_cnt;
    unsigned int ccum_cif_cqe_cnt;
};
template <typename... Args> inline std::string StringFormat(const char *format, Args... args)
{
    using namespace std;
    constexpr size_t bufSize = BUFSIZ;
    char             buffer[bufSize];
    int result = snprintf_s(&buffer[0], bufSize, bufSize, format, args...);
     if (result < 0) {
        HCCL_ERROR("[StringFormat] data snprintf_s failed.");
        return "";
    }
    size_t actualSize = static_cast<size_t>(result);
    if (actualSize + 1 > bufSize) {
        actualSize++;
        std::vector<char> newbuffer(actualSize);
        auto ret = snprintf_s(newbuffer.data(), actualSize, actualSize, format, args...);
        if(ret != EOK){
            HCCL_ERROR("[StringFormat] data snprintf_s failed.");
            return "";
        }
        return newbuffer.data();
    }
    return buffer;
}
void CcuTaskException::PrintPanicLogInfo(const uint8_t *panicLog)
{
    struct ccum_dfx_info *info = reinterpret_cast<struct ccum_dfx_info *>(const_cast<uint8_t*>(panicLog));
    const uint16_t ccumIsEnable = info->lqc_ccu_sec_reg0 & 1;
    if (info->query_result != 0) {
        HCCL_ERROR("get ccu dfx info fail, ccu dfx info not all correct");
    }
    HCCL_ERROR("CCU DFX INFO: SQE_RECV_CNT[%u] SQE_SEND_CNT[%u] MISSION_DFX[%u]"
                "TIF_SQE_CNT[%u] TIF_CQE_CNT[%u] CIF_SQE_CNT[%u] CIF_CQE_CNT[%u]"
                "SQE_DROP_CNT[%u] SQE_ADDR_LEN_ERR_DROP_CNT[%u] ccumIsEnable[%u]",
                info->ccum_sqe_recv_cnt, info->ccum_sqe_send_cnt, info->ccum_mission_dfx,
                info->ccum_tif_sqe_cnt, info->ccum_tif_cqe_cnt, info->ccum_cif_sqe_cnt, info->ccum_cif_cqe_cnt,
                info->ccum_sqe_drop_cnt, info->ccum_sqe_addr_len_err_drop_cnt, ccumIsEnable);
}

void CcuTaskException::ProcessCcuException(const rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo)
{
    auto deviceId = exceptionInfo->deviceid;
    HCCL_ERROR("[CcuTaskException][%s]Task from HCCL run failed.", __func__);
    HCCL_ERROR("[CcuTaskException]Task run failed, base information is deviceID:[%u], %s.",
        deviceId, taskInfo.GetBaseInfo().c_str());
    HCCL_ERROR("[CcuTaskException]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(taskInfo).c_str());
    HCCL_ERROR("[CcuTaskException]Task run failed, opData information is %s.", taskInfo.GetOpInfo().c_str());
    auto& ccuExDetailInfo = exceptionInfo->expandInfo.u.ccuInfo;
    for (uint32_t i = 0; i < ccuExDetailInfo.ccuMissionNum; ++i) { // ccuExDetailInfo.ccuMissionNum为1
        const auto& missionInfo = ccuExDetailInfo.missionInfo[i]; // 异常mission
        uint16_t status = static_cast<uint16_t>(missionInfo.status) << BYTE | missionInfo.subStatus;
        PrintCcuErrorInfo(deviceId, status, taskInfo);
        // 打印寄存器信息
        PrintPanicLogInfo(missionInfo.panicLog);
    }

    const int32_t devLogicId = static_cast<int32_t>(deviceId);
    if (Hccl::CcuCleanTaskKillState(devLogicId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuTaskException][%s] failed to clean ccu task kill state, "
            "devLogicId[%d].", __func__, devLogicId);
    }

    const uint8_t dieId = taskInfo.taskParam_.taskPara.Ccu.dieId;
    if (Hccl::CcuCleanDieCkes(devLogicId, dieId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuTaskException][%s] failed to clean ccu die ckes, "
            "dieId[%u], devLogicId[%d].", __func__, dieId, devLogicId);
    }
}
void CcuTaskException::PrintCcuErrorInfo(uint32_t deviceId, uint16_t status, const Hccl::TaskInfo& taskInfo)
{
    const Hccl::ParaCcu& ccuTaskParam = taskInfo.taskParam_.taskPara.Ccu;

    // auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(deviceId);
    // auto *kernel = kernelMgr.GetKernel(ccuTaskParam.ccuKernelHandle);
    // CHK_PRT_RET(kernel == nullptr, HCCL_ERROR("[%s]GetKernel nullptr, deviceId[%u], ccuKernelHandle[0x%llx]",
    //     __func__, deviceId, ccuTaskParam.ccuKernelHandle),);

    vector<CcuErrorInfo> errorInfos {};
    HcclResult ret = GetCcuErrorMsg(deviceId, status, ccuTaskParam, errorInfos, reinterpret_cast<void*>(kernel));
    const uint8_t missionStatus = (status >> 8) & 0xFF;
    if (ret != HcclResult::HCCL_SUCCESS || errorInfos.empty()) {
        HCCL_ERROR("Get CCU error info failed. deviceId[%u], dieId[%u], missionId[%u], executeId[%llu].",
            deviceId, ccuTaskParam.dieId, ccuTaskParam.missionId,
            ccuTaskParam.executeId);
        return;
    }
    PrintCcuErrorLog(errorInfos, taskInfo);

    if (missionStatus >= 0x01 && missionStatus <= 0x05) { // 如果是UB错误(missionStatus为[0x01, 0x05])，打印Ub Dfx寄存器信息
        PrintCcuUbRegisters(errorInfos, static_cast<s32>(deviceId), taskInfo);
    }
}

void CcuTaskException::PrintCcuErrorLog(const std::vector<CcuErrorInfo>& errorInfos, const Hccl::TaskInfo& taskInfo)
{
    if (errorInfos.empty()) {
        return;
    }
    HCCL_ERROR("[CcuTaskException]Task run failed, ccu runtime information is: %s", __func__);
    for (const auto& errorInfo : errorInfos) {
        HCCL_ERROR("[CcuTaskException][%s]", GetCcuErrorMsgByType(errorInfo, taskInfo).c_str());
    }
}

HcclResult CcuTaskException::PrintCcuUbRegisters(const std::vector<CcuErrorInfo>& errorInfos, s32 devLogicId,
    const Hccl::TaskInfo& taskInfo)
{
    std::vector<CcuJetty *> ccuJettys;

    for (const CcuErrorInfo& errorInfo : errorInfos) {
        // channelId -> channelHandle
        u64 channelHandle = INVALID_U64;
        CHK_RET(GetCcuChannelHandleById(devLogicId, errorInfo.msg.transMem.channelId, taskInfo, channelHandle));

        // channelHandle -> CcuUrmaChannel
        void *channelPtr{nullptr};
        CHK_RET(HcommChannelGet(channelHandle, &channelPtr));
        CHK_PTR_NULL(channelPtr);
        auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));

        // CcuUrmaChannel -> UrmaEndpoint
        EndpointHandle locEndPointHandle = channelImpl->GetlocEndPointHandle();
        void *endpoint{nullptr};
        CHK_RET(HcommEndpointGet(locEndPointHandle, &endpoint));
        CHK_PTR_NULL(endpoint);
        UrmaEndpoint *ccuEndpoint = dynamic_cast<UrmaEndpoint *>(static_cast<Endpoint *>(endpoint));

        // UrmaEndpoint -> CcuChannelCtxPool
        CcuChannelCtxPool *ccuChannelCtxPool = ccuEndpoint->GetCcuChannelCtxPool();
        CHK_PTR_NULL(ccuChannelCtxPool);

        // CcuChannelCtxPool -> CcuJetty
        auto channelIdKey = std::make_pair(errorInfo.dieId, errorInfo.msg.transMem.channelId);
        std::pair<CcuChannelInfo, std::vector<CcuJetty *>> ctx;
        CHK_RET(ccuChannelCtxPool->GetCcuChannelCtxById(channelIdKey, ctx));

        ccuJettys.insert(ccuJettys.end(), ctx.second.begin(), ctx.second.end());
    }

    u32 jettyNum = ccuJettys.size();
    std::vector<JettyHandle> jettyHandles;
    for (auto &ccuJetty : ccuJettys) {
        jettyHandles.push_back(ccuJetty->GetJettyHandle());
    }

    std::vector<JettyStatus> jettyStatusVec;
    RaBatchQueryJettyStatus(jettyHandles, jettyStatusVec, jettyNum);

    for (u32 i = 0; i < jettyNum; ++i) {
        if (jettyStatusVec[i] == JettyStatus::ERROR) {
            auto rdmaHandle = ccuJettys[i]->GetRdmaHandle();
            HCCL_ERROR("[%s]jettyId[%u]", __func__, ccuJettys[i]->GetJettyId());
            PrintUbRegisters(devLogicId, rdmaHandle);
            break;
        }
    }
    return HCCL_SUCCESS;
}
HcclResult RaGetAuxInfo(const RdmaHandle rdmaHandle, AuxInfoIn auxInfoIn, AuxInfoOut &auxInfoOut)
{
    HccpAuxInfoIn in;
    in.type = static_cast<HccpAuxInfoInType>(static_cast<int>(auxInfoIn.auxInfoInType));
    if (auxInfoIn.auxInfoInType == AuxInfoInType::AUX_INFO_IN_TYPE_CQE) {
        in.cqe.status = auxInfoIn.cqe.status;
        in.cqe.sR = auxInfoIn.cqe.sR;
    } else if (auxInfoIn.auxInfoInType == AuxInfoInType::AUX_INFO_IN_TYPE_AE) {
        in.ae.eventType = auxInfoIn.ae.eventType;
    }

    HccpAuxInfoOut out;
    auto ret = RaCtxGetAuxInfo(rdmaHandle, &in, &out);
    if (ret != 0) {
        HCCL_ERROR("RaGetAuxInfo failed.");
        return HCCL_E_NETWORK;
    }

    auxInfoOut.auxInfoNum = out.auxInfoNum;
    for (uint32_t i = 0; i < out.auxInfoNum; i++) {
        auxInfoOut.auxInfoTypes[i] = out.auxInfoType[i];
        auxInfoOut.auxInfoValues[i] = out.auxInfoValue[i];
    }
    return HCCL_SUCCESS;
}
HcclResult TaskExceptionHost::PrintUbRegisters(s32 devLogicId, RdmaHandle rdmaHandle)
{
    HCCL_INFO("[PrintUbRegister] start, devLogicId[%d], rdmaHandle[%p]", devLogicId, rdmaHandle);
    AuxInfoIn in;
    in.cqe.status = 0xffffffff; // 0xffffffff代表查询所有寄存器
    in.auxInfoInType = AuxInfoInType::AUX_INFO_IN_TYPE_CQE;
    in.cqe.sR = 0;
    AuxInfoOut auxInfo;
    auto ret = RaGetAuxInfo(rdmaHandle, in, auxInfo);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[PrintUbRegister]GetUbRegisterInfo failed, devLogicId[%d], rdmaHandle[%p]", devLogicId, rdmaHandle);
        return ret;
    }

    uint16_t isAuxInfoExisted{false};
    for (u32 i = 0; i < auxInfo.auxInfoNum; i++) {
        if (auxInfo.auxInfoValues[i]) { // 非零进行打印
            isAuxInfoExisted = true;
            HCCL_ERROR("devLogicId[%d], cqe_aux_info_type[%u], cqe_aux_info_value[0x%x]",
 	  	            devLogicId, auxInfo.auxInfoTypes[i], auxInfo.auxInfoValues[i]);
 	    } else {
 	        HCCL_INFO("devLogicId[%d], cqe_aux_info_type[%u], cqe_aux_info_value[0x%x]",
 	            devLogicId, auxInfo.auxInfoTypes[i], auxInfo.auxInfoValues[i]);
        }
    }
    if (!isAuxInfoExisted) {
        HCCL_ERROR("devLogicId[%d], all aux_info values are zero.", devLogicId);
    }
    return HCCL_SUCCESS;
}

string CcuTaskException::GetCcuLenErrorMsg(const uint64_t len)
{
    if ((0 < len) && (len <= CCU_MSG_256MB_LEN)) {
        return "";
    }
    return StringFormat("ccu transMem Len[%llu]B > 256MB or is zero, not support!", len);
}

string CcuTaskException::GetCcuErrorMsgLoop(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: Loop startInstrId[%u], endInstrId[%u], executorId[%u], "
                        "totalIter[%u], curIter[%u], addressStride[0x%llx]",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.loop.startInstrId, ccuHostParam.ccuErrorInfo.msg.loop.endInstrId,
                        ccuHostParam.ccuErrorInfo.msg.loop.loopEngineId, ccuHostParam.ccuErrorInfo.msg.loop.loopCnt,
                        ccuHostParam.ccuErrorInfo.msg.loop.loopCurrentCnt, ccuHostParam.ccuErrorInfo.msg.loop.addrStride);
}

string CcuTaskException::GetCcuErrorMsgLoopGroup(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: LoopGroup startLoopInsId[%u], loopInsCnt[%u], "
                        "expandOffset[%u], expandCnt[%u]",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.loopGroup.startLoopInsId,
                        ccuHostParam.ccuErrorInfo.msg.loopGroup.loopInsCnt, ccuHostParam.ccuErrorInfo.msg.loopGroup.expandOffset,
                        ccuHostParam.ccuErrorInfo.msg.loopGroup.expandCnt);
}

string CcuTaskException::GetCcuErrorMsgLocPostSem(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: Set sem[%u], semValue[0x%04x], mask[0x%04x]", ccuHostParam.ccuErrorInfo.instrId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask);
}

string CcuTaskException::GetCcuErrorMsgLocWaitSem(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: Wait sem[%u], semValue[0x%04x], mask[0x%04x]", ccuHostParam.ccuErrorInfo.instrId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask);
}

string CcuTaskException::GetCcuErrorMsgRemPostSem(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x], rankId[%d]", ccuHostParam.ccuErrorInfo.instrId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuHostParam));
}

string CcuTaskException::GetCcuErrorMsgRemWaitSem(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: Wait, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankId[%d]",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalValue, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuHostParam));
}

string CcuTaskException::GetCcuErrorMsgRemPostVar(const CcuHostParam &ccuHostParam)
{
    return StringFormat("InstrId[%u]: Post Variable[0x%016llx] To Param[%u], Use sem[%u], mask[0x%04x], rankId[%d]",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.waitSignal.paramValue,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.paramId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuHostParam));
}

string CcuTaskException::GetCcuErrorMsgRemWaitGroup(const CcuHostParam &ccuHostParam)
{
    stringstream ranks;
    for (uint32_t i = 0; i < WAIT_SIGNAL_CHANNEL_SIZE; ++i) {
        const auto channelId = ccuHostParam.ccuErrorInfo.msg.waitSignal.channelId[i];
        if (channelId == UINT16_MAX) {
            break;
        }
        const auto rankId = GetRankIdByChannelId(ccuHostParam);
        if (i != 0) {
            ranks << ", ";
        }
        ranks << to_string(rankId);
    }
    return StringFormat("InstrId[%u]: Wait Group, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankIds[%s]",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalValue, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask,
                        ranks.str().c_str());
}

string CcuTaskException::GetCcuErrorMsgPostSharedSem(const CcuHostParam &ccuHostParam)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x]", ccuHostParam.ccuErrorInfo.instrId,
                        ccuHostParam.ccuErrorInfo.msg.waitSignal.signalId, ccuHostParam.ccuErrorInfo.msg.waitSignal.signalMask);
}

string CcuTaskException::GetCcuErrorMsgRead(const CcuHostParam &ccuHostParam)
{
    auto pair = GetAddrPairByChannelId(ccuHostParam);
    string printMsg = GetCcuLenErrorMsg(ccuHostParam.ccuErrorInfo.msg.transMem.len);
    return StringFormat(
        "InstrId[%u]: Read Memory[0x%016llx] To Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.transMem.rmtAddr, ccuHostParam.ccuErrorInfo.msg.transMem.locAddr,
        ccuHostParam.ccuErrorInfo.msg.transMem.len, ccuHostParam.ccuErrorInfo.msg.transMem.signalId, ccuHostParam.ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuHostParam),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgWrite(const CcuHostParam &ccuHostParam)
{
    auto pair = GetAddrPairByChannelId(ccuHostParam);
    string printMsg = GetCcuLenErrorMsg(ccuHostParam.ccuErrorInfo.msg.transMem.len);
    return StringFormat(
        "InstrId[%u]: Write Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.transMem.locAddr, ccuHostParam.ccuErrorInfo.msg.transMem.rmtAddr,
        ccuHostParam.ccuErrorInfo.msg.transMem.len, ccuHostParam.ccuErrorInfo.msg.transMem.signalId, ccuHostParam.ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuHostParam),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgLocalCpy(const CcuHostParam &ccuHostParam)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuHostParam.ccuErrorInfo.msg.transMem.len);
    return StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x] %s",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.transMem.locAddr, ccuHostParam.ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuHostParam.ccuErrorInfo.msg.transMem.len, ccuHostParam.ccuErrorInfo.msg.transMem.signalId,
                        ccuHostParam.ccuErrorInfo.msg.transMem.signalMask, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgLocalReduce(const CcuHostParam &ccuHostParam)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x], dataType[%u], opType[%u] %s",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.transMem.locAddr, ccuHostParam.ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuHostParam.ccuErrorInfo.msg.transMem.len, ccuHostParam.ccuErrorInfo.msg.transMem.signalId,
                        ccuHostParam..msg.transMem.signalMask, ccuHostParam.ccuErrorInfo.msg.transMem.dataType,
                        ccuHostParam.ccuErrorInfo.msg.transMem.opType, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufRead(const CcuHostParam &ccuHostParam)
{
    auto pair = GetAddrPairByChannelId(ccuHostParam);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat(
        "InstrId[%u]: Read Rmt Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.addr, ccuHostParam.ccuErrorInfo.msg.bufTransMem.bufId,
        ccuHostParam.ccuErrorInfo.msg.bufTransMem.len, ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalMask,
        GetRankIdByChannelId(ccuHostParam),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufWrite(const CcuHostParam &ccuHostParam)
{
    auto pair = GetAddrPairByChannelId(ccuHostParam);
    string printMsg = GetCcuLenErrorMsg(ccuHostParam.ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat(
        "InstrId[%u]: Write CcuBuffer[%u] To Rmt Mem[0x%016llx], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.bufId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.addr,
        ccuHostParam.ccuErrorInfo.msg.bufTransMem.len, ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalMask,
        GetRankIdByChannelId(ccuHostParam),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufLocRead(const CcuHostParam &ccuHostParam)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuHostParam.ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat("InstrId[%u]: Read Loc Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], sem[%u], mask[0x%04x] %s",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.addr, ccuHostParam.ccuErrorInfo.msg.bufTransMem.bufId,
                        ccuHostParam.ccuErrorInfo.msg.bufTransMem.len, ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalMask, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufLocWrite(const CcuHostParam &ccuHostParam)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat("InstrId[%u]: Write CcuBuffer[%u] To Loc Mem[0x%016llx], Len[%llu], sem[%u], mask[0x%04x] %s",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.bufId, ccuHostParam.ccuErrorInfo.msg.bufTransMem.addr,
                        ccuHostParam.ccuErrorInfo.msg.bufTransMem.len, ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuHostParam.ccuErrorInfo.msg.bufTransMem.signalMask, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufReduce(const CcuHostParam &ccuHostParam)
{
    (void)taskInfo;
    stringstream buffIds;
    for (uint32_t i = 0; i < BUF_REDUCE_ID_SIZE; ++i) {
        const auto buffId = ccuHostParam.ccuErrorInfo.msg.bufReduce.bufIds[i];
        if (buffId == UINT16_MAX) {
            break;
        }
        if (i != 0) {
            buffIds << ", ";
        }
        buffIds << to_string(buffId);
    }

    return StringFormat("InstrId[%u]: Buffer Reduce count[%u], dataType[%u], outputDataType[%u], opType[%u], "
                        "sem[%u], mask[0x%04x], CcuBuffers[%s]",
                        ccuHostParam.ccuErrorInfo.instrId, ccuHostParam.ccuErrorInfo.msg.bufReduce.count, ccuHostParam.ccuErrorInfo.msg.bufReduce.dataType,
                        ccuHostParam.ccuErrorInfo.msg.bufReduce.outputDataType, ccuHostParam.ccuErrorInfo.msg.bufReduce.opType,
                        ccuHostParam.ccuErrorInfo.msg.bufReduce.signalId, ccuHostParam.ccuErrorInfo.msg.bufReduce.signalMask,
                        buffIds.str().c_str());
}

string CcuTaskException::GetCcuErrorMsgDefault(const CcuErrorInfo &ccuErrorInfo)
{
    return StringFormat("InstrId[%u]: CcuErrorType[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.type.Describe().c_str());
}

string CcuTaskException::GetCcuErrorMsgMission(const CcuErrorInfo &ccuErrorInfo)
{
    return StringFormat("InstrId[%u]: dieId[%u], missionId[%u], missionError[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.dieId, ccuErrorInfo.missionId,
        ccuErrorInfo.msg.mission.missionError);
}

string CcuTaskException::GetCcuErrorMsgByType(const CcuHostParam &ccuHostParam)
{
    if (ccuHostParam.ccuErrorInfo.type == CcuErrorType::MISSION) {
        return GetCcuErrorMsgMission(ccuHostParam.ccuErrorInfo);
    }

    using GetCcuErrorMsgFunc = string (*)(const CcuHostParam &ccuHostParam);
    static const map<CcuRepType, GetCcuErrorMsgFunc> handlerMap {
        {CcuRepType::LOOP, &CcuTaskException::GetCcuErrorMsgLoop},
        {CcuRepType::LOOPGROUP, &CcuTaskException::GetCcuErrorMsgLoopGroup},
        {CcuRepType::LOC_RECORD_EVENT, &CcuTaskException::GetCcuErrorMsgLocPostSem},
        {CcuRepType::LOC_WAIT_EVENT, &CcuTaskException::GetCcuErrorMsgLocWaitSem},
        {CcuRepType::LOC_WAIT_NOTIFY, &CcuTaskException::GetCcuErrorMsgLocWaitSem},
        {CcuRepType::REM_POST_SEM, &CcuTaskException::GetCcuErrorMsgRemPostSem},
        {CcuRepType::REM_WAIT_SEM, &CcuTaskException::GetCcuErrorMsgRemWaitSem},
        {CcuRepType::REM_POST_VAR, &CcuTaskException::GetCcuErrorMsgRemPostVar},
        {CcuRepType::REM_WAIT_GROUP, &CcuTaskException::GetCcuErrorMsgRemWaitGroup},
        {CcuRepType::RECORD_SHARED_NOTIFY, &CcuTaskException::GetCcuErrorMsgPostSharedSem},
        {CcuRepType::READ, &CcuTaskException::GetCcuErrorMsgRead},
        {CcuRepType::WRITE, &CcuTaskException::GetCcuErrorMsgWrite},
        {CcuRepType::LOCAL_CPY, &CcuTaskException::GetCcuErrorMsgLocalCpy},
        {CcuRepType::LOCAL_REDUCE, &CcuTaskException::GetCcuErrorMsgLocalReduce},
        {CcuRepType::BUF_READ, &CcuTaskException::GetCcuErrorMsgBufRead},
        {CcuRepType::BUF_WRITE, &CcuTaskException::GetCcuErrorMsgBufWrite},
        {CcuRepType::BUF_LOC_READ, &CcuTaskException::GetCcuErrorMsgBufLocRead},
        {CcuRepType::BUF_LOC_WRITE, &CcuTaskException::GetCcuErrorMsgBufLocWrite},
        {CcuRepType::BUF_REDUCE, &CcuTaskException::GetCcuErrorMsgBufReduce}
    };

    const auto funcIt = handlerMap.find(ccuHostParam.ccuErrorInfo.repType);
    if (funcIt == handlerMap.end()) {
        return GetCcuErrorMsgDefault(ccuHostParam.ccuErrorInfo);
    } else {
        return funcIt->second(ccuHostParam.ccuErrorInfo, ccuHostParam.taskInfo);
    }
}

HcclResult CcuTaskException::GetCcuChannelHandleById(u32 deviceId, u16 channelId, const Hccl::TaskInfo &taskInfo,
    u64& channelHandle)
{
    u64 ccuKernelHandle = taskInfo.taskParam_.taskPara.Ccu.ccuKernelHandle;
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(deviceId);
    auto *kernel = kernelMgr.GetKernel(ccuKernelHandle);
    if (kernel == nullptr) {
        HCCL_ERROR("[%s]GetKernel nullptr, deviceId[%u], ccuKernelHandle[0x%llx]", __func__, deviceId, ccuKernelHandle);
        return HCCL_E_PARA;
    }
    
    if (kernel->GetChannelHandleById(channelId, channelHandle) != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]GetChannelHandleById fail, channelId[%u], channelHandle[0x%llx]", __func__, channelId, channelHandle);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

RankId CcuTaskException::GetRankIdByChannelId(uint16_t channelId, const Hccl::TaskInfo &taskInfo)
{
    if (taskInfo.taskParam_.taskType != Hccl::TaskParamType::TASK_CCU) {
        HCCL_ERROR("[%s]taskType[%s] is not CCU.", __func__, taskInfo.taskParam_.taskType.Describe().c_str());
        return INVALID_UINT;
    }
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[%s]dfxOpInfo[%p] or comm is nullptr.", __func__, taskInfo.dfxOpInfo_);
        return INVALID_UINT;
    }

    u32 deviceId = 0; // zjwTODO: 临时打桩
    u64 channelHandle = INVALID_U64;
    if (GetCcuChannelHandleById(deviceId, channelId, taskInfo, channelHandle) != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]GetCcuChannelHandleById fail, deviceId[%u], channelId[%u], channelHandle[0x%llx]",
            __func__, deviceId, channelId, channelHandle);
        return INVALID_UINT;
    }

    hccl::CollComm *collComm = static_cast<hccl::CollComm*>(taskInfo.dfxOpInfo_->comm_);
    u32 remoteRank = INVALID_UINT;
    if (hccl::HcclCommDfx::GetChannelRemoteRankId(collComm->GetCommId(), channelHandle, remoteRank) != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]GetChannelRemoteRankId fail, channelHandle[0x%llx], commId[%s]",
            __func__, channelHandle, collComm->GetCommId().c_str());
        return INVALID_UINT;
    }

    HCCL_INFO("[%s]channelId[%u], deviceId[%u], channelHandle[0x%llx], commId[%s], remoteRank[%u]",
        __func__, channelId, deviceId, channelHandle, collComm->GetCommId().c_str(), remoteRank);
    return remoteRank;
}

std::pair<Hccl::IpAddress, Hccl::IpAddress> CcuTaskException::GetAddrPairByChannelId(uint16_t channelId,
    const Hccl::TaskInfo &taskInfo)
{
    std::pair<Hccl::IpAddress, Hccl::IpAddress> dummy = {Hccl::IpAddress(), Hccl::IpAddress()};
    if (taskInfo.taskParam_.taskType != Hccl::TaskParamType::TASK_CCU) {
        HCCL_ERROR("[TaskException][%s]Get AddrPair failed, task type error[%s]", __func__,
                   taskInfo.taskParam_.Describe().c_str());
        return dummy;
    }
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskException][%s]Get AddrPair failed, communicator is nullptr.", __func__);
        return dummy;
    }

    u32 deviceId = 0; // zjwTODO: 临时打桩
    u64 channelHandle = INVALID_U64;
    if (GetCcuChannelHandleById(deviceId, channelId, taskInfo, channelHandle) != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]GetCcuChannelHandleById fail, deviceId[%u], channelId[%u], channelHandle[0x%llx]",
            __func__, deviceId, channelId, channelHandle);
        return dummy;
    }

    void *channelPtr{nullptr};
    HcclResult ret = HcommChannelGet(channelHandle, &channelPtr);
    if (ret != HCCL_SUCCESS || channelPtr == nullptr) {
        HCCL_ERROR("[%s]HcommChannelGet failed, ret[%d], channelHandle[0x%llx], channelPtr[%p]",
            __func__, ret, channelHandle, channelPtr);
        return dummy;
    }
    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));

    // 获取locAddr
    EndpointHandle locEndPointHandle = channelImpl->GetlocEndPointHandle();
    void *endpoint{nullptr};
    ret = HcommEndpointGet(locEndPointHandle, &endpoint);
    if (ret != HCCL_SUCCESS || endpoint == nullptr) {
        HCCL_ERROR("[%s]HcommEndpointGet failed, ret[%d], locEndPointHandle[%p], endpoint[%p], channelId[%u]",
            __func__, ret, locEndPointHandle, endpoint, channelId);
        return dummy;
    }
    UrmaEndpoint *ccuEndpoint = dynamic_cast<UrmaEndpoint *>(static_cast<Endpoint *>(endpoint));
    const EndpointDesc &locEndpointDesc = ccuEndpoint->GetEndpointDesc();
    HcclResult locRet = CommAddrToIpAddress(locEndpointDesc.commAddr, dummy.first);

    // 获取remoteAddr
    HcommChannelDesc remoteChannelDesc = channelImpl->GetChannelDesc();
    HcclResult remRet = CommAddrToIpAddress(remoteChannelDesc.remoteEndpoint.commAddr, dummy.second);
    HCCL_INFO("[%s]channelId[%u], channelHandle[0x%llx], locRet[%d], locIpAddr[%s], remRet[%d], remIpAddr[%s]",
        __func__, channelId, channelHandle, locRet, dummy.first.Describe().c_str(),
        remRet, dummy.second.Describe().c_str());
    return dummy;
}
HcclResult CcuTaskException::GetCcuErrorMsg(int32_t deviceId, uint16_t missionStatus, const ParaCcu &ccuTaskParam,
    std::vector<CcuErrorInfo> &errorInfo)
{

    HCCL_INFO("[CcuTaskException]%s: deviceId[%d], dieId[%u], missionId[%u], execMissionId[%u], executeId[%llu].",
            __func__, deviceId, static_cast<u32>(ccuTaskParam.dieId), static_cast<u32>(ccuTaskParam.missionId),
            static_cast<u32>(ccuTaskParam.execMissionId), ccuTaskParam.executeId);

        // 入参校验
    CHK_PRT_RET((deviceId < 0 || static_cast<u32>(deviceId) >= MAX_MODULE_DEVICE_NUM),
            HCCL_ERROR("[CcuTaskException][GetCcuErrorMsg]deviceId[%d] error.", deviceId), HcclResult::HCCL_E_PARA);

    // zjwTODO: 在next目录下重新定义missionContext
    // GetCcuMissionContext这个函数里面牵扯的特别多函数都要重写么
    const auto missionContext = GetCcuMissionContext(deviceId, ccuTaskParam.dieId, ccuTaskParam.execMissionId);
    if (missionStatus == 0) {
        HCCL_INFO("[CcuTaskException][%s] no err found, mission status is 0, deviceId[%d], dieId[%u], execMissionId[%u]",
            __func__, deviceId, static_cast<u32>(ccuTaskParam.dieId), static_cast<u32>(ccuTaskParam.execMissionId));
        return;
    }

    // zjwTODO: 用cclkernelHandle
    CcuRepContext *ctx =
        CtxMgrImp::GetInstance(deviceId).GetCtx(ccuTaskParam.executeId, ccuTaskParam.dieId, ccuTaskParam.missionId);
    if (ctx == nullptr) {
        THROW<CcuApiException>("CcuContext not found, deviceId[%d], dieId[%u], missionId[%u], executeId[%llu]",
                               deviceId, static_cast<u32>(ccuTaskParam.dieId), static_cast<u32>(ccuTaskParam.missionId),
                               ccuTaskParam.executeId);
    }

    const uint16_t currIns = missionContext.GetCurrentIns();

    auto rep = ctx->GetRepByInstrId(currIns);
    auto prevRep = ctx->GetRepByInstrId(currIns - 1);
    if (rep == nullptr) {
        HCCL_WARNING("[CcuErrorHandler][%s] cannot find REP from current CcuContext, instrId[%u]", __func__, currIns);
        return;
    }

    // 分类处理Rep, 返回异常信息
    ErrorInfoBase baseInfo{deviceId, ccuTaskParam.dieId, ccuTaskParam.missionId, currIns, missionStatus};
    GenStatusInfo(baseInfo, errorInfo);

    // 处理Rep为FUNC_BLOCK的场景
    while (rep->Type() == CcuRepType::FUNC_BLOCK) {
        auto blockRep = static_pointer_cast<CcuRepBlock>(rep);
        rep           = blockRep->GetRepByInstrId(currIns);
        if (rep == nullptr) {
            THROW<CcuApiException>("Failed to find REP from FuncBlock, instrId[%u], FuncBlock[%s]", currIns,
                                blockRep->GetLabel().c_str());
        }
    }

    if ((prevRep != nullptr && prevRep->Type() == CcuRepType::LOOPGROUP) || (rep->Type() == CcuRepType::LOOPGROUP)) {
        // 处理LoopGroup
        GenErrorInfoLoopGroup(baseInfo, prevRep, *ctx, errorInfo);
    } else if (rep->Type() == CcuRepType::LOC_WAIT_SEM) {
        GenErrorInfoByRepType(baseInfo, rep, errorInfo);
        uint16_t actValue = errorInfo.back().msg.waitSignal.signalValue;
        uint16_t expValue = errorInfo.back().msg.waitSignal.signalMask;
        for (uint16_t i = 0; i < 16; ++i) { // CKE的bit数最多为16
            uint16_t mask = 1 << i; // 创建一个用于检查第 i 位的掩码
            if ((expValue & mask) != 0 && (actValue & mask) == 0) {
                auto depRepVec = std::static_pointer_cast<CcuRepLocWaitSem>(rep)->GetDependencyInfo(mask);
                for (const auto& depRep : depRepVec) {
                    GenErrorInfoByRepType(baseInfo, depRep, errorInfo);
                }
            }
        }
    } else {
        // 处理可直接解析的Rep
        GenErrorInfoByRepType(baseInfo, rep, errorInfo);
    }

    const uint16_t endIns = missionContext.GetEndIns();
    const uint16_t startIns = missionContext.GetStartIns();
    // 获取异常指令对应的Rep
    HCCL_ERROR("[CcuErrorHandler]device %d, execMissionId[%u], startIns[%u], endIns[%u], currIns[%u]",
               deviceId, ccuTaskParam.execMissionId, startIns, endIns, currIns);
    if (endIns == currIns) {
        HCCL_ERROR("[CcuErrorHandler]device %d SQE != CQE, endIns[%u], currIns[%u]", deviceId, endIns, currIns);
    }

    // 安全地获取currIns - 10的值
    uint16_t loopUpInstrNum = 10; // 获取出错指令前10条指令
    uint16_t beginIns = (currIns < loopUpInstrNum) ? startIns : ((currIns - loopUpInstrNum) > startIns ? (currIns - loopUpInstrNum) : startIns); 
    // 打印报错的前10条指令，并且从第一个非空rep开始
    for (uint16_t instrId = currIns - 1; instrId >= beginIns; instrId--) {
        auto rep = ctx->GetRepByInstrId(instrId);
        if (rep == nullptr) {
           beginIns = instrId + 1;
           break;
        }
    }
    for (uint16_t instrId = beginIns; instrId <= currIns; instrId++) {
        auto rep = ctx->GetRepByInstrId(instrId);
        if (rep == nullptr) {
            HCCL_WARNING("[CcuErrorHandler][%s] cannot find REP from current CcuContext, instrId[%u]", __func__, instrId);
            continue;
        }

        GenErrorInfoByRepType(baseInfo, rep, errorInfo);
    }
}


}