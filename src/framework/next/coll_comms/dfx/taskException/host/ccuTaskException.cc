/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ccuTaskException.h"
#include <memory>
#include "log.h"
#include "coll_comm.h"
#include "acl/acl_rt.h"
#include "orion_adapter_hccp.h"
#include <adapter_error_manager_pub.h>
#include "op_type.h"
#include "task_param.h"
#include "ccu_rep_type.h"
#include "ccu_kernel_mgr.h"
#include "hcomm_c_adpt.h"
#include "ccu_urma_channel.h"
#include "orion_adpt_utils.h"
#include "ccuTaskException.h"
#include "hcomm_adapter_hccp.h"
#include "adapter_rts_common.h"
#include "ccu_rep_loopgroup_v1.h"
#include "ccu_rep_type_v1.h"

namespace hcomm {

using namespace std;
constexpr int BYTE = 8;
constexpr uint64_t CCU_MSG_256MB_LEN = 256 * 1024 * 1024; // CCU消息长度不能大于256MB

const map<uint8_t, string> MISSION_STATUS_MAP {
    {0x01, "Unsupported Opcode(0x01)"},      {0x02, "Local Operation Error(0x02)"},
    {0x03, "Remote Operation Error(0x03)"},  {0x04, "Transaction Retry Counter Exceeded(0x04)"},
    {0x05, "Transaction ACK Timeout(0x05)"}, {0x06, "Jetty Work Request Flushed(0x06)"},
    {0x07, "CCUA Alg Task Error(0x07)"},     {0x08, "Memory ECC Error(0x08)"},
    {0x09, "CCUM Execute Error(0x09)"},      {0x0A, "CCUA Execute Error(0x0A)"},
};

const map<uint8_t, map<uint8_t, string>> MISSION_SUB_STATUS_MAP {
    {0x02,
     {{0x01, "Local Length Error(0x01)"},
      {0x02, "Local Access Error(0x02)"},
      {0x03, "Remote Response Length Error(0x03)"},
      {0x04, "Local Data Poison(0x04)"}}},
    {0x03,
     {{0x01, "Remote Unsupported Request(0x01)"},
      {0x02, "Remote Access Abort(0x02)"},
      {0x04, "Remote Data Poison(0x04)"}}},
    {0x09, {{0x01, "SQE instr and key not match(0x01)"}, {0x02, "CCU Mission Task Killed(0x02)"}}},
    {0x0A,
     {{0x01, "EXOKAY(0x01)"},
      {0x11, "EXOKAY(0x11)"},
      {0x02, "SLVERR(0x02)"},
      {0x12, "SLVERR(0x12)"},
      {0x03, "DECERR(0x03)"},
      {0x13, "DECERR(0x13)"},
      {0x04, "Abort(0x04)"},
      {0x14, "Abort(0x14)"},
      {0x05, "Write Permission Err(0x05)"},
      {0x15, "Write Permission Err(0x15)"},
      {0x06, "Read Permission Err(0x06)"},
      {0x16, "Read Permission Err(0x16)"},
      {0x07, "Atomic Permission Err(0x07)"},
      {0x17, "Atomic Permission Err(0x17)"},
      {0x08, "Tokenval Err(0x08)"},
      {0x18, "Tokenval Err(0x18)"},
      {0x09, "Page Fault(0x09)"},
      {0x0a, "Page Fault(0x0A)"},
      {0x0b, "Page Fault(0x0B)"},
      {0x19, "Page Fault(0x19)"},
      {0x1a, "Page Fault(0x1A)"},
      {0x1b, "Page Fault(0x1B)"},
      {0x0c, "Read Local Mem Poison(0x0C)"}}},
};

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

CcuMissionContext CcuTaskException::GetCcuMissionContext(int32_t deviceId, uint32_t dieId, uint32_t missionId)
{
    CcuMissionContext missionCtx{};

    u32 devicePhyId = 0;
    HcclResult ret = hrtGetDevicePhyIdByIndex(deviceId, devicePhyId);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s]hrtGetDevicePhyIdByIndex fail, deviceId[%s]", __func__, deviceId), missionCtx);

    struct CustomChannelInfoIn  inBuff;
    struct CustomChannelInfoOut outBuff;

    inBuff.op                          = CcuOpcodeType::CCU_U_OP_GET_MISSION_CTX;
    inBuff.data.dataInfo.udieIdx       = dieId;
    inBuff.offsetStartIdx              = missionId;
    inBuff.data.dataInfo.dataArraySize = 1; // 读1个MissionContext
    inBuff.data.dataInfo.dataLen       = sizeof(CcuMissionContext) * inBuff.data.dataInfo.dataArraySize;

    RaCustomChannel(HrtNetworkMode::HDC, devicePhyId, &inBuff, &outBuff);
    (void)memcpy_s(&missionCtx, sizeof(missionCtx), outBuff.data.dataInfo.dataArray, inBuff.data.dataInfo.dataLen);
    return missionCtx;
}

