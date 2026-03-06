/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_CCU_TRANSPORT_H
#define HCOMM_CCU_TRANSPORT_H

#include <memory>
#include <vector>
#include <shared_mutex>

#include "../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "op_mode.h"
#include "binary_stream.h"
#include "ccu_conn.h"

namespace hcomm {

class CcuTransport {
public:
    static constexpr uint32_t INIT_CKE_NUM = 8;
    static constexpr uint32_t INIT_XN_NUM  = 8;
    MAKE_ENUM(TransStatus, INIT, SEND_ALL_INFO, RECV_ALL_INFO, SEND_TRANS_RES, RECV_TRANS_RES, SEND_FIN, RECV_FIN,
              RECVING_FIN, RECVING_TRANS_RES, READY, CONNECT_FAILED, SOCKET_TIMEOUT)

    struct CclBufferInfo {
        uint64_t addr{0};
        uint32_t size{0};
        uint32_t tokenId{0};
        uint32_t tokenValue{0};

        explicit CclBufferInfo() = default;
        CclBufferInfo(const uint64_t addr, const uint32_t size,
            const uint32_t tokenId, const uint32_t tokenValue)
            : addr(addr), size(size), tokenId(tokenId), tokenValue(tokenValue) {}

        void Pack(Hccl::BinaryStream &binaryStream) const {
            binaryStream << addr << size << tokenId << tokenValue;
            HCCL_INFO("Pack Ccl Buffer Info: addr[%llu] size[%u]", addr, size);
        }

        void Unpack(Hccl::BinaryStream &binaryStream) {
            binaryStream >> addr >> size >> tokenId >> tokenValue;
            HCCL_INFO("Unpack Ccl Buffer Info: addr[%llu] size[%u]", addr, size);
        }
    };

    MAKE_ENUM(CcuConnectionType, UBC_TP, UBC_CTP);
    struct CcuConnectionInfo {
        CcuConnectionType type{CcuConnectionType::UBC_TP};
        CommAddr locAddr{};
        CommAddr rmtAddr{};
        CcuChannelInfo channelInfo{};
        std::vector<CcuJetty *> ccuJettys{};

        explicit CcuConnectionInfo() = default;
        CcuConnectionInfo(const CcuConnectionType type,
            const CommAddr &locAddr, const CommAddr &rmtAddr,
            const CcuChannelInfo &channelInfo,
            const std::vector<CcuJetty *> &ccuJettys)
            : type(type), locAddr(locAddr), rmtAddr(rmtAddr),
            channelInfo(channelInfo), ccuJettys(ccuJettys) {}
    };

    CcuTransport(Hccl::Socket *socket, std::unique_ptr<CcuConnection> &&connection, const CclBufferInfo &locCclBufInfo);
    CcuTransport(const CcuTransport &that)             = delete;
    CcuTransport &operator=(const CcuTransport &other) = delete;
    ~CcuTransport();
    HcclResult  Init();
    TransStatus GetStatus();
    void        Clean();

    // 下面接口为平台层接口，不能在框架层使用
    uint32_t    GetDieId() const;
    uint32_t    GetChannelId() const;
    HcclResult  GetLocCkeByIndex(const uint32_t index, uint32_t &locCkeId) const;
    HcclResult  GetLocXnByIndex(const uint32_t index, uint32_t &locXnId) const;
    HcclResult  GetRmtCkeByIndex(const uint32_t index, uint32_t &rmtCkeId) const;
    HcclResult  GetRmtXnByIndex(const uint32_t index, uint32_t &rmtXnId) const;
    HcclResult  GetLocBuffer(CclBufferInfo &bufferInfo, const uint32_t &bufNum) const;
    HcclResult  GetRmtBuffer(CclBufferInfo &bufferInfo, const uint32_t &bufNum) const;
    HcclResult  GetCkeNum(uint32_t &ckeNum) const;

