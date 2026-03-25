/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_connection.h"

#include <cstdlib>
#include "hccp_ctx.h"
#include "buffer.h"
#include "exception_util.h"
#include "orion_adapter_rts.h"
#include "internal_exception.h"
#include "local_ub_rma_buffer.h"
#include "rdma_handle_manager.h"
#include "local_ub_rma_buffer.h"

namespace Hccl {
CcuConnection::CcuConnection(const IpAddress &locAddr, const IpAddress &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys,
    const IpAddress &locIpv4Addr, const IpAddress &rmtIpv4Addr)
    : locAddr_(locAddr), rmtAddr_(rmtAddr), channelInfo_(channelInfo), ccuJettys_(ccuJettys),
    locIpv4Addr(locIpv4Addr), rmtIpv4Addr(rmtIpv4Addr)
{
}

CcuTpConnection::CcuTpConnection(const IpAddress &locAddr, const IpAddress &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys,
    const IpAddress &locIpv4Addr, const IpAddress &rmtIpv4Addr)
    : CcuConnection(locAddr, rmtAddr, channelInfo, ccuJettys, locIpv4Addr, rmtIpv4Addr)
{
    tpProtocol = TpProtocol::TP;
}

CcuCtpConnection::CcuCtpConnection(const IpAddress &locAddr, const IpAddress &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys,
    const IpAddress &locIpv4Addr, const IpAddress &rmtIpv4Addr)
    : CcuConnection(locAddr, rmtAddr, channelInfo, ccuJettys, locIpv4Addr, rmtIpv4Addr)
{
    tpProtocol = TpProtocol::CTP;
}

CcuUboeConnection::CcuUboeConnection(const IpAddress &locAddr, const IpAddress &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys,
    const IpAddress &locIpv4Addr, const IpAddress &rmtIpv4Addr)
    : CcuConnection(locAddr, rmtAddr, channelInfo, ccuJettys, locIpv4Addr, rmtIpv4Addr)
{
    tpProtocol = TpProtocol::UBOE;
}

HcclResult CcuConnection::Init()
{
    TRY_CATCH_RETURN(
        devLogicId = HrtGetDevice();
        uint32_t devPhyId = HrtGetDevicePhyIdByIndex(devLogicId);

        auto &rdmaHandleMgr = RdmaHandleManager::GetInstance();
        rdmaHandle = rdmaHandleMgr.GetByIp(devPhyId, locAddr_);
        dieId = rdmaHandleMgr.GetDieAndFuncId(rdmaHandle).first;
    );

    CHK_RET(GetLocalCcuRmaBufferInfo());

    jettyNum = channelInfo_.jettyInfos.size();
    CHK_PRT_RET(jettyNum == 0,
        HCCL_ERROR("[CcuConnection][%s] failed, jetty num[0] is unexpected.", __func__),
        HcclResult::HCCL_E_PARA);

    GenerateLocalPsn();
    status = CcuConnStatus::INIT;
    innerStatus = InnerStatus::INIT;
    return HcclResult::HCCL_SUCCESS;
}

CcuConnStatus CcuConnection::GetStatus()
{
    if (status == CcuConnStatus::CONNECTED
        || status == CcuConnStatus::CONN_INVALID) {
        return status;
    }

    if (StatusMachine() != HcclResult::HCCL_SUCCESS) {
        status = CcuConnStatus::CONN_INVALID;
        innerStatus = InnerStatus::CONN_INVALID;
    }

    return status;
}
// 获取本端内存，为部分远端不可写的tokenId
HcclResult CcuConnection::GetLocalCcuRmaBufferInfo()
{
    uint64_t ccuBufSize = 0; // 暂未使用
    CHK_RET(CcuDeviceManager::GetCcuResourceSpaceBufInfo(
        devLogicId, dieId, ccuBufAddr, ccuBufSize));

    uint64_t tokenId = 0;
    uint64_t tokenValue = 0;
    CHK_RET(CcuDeviceManager::GetCcuResourceSpaceTokenInfo(
        devLogicId, dieId, tokenId, tokenValue));
    ccuBufTokenId = static_cast<uint32_t>(tokenId);
    ccuBufTokenValue = static_cast<uint32_t>(tokenValue);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::StatusMachine()
{
    TRY_CATCH_RETURN(
        if (status == CcuConnStatus::INIT) {
            UpdateInitStatus();
            return HcclResult::HCCL_SUCCESS;
        }

        if (innerStatus == InnerStatus::JETTY_IMPORTING) {
            UpdateExchangeStatus();
            return HcclResult::HCCL_SUCCESS;
        }
    );

    return HcclResult::HCCL_SUCCESS;
}

void CcuConnection::UpdateInitStatus()
{
    switch (innerStatus) {
        case InnerStatus::INIT:
        case InnerStatus::JETTY_CREATING: {
            if (!CreateJetty()) {
                innerStatus = InnerStatus::JETTY_CREATING;
                break; // 状态不改变退出，下轮状态机进入继续执行
            }

            if (GetTpInfo()) { // 如果有缓存的tp信息，可以直接完成
                innerStatus = InnerStatus::EXCHANGEABLE;
                status      = CcuConnStatus::EXCHANGEABLE;
                break;
            }

            innerStatus = InnerStatus::TP_INFO_GETTING; // 不退出继续调用下个异步接口
            break;
        }
        case InnerStatus::TP_INFO_GETTING: {
            if (!GetTpInfo()) {
                break; // 状态不改变退出，下轮状态机进入继续执行
            }
            innerStatus = InnerStatus::EXCHANGEABLE;
            status      = CcuConnStatus::EXCHANGEABLE;
            break;
        }
        default:
            ThrowAbnormalStatus(std::string(__func__));
    }
}

bool CcuConnection::CreateJetty()
{
    if (isJettyCreated) {
        return true;
    }

    isJettyCreated = true;
    for (size_t i = 0; i < jettyNum; i++) {
        auto ret = ccuJettys_[i]->CreateJetty();
        if (ret == HcclResult::HCCL_E_AGAIN) {
            // 不提供日志避免刷屏
            isJettyCreated = isJettyCreated && false;
            continue;
        }

        if (ret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[CcuConnection][%s] failed, hccl result[%d]", __func__, ret);
            ThrowAbnormalStatus(std::string(__func__));
        }
    }

    return isJettyCreated;
}

inline uint32_t GetRandomNum()
{
    uint32_t randNum = std::rand();
    return randNum;
}

void CcuConnection::GenerateLocalPsn()
{
    jettyImportCfg.localPsn = GetRandomNum();
}

bool CcuConnection::GetTpInfo()
{
    if (tpProtocol == TpProtocol::INVALID) { // 不感知tp建链，当前默认不支持
        HCCL_ERROR("[CcuConnection][%s] failed, tpProtocol[%s] is not expected.",
            __func__, tpProtocol.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    HcclResult ret = TpManager::GetInstance(devLogicId)
        .GetTpInfo({locAddr_, rmtAddr_, tpProtocol}, tpInfo);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        // 此处可能刷屏，非必要勿加日志
        return false;
    }

    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuConnection][%s] failed, hccl result[%d]", __func__, ret);
        ThrowAbnormalStatus(std::string(__func__));
    }

    jettyImportCfg.localTpHandle = tpInfo.tpHandle;
    return true;
}

void CcuConnection::Serialize(std::vector<char> &dtoData)
{
    if (status != CcuConnStatus::EXCHANGEABLE) {
        HCCL_ERROR("[CcuConnection][%s] failed, not init completed yet, "
            "status[%s].", __func__, status.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    BinaryStream dtoStream;
    dtoStream << ccuBufAddr;
    dtoStream << ccuBufTokenId;
    dtoStream << ccuBufTokenValue;
    HCCL_INFO("[CcuConnection][%s], ccuBufAddr[%llx]", __func__, ccuBufAddr);

    dtoStream << jettyNum;
    HCCL_INFO("[CcuConnection][%s], jettyNum[%u]", __func__, jettyNum);
    for (const auto &ccuJetty : ccuJettys_) {
        dtoStream << ccuJetty->GetCreateJettyParam().tokenValue; 
        const auto &outParam = ccuJetty->GetJettyedOutParam();
        dtoStream << outParam.key; // 此处的qpKey是数组
        dtoStream << outParam.keySize;
    }

    if (tpProtocol != TpProtocol::INVALID) {
        dtoStream << jettyImportCfg.localTpHandle;
        dtoStream << jettyImportCfg.localPsn;
        HCCL_INFO("[CcuConnection][%s] tpProtocol[%s], localTpHandle[0x%llx], localPsn[%u].",
            __func__, tpProtocol.Describe().c_str(), jettyImportCfg.localTpHandle,
            jettyImportCfg.localPsn);
    }

    dtoData.clear();
    dtoStream.Dump(dtoData);
}

void CcuConnection::Deserialize(const std::vector<char> &dtoData)
{
    if (status != CcuConnStatus::EXCHANGEABLE) {
        HCCL_ERROR("[CcuConnection][%s] failed, not init completed yet, "
            "status[%s].", __func__, status.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    vector<char> rmtDtoData = dtoData;
    BinaryStream dtoStream(rmtDtoData);
    dtoStream >> rmtCcuBufAddr;
    dtoStream >> rmtCcuBufTokenId;
    dtoStream >> rmtCcuBufTokenValue;
    HCCL_INFO("[CcuConnection][%s], rmtCcuBufAddr[%llx].", __func__, rmtCcuBufAddr);

    uint32_t remoteJettySize{0};
    dtoStream >> remoteJettySize;

    importJettyCtxs.clear();
    importJettyCtxs.resize(remoteJettySize);
    HCCL_INFO("[CcuConnection][%s], remoteJettySize[%u].", __func__, remoteJettySize);

    for (auto &importCtx : importJettyCtxs) {
        dtoStream >> importCtx.inParam.tokenValue;
        dtoStream >> importCtx.remoteQpKey; // 保存key数组
        importCtx.inParam.key = importCtx.remoteQpKey; // 保存指针用于接口调用
        dtoStream >> importCtx.inParam.keyLen;
    }

    if (tpProtocol != TpProtocol::INVALID) {
        dtoStream >> jettyImportCfg.remoteTpHandle;
        dtoStream >> jettyImportCfg.remotePsn;

        HCCL_INFO("[CcuConnection][%s] tpEnable, remoteTpHandle[0x%llx], remotePsn[%u].",
            __func__, jettyImportCfg.remoteTpHandle, jettyImportCfg.remotePsn);
    }
}

void CcuConnection::ImportJetty()
{
    if (isJettyImported) {
        HCCL_INFO("[CcuConnection][%s] taJettys has been imported already.", __func__);
        return;
    }

    if (innerStatus != InnerStatus::EXCHANGEABLE) {
        ThrowAbnormalStatus(std::string(__func__));
    }

    if (jettyNum != importJettyCtxs.size()) {
        HCCL_ERROR("[CcuConnection][%s] failed to ImportJetty, "
            "jettyNum[%u] is not equal to importJettyCtxs.size[%u].",
            __func__, jettyNum, importJettyCtxs.size());
        ThrowAbnormalStatus(std::string(__func__));
    }

    ResetRequestCtxs();
    for (size_t i = 0; i < jettyNum; i++) {
        if (StartImportJettyRequest(i, reqHandles[i]) != HcclResult::HCCL_SUCCESS) {
            ThrowAbnormalStatus(std::string(__func__));
        }
    }

    innerStatus = InnerStatus::JETTY_IMPORTING;
}

void CcuConnection::ResetRequestCtxs()
{
    reqHandles.clear();
    reqHandles.resize(jettyNum);

    reqDataBuffers.clear();
    reqDataBuffers.resize(jettyNum);

    remoteJettyHandlePtrs.clear();
    remoteJettyHandlePtrs.resize(jettyNum);
}

HcclResult CcuConnection::StartImportJettyRequest(uint32_t jettyIndex, RequestHandle &reqHandle)
{
    if (tpProtocol == TpProtocol::INVALID) {
        ThrowAbnormalStatus(std::string(__func__));
    }

    auto &importCtxInParam = importJettyCtxs[jettyIndex].inParam;
    importCtxInParam.jettyImportCfg = jettyImportCfg;
    importCtxInParam.jettyImportCfg.protocol = tpProtocol;
    TRY_CATCH_RETURN(
        reqHandle = RaUbTpImportJettyAsync(rdmaHandle, importCtxInParam, reqDataBuffers[jettyIndex],
            remoteJettyHandlePtrs[jettyIndex]);
    );
    
    return HcclResult::HCCL_SUCCESS;
}

bool CcuConnection::CheckRequestResults()
{
    if (reqHandles.size() == 0) {
        return true;
    }

    // 检查所有下发异步请求是否完成
    vector<size_t> completedReqs;
    for (size_t i = 0; i < reqHandles.size(); i++) {
        ReqHandleResult result = HrtRaGetAsyncReqResult(reqHandles[i]);
        if (result == ReqHandleResult::NOT_COMPLETED) {
            continue;
        }

        if (result != ReqHandleResult::COMPLETED) {
            THROW<InternalException>(
                StringFormat("[CcuConnection][%s] failed, result[%s] is unexpected.",
                __func__, result.Describe().c_str()));
        }

        // 记录已完成的reqHandles
        completedReqs.push_back(i);
    }

    // 删除已完成的reqHandles，避免重复查询
    for (int i = completedReqs.size() - 1; i >= 0; --i) {
        reqHandles.erase(reqHandles.begin() + completedReqs[i]);
    }

    // 检查是否有剩余reqHandles
    return reqHandles.size() == 0;
}

void CcuConnection::UpdateExchangeStatus()
{
    // 状态机保证为 InnerStatus::JETTY_IMPORTING
    if (!CheckRequestResults()) {
        return;
    }

    for (size_t i = 0; i < jettyNum; i++) {
        auto &outParam = importJettyCtxs[i].outParam;
        struct QpImportInfoT *infoPtr = reinterpret_cast<QpImportInfoT *>(reqDataBuffers[i].data());
        outParam.handle        = reinterpret_cast<TargetJettyHandle>(remoteJettyHandlePtrs[i]);
        outParam.targetJettyVa = infoPtr->out.ub.tjettyHandle; // 该信息当前未使用
        outParam.tpn           = infoPtr->out.ub.tpn;
    }
    isJettyImported = true;

    ConfigChannel();
    status = CcuConnStatus::CONNECTED;
    innerStatus = InnerStatus::CONNECTED;
}

void CcuConnection::ConfigChannel()
{
    if (jettyNum != importJettyCtxs.size()) {
        HCCL_ERROR("[CcuConnection][%s] failed, jettyNum[%u] is not equal to "
            "importJettyCtxs.size[%u].", __func__, jettyNum, importJettyCtxs.size());
        ThrowAbnormalStatus(std::string(__func__));
    }

    ChannelCfg cfg{};
    cfg.channelId = channelInfo_.channelId;
    cfg.remoteEid = rmtAddr_.GetReverseEid();
    HCCL_INFO("[CcuConnection::ConfigChannel] cfg.remoteEid=%s", cfg.remoteEid.Describe().c_str());
    cfg.tpn       = importJettyCtxs[0].outParam.tpn; // tp handle复用所以tpn一致
    cfg.remoteCcuVa   = rmtCcuBufAddr;
    cfg.memTokenId    = rmtCcuBufTokenId;
    cfg.memTokenValue = rmtCcuBufTokenValue;

    for (size_t i = 0; i < jettyNum; i++) {
        const auto &ccuJetty = ccuJettys_[i];
        const auto &inParam = ccuJetty->GetCreateJettyParam();
        const auto &outParam = ccuJetty->GetJettyedOutParam();
        const auto &jettyInfo = channelInfo_.jettyInfos[i];
        cfg.jettyCfgs.emplace_back(JettyCfg{
            jettyInfo.jettyCtxId,
            outParam.dbVa,
            outParam.dbTokenId,
            inParam.tokenValue}); // 安全问题，禁止打印token相关信息
    }

    if (CcuDeviceManager::ConfigChannel(devLogicId, dieId, cfg) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuConnection][%s] failed, devLogicId[%d], dieId[%u] channelId[%u].",
            __func__, devLogicId, dieId, cfg.channelId);
        ThrowAbnormalStatus(std::string(__func__));
    }
}

CcuConnection::~CcuConnection()
{
    DECTOR_TRY_CATCH("CcuConnection", ReleaseConnRes());
}

HcclResult CcuConnection::ReleaseConnRes()
{
    for (auto &item : importJettyCtxs) {
        if (item.outParam.handle != 0) {
            HrtRaUbUnimportJetty(rdmaHandle, item.outParam.handle);
            item.outParam.handle = 0;
        }
    }

    if (tpInfo.tpHandle != 0) { // tp handle 复用，只释放一次
        (void)TpManager::GetInstance(devLogicId)
            .ReleaseTpInfo({locAddr_, rmtAddr_, tpProtocol}, tpInfo);
        tpInfo.tpHandle = 0;
    }

    // CcuJetty 生命周期跟随通信域CcuJettyMgr
    // 不需要connection主动销毁
    return HcclResult::HCCL_SUCCESS;
}

void CcuConnection::ThrowAbnormalStatus(const string &funcName)
{
    auto errMsg = StringFormat("[CcuConnection][%s] failed, [%s].",
        funcName.c_str(), Describe().c_str());
    status = CcuConnStatus::CONN_INVALID;
    innerStatus = InnerStatus::CONN_INVALID;
    THROW<InternalException>(errMsg);
}

std::string CcuConnection::Describe()
{
    return StringFormat("[CcuConnection[locAddr=%s, rmtAddr=%s, protocol=%s, "
        "status=%s, innerStatus=%s, [dieId=%u, channelId=%u, jettyNum=%u]]]",
        locAddr_.Describe().c_str(), rmtAddr_.Describe().c_str(), tpProtocol.Describe().c_str(),
        status.Describe().c_str(), innerStatus.Describe().c_str(), dieId, channelInfo_.channelId,
        jettyNum);
}

uint32_t CcuConnection::GetDieId() const
{
    return dieId;
}

uint32_t CcuConnection::GetChannelId() const
{
    return channelInfo_.channelId;
}

int32_t CcuConnection::GetDevLogicId() const
{
    return devLogicId;
}

std::vector<ConnJettyInfo> CcuConnection::GetDeleteJettyInfo()
{
    std::vector<ConnJettyInfo> connDeleteJettyInfos;
    ConnJettyInfo jettyInfo;
    for (auto &ccuJetty : ccuJettys_) {
        if (ccuJetty != nullptr) {
            ccuJetty->GetJettyInfo(jettyInfo);
            jettyInfo.rdmaHandle = rdmaHandle;
            connDeleteJettyInfos.push_back(jettyInfo);
        }
    }
    return connDeleteJettyInfos;
}

std::vector<ConnJettyInfo> CcuConnection::GetUnimportJettyInfo()
{
    std::vector<ConnJettyInfo> connUnimportJettyInfos;
    ConnJettyInfo jettyInfo;
    for (auto &item : importJettyCtxs) {
        if (item.outParam.handle != 0) {
            jettyInfo.remoteJetty = item.outParam.handle;
            jettyInfo.rdmaHandle = rdmaHandle;
            item.outParam.handle = 0;
            connUnimportJettyInfos.push_back(jettyInfo);
        }
    }
    return connUnimportJettyInfos;
}

void CcuConnection::Clean()
{
    status = CcuConnStatus::INIT;
    innerStatus = InnerStatus::INIT;
    isJettyCreated = false;
    isJettyImported = false;
    ReleaseConnRes();
    GenerateLocalPsn();

    // 销毁jetty要在ReleaseConnRes之后
    for (auto &ccuJetty : ccuJettys_) {
        ccuJetty->Clean();
    }
}

HcclResult CcuConnection::Ipv4ToIpArray(const char *ipv4Str, uint8_t ipArr[16U])
{
    if (ipv4Str == NULL || ipArr == NULL) {
        HCCL_ERROR("[CcuConnection::%s] ipv4Str or ipArr is null", __func__);
        return HCCL_E_PARA;
    }

    // inet_pton: 将点分十进制IP转为网络字节序的二进制(sip: 128 bit->16字节，IPv4填充后4字节，后12字节留0)
    struct in_addr addr;
    int ret = inet_pton(AF_INET, ipv4Str, &addr);
    if (ret != 1) {
        HCCL_ERROR("[%s] Failed to convert the ipv4Str[%s] to ipArr.", __func__, ipv4Str);
        return HCCL_E_PARA;
    }

    // 将ipArr清零
    memset_s(&ipArr[0], 16, 0, 16);

    uint32_t ipNet = addr.s_addr;   // 网络字节序的 IP 整数
    // 拆分网络序整数为4个字节，写入 ipArr 前4位（大端序）
    // ipArr[15] = 最高位字节（如 192.168.100.2 的 2）
    ipArr[12] = ipNet & 0xFF;           // 192
    ipArr[13] = (ipNet >> 8) & 0xFF;    // 168
    ipArr[14] = (ipNet >> 16) & 0xFF;   // 100
    ipArr[15] = (ipNet >> 24) & 0xFF;   // 2
    return HCCL_SUCCESS;
}

HcclResult CcuConnection::SetTpAttrAsync()
{
    RequestHandle reqHandle{0};
    TpHandle tpHandle = tpInfo.tpHandle;
    /*  bitmap 至少配置为1FC，转2进制: 0011 1111 1000(前两位retry_times_init+at不用配置、后三位at_times+sl+ttl不用配置)，转10进制:508 
        0-retry_times_init: 3 bit   1-at: 5 bit             2-sip: 128 bit
        3-dip: 128 bit              4-sma: 48 bit           5-dma: 48 bit
        6-vlan_id: 12 bit           7-vlan_en: 1 bit        8-dscp: 6 bit
        9-at_times: 5 bit           10-sl: 4 bit             11-ttl: 8 bit
    */
    uint32_t attrBitmap = 508;
    struct TpAttr tpAttr = {0};

    // 填充本端IP
    // inet_pton: 将点分十进制IP转为网络字节序的二进制(sip: 128 bit->16字节，IPv4填充前4字节，后12字节留0)
    const char* localIp = locIpv4Addr.GetIpStr().c_str();
    CHK_RET(Ipv4ToIpArray(localIp, tpAttr.sip));
    HCCL_INFO("[CcuConnection::%s] localIpv4Str[%s], sip[%u:%u:%u:%u]",
        __func__, localIp, tpAttr.sip[12], tpAttr.sip[13], tpAttr.sip[14], tpAttr.sip[15]);

    // 填充对端IP
    const char* rmtIp = rmtIpv4Addr.GetIpStr().c_str();
    CHK_RET(Ipv4ToIpArray(rmtIp, tpAttr.dip));
    HCCL_INFO("[CcuConnection::%s] rmtIpv4Str[%s], dip[%u:%u:%u:%u]",
        __func__, rmtIp, tpAttr.dip[12], tpAttr.dip[13], tpAttr.dip[14], tpAttr.dip[15]);

    CHK_RET(HrtRaSetTpAttrAsync(rdmaHandle, tpHandle, attrBitmap, tpAttr, reqHandle));
    return HCCL_SUCCESS;
}

HcclResult CcuConnection::GetTpAttrAsync()
{
    RequestHandle reqHandle{0};
    TpHandle tpHandle = tpInfo.tpHandle;
    uint32_t attrBitmap = 0;
    struct TpAttr tpAttr = {0};

    CHK_RET(HrtRaGetTpAttrAsync(rdmaHandle, tpHandle, attrBitmap, tpAttr, reqHandle));
    HCCL_INFO("[CcuConnection::%s] locIpv4Addr[%s], rmtIpv4Addr[%s], locAddr[%s], rmtAddr[%s]",
        __func__, locIpv4Addr.Describe().c_str(), rmtIpv4Addr.Describe().c_str(),
        locAddr.Describe().c_str(), rmtAddr.Describe().c_str());

    HCCL_INFO("[CcuConnection::%s] attrBitmap[%u], "
        "sip[%u:%u:%u:%u], dip[%u:%u:%u:%u], "
        "sma[%#x:%#x:%#x:%#x:%#x:%#x], dma[%#x:%#x:%#x:%#x:%#x:%#x]",
        __func__, attrBitmap,
        tpAttr.sip[12], tpAttr.sip[13], tpAttr.sip[14], tpAttr.sip[15],
        tpAttr.dip[12], tpAttr.dip[13], tpAttr.dip[14], tpAttr.dip[15],
        tpAttr.sma[0], tpAttr.sma[1], tpAttr.sma[2], tpAttr.sma[3], tpAttr.sma[4], tpAttr.sma[5],
        tpAttr.dma[0], tpAttr.dma[1], tpAttr.dma[2], tpAttr.dma[3], tpAttr.dma[4], tpAttr.dma[5]);
    return HCCL_SUCCESS;
}

} // namespace Hccl