static string StatusCode2Str(uint8_t highPart, uint8_t lowPart)
{
    HCCL_INFO("Mission Status Code: highPart[0x%02x], lowPart[0x%02x]", highPart, lowPart);
    const auto status = MISSION_STATUS_MAP.find(highPart);
    if (status == MISSION_STATUS_MAP.end()) {
        return "Unknown Status";
    }
    stringstream result;
    result << status->second;

    const auto lowMap = MISSION_SUB_STATUS_MAP.find(highPart);
    if (lowMap == MISSION_SUB_STATUS_MAP.end()) {
        HCCL_ERROR("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
        return result.str();
    }

    const auto subStatus = lowMap->second.find(lowPart);
    const string subStatusMsg = subStatus == lowMap->second.end() ? "Unknown Status" : subStatus->second;
    result << ", " << subStatusMsg;
    return result.str();
}

void CcuTaskException::GenStatusInfo(const ErrorInfoBase &baseInfo, vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type = CcuErrorType::MISSION;
    errorMsg.SetBaseInfo(CcuRep::CcuRepType::BASE, baseInfo.dieId, baseInfo.missionId, baseInfo.currentInsId);

    const uint8_t highPart  = (baseInfo.status >> 8) & 0xFF; // 高8位
    const uint8_t lowPart   = baseInfo.status & 0xFF;        // 低8位
    const string  statusMsg = StatusCode2Str(highPart, lowPart);
    const auto    sRet
        = strncpy_s(errorMsg.msg.mission.missionError, MISSION_STATUS_MSG_LEN, statusMsg.c_str(), statusMsg.length());
    if (sRet != EOK) {
        HCCL_ERROR("[CcuErrorHandler][%s] strcpy failed, statusMsg: %s.", __func__, statusMsg.c_str());
    }

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoLocPostSem(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                             vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                      = static_pointer_cast<CcuRepLocPostSem>(repBase);
    errorMsg.msg.waitSignal.signalId    = rep->sem.Id();
    errorMsg.msg.waitSignal.signalValue = GetCcuCKEValue(baseInfo.deviceId, baseInfo.dieId, rep->sem.Id());
    errorMsg.msg.waitSignal.signalMask  = rep->mask;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoLocWaitSem(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                             vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                      = static_pointer_cast<CcuRepLocWaitSem>(repBase);
    errorMsg.msg.waitSignal.signalId    = rep->sem.Id();
    errorMsg.msg.waitSignal.signalValue = GetCcuCKEValue(baseInfo.deviceId, baseInfo.dieId, rep->sem.Id());
    errorMsg.msg.waitSignal.signalMask  = rep->mask;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoRemPostSem(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                             vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                     = static_pointer_cast<CcuRepRemPostSem>(repBase);
    errorMsg.msg.waitSignal.signalId   = rep->transport.GetRmtCntCkeByIndex(rep->semIndex);
    errorMsg.msg.waitSignal.signalMask = rep->mask;
    (void)memset_s(errorMsg.msg.waitSignal.channelId, sizeof(errorMsg.msg.waitSignal.channelId), 0xFF,
                   sizeof(errorMsg.msg.waitSignal.channelId));
    errorMsg.msg.waitSignal.channelId[0] = rep->transport.GetChannelId();

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoRemWaitSem(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                             vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                     = static_pointer_cast<CcuRepRemWaitSem>(repBase);
    errorMsg.msg.waitSignal.signalId    = rep->transport.GetLocCntCkeByIndex(rep->semIndex);
    errorMsg.msg.waitSignal.signalValue = GetCcuCKEValue(baseInfo.deviceId, baseInfo.dieId, errorMsg.msg.waitSignal.signalId);
    errorMsg.msg.waitSignal.signalMask  = rep->mask;
    (void)memset_s(errorMsg.msg.waitSignal.channelId, sizeof(errorMsg.msg.waitSignal.channelId), 0xFF,
                   sizeof(errorMsg.msg.waitSignal.channelId));
    errorMsg.msg.waitSignal.channelId[0] = rep->transport.GetChannelId();

    errorInfo.push_back(errorMsg);
}

uint64_t CcuTaskException::GetCcuXnValue(int32_t deviceId, uint32_t dieId, uint32_t xnId)
{
    HRaInfo                      info(HrtNetworkMode::HDC, HrtGetDevicePhyIdByIndex(deviceId));
    struct CustomChannelInfoIn  inBuff;
    struct CustomChannelInfoOut outBuff;

    inBuff.op                          = CcuOpcodeType::CCU_U_OP_GET_XN;
    inBuff.data.dataInfo.udieIdx       = dieId;
    inBuff.offsetStartIdx              = xnId;
    inBuff.data.dataInfo.dataArraySize = 1; // 读1个Xn
    inBuff.data.dataInfo.dataLen       = sizeof(uint64_t) * inBuff.data.dataInfo.dataArraySize;

    HrtRaCustomChannel(info, &inBuff, &outBuff);

    uint64_t xnVal{0};
    (void)memcpy_s(&xnVal, sizeof(xnVal), outBuff.data.dataInfo.dataArray, inBuff.data.dataInfo.dataLen);
    return xnVal;
}


void CcuTaskException::GenErrorInfoRemPostVar(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                             vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                     = static_pointer_cast<CcuRepRemPostVar>(repBase);
    errorMsg.msg.waitSignal.signalId   = rep->transport.GetRmtCntCkeByIndex(rep->semIndex);
    errorMsg.msg.waitSignal.signalMask = rep->mask;
    (void)memset_s(errorMsg.msg.waitSignal.channelId, sizeof(errorMsg.msg.waitSignal.channelId), 0xFF,
                   sizeof(errorMsg.msg.waitSignal.channelId));
    errorMsg.msg.waitSignal.channelId[0] = rep->transport.GetChannelId();
    errorMsg.msg.waitSignal.paramId      = rep->transport.GetRmtXnByIndex(rep->paramIndex);
    errorMsg.msg.waitSignal.paramValue   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->param.Id());

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoRemWaitGroup(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                               vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                           = static_pointer_cast<CcuRepWaitGroup>(repBase);
    errorMsg.msg.waitSignal.signalId         = rep->transportGroup.GetCntCkeId(rep->semIndex);
    errorMsg.msg.waitSignal.signalValue      = GetCcuCKEValue(baseInfo.deviceId, baseInfo.dieId, errorMsg.msg.waitSignal.signalId);
    errorMsg.msg.waitSignal.signalMask       = rep->mask;
    const vector<CcuTransport*> &transports  = rep->transportGroup.GetTransports();
    (void)memset_s(errorMsg.msg.waitSignal.channelId, sizeof(errorMsg.msg.waitSignal.channelId), 0xFF,
                   sizeof(errorMsg.msg.waitSignal.channelId));
    for (uint32_t i = 0; i < transports.size() && i < WAIT_SIGNAL_CHANNEL_SIZE; ++i) {
        errorMsg.msg.waitSignal.channelId[i] = transports[i]->GetChannelId();
    }

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoPostSharedSem(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                                vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::WAIT_SIGNAL;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                      = static_pointer_cast<CcuRepPostSharedSem>(repBase);
    errorMsg.msg.waitSignal.signalId    = rep->sem.Id();
    errorMsg.msg.waitSignal.signalMask  = rep->mask;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoRead(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                       vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                   = static_pointer_cast<CcuRepRead>(repBase);
    errorMsg.msg.transMem.locAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->loc.addr.Id());
    errorMsg.msg.transMem.locToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->loc.token.Id());
    errorMsg.msg.transMem.rmtAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->rem.addr.Id());
    errorMsg.msg.transMem.rmtToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->rem.token.Id());
    errorMsg.msg.transMem.len        = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.transMem.signalId   = rep->sem.Id();
    errorMsg.msg.transMem.signalMask = rep->mask;
    errorMsg.msg.transMem.channelId  = rep->transport.GetChannelId();

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoWrite(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                        vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                   = static_pointer_cast<CcuRepWrite>(repBase);
    errorMsg.msg.transMem.locAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->loc.addr.Id());
    errorMsg.msg.transMem.locToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->loc.token.Id());
    errorMsg.msg.transMem.rmtAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->rem.addr.Id());
    errorMsg.msg.transMem.rmtToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->rem.token.Id());
    errorMsg.msg.transMem.len        = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.transMem.signalId   = rep->sem.Id();
    errorMsg.msg.transMem.signalMask = rep->mask;
    errorMsg.msg.transMem.channelId  = rep->transport.GetChannelId();

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoLocalCpy(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                           vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                   = static_pointer_cast<CcuRepLocCpy>(repBase);
    errorMsg.msg.transMem.locAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->src.addr.Id());
    errorMsg.msg.transMem.locToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->src.token.Id());
    errorMsg.msg.transMem.rmtAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.addr.Id());
    errorMsg.msg.transMem.rmtToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.token.Id());
    errorMsg.msg.transMem.len        = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.transMem.signalId   = rep->sem.Id();
    errorMsg.msg.transMem.signalMask = rep->mask;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoLocalReduce(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                              vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                   = static_pointer_cast<CcuRepLocCpy>(repBase);
    errorMsg.msg.transMem.locAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->src.addr.Id());
    errorMsg.msg.transMem.locToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->src.token.Id());
    errorMsg.msg.transMem.rmtAddr    = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.addr.Id());
    errorMsg.msg.transMem.rmtToken   = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.token.Id());
    errorMsg.msg.transMem.len        = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.transMem.signalId   = rep->sem.Id();
    errorMsg.msg.transMem.signalMask = rep->mask;
    errorMsg.msg.transMem.opType     = rep->opType;
    errorMsg.msg.transMem.dataType   = rep->dataType;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoBufRead(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                          vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::BUF_TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                    = static_pointer_cast<CcuRepBufRead>(repBase);
    errorMsg.msg.bufTransMem.bufId    = GetMSIdPerDie(rep->dst.Id());
    errorMsg.msg.bufTransMem.addr     = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->src.addr.Id());
    errorMsg.msg.bufTransMem.token    = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->src.token.Id());
    errorMsg.msg.bufTransMem.len      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.bufTransMem.signalId = rep->sem.Id();
    errorMsg.msg.bufTransMem.signalMask = rep->mask;
    errorMsg.msg.bufTransMem.channelId  = rep->transport.GetChannelId();

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoBufWrite(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                           vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::BUF_TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                      = static_pointer_cast<CcuRepBufWrite>(repBase);
    errorMsg.msg.bufTransMem.bufId      = GetMSIdPerDie(rep->src.Id());
    errorMsg.msg.bufTransMem.addr       = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.addr.Id());
    errorMsg.msg.bufTransMem.token      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.token.Id());
    errorMsg.msg.bufTransMem.len      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.bufTransMem.signalId   = rep->sem.Id();
    errorMsg.msg.bufTransMem.signalMask = rep->mask;
    errorMsg.msg.bufTransMem.channelId  = rep->transport.GetChannelId();

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoBufLocRead(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                             vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::BUF_TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                      = static_pointer_cast<CcuRepBufLocRead>(repBase);
    errorMsg.msg.bufTransMem.bufId      = GetMSIdPerDie(rep->dst.Id());
    errorMsg.msg.bufTransMem.addr       = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->src.addr.Id());
    errorMsg.msg.bufTransMem.token      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->src.token.Id());
    errorMsg.msg.bufTransMem.len      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.bufTransMem.signalId   = rep->sem.Id();
    errorMsg.msg.bufTransMem.signalMask = rep->mask;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoBufLocWrite(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                              vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::BUF_TRANS_MEM;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                      = static_pointer_cast<CcuRepBufLocWrite>(repBase);
    errorMsg.msg.bufTransMem.bufId      = GetMSIdPerDie(rep->src.Id());
    errorMsg.msg.bufTransMem.addr       = GetCcuGSAValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.addr.Id());
    errorMsg.msg.bufTransMem.token      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->dst.token.Id());
    errorMsg.msg.bufTransMem.len      = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->len.Id());
    errorMsg.msg.bufTransMem.signalId   = rep->sem.Id();
    errorMsg.msg.bufTransMem.signalMask = rep->mask;

    errorInfo.push_back(errorMsg);
}