    std::string Describe() const;

public:
    struct Attribution {
        Hccl::OpMode opMode{Hccl::OpMode::OPBASE};
        u32          devicePhyId{0};
        std::vector<char> handshakeMsg{};
        std::string Describe() const {
            return Hccl::StringFormat("CcuTransportAttribution[opMode=%s, devicePhyId=%u, handshakeMsg=%s]",
                                opMode.Describe().c_str(), devicePhyId,
                                Hccl::Bytes2hex(handshakeMsg.data(), handshakeMsg.size()).c_str());
        }
    };

    std::vector<char> &GetRmtHandshakeMsg() // 返回握手消息
    {
        return rmtHandshakeMsg_;
    }

    std::vector<char> &GetLocalHandshakeMsg() // 返回握手消息
    {
        return attr_.handshakeMsg;
    }

    void SetHandshakeMsg(const std::vector<char> &handshakeMsg)
    {
        attr_.handshakeMsg = handshakeMsg;
    }

private:
    HcclResult StatusMachine();
    HcclResult AppendCkes(uint32_t ckesNum);
    HcclResult AppendXns(uint32_t xnsNum);
    HcclResult SendFinish();
    HcclResult RecvFinish();
    HcclResult CheckFinish();
    HcclResult RecvDataProcess();
    HcclResult RecvTransInfoProcess();
    HcclResult ReleaseTransRes();
    HcclResult SendConnAndTransInfo();
    HcclResult RecvConnAndTransInfo();
    HcclResult SendTransInfo();
    HcclResult RecvTransInfo();
    HcclResult HandshakeMsgPack(Hccl::BinaryStream &binaryStream);
    HcclResult ConnInfoPack(Hccl::BinaryStream &binaryStream) const;
    HcclResult TransResPack(Hccl::BinaryStream &binaryStream);
    HcclResult CclBufferInfoPack(Hccl::BinaryStream &binaryStream) const;
    HcclResult HandshakeMsgUnpack(Hccl::BinaryStream &binaryStream);
    HcclResult ConnInfoUnpackProc(Hccl::BinaryStream &binaryStream) const;
    HcclResult TransResUnpackProc(Hccl::BinaryStream &binaryStream);
    HcclResult CclBufferInfoUnpack(Hccl::BinaryStream &binaryStream);

    HcclResult ReturnErrorStatus(const std::string &funcName);

private:
    // 保存transport中需要使用的cke，xn等ccu资源
    struct TransRes {
        std::vector<uint32_t> ckes{};
        std::vector<uint32_t> xns{};
    };
    
    uint32_t                                 dieId_{0};
    int32_t                                  devLogicId_{0};
    Attribution                              attr_{};
    std::vector<char>                        rmtHandshakeMsg_{0}; // 远端握手消息
    Hccl::Socket                             *socket_{nullptr};
    std::unique_ptr<CcuConnection>           ccuConnection_;
    TransRes                                 locRes_{};
    TransRes                                 rmtRes_{};
    TransStatus                              transStatus_{TransStatus::INVALID};
    std::vector<std::vector<ResInfo>>        ckesRes_{};
    std::vector<std::vector<ResInfo>>        xnsRes_{};
    CclBufferInfo                            locCclBufInfo_{};
    CclBufferInfo                            rmtCclBufInfo_{};
    uint32_t                                 exchangeDataSize_{0};
    std::vector<char>                        recvData_{};
    std::vector<char>                        recvTrans_{};
    std::vector<char>                        sendData_{};
    std::vector<char>                        sendTrans_{};
    std::vector<char>                        recvFinishMsg_{};
    std::vector<char>                        sendFinishMsg_{};
};

HcclResult CcuCreateTransport(Hccl::Socket *socket, const CcuTransport::CcuConnectionInfo &ccuConnectionInfo,
    const CcuTransport::CclBufferInfo &cclBufferInfo, std::unique_ptr<CcuTransport> &ccuTransport);

} // namespace hcomm
#endif // HCOMM_CCU_TRANSPORT_H