void CcuTaskException::GenErrorInfoBufReduce(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                            vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::BUF_REDUCE;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto rep                        = static_pointer_cast<CcuRepBufReduce>(repBase);
    errorMsg.msg.bufReduce.count          = rep->count;
    errorMsg.msg.bufReduce.dataType       = rep->dataType;
    errorMsg.msg.bufReduce.outputDataType = rep->outputDataType;
    errorMsg.msg.bufReduce.opType         = rep->opType;
    errorMsg.msg.bufReduce.signalId       = rep->sem.Id();
    errorMsg.msg.bufReduce.signalMask     = rep->mask;
    errorMsg.msg.bufReduce.xnIdLength     = rep->xnIdLength_.Id();
    const auto &buffs                     = rep->mem;
    (void)memset_s(errorMsg.msg.bufReduce.bufIds, sizeof(errorMsg.msg.bufReduce.bufIds), 0xFF,
                   sizeof(errorMsg.msg.bufReduce.bufIds));
    for (uint32_t i = 0; i < buffs.size() && i < BUF_REDUCE_ID_SIZE; ++i) {
        errorMsg.msg.bufReduce.bufIds[i] = GetMSIdPerDie(buffs[i].Id());
    }

    errorInfo.push_back(errorMsg);
}


void CcuTaskException::GenErrorInfoByRepType(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                            vector<CcuErrorInfo> &errorInfo)
{
    using GenErrorInfoFunc = void (*)(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                                       vector<CcuErrorInfo> &errorInfo);
    static const map<CcuRep::CcuRepType, GenErrorInfoFunc> handlerMap {
        // WAIT_SIGNAL
        {CcuRep::CcuRepType::LOC_RECORD_EVENT, &CcuTaskException::GenErrorInfoLocPostSem},
        {CcuRep::CcuRepType::LOC_WAIT_EVENT, &CcuTaskException::GenErrorInfoLocWaitSem},
        {CcuRep::CcuRepType::LOC_WAIT_NOTIFY, &CcuTaskException::GenErrorInfoLocWaitSem},
        {CcuRep::CcuRepType::REM_POST_SEM, &CcuTaskException::GenErrorInfoRemPostSem},
        {CcuRep::CcuRepType::REM_WAIT_SEM, &CcuTaskException::GenErrorInfoRemWaitSem},
        {CcuRep::CcuRepType::REM_POST_VAR, &CcuTaskException::GenErrorInfoRemPostVar},
        {CcuRep::CcuRepType::REM_WAIT_GROUP, &CcuTaskException::GenErrorInfoRemWaitGroup},
        {CcuRep::CcuRepType::RECORD_SHARED_NOTIFY, &CcuTaskException::GenErrorInfoPostSharedSem},
        // TRANS_MEM
        {CcuRep::CcuRepType::READ, &CcuTaskException::GenErrorInfoRead},
        {CcuRep::CcuRepType::WRITE, &CcuTaskException::GenErrorInfoWrite},
        {CcuRep::CcuRepType::LOCAL_CPY, &CcuTaskException::GenErrorInfoLocalCpy},
        {CcuRep::CcuRepType::LOCAL_REDUCE, &CcuTaskException::GenErrorInfoLocalReduce},
        // BUF_TRANS_MEM
        {CcuRep::CcuRepType::BUF_READ, &CcuTaskException::GenErrorInfoBufRead},
        {CcuRep::CcuRepType::BUF_WRITE, &CcuTaskException::GenErrorInfoBufWrite},
        {CcuRep::CcuRepType::BUF_LOC_READ, &CcuTaskException::GenErrorInfoBufLocRead},
        {CcuRep::CcuRepType::BUF_LOC_WRITE, &CcuTaskException::GenErrorInfoBufLocWrite},
        // BUF_REDUCE
        {CcuRep::CcuRepType::BUF_REDUCE, &CcuTaskException::GenErrorInfoBufReduce}
    };
    const auto funcIt = handlerMap.find(repBase->Type());
    if (funcIt == handlerMap.end()) {
        // DEFAULT, chip error
        GenErrorInfoDefault(baseInfo, repBase, errorInfo);
    } else {
        (funcIt->second)(baseInfo, repBase, errorInfo);
    }
}

CcuLoopContext CcuTaskException::GetCcuLoopContext(int32_t deviceId, uint32_t dieId, uint32_t loopCtxId)
{
    HRaInfo                      info(HrtNetworkMode::HDC, HrtGetDevicePhyIdByIndex(deviceId));
    struct CustomChannelInfoIn  inBuff;
    struct CustomChannelInfoOut outBuff;

    inBuff.op                          = CcuOpcodeType::CCU_U_OP_GET_LOOP_CTX;
    inBuff.data.dataInfo.udieIdx       = dieId;
    inBuff.offsetStartIdx              = loopCtxId;
    inBuff.data.dataInfo.dataArraySize = 1; // 读1个LoopContext
    inBuff.data.dataInfo.dataLen       = sizeof(CcuLoopContext) * inBuff.data.dataInfo.dataArraySize;

    HrtRaCustomChannel(info, &inBuff, &outBuff);

    CcuLoopContext loopCtx{};
    (void)memcpy_s(&loopCtx, sizeof(loopCtx), outBuff.data.dataInfo.dataArray, inBuff.data.dataInfo.dataLen);
    return loopCtx;
}

void CcuTaskException::GenErrorInfoLoop(const ErrorInfoBase &baseInfo, CcuRepContext &ctx,
                                       vector<CcuErrorInfo> &errorInfo)
{
    // 找LoopRep
    auto repBase = ctx.GetRepByInstrId(baseInfo.currentInsId);
    if (repBase == nullptr || repBase->Type() != CcuRepType::LOOP) {
        THROW<CcuApiException>("Failed to find Loop REP from CcuContext, instrId[%u]", baseInfo.currentInsId);
    }
    const auto rep = static_pointer_cast<CcuRepLoop>(repBase);

    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::LOOP;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, baseInfo.currentInsId);

    LoopXm loopXm{};
    loopXm.value                     = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->loopParam.Id());
    const auto ccuLoopContext        = GetCcuLoopContext(baseInfo.deviceId, baseInfo.dieId, loopXm.loopCtxId);
    errorMsg.msg.loop.startInstrId   = rep->loopBlock->StartInstrId();
    errorMsg.msg.loop.endInstrId     = rep->loopBlock->StartInstrId() + rep->loopBlock->InstrCount() - 1;
    errorMsg.msg.loop.loopEngineId   = loopXm.loopCtxId;
    errorMsg.msg.loop.loopCnt        = static_cast<uint16_t>(loopXm.loopCnt);
    errorMsg.msg.loop.loopCurrentCnt = ccuLoopContext.GetCurrentCnt();
    errorMsg.msg.loop.addrStride     = ccuLoopContext.GetAddrStride();

    errorInfo.push_back(errorMsg);

    // 解析Loop内的异常Rep
    for (uint16_t loopCurrentIns = errorMsg.msg.loop.startInstrId; loopCurrentIns <= errorMsg.msg.loop.endInstrId;
         loopCurrentIns++) {
        auto inLoopExRep = rep->loopBlock->GetRepByInstrId(loopCurrentIns);
        if (inLoopExRep == nullptr) {
            THROW<CcuApiException>("Failed to find REP from Loop, instrId[%u], Loop[%s]", loopCurrentIns,
                                   rep->GetLabel().c_str());
        }
        ErrorInfoBase loopErrBase{baseInfo.deviceId, baseInfo.dieId, baseInfo.missionId, loopCurrentIns,
                                  baseInfo.status};
        GenErrorInfoByRepType(loopErrBase, inLoopExRep, errorInfo);
    }
}

void CcuTaskException::GenErrorInfoLoopGroup(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                            CcuRepContext &ctx, vector<CcuErrorInfo> &errorInfo)
{
    CcuErrorInfo errorMsg{};
    errorMsg.type    = CcuErrorType::LOOP_GROUP;
    errorMsg.SetBaseInfo(repBase->Type(), baseInfo.dieId, baseInfo.missionId, repBase->StartInstrId());

    const auto  rep              = static_pointer_cast<CcuRep::CcuRepLoopGroup>(repBase);
    const auto  startLoopInstrId = rep->GetStartLoopInstrId();
    LoopGroupXn loopGroupXn{};
    loopGroupXn.value                     = GetCcuXnValue(baseInfo.deviceId, baseInfo.dieId, rep->parallelParam.Id());
    errorMsg.msg.loopGroup.startLoopInsId = startLoopInstrId;
    errorMsg.msg.loopGroup.loopInsCnt     = static_cast<uint16_t>(loopGroupXn.loopInsCnt);
    errorMsg.msg.loopGroup.expandOffset   = static_cast<uint16_t>(loopGroupXn.expandOffset);
    errorMsg.msg.loopGroup.expandCnt      = static_cast<uint16_t>(loopGroupXn.expandCnt);

    errorInfo.push_back(errorMsg);

    // 处理loop
    for (uint16_t i = 0; i < loopGroupXn.loopInsCnt; ++i) {
        uint16_t      loopInsId = startLoopInstrId + i;
        ErrorInfoBase loopErrInfoBase{baseInfo.deviceId, baseInfo.dieId, baseInfo.missionId, loopInsId,
                                      baseInfo.status};
        GenErrorInfoLoop(loopErrInfoBase, ctx, errorInfo);
    }
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

    const auto missionContext = GetCcuMissionContext(deviceId, ccuTaskParam.dieId, ccuTaskParam.execMissionId);
    if (missionStatus == 0) {
        HCCL_INFO("[CcuErrorHandler][%s] no err found, mission status is 0, deviceId[%d], dieId[%u], execMissionId[%u]",
            __func__, deviceId, static_cast<u32>(ccuTaskParam.dieId), static_cast<u32>(ccuTaskParam.execMissionId));
        return HCCL_E_PARA;
    }

    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(deviceId);
    auto *kernel = kernelMgr.GetKernel(ccuTaskParam.ccuKernelHandle);
    CHK_PRT_RET(kernel == nullptr, HCCL_ERROR("[%s]GetKernel nullptr, deviceId[%u], ccuKernelHandle[0x%llx]",
                __func__, deviceId, ccuTaskParam.ccuKernelHandle), HCCL_E_PARA);

    CcuRepContext *ctx = reinterpret_cast<CcuRepContext *>(kernel);
    CHK_PRT_RET(ctx == nullptr, HCCL_ERROR("CcuContext not found, deviceId[%d], dieId[%u], missionId[%u], executeId[%llu]",
                               deviceId, static_cast<u32>(ccuTaskParam.dieId), static_cast<u32>(ccuTaskParam.missionId),
                               ccuTaskParam.executeId), HCCL_E_PARA);
    const uint16_t currIns = missionContext.GetCurrentIns();

    auto rep = ctx->GetRepByInstrId(currIns);
    CHK_PRT_RET(rep == nullptr, HCCL_ERROR("[CcuErrorHandler][%s] cannot find REP from current CcuContext, instrId[%u]",
                                            __func__, currIns), HCCL_E_PARA);
    auto prevRep = ctx->GetRepByInstrId(currIns - 1);

    // 分类处理Rep, 返回异常信息
    ErrorInfoBase baseInfo{deviceId, ccuTaskParam.dieId, ccuTaskParam.missionId, currIns, missionStatus};
    GenStatusInfo(baseInfo, errorInfo);

    // 处理Rep为FUNC_BLOCK的场景
    while (rep->Type() == CcuRep::CcuRepType::FUNC_BLOCK) {
        auto blockRep = static_pointer_cast<CcuRepBlock>(rep);
        rep           = blockRep->GetRepByInstrId(currIns);
        CHK_PRT_RET(rep == nullptr, HCCL_ERROR("Failed to find REP from FuncBlock, instrId[%u], FuncBlock[%s]", currIns,
                                blockRep->GetLabel().c_str()), HcclResult::HCCL_E_PARA);
    }

    if ((prevRep != nullptr && prevRep->Type() == CcuRep::CcuRepType::LOOPGROUP) || (rep->Type() == CcuRep::CcuRepType::LOOPGROUP)) {
        // 处理LoopGroup
        GenErrorInfoLoopGroup(baseInfo, prevRep, *ctx, errorInfo);
    } else if (rep->Type() == CcuRep::CcuRepType::LOC_WAIT_EVENT || rep->Type() == CcuRep::CcuRepType::LOC_WAIT_NOTIFY) {
        GenErrorInfoByRepType(baseInfo, rep, errorInfo);
        uint16_t actValue = errorInfo.back().msg.waitSignal.signalValue;
        uint16_t expValue = errorInfo.back().msg.waitSignal.signalMask;
        for (uint16_t i = 0; i < 16; ++i) { // CKE的bit数最多为16
            uint16_t mask = 1 << i; // 创建一个用于检查第 i 位的掩码
            if ((expValue & mask) != 0 && (actValue & mask) == 0) {
                // auto depRepVec = std::static_pointer_cast<CcuRepLocWaitSem>(rep)->GetDependencyInfo(mask); // zjwTodo: 待和哲芃确认
                // for (const auto& depRep : depRepVec) {
                //     GenErrorInfoByRepType(baseInfo, depRep, errorInfo);
                // }
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
        return HCCL_E_PARA;
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
    return HCCL_SUCCESS;
}

void CcuTaskException::PrintCcuErrorInfo(uint32_t deviceId, uint16_t status, const Hccl::TaskInfo& taskInfo)
{
    const Hccl::ParaCcu& ccuTaskParam = taskInfo.taskParam_.taskPara.Ccu;

    // auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(deviceId);
    // auto *kernel = kernelMgr.GetKernel(ccuTaskParam.ccuKernelHandle);
    // CHK_PRT_RET(kernel == nullptr, HCCL_ERROR("[%s]GetKernel nullptr, deviceId[%u], ccuKernelHandle[0x%llx]",
    //     __func__, deviceId, ccuTaskParam.ccuKernelHandle),);

    vector<CcuErrorInfo> errorInfos {};
    HcclResult ret = GetCcuErrorMsg(deviceId, status, ccuTaskParam, errorInfos);
    const uint8_t missionStatus = (status >> 8) & 0xFF;
    if (ret != HcclResult::HCCL_SUCCESS || errorInfos.empty()) {
        HCCL_ERROR("Get CCU error info failed. deviceId[%u], dieId[%u], missionId[%u], executeId[%llu].",
            deviceId, ccuTaskParam.dieId, ccuTaskParam.missionId, ccuTaskParam.executeId);
        return;
    }
    PrintCcuErrorLog(errorInfos, taskInfo, deviceId);

    if (missionStatus >= 0x01 && missionStatus <= 0x05) { // 如果是UB错误(missionStatus为[0x01, 0x05])，打印Ub Dfx寄存器信息
        PrintCcuUbRegisters(errorInfos, static_cast<s32>(deviceId), taskInfo);
    }
}

void CcuTaskException::PrintCcuErrorLog(const std::vector<CcuErrorInfo>& errorInfos, const Hccl::TaskInfo& taskInfo,
    u32 deviceId)
{
    if (errorInfos.empty()) {
        return;
    }
    HCCL_ERROR("[CcuTaskException]Task run failed, ccu runtime information is: %s", __func__);
    for (const auto& errorInfo : errorInfos) {
        HCCL_ERROR("[CcuTaskException][%s]", GetCcuErrorMsgByType(errorInfo, taskInfo, deviceId).c_str());
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

string CcuTaskException::GetCcuErrorMsgLoop(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    return StringFormat("InstrId[%u]: Loop startInstrId[%u], endInstrId[%u], executorId[%u], "
                        "totalIter[%u], curIter[%u], addressStride[0x%llx]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.loop.startInstrId, ccuErrorInfo.msg.loop.endInstrId,
                        ccuErrorInfo.msg.loop.loopEngineId, ccuErrorInfo.msg.loop.loopCnt,
                        ccuErrorInfo.msg.loop.loopCurrentCnt, ccuErrorInfo.msg.loop.addrStride);
}

string CcuTaskException::GetCcuErrorMsgLoopGroup(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    return StringFormat("InstrId[%u]: LoopGroup startLoopInsId[%u], loopInsCnt[%u], "
                        "expandOffset[%u], expandCnt[%u]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.loopGroup.startLoopInsId,
                        ccuErrorInfo.msg.loopGroup.loopInsCnt, ccuErrorInfo.msg.loopGroup.expandOffset,
                        ccuErrorInfo.msg.loopGroup.expandCnt);
}

string CcuTaskException::GetCcuErrorMsgLocPostSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    return StringFormat("InstrId[%u]: Set sem[%u], semValue[0x%04x], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string CcuTaskException::GetCcuErrorMsgLocWaitSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    return StringFormat("InstrId[%u]: Wait sem[%u], semValue[0x%04x], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string CcuTaskException::GetCcuErrorMsgRemPostSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    return StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x], rankId[%d]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId));
}

string CcuTaskException::GetCcuErrorMsgRemWaitSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    return StringFormat("InstrId[%u]: Wait, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalValue, ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId));
}

string CcuTaskException::GetCcuErrorMsgRemPostVar(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    return StringFormat("InstrId[%u]: Post Variable[0x%016llx] To Param[%u], Use sem[%u], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.paramValue,
                        ccuErrorInfo.msg.waitSignal.paramId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId));
}

string CcuTaskException::GetCcuErrorMsgRemWaitGroup(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    stringstream ranks;
    for (uint32_t i = 0; i < WAIT_SIGNAL_CHANNEL_SIZE; ++i) {
        const auto channelId = ccuErrorInfo.msg.waitSignal.channelId[i];
        if (channelId == UINT16_MAX) {
            break;
        }
        const auto rankId = GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId);
        if (i != 0) {
            ranks << ", ";
        }
        ranks << to_string(rankId);
    }
    return StringFormat("InstrId[%u]: Wait Group, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankIds[%s]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalValue, ccuErrorInfo.msg.waitSignal.signalMask,
                        ranks.str().c_str());
}

string CcuTaskException::GetCcuErrorMsgPostSharedSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    return StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalMask);
}

string CcuTaskException::GetCcuErrorMsgRead(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return StringFormat(
        "InstrId[%u]: Read Memory[0x%016llx] To Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.rmtAddr, ccuErrorInfo.msg.transMem.locAddr,
        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId, ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgWrite(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return StringFormat(
        "InstrId[%u]: Write Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId, ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgLocalCpy(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgLocalReduce(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x], dataType[%u], opType[%u] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask, ccuErrorInfo.msg.transMem.dataType,
                        ccuErrorInfo.msg.transMem.opType, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufRead(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat(
        "InstrId[%u]: Read Rmt Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.addr, ccuErrorInfo.msg.bufTransMem.bufId,
        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId, ccuErrorInfo.msg.bufTransMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufWrite(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat(
        "InstrId[%u]: Write CcuBuffer[%u] To Rmt Mem[0x%016llx], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.bufId, ccuErrorInfo.msg.bufTransMem.addr,
        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId, ccuErrorInfo.msg.bufTransMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo, deviceId),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufLocRead(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat("InstrId[%u]: Read Loc Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], sem[%u], mask[0x%04x] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.addr, ccuErrorInfo.msg.bufTransMem.bufId,
                        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufLocWrite(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return StringFormat("InstrId[%u]: Write CcuBuffer[%u] To Loc Mem[0x%016llx], Len[%llu], sem[%u], mask[0x%04x] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.bufId, ccuErrorInfo.msg.bufTransMem.addr,
                        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask, printMsg.c_str());
}

string CcuTaskException::GetCcuErrorMsgBufReduce(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    (void)taskInfo;
    (void)deviceId;
    stringstream buffIds;
    for (uint32_t i = 0; i < BUF_REDUCE_ID_SIZE; ++i) {
        const auto buffId = ccuErrorInfo.msg.bufReduce.bufIds[i];
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
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufReduce.count, ccuErrorInfo.msg.bufReduce.dataType,
                        ccuErrorInfo.msg.bufReduce.outputDataType, ccuErrorInfo.msg.bufReduce.opType,
                        ccuErrorInfo.msg.bufReduce.signalId, ccuErrorInfo.msg.bufReduce.signalMask,
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

string CcuTaskException::GetCcuErrorMsgByType(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId)
{
    if (ccuErrorInfo.type == CcuErrorType::MISSION) {
        return GetCcuErrorMsgMission(ccuErrorInfo);
    }

    using GetCcuErrorMsgFunc = string (*)(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static const map<CcuRep::CcuRepType, GetCcuErrorMsgFunc> handlerMap {
        {CcuRep::CcuRepType::LOOP, &CcuTaskException::GetCcuErrorMsgLoop},
        {CcuRep::CcuRepType::LOOPGROUP, &CcuTaskException::GetCcuErrorMsgLoopGroup},
        {CcuRep::CcuRepType::LOC_RECORD_EVENT, &CcuTaskException::GetCcuErrorMsgLocPostSem},
        {CcuRep::CcuRepType::LOC_WAIT_EVENT, &CcuTaskException::GetCcuErrorMsgLocWaitSem},
        {CcuRep::CcuRepType::LOC_WAIT_NOTIFY, &CcuTaskException::GetCcuErrorMsgLocWaitSem},
        {CcuRep::CcuRepType::REM_POST_SEM, &CcuTaskException::GetCcuErrorMsgRemPostSem},
        {CcuRep::CcuRepType::REM_WAIT_SEM, &CcuTaskException::GetCcuErrorMsgRemWaitSem},
        {CcuRep::CcuRepType::REM_POST_VAR, &CcuTaskException::GetCcuErrorMsgRemPostVar},
        {CcuRep::CcuRepType::REM_WAIT_GROUP, &CcuTaskException::GetCcuErrorMsgRemWaitGroup},
        {CcuRep::CcuRepType::RECORD_SHARED_NOTIFY, &CcuTaskException::GetCcuErrorMsgPostSharedSem},
        {CcuRep::CcuRepType::READ, &CcuTaskException::GetCcuErrorMsgRead},
        {CcuRep::CcuRepType::WRITE, &CcuTaskException::GetCcuErrorMsgWrite},
        {CcuRep::CcuRepType::LOCAL_CPY, &CcuTaskException::GetCcuErrorMsgLocalCpy},
        {CcuRep::CcuRepType::LOCAL_REDUCE, &CcuTaskException::GetCcuErrorMsgLocalReduce},
        {CcuRep::CcuRepType::BUF_READ, &CcuTaskException::GetCcuErrorMsgBufRead},
        {CcuRep::CcuRepType::BUF_WRITE, &CcuTaskException::GetCcuErrorMsgBufWrite},
        {CcuRep::CcuRepType::BUF_LOC_READ, &CcuTaskException::GetCcuErrorMsgBufLocRead},
        {CcuRep::CcuRepType::BUF_LOC_WRITE, &CcuTaskException::GetCcuErrorMsgBufLocWrite},
        {CcuRep::CcuRepType::BUF_REDUCE, &CcuTaskException::GetCcuErrorMsgBufReduce}
    };

    const auto funcIt = handlerMap.find(ccuErrorInfo.repType);
    if (funcIt == handlerMap.end()) {
        return GetCcuErrorMsgDefault(ccuErrorInfo);
    } else {
        return funcIt->second(ccuErrorInfo, taskInfo, deviceId);
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

    uint64_t handle = 0;
    if (kernel->GetChannelHandleById(channelId, handle) != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]GetChannelHandleById fail, channelId[%u], handle[0x%llx]", __func__, channelId, handle);
        return HCCL_E_PARA;
    }
    channelHandle = handle;
    return HCCL_SUCCESS;
}

RankId CcuTaskException::GetRankIdByChannelId(uint16_t channelId, const Hccl::TaskInfo &taskInfo, u32 deviceId)
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
    const Hccl::TaskInfo &taskInfo, u32 deviceId)
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

}