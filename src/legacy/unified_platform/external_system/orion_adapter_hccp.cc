/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "orion_adapter_hccp.h"
#include <chrono>
#include <unistd.h>
#include <memory>
#include <unordered_map>
#include "sal.h"
#include "network_api_exception.h"
#include "internal_exception.h"
#include "hccp.h"
#include "hccp_tlv.h"
#include "hccp_ctx.h"
#include "hccp_async_ctx.h"
#include "hccp_async.h"
#include "env_config.h"
#include "hccp_common.h"
#include "orion_adapter_hccp.h"
#include "exception_util.h"

using namespace std;

namespace Hccl {
constexpr u32 ONE_HUNDRED_MICROSECOND_OF_USLEEP = 100;
constexpr u32 ONE_MILLISECOND_OF_USLEEP         = 1000;
constexpr unsigned int SOCKET_NUM_ONE           = 1;
constexpr u32 MAX_NUM_OF_WHITE_LIST_NUM         = 16;
constexpr uint32_t TP_HANDLE_REQUEST_NUM        = 1;
constexpr u32      AUTO_LISTEN_PORT             = 0;
constexpr u64 SOCKET_SEND_MAX_SIZE              = 0x7FFFFFFFFFFFFFFF;
constexpr u32 MAX_WR_NUM = 1024;
constexpr u32 MAX_SEND_SGE_NUM = 8;
constexpr u32 MAX_RECV_SGE_NUM = 1;
constexpr u32 MAX_CQ_DEPTH = 65535;
constexpr u32 MAX_INLINE_DATA = 128;
constexpr u32 RA_TLV_REQUEST_UNAVAIL = 328307;

const std::unordered_map<HrtNetworkMode, NetworkMode, EnumClassHash> HRT_NETWORK_MODE_MAP
    = {{HrtNetworkMode::PEER, NetworkMode::NETWORK_PEER_ONLINE}, {HrtNetworkMode::HDC, NetworkMode::NETWORK_OFFLINE}};

s32 g_linkTimeout = 0;
inline s32 EnvLinkTimeoutGet()
{
    g_linkTimeout = g_linkTimeout != 0 ? g_linkTimeout : EnvConfig::GetInstance().GetSocketConfig().GetLinkTimeOut();
    return g_linkTimeout;
}

inline union HccpIpAddr IpAddressToHccpIpAddr(IpAddress &addr)
{
    union HccpIpAddr hccpIpAddr;
    if (addr.GetFamily() == AF_INET) {
        hccpIpAddr.addr = addr.GetBinaryAddress().addr;
    } else {
        hccpIpAddr.addr6 = addr.GetBinaryAddress().addr6;
    }
    return hccpIpAddr;
}

inline IpAddress IfAddrInfoToIpAddress(struct InterfaceInfo info)
{
    BinaryAddr addr;
    if (info.family == AF_INET) {
        addr.addr = info.ifaddr.ip.addr;
    } else {
        addr.addr6 = info.ifaddr.ip.addr6;
    }
    return IpAddress(addr, info.family, info.scopeId);
}

void* HrtRaTlvInit(HRaTlvInitConfig  &cfg) 
{
    struct TlvInitInfo init_info {};
    init_info.version       = cfg.version;
    init_info.phyId         = cfg.phyId;
    init_info.nicPosition   = HRT_NETWORK_MODE_MAP.at(cfg.mode);

    s32 ret = 0;
    unsigned int buffer_size;
    void *tlv_handle;

    ret = RaTlvInit(&init_info, &buffer_size, &tlv_handle);
    if (ret != 0 || tlv_handle == nullptr) {
        HCCL_ERROR("[Init][RaTlv]errNo[0x%016llx] ra tlv init fail. params: mode[%u]. return: ret[%d]", 
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), cfg.mode, ret);
        throw NetworkApiException(StringFormat("call ra_tlv_init failed, mode=%u, device id=%u, version=%d",
                                               cfg.mode, init_info.phyId, init_info.version));
    }

    HCCL_INFO("tlv init success, device id[%u]", init_info.phyId);

    return tlv_handle; 
}

HcclResult HrtRaTlvRequest(void* tlv_handle, u32 tlv_module_type, u32 tlv_ccu_msg_type) 
{
    s32 ret = 0;

    struct TlvMsg send_msg {};
    struct TlvMsg recv_msg {};
    send_msg.type = tlv_ccu_msg_type;

    ret = RaTlvRequest(tlv_handle, tlv_module_type, &send_msg, &recv_msg);
    if (ret != 0) {
        if (ret == RA_TLV_REQUEST_UNAVAIL) {
            HCCL_WARNING("[HrtRaTlvRequest]ra tlv request UNAVAIL. return: ret[%d]", ret);
            return HCCL_E_UNAVAIL;
        }
        HCCL_ERROR("[Request][RaTlv]errNo[0x%016llx] ra tlv request fail. return: ret[%d], module type[%u], message type[%u]", 
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret, tlv_module_type, tlv_ccu_msg_type);
        throw NetworkApiException(StringFormat("call ra_tlv_request failed"));
    }

    HCCL_INFO("tlv request success, tlv module type[%u], message type[%u]", tlv_module_type, tlv_ccu_msg_type);
    return HCCL_SUCCESS;
}

void HrtRaTlvDeInit(void* tlv_handle)
{
    s32 ret = 0;

    ret = RaTlvDeinit(tlv_handle); 
    if (ret != 0) {
        HCCL_ERROR("[DeInit][RaTlv]errNo[0x%016llx] ra tlv deinit fail. return: ret[%d]", 
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
        throw NetworkApiException(StringFormat("call RaTlvDeinit failed"));
    }
}

void HrtRaInit(HRaInitConfig &cfg)
{
    struct RaInitConfig config {};
    config.phyId           = cfg.phyId;
    config.nicPosition     = HRT_NETWORK_MODE_MAP.at(cfg.mode);
    config.hdcType         = PID_HDC_TYPE;
    config.enableHdcAsync = true;

    s32  ret       = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());

    while (true) {
        ret = RaInit(&config);
        if (!ret) {
            break; // 成功跳出
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[Init][Ra]errNo[0x%016llx] ra init timeout[%lld]. return[%d], ",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), timeout, ret);
                throw NetworkApiException(StringFormat("call RaInit timeout, phy_id=%u, nic_position=%u",
                                                       config.phyId, config.nicPosition));
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[Init][Ra]errNo[0x%016llx] ra init fail. return[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
            // 非ra限速场景错误，不轮询。直接退出
            throw NetworkApiException(
                StringFormat("call RaInit failed, phy_id=%u, nic_position=%u", config.phyId, config.nicPosition));
        }
    }
    HCCL_INFO("init ra success.");
}

void HrtRaDeInit(HRaInitConfig &cfg)
{
    struct RaInitConfig config {};
    config.phyId       = cfg.phyId;
    config.nicPosition = HRT_NETWORK_MODE_MAP.at(cfg.mode);
    config.hdcType     = PID_HDC_TYPE;

    s32  ret       = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        ret = RaDeinit(&config);
        if (!ret) {
            break; // 成功跳出
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[DeInit][Ra]errNo[0x%016llx] ra deinit timeout[%lld]. return[%d], ",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), timeout, ret);
                throw NetworkApiException(StringFormat("call RaDeinit timeout, phy_id=%u, nic_position=%u",
                                                       config.phyId, config.nicPosition));
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[DeInit][Ra]errNo[0x%016llx] ra deinit fail. return[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
            // 非ra限速场景错误，不轮询。直接退出
            throw NetworkApiException(StringFormat("call RaDeinit failed, phy_id=%u, nic_position=%u", config.phyId,
                                                   config.nicPosition));
        }
    }
}

static void SocketBatchConnect(SocketConnectInfoT conn[], u32 num)
{
    s32  ret       = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        ret = RaSocketBatchConnect(conn, num);
        if (!ret) {
            break; // 成功跳出
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[BatchConnect][RaSocket]errNo[0x%016llx] ra socket batch connect "
                           "timeout[%lld]. return[%d]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), timeout, ret);
                throw NetworkApiException(StringFormat("call RaSocketBatchConnect timeout, num=%u", num));
            }

            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[BatchConnect][RaSocket]errNo[0x%016llx] ra socket batch connect fail. return[%d], params: ",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
            throw NetworkApiException(StringFormat("call RaSocketBatchConnect failed, num=%u", num));
        }
    }
}

void HrtRaSocketConnectOne(RaSocketConnectParam &in)
{
    struct SocketConnectInfoT connInfo {};
    connInfo.socketHandle = in.socketHandle;
    connInfo.remoteIp     = IpAddressToHccpIpAddr(in.remoteIp);
    connInfo.port          = in.port;

    int sret = strcpy_s(connInfo.tag, sizeof(connInfo.tag), in.tag.c_str());
    if (sret != 0) {
        string msg
            = StringFormat("[HrtRaSocketConnectOne] copy tag[%s] to hccp tag failed, ret=%d", in.tag.c_str(), sret);
        throw NetworkApiException(msg);
    }

    HCCL_INFO("Socket Connect tag=[%s], remoteIp[%s]", connInfo.tag, in.remoteIp.Describe().c_str());
    SocketBatchConnect(&connInfo, 1);
}

static void HRaSocketBatchClose(struct SocketCloseInfoT conn[], u32 num)
{
    HCCL_INFO("ra socket batch close");
    s32  ret       = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        ret = RaSocketBatchClose(conn, num);
        if (!ret) {
            break; // 成功跳出
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[BatchClose][RaSocket]errNo[0x%016llx]  ra socket batch close "
                           "timeout[%d]. return[%d]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), timeout, ret);
                throw NetworkApiException(StringFormat("call RaSocketBatchClose timeout, num=%u", num));
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[BatchClose][RaSocket]errNo[0x%016llx] ra socket batch close fail. return[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
            // 非ra限速场景错误，不轮询，直接退出
            throw NetworkApiException(StringFormat("call RaSocketBatchClose failed, num=%u", num));
        }
    }
}

void HrtRaSocketCloseOne(RaSocketCloseParam &in)
{
    struct SocketCloseInfoT closeInfo;
    closeInfo.fdHandle     = in.fdHandle;
    closeInfo.socketHandle = in.socketHandle;
    HRaSocketBatchClose(&closeInfo, 1);
}

static void HRaSocketListenStart(struct SocketListenInfoT conn[], u32 num)
{
    s32  ret       = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());

    while (true) {
        ret = RaSocketListenStart(conn, num);
        if (ret == 0) {
            break; // 成功跳出
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[ListenStart][RaSocket]errNo[0x%016llx] ra socket listen start "
                           "timeout[%d]. return[%d]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), EnvLinkTimeoutGet(), ret);
                throw NetworkApiException(StringFormat("call RaSocketListenStart timeout, num=%u", num));
            }

            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("errNo[0x%016llx] ra socket listen start fail. return[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
            // 非ra限速场景错误，不轮询，直接退出
            throw NetworkApiException(StringFormat("call RaSocketListenStart failed, num=%u", num));
        }
    }
}

static bool RaSocketTryListenStart(struct SocketListenInfoT conn[], u32 num)
{
    s32 ret = RaSocketListenStart(conn, num);
    if (ret == 0) {
        return true;
    } else if (ret == SOCK_EAGAIN) {
        HCCL_INFO("[%s] listen eagain", __func__);
        return true;
    } else if (ret == SOCK_EADDRINUSE){
        HCCL_INFO("[%s]ra socket listen could not start, due to the port[%u] has already been bound. please try"
                    " another port or check the port status", __func__, (num > 0 ? conn[0].port : HCCL_INVALID_PORT));
        return false;
    } else {
        HCCL_ERROR("errNo[0x%016llx] ra socket listen start fail. return[%d]",
                    HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
        // 非ra限速场景错误，不轮询，直接退出
        throw NetworkApiException(StringFormat("call RaSocketListenStart failed, num=%u", num));
    }
}

static void HRaSocketListenStop(struct SocketListenInfoT conn[], u32 num)
{
    s32  ret       = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        ret = RaSocketListenStop(conn, num);
        if (!ret || ret == 228202) { // 待修改: 同步版本后 228202 修改为 SOCK_ENODEV
            break;                   // 成功跳出
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[ListenStop][RaSocket]errNo[0x%016llx]  ra socket listen stop fail "
                           "timeout[%d]. return[%d]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), timeout, ret);
                throw NetworkApiException(StringFormat("call RaSocketListenStop timeout, num=%u", num));
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[ListenStop][RaSocket]errNo[0x%016llx] ra socket listen stop fail. return[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
            // 非ra限速场景错误，不轮询，直接退出
            throw NetworkApiException(StringFormat("call RaSocketListenStop failed, num=%u", num));
        }
    }
}

void HrtRaSocketListenOneStart(RaSocketListenParam &in)
{
    struct SocketListenInfoT listenInfo {};
    listenInfo.socketHandle = in.socketHandle;
    listenInfo.port = in.port;
    HRaSocketListenStart(&listenInfo, 1);
}

bool HrtRaSocketTryListenOneStart(RaSocketListenParam &in)
{
    struct SocketListenInfoT listenInfo {};
    listenInfo.socketHandle = in.socketHandle;
    listenInfo.port = in.port;
    bool ret = RaSocketTryListenStart(&listenInfo, 1);
    if (ret && in.port == AUTO_LISTEN_PORT) {
        in.port = listenInfo.port;
    }
    return ret;
}

void HrtRaSocketListenOneStop(RaSocketListenParam &in)
{
    struct SocketListenInfoT listenInfo {};
    listenInfo.socketHandle = in.socketHandle;
    listenInfo.port = in.port;
    HRaSocketListenStop(&listenInfo, 1);
}

void RaBlockGetSockets(u32 role, SocketInfoT conn[], u32 num) // 修改为内部函数，不对外
{
    s32  sockRet;
    u32  gotSocketsCnt = 0;
    auto startTime     = std::chrono::steady_clock::now();
    auto timeout       = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[HrtRaBlockGetSockets] get rasocket timeout role[%u], num[%u], goten[%u], "
                       "timeout[%lld]s, the HCCL_CONNECT_TIMEOUT may be insufficient.",
                       role, num, gotSocketsCnt, timeout);
            throw NetworkApiException(StringFormat("call RaGetSockets timeout, num=%u", num));
        }
        u32 connectedNum = 0;
        sockRet          = RaGetSockets(role, conn, num, &connectedNum);
        if ((connectedNum == 0 && sockRet == 0) || (sockRet == SOCK_EAGAIN)) {
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else if (sockRet != 0) {
            HCCL_ERROR("[Get][RaSocket]get rasocket error. role[%u], num[%u] sockRet[%d] connectednum[%u]", role, num,
                       sockRet, connectedNum);
            throw NetworkApiException(StringFormat("call RaGetSockets failed, num=%u", num));
        } else {
            gotSocketsCnt += connectedNum;
            if (gotSocketsCnt == num) {
                HCCL_INFO("block get sockets success, socket num[%u]", gotSocketsCnt);
                break;
            } else if (gotSocketsCnt > num) {
                HCCL_ERROR("[Get][RaSocket]total Sockets[%u], more than needed num[%u]!", gotSocketsCnt, num);
                throw NetworkApiException(StringFormat("call RaGetSockets failed, num=%u", num));
            } else {
                SaluSleep(ONE_MILLISECOND_OF_USLEEP);
            }
        }
    }
}

RaSocketFdHandleParam HrtRaBlockGetOneSocket(u32 role, RaSocketGetParam &param)
{
    struct SocketInfoT socketInfo {};

    socketInfo.socketHandle = param.socketHandle;
    socketInfo.fdHandle     = param.fdHandle;
    socketInfo.remoteIp     = IpAddressToHccpIpAddr(param.remoteIp);
    socketInfo.status        = SOCKET_NOT_CONNECTED;

    int sret = strcpy_s(socketInfo.tag, sizeof(socketInfo.tag), param.tag.c_str());
    if (sret != 0) {
        string msg = StringFormat("[HrtRaBlockGetOneSocket] copy tag[%s] to hccp failed, ret=%d",
                                  param.tag.c_str(), sret);
        throw NetworkApiException(msg);
    }
    HCCL_INFO("Socket Get tag=[%s], remoteIp[%s]", socketInfo.tag, param.remoteIp.Describe().c_str());
    RaBlockGetSockets(role, &socketInfo, 1);

    return RaSocketFdHandleParam(socketInfo.fdHandle, socketInfo.status);
}

void HrtRaSocketBlockSend(const FdHandle fdHandle, const void *data, u32 sendSize)
{
    CHECK_NULLPTR(data, "[HrtRaSocketBlockSend] data is nullptr!");
    s32                        ret           = 0;
    void                      *sendData      = const_cast<void *>(data);
    const std::chrono::seconds timeout       = std::chrono::seconds(EnvLinkTimeoutGet());
    const auto                 start         = std::chrono::steady_clock::now();
    u32                        totalSentSize = 0;
    unsigned long long         sentSize      = 0;

    HCCL_INFO("before ra socket send, para: data[%p], size[%u]", sendData, sendSize);

    while (true) {
        // 底层ra_socket_send host网卡无限制，device网卡由于HDC通道限制的限制有大小限制(目前大小为64KB)
        ret = RaSocketSend(fdHandle, reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(sendData) + totalSentSize),
                             sendSize - totalSentSize, &sentSize);
        HCCL_INFO("ra socket send, data[%p], size[%u] send size[%u]", sendData, sendSize, totalSentSize);
        if (ret == 0) {
            totalSentSize += sentSize;
            if (totalSentSize == sendSize) { // 只有完全发送完才返回成功
                break;
            }

            if (totalSentSize > sendSize) {
                HCCL_ERROR("[Send][RaSocket]errNo[0x%016llx] ra socket send failed, "
                           "data[%p] size[%u] retSize[%u]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), data, sendSize, sentSize);
                throw NetworkApiException(
                    StringFormat("call RaSocketSend failed, fdHandle=%p data=%p size=%u retSize=%u", fdHandle, data,
                                 sendSize, sentSize));
            }

            SaluSleep(ONE_HUNDRED_MICROSECOND_OF_USLEEP);
        } else if (ret == SOCK_EAGAIN) {
            /* ra速率限制 retry */
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[Send][RaSocket]errNo[0x%016llx] ra socket send failed, data[%p] size[%u], "
                       "sent[%u] ret[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), data, sendSize, sentSize, ret);
            throw NetworkApiException(StringFormat("call RaSocketSend failed, fdHandle=%p data=%p size=%u retSize=%u",
                                                   fdHandle, data, sendSize, sentSize));
        }

        /* 获取当前时间，如果耗时超过timeout，则返回错误 */
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > timeout) {
            HCCL_ERROR("[Send][RaSocket]errNo[0x%016llx] Wait timeout for sockets send, data[%p], "
                       "size[%u], sentsize[%u]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), data, sendSize, sentSize);
            throw NetworkApiException(StringFormat("call RaSocketSend failed, fdHandle=%p data=%p size=%u retSize=%u",
                                                   fdHandle, data, sendSize, sentSize));
        }
    }
    HCCL_INFO("ra socket send finished");
}

bool HrtRaSocketNonBlockSend(const FdHandle fdHandle, void *data, u64 size, u64 *sentSize)
{
    CHECK_NULLPTR(data, "[HrtRaSocketNonBlockSend] data is nullptr!");
    
    if (size > SOCKET_SEND_MAX_SIZE) {
        throw NetworkApiException(StringFormat("[hrtRaSocketNonBlockSend]errNo[0x%016llx] ra socket send size is too large, "
                                 "data[%p], size[%llu]", HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), data, size));
    }

    s32 ret = RaSocketSend(fdHandle, data, size, sentSize);
    if (ret == 0 || ret == SOCK_EAGAIN) {
        HCCL_INFO("[HrtRaSocketNonBlockSend] ra socket send, data[%p], size[%llu] send size[%llu]", data, size, *sentSize);
        return true;
    } else {
        HCCL_ERROR("call RaSocketSend failed, fdHandle=%p data=%p size=%llu sentSize=%llu",
                    fdHandle, data, size, *sentSize);
        return false;
    }
}

void HrtRaSocketBlockRecv(const FdHandle fdHandle, void *data, u32 size)
{
    auto                       startTime = std::chrono::steady_clock::now();
    unsigned long long         recvSize  = 0;
    s32                        rtRet     = 0;
    u32                        getedLen  = 0;
    const std::chrono::seconds timeout   = std::chrono::seconds(EnvLinkTimeoutGet());

    CHECK_NULLPTR(data, "[HrtRaSocketBlockRecv] data is nullptr!");
    HCCL_INFO("before ra socket recv, para: data[%p], size[%u]", data, size);
    while (true) {
        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[Recv][RaSocket]errNo[0x%016llx] Wait timeout for sockets recv, data[%p], "
                       "size[%u], recvSize[%u], The most common cause is that the firewall is incorrectly "
                       "configured. Check the firewall configuration or try to disable the firewall.",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), data, size, recvSize);

            throw NetworkApiException(
                StringFormat("call RaSocketRecv timeout, fdHandle=%p data=%p size=%u retSize=%u ret=%d", fdHandle,
                             data, size, recvSize, rtRet));
        }
        rtRet = RaSocketRecv(fdHandle, reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(data) + getedLen),
                               size - getedLen, &recvSize);
        if ((rtRet == 0) && (recvSize > 0)) { // 接收完成，也有可能要多次接收
            getedLen += recvSize;
            if (getedLen > size) {
                HCCL_ERROR("[Recv][RaSocket]errNo[0x%016llx] socket receive "
                           "rtSize[%u] bigger size[%zu]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_TRANSFER), getedLen, size);
                throw NetworkApiException(
                    StringFormat("call RaSocketRecv failed, fdHandle=%p data=%p size=%u retSize=%u ret=%d", fdHandle,
                                 data, size, recvSize, rtRet));
            }
            if (getedLen == size) {
                break;
            }
        } else if ((rtRet == 0) && (recvSize == 0)) {
            HCCL_ERROR("[Recv][RaSocket]recv fail, bufLen[%u], recLen[%llu]", size, recvSize);
            throw NetworkApiException(
                StringFormat("call RaSocketRecv failed, fdHandle=%p data=%p bufLen=%u, recLen=%lld, ret=%d", fdHandle,
                             data, size, recvSize, rtRet));
        } else if (rtRet == SOCK_ESOCKCLOSED || rtRet == SOCK_CLOSE) { // 连接关闭，出错
            HCCL_ERROR("[Recv][RaSocket]errNo[0x%016llx] recv fail, data[%p], size[%u]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_TRANSFER), data, size);
            throw NetworkApiException(StringFormat(
                "call RaSocketRecv failed, sock_esockclosed, fdhandle=%p data=%p bufLen=%u, recLen=%lld, ret=%d",
                fdHandle, data, size, recvSize, rtRet));
        } else if (rtRet != 0) {
            SaluSleep(ONE_MILLISECOND_OF_USLEEP); // 尚未接收到数据,延时1ms
            continue;
        }
    }
    HCCL_INFO("ra socket receive finished");
}

SocketHandle HrtRaSocketInit(HrtNetworkMode  netMode, RaInterface &in)
{
    int mode = HRT_NETWORK_MODE_MAP.at(netMode);
    struct rdev rdevInfo {};
    rdevInfo.phyId   = in.phyId;
    rdevInfo.family   = in.address.GetFamily();
    rdevInfo.localIp = IpAddressToHccpIpAddr(in.address);

    SocketHandle socketHandle = nullptr;
    s32          ret          = RaSocketInit(mode, rdevInfo, &socketHandle);
    if (ret != 0 || (socketHandle == nullptr)) {
        HCCL_ERROR("[Init][RaSock]errNo[0x%016llx] ra socket init fail. params: mode[%d]. return: ret[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), mode, ret);
        throw NetworkApiException(StringFormat("call RaSocketInit failed, mode=%u, ip=%u device id=%u, family=%u",
                                               mode, rdevInfo.localIp.addr.s_addr, rdevInfo.phyId, rdevInfo.family));
    }

    HCCL_INFO("socket init success, ip[%u] device id[%u]", rdevInfo.localIp.addr.s_addr, rdevInfo.phyId);
    return socketHandle;
}

void HrtRaSocketDeInit(SocketHandle socketHandle)
{
    s32 ret = RaSocketDeinit(socketHandle);
    if (ret != 0) {
        HCCL_ERROR("[DeInit][RaSocket]errNo[0x%016llx] rt socket deinit fail. return[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
        throw NetworkApiException(StringFormat("call RaSocketDeinit failed, socketHandle=%p", socketHandle));
    }
}

void HrtRaSocketSetWhiteListStatus(u32 enable)
{
    s32 ret = RaSocketSetWhiteListStatus(enable);
    if (ret != 0) {
        HCCL_ERROR(
            "[Set][WhiteListStatus]errNo[0x%016llx] ra socekt set whilte list fail, return[%d]. para: enable[%u]",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret, enable);
        throw NetworkApiException(StringFormat("call RaSocketSetWhiteListStatus failed, enable=%u", enable));
    }
    HCCL_INFO("set host socket whitelist status[%u] success.", enable);
}

u32 HrtRaSocketGetWhiteListStatus()
{
    u32 enable;
    s32 ret = RaSocketGetWhiteListStatus(&enable);
    if (ret != 0) {
        HCCL_ERROR("[Get][WhiteListStatus]errNo[0x%016llx] ra socekt get whilte list fail, return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
        throw NetworkApiException("call RaSocketGetWhiteListStatus failed.");
    }
    return enable;
}

void HrtRaSocketWhiteListAdd(SocketHandle socketHandle, vector<RaSocketWhitelist> &wlists)
{
    vector<struct SocketWlistInfoT> wlistInfoVec;
    wlistInfoVec.reserve(MAX_NUM_OF_WHITE_LIST_NUM);
    size_t wlistNum = wlists.size();
    size_t startIdx = 0;
    while (wlistNum > 0) {
        size_t addListNum = wlistNum > MAX_NUM_OF_WHITE_LIST_NUM ? MAX_NUM_OF_WHITE_LIST_NUM : wlistNum;
        for (size_t idx = startIdx; idx < addListNum + startIdx; idx++) {
            struct SocketWlistInfoT wlistInfo {};
            wlistInfo.connLimit = wlists[idx].connLimit;
            wlistInfo.remoteIp  = IpAddressToHccpIpAddr(wlists[idx].remoteIp);

            int sret = strcpy_s(wlistInfo.tag, sizeof(wlistInfo.tag), wlists[idx].tag.c_str());
            if (sret != EOK) {
                auto msg = StringFormat("[Add][RaSocketWhiteList]errNo[0x%016llx]errName[HCCL_E_MEMORY] memory copy failed. ret[%d]",
                                        HCOM_ERROR_CODE(HcclResult::HCCL_E_MEMORY), sret);
                THROW<InternalException>(msg);
            }

            HCCL_INFO("add whitelistInfo tag=[%s], remoteIp[%s]",
                    wlistInfo.tag, wlists[idx].remoteIp.Describe().c_str());
            wlistInfoVec.push_back(wlistInfo);
        }

        s32 ret = RaSocketWhiteListAdd(socketHandle, wlistInfoVec.data(), wlistInfoVec.size());
        if (ret != 0) {
            HCCL_ERROR("[Add][RaSocketWhiteList]errNo[0x%016llx]errName[HCCL_E_TCP_CONNECT] ra white list add fail, return[%d].",
                    HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
            throw NetworkApiException(StringFormat("call RaSocketWhiteListAdd failed, num=%llu",
                    wlistInfoVec.size()+startIdx));
        }
        HCCL_INFO("add white list: num[%llu], remain [%llu].", addListNum, (wlistNum - addListNum));

        wlistInfoVec.clear();
        wlistNum -= addListNum;
        startIdx += addListNum;
    }
    HCCL_INFO("[HrtRaSocketWhiteListAdd] Success. Total add num [%llu]", wlists.size());
}

void HrtRaSocketWhiteListDel(SocketHandle socketHandle, vector<RaSocketWhitelist> &wlists)
{
    vector<struct SocketWlistInfoT> wlistInfoVec;
    wlistInfoVec.reserve(MAX_NUM_OF_WHITE_LIST_NUM);
    size_t wlistNum = wlists.size();
    size_t startIdx = 0;
    while (wlistNum > 0) {
        size_t delListNum = wlistNum > MAX_NUM_OF_WHITE_LIST_NUM ? MAX_NUM_OF_WHITE_LIST_NUM : wlistNum;
        for (size_t idx = startIdx; idx < delListNum + startIdx; idx++) {
            struct SocketWlistInfoT wlistInfo {};
            wlistInfo.connLimit = wlists[idx].connLimit;
            wlistInfo.remoteIp  = IpAddressToHccpIpAddr(wlists[idx].remoteIp);

            int sret = strcpy_s(wlistInfo.tag, sizeof(wlistInfo.tag), wlists[idx].tag.c_str());
            if (sret != EOK) {
                auto msg = StringFormat("[Del][RaSocketWhiteList]errNo[0x%016llx] memory copy failed. ret[%d]",
                                        HCOM_ERROR_CODE(HcclResult::HCCL_E_MEMORY), sret);
                THROW<InternalException>(msg);
            }
            wlistInfoVec.push_back(wlistInfo);
        }

        s32 ret = RaSocketWhiteListDel(socketHandle, wlistInfoVec.data(), wlistInfoVec.size());
        if (ret != 0) {
            HCCL_ERROR("[Del][RaSocketWhiteList]errNo[0x%016llx] ra white list del fail, return[%d].",
                    HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret);
            throw NetworkApiException(StringFormat("call RaSocketWhiteListDel failed, num=%llu", wlists.size()));
        }
        HCCL_INFO("del white list: num[%llu], remain [%llu].", delListNum, (wlistNum - delListNum));

        wlistInfoVec.clear();
        wlistNum -= delListNum;
        startIdx += delListNum;
    }
    HCCL_INFO("[HrtRaSocketWhiteListDel] Success. Total delete num[%llu]", wlists.size());
}

static u32 HrtGetIfNum(struct RaGetIfattr &config)
{
    u32 num = 0;
    s32 ret = RaGetIfnum(&config, &num);
    if (ret != 0) {
        HCCL_ERROR("[Get][IfNum]errNo[0x%016llx] ra get if num fail. ret[%d], num[%u]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret, num);
        throw NetworkApiException(
            StringFormat("call RaGetIfnum failed, phyId=%u, nicPosistion=%u", config.phyId, config.nicPosition));
    }
    return num;
}

static void HrtGetIfAddress(struct RaGetIfattr &config, InterfaceInfo ifaddrInfos[], u32 &num)
{
    s32 ret = RaGetIfaddrs(&config, ifaddrInfos, &num);
    if (ret != 0) {
        HCCL_ERROR("[Get][IfAddress]errNo[0x%016llx] ra get if address fail. ret[%d], num[%u]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret, num);
        throw NetworkApiException(StringFormat("call RaGetIfaddrs failed, num=%u", num));
    }
}

std::vector<std::pair<std::string, IpAddress>> HrtGetHostIf(u32 devPhyId)
{
    std::vector<std::pair<std::string, IpAddress>> hostIfs;
    struct RaGetIfattr                           config = {0};
    config.phyId                                         = devPhyId;
    config.nicPosition                                   = static_cast<u32>(NetworkMode::NETWORK_PEER_ONLINE);

    u32 ifAddrNum = HrtGetIfNum(config);
    HCCL_RUN_INFO("[Get][HostIf]hrtGetIfNum success. ifAddrNum[%u].", ifAddrNum);
    if (ifAddrNum == 0) {
        HCCL_WARNING("[Get][HostIf]there is no valid host interface, ifAddrNum[%u].", ifAddrNum);
        return hostIfs;
    }

    std::shared_ptr<struct InterfaceInfo> ifAddrInfoPtrs(new InterfaceInfo[ifAddrNum](),
                                                          std::default_delete<InterfaceInfo[]>());
    struct InterfaceInfo                 *ifAddrInfos = ifAddrInfoPtrs.get();

    (void)memset_s(ifAddrInfos, ifAddrNum * sizeof(InterfaceInfo), 0, ifAddrNum * sizeof(InterfaceInfo));

    HrtGetIfAddress(config, ifAddrInfos, ifAddrNum);

    for (u32 i = 0; i < ifAddrNum; i++) {
        IpAddress ip = IfAddrInfoToIpAddress(ifAddrInfos[i]);
        hostIfs.emplace_back(ifAddrInfos[i].ifname, ip);
        HCCL_INFO("HrtGetIfAddress: idx[%u] ifName[%s] ip[%s]", i, ifAddrInfos[i].ifname, ip.GetIpStr().c_str());
    }

    return hostIfs;
}

vector<IpAddress> HrtGetDeviceIp(u32 devicePhyId, NetworkMode netWorkMode)
{
    vector<IpAddress>     ipAddr;
    struct RaGetIfattr  config = {0};
    config.phyId                = devicePhyId;
    config.nicPosition          = static_cast<u32>(netWorkMode);

    u32 ifAddrNum = HrtGetIfNum(config);
    HCCL_RUN_INFO("[Get][DeviceIP]HrtGetIfNum success. ifAddrNum[%u].", ifAddrNum);

    if (ifAddrNum == 0) {
        HCCL_WARNING("[Get][DeviceIP]device has no ip information, phyId[%u]", devicePhyId);
        return ipAddr;
    }

    std::shared_ptr<struct InterfaceInfo> ifAddrInfoPtrs(new InterfaceInfo[ifAddrNum](),
                                                          std::default_delete<InterfaceInfo[]>());
    struct InterfaceInfo                 *ifAddrInfos = ifAddrInfoPtrs.get();

    (void)memset_s(ifAddrInfos, ifAddrNum * sizeof(InterfaceInfo), 0, ifAddrNum * sizeof(InterfaceInfo));

    HrtGetIfAddress(config, ifAddrInfos, ifAddrNum);

    for (u32 i = 0; i < ifAddrNum; i++) {
        IpAddress ip = IfAddrInfoToIpAddress(ifAddrInfos[i]);
        ipAddr.emplace_back(ip);
        HCCL_INFO("HrtGetIfAddress: idx[%u] ifName[%s] ip[%s]", i, ifAddrInfos[i].ifname, ip.GetIpStr().c_str());
    }

    return ipAddr;
}

RdmaHandle HrtRaRdmaInit(HrtNetworkMode netMode, RaInterface &in)
{
    RdmaHandle rdmaHandle = nullptr;
    int        mode       = HRT_NETWORK_MODE_MAP.at(netMode);

    unsigned int notifyType = netMode == HrtNetworkMode::PEER ? NO_USE : NOTIFY;
    struct rdev rdevInfo {};
    rdevInfo.phyId   = in.phyId;
    rdevInfo.family   = in.address.GetFamily();
    rdevInfo.localIp = IpAddressToHccpIpAddr(in.address);
    s32 ret           = RaRdevInit(mode, notifyType, rdevInfo, &rdmaHandle);
    if (ret != 0 || (rdmaHandle == nullptr)) {
        HCCL_ERROR("[Init][RaRdma]errNo[0x%016llx] rdma init fail. "
                   "params: mode[%d]. return: ret[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), mode, ret);
        throw NetworkApiException(StringFormat("call RaRdevInit failed, mode=%d", mode));
    }
    return rdmaHandle;
}

void HrtRaRdmaDeInit(RdmaHandle rdmaHandle, HrtNetworkMode netMode)
{
    unsigned int notifyType = netMode == HrtNetworkMode::PEER ? NO_USE : NOTIFY;
    s32 ret = RaRdevDeinit(rdmaHandle, notifyType);
    if (ret != 0) {
        HCCL_ERROR("[DeInit][RaRdma]errNo[0x%016llx] rt rdev deinit fail. return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
        throw NetworkApiException(StringFormat("call RaRdevDeinit failed, rdmaHandle=%p", rdmaHandle));
    }
}

void HrtRaGetNotifyBaseAddr(RdmaHandle rdmaHandle, u64 *va, u64 *size)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        unsigned long long notifyVa;
        unsigned long long notifySize;
        s32                ret = RaGetNotifyBaseAddr(rdmaHandle, &notifyVa, &notifySize);
        if (ret == 0) {
            *va   = notifyVa;
            *size = notifySize;
            break;
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout != 0) {
                HCCL_ERROR("[Get][RaNotifyBaseAddr]errNo[0x%016llx] ra get notify base addr "
                           "timeout[%d]. return[%d], params: va[0x%llx], size[%llu]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), timeout, ret, notifyVa, notifySize);
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[Get][RaNotifyBaseAddr]errNo[0x%016llx] ra get notify base addr fail. return[%d], params: "
                       "va[0x%llx], size[%llu]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret, notifySize, notifySize);
            throw NetworkApiException(StringFormat("call RaGetNotifyBaseAddr failed, rdmaHandle=%p", rdmaHandle));
        }
    }
}

QpHandle HrtRaQpCreate(RdmaHandle rdmaHandle, int flag, int qpMode)
{
    QpHandle connHandle = nullptr;

    s32 ret = RaQpCreate(rdmaHandle, flag, qpMode, &connHandle);
    if (ret != 0 || connHandle == nullptr) {
        HCCL_ERROR("[Create][RaQp]errNo[0x%016llx] ra qp create fail. "
                   "params: flag[%d], qpMode[%d]. return: ret[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), flag, qpMode, ret);
        throw NetworkApiException(
            StringFormat("call RaGetNotifyBaseAddr, rdmaHandle=%p, flag=%d, qpMode=%d", rdmaHandle, flag, qpMode));
    }
    return connHandle;
}

void HrtRaQpDestroy(QpHandle qpHandle)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        s32 ret = RaQpDestroy(qpHandle);
        if (ret == 0) {
            break;
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout != 0) {
                HCCL_ERROR("[Destroy][RaQp]errNo[0x%016llx] ra qp destroy timeout[%d]. "
                           "return[%d].",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), timeout, ret);
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[Destroy][RaQp]errNo[0x%016llx] ra qp destroy fail. return[%d].",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
            throw NetworkApiException(StringFormat("call RaQpDestroy failed, qpHandle=%p", qpHandle));
        }
    }
}

void HrtRaQpConnectAsync(QpHandle qpHandle, FdHandle fdHandle)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        s32 ret = RaQpConnectAsync(qpHandle, fdHandle);
        if (ret == 0) {
            break;
        } else if (ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout != 0) {
                HCCL_ERROR("[ConnectAsync][RaQp]errNo[0x%016llx] ra qp connect async "
                           "timeout[%lld]. return[%d].",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), timeout, ret);
            }
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        } else {
            HCCL_ERROR("[ConnectAsync][RaQp]errNo[0x%016llx] ra qp connect async fail. return[%d]",
                       HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
            throw NetworkApiException(
                StringFormat("call RaQpConnectAsync failed, qpHandle=%p, fdHandle=%p", qpHandle, fdHandle));
        }
    }
}

int HrtGetRaQpStatus(QpHandle qpHandle)
{
    int status = 0;
    s32 ret    = RaGetQpStatus(qpHandle, &status);
    if (ret != 0) {
        HCCL_ERROR("[GetStatus][RaQp]errNo[0x%016llx] ra qp get status failed. return[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
        throw NetworkApiException(StringFormat("call ra_get_status failed, qpHandle=%p", qpHandle));
    }
    return status;
}

void HrtRaMrReg(QpHandle qpHandle, RaMrInfo &info)
{
    struct MrInfoT mrInfo;
    mrInfo.addr = info.addr;
    mrInfo.size = info.size;
    mrInfo.access = info.access;
    mrInfo.lkey = info.lkey;
    HCCL_INFO("ra mr reg: addr[%p], size[%llu], access[%d]", mrInfo.addr, mrInfo.size, mrInfo.access);
    s32 ret = RaMrReg(qpHandle, &mrInfo);
    if (ret != 0) {
        HCCL_ERROR("[Reg][RaMr]errNo[0x%016llx] ra mr reg fail. return[%d], params: "
                   "addr[%p], size[%llu], access[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret, mrInfo.addr, mrInfo.size, mrInfo.access);
        throw NetworkApiException(StringFormat("call RaMrReg failed, qpHandle=%p, addr=%p, size=%llu, access=%d",
                                               qpHandle, mrInfo.addr, mrInfo.size, mrInfo.access));
    }
}

void HrtRaMrDereg(QpHandle qpHandle, RaMrInfo &info)
{
    struct MrInfoT mrInfo;
    mrInfo.addr = info.addr;
    mrInfo.size = info.size;
    mrInfo.access = info.access;
    mrInfo.lkey = info.lkey;
    HCCL_INFO("ra mr dereg: addr[%p], size[%llu], access[%d]", mrInfo.addr, mrInfo.size, mrInfo.access);
    s32 ret = RaMrDereg(qpHandle, &mrInfo);
    if (ret != 0) {
        string msg = StringFormat("call RaMrDereg failed, qpHandle=%p, addr=%p, size=%llu, access=%d", qpHandle,
                                  mrInfo.addr, mrInfo.size, mrInfo.access);
        THROW<NetworkApiException>(msg);
    }
}

static void HrtRaSendWr(QpHandle qpHandle, struct SendWr *wr, struct SendWrRsp *opRsp)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvLinkTimeoutGet());
    while (true) {
        s32 ret = RaSendWr(qpHandle, wr, opRsp);
        if (ret == 0) {
            break;
        } else if (ret == SOCK_ENOENT || ret == SOCK_EAGAIN) {
            bool bTimeout = ((std::chrono::steady_clock::now() - startTime) >= timeout);
            if (bTimeout) {
                HCCL_ERROR("[Send][RaWr]errNo[0x%016llx] ra get send async timeout[%d]. "
                           "return[%d], params: send_wrAddr[%p], opRspAddr[%p]",
                           HCCL_ERROR_CODE(HcclResult::HCCL_E_ROCE_TRANSFER), timeout, ret, wr, opRsp);
                SaluSleep(ONE_MILLISECOND_OF_USLEEP);
            }
        } else {
            string msg
                = StringFormat("call RaSendWr failed, qpHandle=%p, send_wrAddr=%p opRspAddr=%p", qpHandle, wr, opRsp);
            THROW<NetworkApiException>(msg);
        }
    }
}

RaSendWrResp HrtRaSendOneWr(QpHandle qpHandle, HRaSendWr &in)
{
    struct SgList bufList {};
    bufList.addr = in.locAddr;
    bufList.len  = in.len;

    struct SendWr wr;
    wr.op        = in.op;
    wr.dstAddr   = in.rmtAddr;
    wr.sendFlag = in.sendFlag;
    wr.bufNum   = 1; // 此处list只有一个，设置为1
    wr.bufList  = &bufList;
    struct SendWrRsp opRsp;
    HrtRaSendWr(qpHandle, &wr, &opRsp);

    return RaSendWrResp(opRsp.wqeTmp.sqIndex, opRsp.wqeTmp.wqeIndex, opRsp.db.dbIndex, opRsp.db.dbInfo);
}

string HrtRaGetKeyDescribe(const u8 *key, u32 len)
{
    string desc = "0x";
    for (u32 idx = 0; idx < len; idx++) {
        desc += StringFormat("%02x", key[idx]);
    }
    return desc;
}

RdmaHandle HrtRaUbCtxInit(const HrtRaUbCtxInitParam &in)
{
    struct ctx_init_cfg initCfg {};
    initCfg.rdma.disabled_lite_thread = false;
    initCfg.mode                 = HRT_NETWORK_MODE_MAP.at(in.mode);

    struct ctx_init_attr ctxInfo {};
    ctxInfo.phy_id       = in.phyId;
    ctxInfo.ub.eid_index = 0;

    HCCL_INFO("[HrtRaUbCtxInit] use eid[%s]", in.addr.Describe().c_str());
    s32 sRet = memcpy_s(ctxInfo.ub.eid.raw, sizeof(ctxInfo.ub.eid.raw), in.addr.GetEid().raw, sizeof(in.addr.GetEid().raw));
    if (sRet != EOK) {
        THROW<InternalException>("[HrtRaUbCtxInit]memcpy_s failed");
    }

    RdmaHandle handle;
    s32        ret = ra_ctx_init(&initCfg, &ctxInfo, &handle);
    if (ret != 0) {
        string msg = StringFormat(
            "[Init][RaUbCtx]errNo[0x%016llx] ub ctx init fail, mode[%d], phyId[%u], addr[%s], ret[%d]",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), in.mode, in.phyId, in.addr.GetIpStr().c_str(), ret);
        THROW<NetworkApiException>(msg);
    }
    return handle;
}

void HrtRaUbCtxDestroy(RdmaHandle handle)
{
    HCCL_INFO("[HrtRaUbCtxDestroy] rdmaHandle[%llu].", handle);
    s32 ret = ra_ctx_deinit(handle);
    if (ret != 0) {
        string msg = StringFormat("[DeInit][RaRdma]errNo[0x%016llx] rt ctx deinit fail. return[%d].",
                                  HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), ret);
        THROW<NetworkApiException>(msg);
    }
}

std::pair<TokenIdHandle, uint32_t> RaUbAllocTokenIdHandle(RdmaHandle handle)
{
    struct hccp_token_id out {};
    void *tokenIdHandle = nullptr;
    s32 ret = ra_ctx_token_id_alloc(handle, &out, &tokenIdHandle);
    if (ret != 0) {
        string msg = StringFormat("%s failed, set=%d, rdmaHandle=%p", __func__, ret, handle);
        THROW<NetworkApiException>(msg);
    }
    HCCL_INFO("[RaUbAllocTokenIdHandle] tokenIdHandle[%p] rdmaHandle[%p]", tokenIdHandle, handle);
    return {reinterpret_cast<TokenIdHandle>(tokenIdHandle), out.token_id >> URMA_TOKEN_ID_RIGHT_SHIFT};
}

void RaUbFreeTokenIdHandle(RdmaHandle handle, TokenIdHandle tokenIdHandle)
{
    HCCL_INFO("[RaUbFreeTokenIdHandle] rdmaHandle[%p] tokenIdHandle[0x%llx].", handle, tokenIdHandle);
    s32 ret = ra_ctx_token_id_free(handle, reinterpret_cast<void *>(tokenIdHandle));
    if (ret != 0) {
        string msg = StringFormat("%s failed, set=%d, rdmaHandle=%p, tokenIdHandle=0x%llx.",
                                 __func__, ret, handle, tokenIdHandle);
        THROW<NetworkApiException>(msg);
    }
}

constexpr u64 UB_MEM_PAGE_SIZE = 4096;

std::pair<u64, u64> BufAlign(u64 addr, u64 size)
{
    // 待解决: 正式方案待讨论
    u64 pageSize = UB_MEM_PAGE_SIZE;
    u64 newAddr  = addr & (~(static_cast<u64>(pageSize - 1))); // UB内存注册要求起始地址4k对齐
    u64 offset   = addr - newAddr;
    u64 newSize  = size + offset;
    HCCL_INFO("UB mem info: newAddr[%llx] newSize[%llu]", newAddr, newSize);

    return std::make_pair(newAddr, newSize);
}

HrtRaUbLocalMemRegOutParam HrtRaUbLocalMemReg(RdmaHandle handle, const HrtRaUbLocMemRegParam &in)
{
    struct mr_reg_info_t info {};
    info.in.mem.addr                   = in.addr;
    info.in.mem.size                   = in.size;

    info.in.ub.flags.value             = 0;
    info.in.ub.flags.bs.token_policy   = TOKEN_POLICY_PLAIN_TEXT;
    info.in.ub.flags.bs.token_id_valid = 1;
    info.in.ub.flags.bs.access = MEM_SEG_ACCESS_READ | MEM_SEG_ACCESS_WRITE
                                 | MEM_SEG_ACCESS_ATOMIC;
    info.in.ub.flags.bs.non_pin = in.nonPin;
    info.in.ub.token_value      = in.tokenValue;
    info.in.ub.token_id_handle  = reinterpret_cast<void *>(in.tokenIdHandle);

    void *lmemHandle = nullptr;
    s32   ret        = ra_ctx_lmem_register(handle, &info, &lmemHandle);
    if (ret != 0) {
        string msg = StringFormat("localMemReg failed, addr=0x%llx, size=0x%llx", in.addr, in.size);
        THROW<NetworkApiException>(msg);
    }

    HrtRaUbLocalMemRegOutParam out;
    s32 sRet = memcpy_s(out.key, sizeof(out.key), info.out.key.value, info.out.key.size);
    if (sRet != EOK) {
        THROW<InternalException>("[HrtRaUbLocalMemReg]memcpy_s failed");
    }

    HCCL_INFO("[HrtRaUbLocalMemReg]UbLocalMemReg key.size=%u", info.out.key.size);
    out.keySize     = info.out.key.size;
    out.handle      = reinterpret_cast<LocMemHandle>(lmemHandle);
    out.targetSegVa = info.out.ub.target_seg_handle;
    info.in.ub.token_value = 0;
    HCCL_INFO("[HrtRaUbLocalMemReg]UB mem reg info: in.addr[%llx] in.size[%llu] out.targetSegVa[%llx]",
              in.addr, in.size, out.targetSegVa);
    return out;
}

void HrtRaUbLocalMemUnreg(RdmaHandle rdmaHandle, LocMemHandle lmemHandle)
{
    s32 ret = ra_ctx_lmem_unregister(rdmaHandle, reinterpret_cast<void *>(lmemHandle));
    if (ret != 0) {
        string msg = StringFormat("localMemUnreg failed, rdmaHandle=%p, lmemHandle=0x%llx", rdmaHandle, lmemHandle);
        THROW<NetworkApiException>(msg);
    }
}

HrtRaUbRemMemImportedOutParam HrtRaUbRemoteMemImport(RdmaHandle handle, u8 *key, u32 keyLen, u32 tokenValue)
{
    struct mr_import_info_t info {};
    int res = memcpy_s(info.in.key.value, sizeof(info.in.key.value), key, keyLen);
    if (res != 0) {
        THROW<InternalException>(StringFormat("[%s] memcpy_s failed, ret = %d", __func__, res));
    }
    info.in.key.size = keyLen;

    info.in.ub.token_value     = tokenValue;
    info.in.ub.mapping_addr    = 0;
    info.in.ub.flags.value     = 0;
    info.in.ub.flags.bs.access = MEM_SEG_ACCESS_READ | MEM_SEG_ACCESS_WRITE
                                 | MEM_SEG_ACCESS_ATOMIC;

    void *rmemHandle = nullptr;
    s32   ret        = ra_ctx_rmem_import(handle, &info, &rmemHandle);
    if (ret != 0) {
        string msg = StringFormat("ubRemoteMemImport failed!");
        THROW<NetworkApiException>(msg);
    }

    HrtRaUbRemMemImportedOutParam out;
    out.handle      = reinterpret_cast<LocMemHandle>(rmemHandle);
    out.targetSegVa = info.out.ub.target_seg_handle;
    info.in.ub.token_value = 0;
    return out;
}
void HrtRaUbRemoteMemUnimport(RdmaHandle rdmaHandle, RemMemHandle rmemHandle)
{
    s32 ret = ra_ctx_rmem_unimport(rdmaHandle, reinterpret_cast<void *>(rmemHandle));
    if (ret != 0) {
        string msg
            = StringFormat("ubRemoteMemUnimport failed, rdmaHandle=%p, rmemHandle=0x%llx", rdmaHandle, rmemHandle);
        THROW<NetworkApiException>(msg);
    }
}

const std::map<HrtUbJfcMode, jfc_mode> HRT_UB_JFC_MODE_MAP = {{HrtUbJfcMode::NORMAL, jfc_mode::JFC_MODE_NORMAL},
                                                              {HrtUbJfcMode::STARS_POLL, jfc_mode::JFC_MODE_STARS_POLL},
                                                              {HrtUbJfcMode::CCU_POLL, jfc_mode::JFC_MODE_CCU_POLL}};

constexpr u32 CQ_DEPTH     = 1024 * 1024 / 64;
constexpr u32 CCU_CQ_DEPTH = 64;

JfcHandle HrtRaUbCreateJfc(RdmaHandle handle, HrtUbJfcMode mode)
{
    struct cq_info_t info {};

    info.in.chan_handle = nullptr;
    if (mode == HrtUbJfcMode::CCU_POLL) {
        info.in.depth = CCU_CQ_DEPTH;
    } else {
        info.in.depth = CQ_DEPTH;
    }
    info.in.ub.user_ctx   = 0;
    info.in.ub.mode       = HRT_UB_JFC_MODE_MAP.at(mode);
    info.in.ub.ceqn       = 0;
    info.in.ub.flag.value = 0;

    void *jfcHandle = nullptr;

    s32 ret = ra_ctx_cq_create(handle, &info, &jfcHandle);
    if (ret != 0) {
        string msg = StringFormat("ubCreateCq failed, rdmaHandle=%p,", handle);
        THROW<NetworkApiException>(msg);
    }

    return reinterpret_cast<JfcHandle>(jfcHandle);
}

void HrtRaUbDestroyJfc(RdmaHandle handle, JfcHandle jfcHandle)
{
    s32 ret = ra_ctx_cq_destroy(handle, reinterpret_cast<void *>(jfcHandle));
    if (ret != 0) {
        string msg = StringFormat("ubCqDestroy failed, rdmaHandle=%p, jfcHandle=0x%llx", handle, jfcHandle);
        THROW<NetworkApiException>(msg);
    }
}

const std::map<HrtTransportMode, transport_mode_t> HRT_TRANSPORT_MODE_MAP
    = {{HrtTransportMode::RC, transport_mode_t::CONN_RC}, {HrtTransportMode::RM, transport_mode_t::CONN_RM}};

const std::map<HrtJettyMode, jetty_mode> HRT_JETTY_MODE_MAP
    = {{HrtJettyMode::STANDARD, jetty_mode::JETTY_MODE_URMA_NORMAL},
       {HrtJettyMode::HOST_OFFLOAD, jetty_mode::JETTY_MODE_USER_CTL_NORMAL},
       {HrtJettyMode::HOST_OPBASE, jetty_mode::JETTY_MODE_USER_CTL_NORMAL},
       {HrtJettyMode::DEV_USED, jetty_mode::JETTY_MODE_USER_CTL_NORMAL},
       {HrtJettyMode::CACHE_LOCK_DWQE, jetty_mode::JETTY_MODE_CACHE_LOCK_DWQE},
       {HrtJettyMode::CCU_CCUM_CACHE, jetty_mode::JETTY_MODE_CCU}};

constexpr u8  RNR_RETRY = 7;
constexpr u32 RQ_DEPTH  = 256;

static struct qp_create_attr GetQpCreateAttr(const HrtRaUbCreateJettyParam &in)
{
    struct qp_create_attr attr {};
    attr.scq_handle     = reinterpret_cast<void *>(in.sjfcHandle);
    attr.rcq_handle     = reinterpret_cast<void *>(in.rjfcHandle);
    attr.srq_handle     = reinterpret_cast<void *>(in.sjfcHandle);
    attr.rq_depth       = RQ_DEPTH;
    attr.sq_depth       = in.sqDepth;
    attr.transport_mode = HRT_TRANSPORT_MODE_MAP.at(in.transMode);
    attr.ub.mode        = HRT_JETTY_MODE_MAP.at(in.jettyMode);

    attr.ub.token_value       = in.tokenValue;
    attr.ub.token_id_handle   = reinterpret_cast<void *>(in.tokenIdHandle);
    attr.ub.flag.value        = 0;
    attr.ub.err_timeout       = 0;
    // CTP默认优先级使用2, TP/UBG等模式后续QoS特性统一适配
    attr.ub.priority          = 2;
    attr.ub.rnr_retry         = RNR_RETRY;
    attr.ub.flag.bs.share_jfr = 1;
    attr.ub.jetty_id          = in.jettyId;
    // 在continue模式下+配置了wqe的fence标记，并且远端有一些权限校验错误/内存异常错误，硬件会直接挂死
    // jfs_flag 的 error_suspend 设置为 1，
    attr.ub.jfs_flag.bs.error_suspend = 1;

    attr.ub.ext_mode.sqebb_num = in.sqDepth;
    if (in.jettyMode == HrtJettyMode::HOST_OFFLOAD) {
        attr.ub.ext_mode.pi_type = 1;
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 0; // 表示不指定Va，由HCCP返回Va
    } else if (in.jettyMode == HrtJettyMode::CCU_CCUM_CACHE) {
        attr.ub.token_value                   = in.tokenValue;
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 1;
        attr.ub.ext_mode.sq.buff_size         = in.sqBufSize;
        attr.ub.ext_mode.sq.buff_va           = in.sqBufVa;
    } else if (in.jettyMode == HrtJettyMode::DEV_USED ||
                in.jettyMode == HrtJettyMode::CACHE_LOCK_DWQE) {
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 0; // 表示不指定Va，由HCCP返回Va
        attr.ub.ext_mode.sq.buff_size         = in.sqBufSize;
        attr.ub.ext_mode.sq.buff_va           = in.sqBufVa;
    } // 预埋HrtJettyMode::CACHE_LOCK_DWQE类型，当前流程暂未使用

    // 其他Mode暂时不需要额外更新特定字段
    HCCL_INFO("Create jetty, input params: attr.ub.jetty_id[%u], attr.rq_depth[%u], "
              "attr.sq_depth[%u], attr.transport_mode[%d], attr.ub.mode[%d], "
              "attr.ub.ext_mode.sqebb_num[%u], attr.ub.ext_mode.sq.buff_va[%llx], "
              "attr.ub.ext_mode.sq.buff_size[%u], attr.ub.ext_mode.pi_type[%u], attr.ub.priority[%u].",
               attr.ub.jetty_id, attr.rq_depth, attr.sq_depth, attr.transport_mode, attr.ub.mode,
               attr.ub.ext_mode.sqebb_num, attr.ub.ext_mode.sq.buff_va, attr.ub.ext_mode.sq.buff_size,
               attr.ub.ext_mode.pi_type, attr.ub.priority);
    return attr;
}

HrtRaUbJettyCreatedOutParam HrtRaUbCreateJetty(RdmaHandle handle, const HrtRaUbCreateJettyParam &in)
{
    struct qp_create_attr attr = GetQpCreateAttr(in);

    struct qp_create_info info {};
    void *qpHandle = nullptr;
    s32   ret      = ra_ctx_qp_create(handle, &attr, &info, &qpHandle);
    if (ret != 0) {
        string msg = StringFormat("ubCreateJetty failed, rdmaHandle=%p,", handle);
        THROW<NetworkApiException>(msg);
    }

    HrtRaUbJettyCreatedOutParam out;
    out.handle    = reinterpret_cast<JettyHandle>(qpHandle);
    out.id        = info.ub.id;
    out.uasid     = info.ub.uasid;
    out.jettyVa   = info.va;
    out.dbVa      = info.ub.db_addr;
    out.dbTokenId = info.ub.db_token_id >> URMA_TOKEN_ID_RIGHT_SHIFT;
    out.sqBuffVa  = info.ub.sq_buff_va; // 适配HCCP修改，jettybufva由HCCP提供，不再由HCCL分配

    s32 sRet = memcpy_s(out.key, sizeof(out.key), info.key.value, info.key.size);
    if (sRet != EOK) {
        THROW<InternalException>("HrtRaUbCreateJetty memcpy_s failed");
    }
    out.keySize = info.key.size;
    attr.ub.token_value = 0;
    HCCL_INFO("Create jetty success, output params: out.id[%u], out.dbVa[%llx]", out.id, out.dbVa);

    return out;
}

void HrtRaUbDestroyJetty(JettyHandle jettyHandle)
{
    s32 ret = ra_ctx_qp_destroy(reinterpret_cast<void *>(jettyHandle));
    if (ret != 0) {
        string msg = StringFormat("ubDestroyJetty failed, jettyHandle=0x%llx", jettyHandle);
        THROW<NetworkApiException>(msg);
    }
}

static HrtRaUbJettyImportedOutParam ImportJetty(RdmaHandle handle, u8 *key, u32 keyLen,
    u32 tokenValue, jetty_import_exp_cfg cfg, jetty_import_mode mode, TpProtocol protocol = TpProtocol::INVALID)
{
    if (mode == jetty_import_mode::JETTY_IMPORT_MODE_NORMAL) {
        THROW<NotSupportException>("[%s] currently not support JETTY_IMPORT_MODE_NORMAL.",
            __func__);
    }

    struct qp_import_info_t info {};

    int res = memcpy_s(info.in.key.value, sizeof(info.in.key.value), key, keyLen);
    if (res != 0) {
        THROW<InternalException>("[%s] memcpy_s failed, ret = %d", __func__, res);
    }
    info.in.key.size = keyLen;

    info.in.ub.mode = mode;
    info.in.ub.token_value = tokenValue;
    info.in.ub.policy = jetty_grp_policy::JETTY_GRP_POLICY_RR;
    info.in.ub.type = target_type::TARGET_TYPE_JETTY;

    info.in.ub.flag.value = 0;
    info.in.ub.flag.bs.token_policy = TOKEN_POLICY_PLAIN_TEXT;

    info.in.ub.exp_import_cfg = cfg;

    if (protocol != TpProtocol::TP && protocol != TpProtocol::CTP) {
        THROW<NetworkApiException>("[%s] failed, tp protocol[%s] is not expected.",
        __func__, protocol.Describe().c_str());
    }
    // tp_type: 0->RTP, 1->CTP
    info.in.ub.tp_type = protocol == TpProtocol::TP ? 0 : 1;

    void *remQpHandle = nullptr;
    s32   ret         = ra_ctx_qp_import(handle, &info, &remQpHandle);
    if (ret != 0) {
        string msg = StringFormat("UbImportJetty failed, rdmaHandle=%p,", handle);
        THROW<NetworkApiException>(msg);
    }

    HrtRaUbJettyImportedOutParam out;
    out.handle        = reinterpret_cast<TargetJettyHandle>(remQpHandle);
    out.targetJettyVa = info.out.ub.tjetty_handle;
    out.tpn           = info.out.ub.tpn;
    info.in.ub.token_value = 0;
    return out;
}

static struct jetty_import_exp_cfg GetTpImportCfg(const JettyImportCfg &jettyImportCfg)
{
    struct jetty_import_exp_cfg cfg = {};

    cfg.tp_handle = jettyImportCfg.localTpHandle;
    cfg.peer_tp_handle = jettyImportCfg.remoteTpHandle;
    cfg.tag = jettyImportCfg.localTag;
    cfg.tx_psn = jettyImportCfg.localPsn;
    cfg.rx_psn = jettyImportCfg.remotePsn;

    return cfg;
}

HrtRaUbJettyImportedOutParam RaUbImportJetty(RdmaHandle handle, u8 *key, u32 keyLen, u32 tokenValue)
{
    // 该接口仅适配非管控面模式，当前不期望使用
    struct jetty_import_exp_cfg cfg = {};
    const auto mode = jetty_import_mode::JETTY_IMPORT_MODE_NORMAL;
    return ImportJetty(handle, key, keyLen, tokenValue, cfg, mode);
}

HrtRaUbJettyImportedOutParam RaUbTpImportJetty(RdmaHandle handle, u8 *key, u32 keyLen,
    u32 tokenValue, const JettyImportCfg &jettyImportCfg)
{
    struct jetty_import_exp_cfg cfg = GetTpImportCfg(jettyImportCfg);
    const auto mode = jetty_import_mode::JETTY_IMPORT_MODE_EXP;
    return ImportJetty(handle, key, keyLen, tokenValue, cfg, mode, jettyImportCfg.protocol);
}

void HrtRaUbUnimportJetty(RdmaHandle handle, TargetJettyHandle targetJettyHandle)
{
    s32 ret = ra_ctx_qp_unimport(reinterpret_cast<void *>(handle), reinterpret_cast<void *>(targetJettyHandle));
    if (ret != 0) {
        string msg
            = StringFormat("ubCqDestroy failed, rdmaHandle=%p, targetJettyHandle=0x%llx", handle, targetJettyHandle);
        THROW<NetworkApiException>(msg);
    }
}

void HrtRaUbJettyBind(JettyHandle jettyHandle, TargetJettyHandle targetJettyHandle)
{
    s32 ret = ra_ctx_qp_bind(reinterpret_cast<void *>(jettyHandle), reinterpret_cast<void *>(targetJettyHandle));
    if (ret != 0) {
        string msg = StringFormat("ubJettyBind failed, jettyHandle=0x%llx, targetJettyHandle=0x%llx", jettyHandle,
                                  targetJettyHandle);
        THROW<NetworkApiException>(msg);
    }
}

void HrtRaUbJettyUnbind(JettyHandle jettyHandle)
{
    s32 ret = ra_ctx_qp_unbind(reinterpret_cast<void *>(jettyHandle));
    if (ret != 0) {
        string msg = StringFormat("ubJettyUbbind failed, jettyHandle=0x%llx", jettyHandle);
        THROW<NetworkApiException>(msg);
    }
}

const std::map<HrtUbSendWrOpCode, ra_ub_opcode> HRT_UB_SEND_WR_OP_CODE_MAP
    = {{HrtUbSendWrOpCode::WRITE, ra_ub_opcode::RA_UB_OPC_WRITE},
       {HrtUbSendWrOpCode::WRITE_WITH_NOTIFY, ra_ub_opcode::RA_UB_OPC_WRITE_NOTIFY},
       {HrtUbSendWrOpCode::READ, ra_ub_opcode::RA_UB_OPC_READ},
       {HrtUbSendWrOpCode::NOP, ra_ub_opcode::RA_UB_OPC_NOP}};

const std::map<ReduceOp, u8> HRT_UB_REDUCE_OP_CODE_MAP
    = {{ReduceOp::SUM, 0xA}, {ReduceOp::MAX, 0x8}, {ReduceOp::MIN, 0x9}};

const std::map<DataType, u8> HRT_UB_REDUCE_DATA_TYPE_MAP
    = {{DataType::INT8, 0x0},   {DataType::INT16, 0x1},   {DataType::INT32, 0x2}, {DataType::UINT8, 0x3},
       {DataType::UINT16, 0x4}, {DataType::UINT32, 0x5},  {DataType::FP16, 0x6},  {DataType::FP32, 0x7},
       {DataType::BFP16, 0x8},  {DataType::BF16_SAT, 0x9}};

static void ConstructWrSge(HrtRaUbSendWrReqParam &in, struct wr_sge_list &sge)
{
    sge.addr        = in.localAddr;
    sge.len         = in.size;
    sge.lmem_handle = reinterpret_cast<void *>(in.lmemHandle);
}

static void ConstructSendWrReq(HrtRaUbSendWrReqParam &in, struct wr_sge_list &sge, struct send_wr_data &sendWr)
{
    // 看一下hccp测试用例的入参
    sendWr.num_sge                      = 1;
    sendWr.sges                         = &sge;
    sendWr.remote_addr                  = in.remoteAddr;
    sendWr.rmem_handle                  = reinterpret_cast<void *>(in.rmemHandle);
    sendWr.ub.user_ctx                  = 0;
    sendWr.ub.opcode                    = HRT_UB_SEND_WR_OP_CODE_MAP.at(in.opcode);
    sendWr.ub.flags.value               = 0;
    sendWr.ub.flags.bs.comp_order       = 1;
    sendWr.ub.flags.bs.complete_enable  = in.cqeEn;
    sendWr.ub.flags.bs.fence            = 1;
    sendWr.ub.flags.bs.solicited_enable = 1;
    sendWr.ub.rem_qp_handle             = reinterpret_cast<void *>(in.handle);
    sendWr.ub.flags.bs.inline_flag      = in.inlineFlag;
    if (sendWr.ub.flags.bs.inline_flag) {
        sendWr.inline_data = in.inlineData;
        sendWr.inline_size = in.size;
    }
    sendWr.ub.reduce_info.reduce_en = in.inlineReduceFlag;
    if (sendWr.ub.reduce_info.reduce_en) {
        sendWr.ub.reduce_info.reduce_opcode    = HRT_UB_REDUCE_OP_CODE_MAP.at(in.reduceOp);
        sendWr.ub.reduce_info.reduce_data_type = HRT_UB_REDUCE_DATA_TYPE_MAP.at(in.dataType);
    }
    if (sendWr.ub.opcode == ra_ub_opcode::RA_UB_OPC_WRITE_NOTIFY) {
        sendWr.ub.notify_info.notify_data   = in.notifyData;
        sendWr.ub.notify_info.notify_addr   = in.notifyAddr;
        sendWr.ub.notify_info.notify_handle = reinterpret_cast<void *>(in.notifyHandle);
    }
}

HrtRaUbSendWrRespParam HrtRaUbPostSend(JettyHandle jettyHandle, HrtRaUbSendWrReqParam &in)
{
    struct wr_sge_list sge;
    struct send_wr_data sendWr {};

    ConstructWrSge(in, sge);
    ConstructSendWrReq(in, sge, sendWr);

    HCCL_INFO("Sge addr = 0x%llx", in.localAddr);
    HCCL_INFO("SendWR lmemHandle = 0x%llx", in.lmemHandle); // 和notifyFixedValue能否对齐
    HCCL_INFO("SendWR rmemHandle = 0x%llx", in.rmemHandle); // remote
    HCCL_INFO("SendWR remote addr = 0x%llx", in.remoteAddr);
    HCCL_INFO("SendWR remote qp handle = 0x%llx", in.handle);
    HCCL_INFO("SendWR jetty handle = 0x%llx", jettyHandle);

    send_wr_resp sendWrResp{};

    u32 compNum = 0;
    s32 ret     = ra_batch_send_wr(reinterpret_cast<void *>(jettyHandle), &sendWr, &sendWrResp, 1, &compNum);
    if (ret != 0) {
        string msg = StringFormat("UbJettySendWr failed, jettyHandle=0x%llx,", jettyHandle);
        THROW<NetworkApiException>(msg);
    }
    HrtRaUbSendWrRespParam out;
    out.dieId    = sendWrResp.doorbell_info.dieId;
    out.funcId   = sendWrResp.doorbell_info.funcId;
    out.jettyId  = sendWrResp.doorbell_info.jettyId;
    out.piVal    = sendWrResp.doorbell_info.piVal;
    out.dwqeSize = sendWrResp.doorbell_info.dwqe_size;
    ret          = memcpy_s(out.dwqe, sizeof(out.dwqe), sendWrResp.doorbell_info.dwqe, out.dwqeSize);
    if (ret != 0) {
        string msg = StringFormat("HrtRaUbPostSend copy dwqe failed, ret=%d", ret);
        THROW<InternalException>(msg);
    }

    return out;
}

std::pair<uint32_t, uint32_t> HraGetDieAndFuncId(RdmaHandle handle)
{
    struct dev_base_attr out {};
    auto                   ret = ra_get_dev_base_attr(handle, &out);
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat("call ra_get_dev_base_attr failed, error code =%d.", ret));
    }
    return std::make_pair(out.ub.die_id, out.ub.func_id);
}

void HrtRaCustomChannel(const HRaInfo &raInfo, void *customIn, void *customOut)
{
    struct RaInfo info {};
    info.mode   = HRT_NETWORK_MODE_MAP.at(raInfo.mode);
    info.phyId = raInfo.phyId;

    struct custom_chan_info_in  *in  = reinterpret_cast<struct custom_chan_info_in *>(customIn);
    struct custom_chan_info_out *out = reinterpret_cast<struct custom_chan_info_out *>(customOut);

    int ret = ra_custom_channel(info, in, out);
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat("call ra_custom_channel failed, error code =%d.", ret));
    }
}

void HrtRaUbPostNops(JettyHandle jettyHandle, JettyHandle remoteJettyHandle, const u32 numNop)
{
    HCCL_INFO("HrtRaUbPostNops: numNop[%u]", numNop);
    struct send_wr_data sendWrList[numNop] = {};
    for (auto &sendWr : sendWrList) {
        sendWr.ub.opcode = HRT_UB_SEND_WR_OP_CODE_MAP.at(HrtUbSendWrOpCode::NOP);
        HCCL_INFO("SendWR opcode = %u", static_cast<u32>(sendWr.ub.opcode));
    }
    sendWrList[numNop - 1].ub.flags.bs.complete_enable = 1;

    send_wr_resp sendWrRespList[numNop] = {};
    u32          compNum                = 0;
    s32 ret = ra_batch_send_wr(reinterpret_cast<void *>(jettyHandle), sendWrList, sendWrRespList, numNop, &compNum);
    if (ret != 0) {
        string msg = StringFormat("UbJettySendWr failed, jettyHandle=0x%llx,", jettyHandle);
        THROW<NetworkApiException>(msg);
    }
}

void RaUbUpdateCi(JettyHandle jettyHandle, u32 ci)
{
    HCCL_INFO("RaUbUpdateCi: jettyHandle=0x%llx, ci=%u", jettyHandle, ci);
    s32 ret = ra_ctx_update_ci(reinterpret_cast<void *>(jettyHandle), ci);
    if (ret != 0) {
        string msg = StringFormat("UbUpdateCi failed, ret=%d, jettyHandle=0x%llx, ci=%u", ret, jettyHandle, ci);
        THROW<NetworkApiException>(msg);
    }
}

inline string HccpEidDesc(union hccp_eid& hccpEid)
{
    return StringFormat("hccp_eid[%016llx:%016llx]",
                        static_cast<unsigned long long>(be64toh(hccpEid.in6.subnet_prefix)),
                        static_cast<unsigned long long>(be64toh(hccpEid.in6.interface_id)));
}

inline IpAddress HccpEidToIpAddress(union hccp_eid& hccpEid)
{
    Eid eid{};
    HCCL_INFO("[HccpEidToIpAddress] %s", HccpEidDesc(hccpEid).c_str());
    s32 sRet = memcpy_s(eid.raw, sizeof(eid.raw), hccpEid.raw, sizeof(hccpEid.raw));
    if (sRet != EOK) {
        THROW<InternalException>("[HccpEidToIpAddress]memcpy_s failed");
    }
    return IpAddress(eid);
}

std::vector<HrtDevEidInfo> HrtRaGetDevEidInfoList(const HRaInfo &raInfo)
{
    std::vector<HrtDevEidInfo> hrtDevEidInfo;
    struct RaInfo info {};
    u32 num = 0;

    info.mode = HRT_NETWORK_MODE_MAP.at(raInfo.mode);
    info.phyId = raInfo.phyId;

    s32 ret = ra_get_dev_eid_info_num(info, &num);
    if (ret != 0) {
        string msg = StringFormat("call ra_get_dev_eid_info_num failed, error code =%d.", ret);
        THROW<NetworkApiException>(msg);
    }

    struct dev_eid_info infoList[num] = {};
    ret = ra_get_dev_eid_info_list(info, infoList, &num);
    if (ret != 0) {
        string msg = StringFormat("call ra_get_dev_eid_info_list failed, error code =%d.", ret);
        THROW<NetworkApiException>(msg);
    }

    hrtDevEidInfo.resize(num);
    for (u32 i = 0; i < num; i++) {
        hrtDevEidInfo[i].name = (infoList[i].name);
        hrtDevEidInfo[i].ipAddress = HccpEidToIpAddress(infoList[i].eid);
        hrtDevEidInfo[i].type = infoList[i].type;
        hrtDevEidInfo[i].eidIndex = infoList[i].eid_index;
        hrtDevEidInfo[i].dieId = infoList[i].die_id;
        hrtDevEidInfo[i].chipId = infoList[i].chip_id;
        hrtDevEidInfo[i].funcId = infoList[i].func_id;
    }

    return hrtDevEidInfo;
}

ReqHandleResult HrtRaGetAsyncReqResult(RequestHandle &reqHandle)
{
    if (reqHandle == 0) {
        HCCL_ERROR("[%s] failed, reqHandle is 0.", __func__);
        return ReqHandleResult::INVALID_PARA;
    }

    int reqResult = 0;
    s32 ret = RaGetAsyncReqResult(reinterpret_cast<void *>(reqHandle), &reqResult);
    // 返回 OTHERS_EAGAIN 代表查询到异步任务未完成，需要重新查询，此时保留handle
    if (ret == OTHERS_EAGAIN) {
        return ReqHandleResult::NOT_COMPLETED;
    }

    // 返回码非0代表调用查询接口失败，当前仅入参错误时触发
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, call interface error[%d], "
            "reqhandle[%llu].", __func__, ret, reqHandle));
    }

    RequestHandle tmpReqHandle = reqHandle;
    reqHandle = 0;
    // 返回码为 0 时，reqResult为异步任务完成结果，0代表成功，其他值代表失败
    // SOCK_EAGAIN 为 socket 类执行结果，代表 socket 接口失败需要重试
    if (reqResult == SOCK_EAGAIN) {
        return ReqHandleResult::SOCK_E_AGAIN;
    }

    if (reqResult != 0) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, the asynchronous request "
            "error[%d], reqhandle[%llu].", __func__, reqResult, tmpReqHandle));
    }

    return ReqHandleResult::COMPLETED;
}

RequestHandle RaSocketConnectOneAsync(RaSocketConnectParam &in)
{
    struct SocketConnectInfoT connInfo {};
    connInfo.socketHandle = in.socketHandle;
    connInfo.remoteIp     = IpAddressToHccpIpAddr(in.remoteIp);
    connInfo.port          = in.port;

    int sret = strcpy_s(connInfo.tag, sizeof(connInfo.tag), in.tag.c_str());
    if (sret != 0) {
        THROW<NetworkApiException>(StringFormat(
            "[%s] copy tag[%s] to hccp tag failed, ret=%d",
            __func__, in.tag.c_str(), sret));
    }

    HCCL_INFO("Socket Connect tag=[%s], remoteIp[%s]", connInfo.tag, in.remoteIp.Describe().c_str());
    void *raReqHandle = nullptr;
    int ret = RaSocketBatchConnectAsync(&connInfo, SOCKET_NUM_ONE, &raReqHandle);
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat(
            "[BatchConnect][RaSocket]errNo[0x%016llx] ra socket batch connect fail. return[%d], params: ",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret));
    }

    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaSocketCloseOneAsync(RaSocketCloseParam &in)
{
    struct SocketCloseInfoT closeInfo;
    closeInfo.fdHandle     = in.fdHandle;
    closeInfo.socketHandle = in.socketHandle;

    void *raReqHandle = nullptr;
    int ret = RaSocketBatchCloseAsync(&closeInfo, SOCKET_NUM_ONE, &raReqHandle);
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat(
            "[BatchClose][RaSocket]errNo[0x%016llx] ra socket batch close fail. return[%d]",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret));
    }

    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaSocketListenOneStartAsync(RaSocketListenParam &in)
{
    struct SocketListenInfoT listenInfo {};
    listenInfo.socketHandle = in.socketHandle;
    listenInfo.port = in.port;

    void *raReqHandle = nullptr;
    int ret = RaSocketListenStartAsync(&listenInfo, SOCKET_NUM_ONE, &raReqHandle);
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat(
            "errNo[0x%016llx] ra socket listen start fail. return[%d]",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret));
    }

    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaSocketListenOneStopAsync(RaSocketListenParam &in)
{
    struct SocketListenInfoT listenInfo {};
    listenInfo.socketHandle = in.socketHandle;
    listenInfo.port = in.port;

    void *raReqHandle = nullptr;
    int ret = RaSocketListenStopAsync(&listenInfo, SOCKET_NUM_ONE, &raReqHandle);
    if (ret != 0) {
        THROW<NetworkApiException>(StringFormat(
            "[ListenStop][RaSocket]errNo[0x%016llx] ra socket listen stop fail. return[%d]",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_TCP_CONNECT), ret));
    }

    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RaSocketFdHandleParam RaGetOneSocket(u32 role, RaSocketGetParam &param)
{
    struct SocketInfoT socketInfo {};

    socketInfo.socketHandle = param.socketHandle;
    socketInfo.fdHandle     = param.fdHandle;
    socketInfo.remoteIp     = IpAddressToHccpIpAddr(param.remoteIp);
    socketInfo.status        = SOCKET_NOT_CONNECTED;

    int sret = strcpy_s(socketInfo.tag, sizeof(socketInfo.tag), param.tag.c_str());
    if (sret != 0) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, copy tag[%s] to hccp failed, ret=%d",
            __func__, param.tag.c_str(), sret));
    }
    
    u32 connectedNum = 0;
    s32 sockRet = RaGetSockets(role, &socketInfo, SOCKET_NUM_ONE, &connectedNum);
    if ((connectedNum == 0 && sockRet == 0) || sockRet == SOCK_EAGAIN) {
        // 更新为 connecting 状态，表示连接未完成
        socketInfo.status = SOCKET_CONNECTING;
        return RaSocketFdHandleParam(socketInfo.fdHandle, socketInfo.status);
    }

    if (sockRet != 0) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, call interface error[%d], "
            "role[%u] num[%u] connectednum[%u]", __func__, sockRet, role, SOCKET_NUM_ONE, connectedNum));
    }

    if (connectedNum > SOCKET_NUM_ONE) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, connetedNum[%u] is more "
            "than expected[%u], role[%u] num[%u] connectednum[%u]", __func__, connectedNum,
            SOCKET_NUM_ONE, sockRet, role, SOCKET_NUM_ONE, connectedNum));
    }

    return RaSocketFdHandleParam(socketInfo.fdHandle, socketInfo.status);
}

RequestHandle HrtRaSocketSendAsync(const FdHandle fdHandle, const void *data, u32 size,
    unsigned long long &sentSize)
{
    void *raReqHandle = nullptr;
    s32 ret = RaSocketSendAsync(fdHandle, data, size, &sentSize, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d] "
            "raReqHandle[%p], fdHandle[%p] data[%p] size[%u] sentSize[%u].",
            __func__, ret, raReqHandle, fdHandle, data, size, sentSize);
    }

    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle HrtRaSocketRecvAsync(const FdHandle fdHandle, void *data, u32 size,
    unsigned long long &recvSize)
{
    void *raReqHandle = nullptr;
    s32 ret = RaSocketRecvAsync(fdHandle, data, size, &recvSize, &raReqHandle);
        if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d], "
            "raReqHandle[%p], fdHandle[%p] data[%p] size[%u] recvSize[%u].",
            __func__, ret, raReqHandle, fdHandle, data, size, recvSize);
    }

    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaUbLocalMemRegAsync(RdmaHandle handle, const HrtRaUbLocMemRegParam &in,
    vector<char_t> &out, void *&lmemHandle)
{
    u64 pageSize = UB_MEM_PAGE_SIZE;
    u64 newAddr  = in.addr & (~(static_cast<u64>(pageSize - 1))); // UB内存注册要求起始地址4k对齐
    u64 offset   = in.addr - newAddr;
    u64 newSize  = in.size + offset + 4;

    out.resize(sizeof(struct mr_reg_info_t));
    struct mr_reg_info_t *info = reinterpret_cast<struct mr_reg_info_t *>(out.data());
    info->in.mem.addr                   = newAddr;
    info->in.mem.size                   = newSize;

    info->in.ub.flags.value             = 0;
    info->in.ub.flags.bs.token_policy   = TOKEN_POLICY_PLAIN_TEXT;
    info->in.ub.flags.bs.token_id_valid = 1;
    info->in.ub.flags.bs.access = MEM_SEG_ACCESS_READ
        | MEM_SEG_ACCESS_WRITE | MEM_SEG_ACCESS_ATOMIC;
    info->in.ub.flags.bs.non_pin = in.nonPin;
    info->in.ub.token_value      = in.tokenValue;
    info->in.ub.token_id_handle  = reinterpret_cast<void *>(in.tokenIdHandle);

    void *raReqHandle = nullptr;
    s32 ret = ra_ctx_lmem_register_async(handle, info, &lmemHandle, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, call interface "
            "error[%d] raReqHandle[%p], addr=0x%llx, size=0x%llx",
            __func__, ret, raReqHandle, in.addr, in.size));
    }
    info->in.ub.token_value = 0;
    HCCL_INFO("[%s] ok, get handle[%llu].", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaUbLocalMemUnregAsync(RdmaHandle rdmaHandle, LocMemHandle lmemHandle)
{
    void *raReqHandle = nullptr;
    s32 ret = ra_ctx_lmem_unregister_async(rdmaHandle,
        reinterpret_cast<void *>(lmemHandle), &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>(StringFormat("[%s] failed, call interface error[%d] "
            "raReqResult[%p], rdmaHandle=%p, lmemHandle=0x%llx.", __func__, ret, raReqHandle,
            rdmaHandle, lmemHandle));
    }

    HCCL_INFO("[%s] ok, get handle[%llu]", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaUbCreateJettyAsync(const RdmaHandle handle, const HrtRaUbCreateJettyParam &in,
    vector<char_t> &out, void *&jettyHandle)
{
    struct qp_create_attr attr = GetQpCreateAttr(in);

    void *raReqHandle = nullptr;
    out.resize(sizeof(qp_create_info));
    s32 ret = ra_ctx_qp_create_async(handle, &attr, reinterpret_cast<qp_create_info *>(out.data()),
        &jettyHandle, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d], raReqHandle[%p], "
            "rdmaHanlde[%p].", __func__, ret, raReqHandle, handle);
    }
    attr.ub.token_value = 0;
    HCCL_INFO("[%s] ok, get handle[%llu].", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaUbDestroyJettyAsync(void *jettyHandle)
{
    void *raReqHandle = nullptr;
    s32 ret = ra_ctx_qp_destroy_async(jettyHandle, &raReqHandle);
    if (ret != 0) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "jettyHandle[%p].", __func__, ret, raReqHandle, jettyHandle);
    }

    HCCL_INFO("[%s] ok, get handle[%llu].", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

inline hccp_eid IpAddressToHccpEid(const IpAddress &ipAddr)
{
    hccp_eid eid = {};
    HCCL_INFO("EID ipAddr[%s]", ipAddr.Describe().c_str());
    s32 sRet = memcpy_s(eid.raw, sizeof(eid.raw), ipAddr.GetEid().raw, sizeof(ipAddr.GetEid().raw));
    if (sRet != EOK) {
        THROW<InternalException>("[IpAddressToHccpEid]memcpy_s failed");
    }
    HCCL_INFO("[IpAddressToHccpEid] %s", HccpEidDesc(eid).c_str());
    return eid;
}

RequestHandle RaUbGetTpInfoAsync(const RdmaHandle rdmaHandle, const RaUbGetTpInfoParam &param,
    vector<char_t> &out, uint32_t &num)
{
    const auto &locAddr    = param.locAddr;
    const auto &rmtAddr    = param.rmtAddr;
    const auto &tpProtocol = param.tpProtocol;

    struct get_tp_cfg cfg{};
    cfg.flag.bs.rtp = tpProtocol == TpProtocol::TP ? 1 : 0;
    cfg.flag.bs.ctp = tpProtocol == TpProtocol::CTP ? 1 : 0;
    cfg.trans_mode = transport_mode_t::CONN_RM; // 当前只使用RM Jetty
    cfg.local_eid = IpAddressToHccpEid(locAddr);
    HCCL_INFO("RaUbGetTpInfoAsync cfg.local_eid=%s", HccpEidDesc(cfg.local_eid).c_str());
    cfg.peer_eid = IpAddressToHccpEid(rmtAddr);
    HCCL_INFO("RaUbGetTpInfoAsync cfg.peer_eid=%s", HccpEidDesc(cfg.peer_eid).c_str());

    out.resize(sizeof(tp_info));
    struct tp_info *info = reinterpret_cast<struct tp_info *>(out.data());

    void *raReqHandle = nullptr;
    num = TP_HANDLE_REQUEST_NUM; // 指定需要从管控面申请tp handle的数量, hccp 会返回实际个数
    s32 ret = ra_get_tp_info_list_async(rdmaHandle, &cfg, info, &num, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "rdmaHandle[%p] locAddr[%s] rmtAddr[%s].", __func__, ret, raReqHandle, rdmaHandle,
            locAddr.Describe().c_str(), rmtAddr.Describe().c_str());
    }

    HCCL_INFO("[%s] ok, get handle[%llu].", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

static RequestHandle ImportJettyAsync(RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void *&remQpHandle, const jetty_import_exp_cfg &cfg, jetty_import_mode mode,
    TpProtocol protocol = TpProtocol::INVALID)
{
    if (mode == jetty_import_mode::JETTY_IMPORT_MODE_NORMAL) {
        THROW<NotSupportException>("[%s] currently not support JETTY_IMPORT_MODE_NORMAL.",
            __func__);
    }

    out.resize(sizeof(qp_import_info_t));
    struct qp_import_info_t *info = reinterpret_cast<qp_import_info_t *>(out.data());

    s32 ret = memcpy_s(info->in.key.value, sizeof(info->in.key.value), in.key, in.keyLen);
    if (ret != 0) {
        THROW<InternalException>("[%s] memcpy_s failed, ret=%d.", __func__, ret);
    }

    info->in.key.size = in.keyLen;
    info->in.ub.mode = mode;
    info->in.ub.token_value = in.tokenValue;
    info->in.ub.policy = jetty_grp_policy::JETTY_GRP_POLICY_RR;
    info->in.ub.type = target_type::TARGET_TYPE_JETTY;

    info->in.ub.flag.value = 0;
    info->in.ub.flag.bs.token_policy = TOKEN_POLICY_PLAIN_TEXT;

    info->in.ub.exp_import_cfg = cfg;

    if (protocol != TpProtocol::TP && protocol != TpProtocol::CTP) {
        THROW<NetworkApiException>("[%s] failed, tp protocol[%s] is not expected, %s.",
        __func__, protocol.Describe().c_str());
    }
    // tp_type: 0->RTP, 1->CTP
    info->in.ub.tp_type = protocol == TpProtocol::TP ? 0 : 1;

    void *raReqHandle = nullptr;
    ret = ra_ctx_qp_import_async(rdmaHandle, info, &remQpHandle, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "rdmaHandle[%p].", __func__, ret, raReqHandle, rdmaHandle);
    }
    info->in.ub.token_value = 0;
    HCCL_INFO("[%s] ok, get handle[%llu]", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

RequestHandle RaUbImportJettyAsync(const RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void *&remQpHandle)
{
    // 该接口仅适配非管控面模式，当前不期望使用
    struct jetty_import_exp_cfg cfg = {};
    const auto mode = jetty_import_mode::JETTY_IMPORT_MODE_NORMAL;
    return ImportJettyAsync(rdmaHandle, in, out, remQpHandle, cfg, mode);
}

RequestHandle RaUbTpImportJettyAsync(const RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void *&remQpHandle)
{
    struct jetty_import_exp_cfg cfg = GetTpImportCfg(in.jettyImportCfg);
    const auto mode = jetty_import_mode::JETTY_IMPORT_MODE_EXP;
    return ImportJettyAsync(rdmaHandle, in, out, remQpHandle, cfg, mode, in.jettyImportCfg.protocol);
}

RequestHandle RaUbUnimportJettyAsync(void *targetJettyHandle)
{
    void *raReqHandle = nullptr;
    s32 ret = ra_ctx_qp_unimport_async(targetJettyHandle, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "targetJettyHandle[%p].", __func__, ret, raReqHandle, targetJettyHandle);
    }

    HCCL_INFO("[%s] ok, get handle[%llu].", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    return reinterpret_cast<RequestHandle>(raReqHandle);
}

void HrtRaWaitEventHandle(int event_handle, std::vector<SocketEventInfo> &event_infos, int timeout,
    unsigned int maxevents, u32 &events_num)
{
    std::vector<struct SocketEventInfoT> raEventInfos(maxevents);
    s32 ret = RaWaitEventHandle(event_handle, raEventInfos.data(), timeout, maxevents, &events_num);
    if (ret != 0) {
        THROW<NetworkApiException>("[%s] failed, call interface error[%d].", __func__, ret);
    }
    for (u32 i = 0; i < events_num; i++) {
        event_infos[i].fdHandle = raEventInfos[i].fdHandle;
    }
}

void HrtRaGetSecRandom(u32 *value, u32 &devPhyId)
{
    struct RaInfo raInfo;
    raInfo.mode = HrtNetworkMode::HDC;
    raInfo.phyId = devPhyId;

    s32 ret = ra_get_sec_random(&raInfo, value);
    if (ret != 0) {
        HCCL_ERROR("[HrtRaGetSecRandom] ra_get_sec_random failed, call interface error[%d]", ret);
        THROW<NetworkApiException>("[%s] failed, call interface error[%d].", __func__, ret);
    }
}
HcclResult HrtRaCreateQpWithCq(RdmaHandle rdmaHandle, s32 sqEvent, s32 rqEvent,
    void *sendChannel, void *recvChannel, QpInfo &info, bool isHdcMode)
{
    struct ibv_comp_channel *sChannel = reinterpret_cast<struct ibv_comp_channel *>(sendChannel);
    struct ibv_comp_channel *rChannel = reinterpret_cast<struct ibv_comp_channel *>(recvChannel);

    QpConfig config(MAX_WR_NUM, MAX_SEND_SGE_NUM, MAX_RECV_SGE_NUM, sqEvent, rqEvent);
    CqInfo cq(nullptr, nullptr, nullptr, MAX_CQ_DEPTH, config.sqEvent, config.rqEvent, info.srqContext,
        sChannel, rChannel);
    // hdc模式下hccp没有对外提供创建CQ的接口
    if (!isHdcMode) {
        CHK_RET(HrtRaCreateCq(rdmaHandle, cq));
    }
    info.attr = config;
    info.rdmaHandle = rdmaHandle;
    info.context = cq.context;
    info.sendCq = cq.sq;
    info.recvCq = cq.rq;
    info.recvChannel = rChannel;
    info.sendChannel = sChannel;

    if (isHdcMode) {
        TRY_CATCH_RETURN(info.qpHandle = HrtRaQpCreate(rdmaHandle, info.flag, info.qpMode));
    } else {
        CHK_RET(HrtRaNormalQpCreate(rdmaHandle, info));
    }

    return HCCL_SUCCESS;
}

HcclResult HrtRaDestroyQpWithCq(const QpInfo& info, bool isHdcMode)
{
    if (info.qpHandle == nullptr) {
        return HCCL_SUCCESS;
    }

    if (isHdcMode) {
        TRY_CATCH_RETURN(HrtRaQpDestroy(info.qpHandle));
    } else {
        CHK_RET(HrtRaNormalQpDestroy(info.qpHandle));
        CqInfo cq;
        cq.context = info.context;
        cq.rq = info.recvCq;
        cq.sq = info.sendCq;
        CHK_RET(HrtRaDestroyCq(info.rdmaHandle, cq));
    }

    return HCCL_SUCCESS;
}

// ra_cq_create
HcclResult HrtRaCreateCq(RdmaHandle rdmaHandle, CqInfo& cq)
{
    if (rdmaHandle == nullptr) {
        HCCL_ERROR("[HrtRaCreateCq] rdmaHandle is nullptr");
        return HCCL_E_ROCE_CONNECT;
    }

    struct CqAttr attr{};
    attr.qpContext = &(cq.context);
    attr.ibSendCq = &(cq.sq);
    attr.ibRecvCq = &(cq.rq);
    attr.sendCqDepth = cq.depth;
    attr.recvCqDepth = cq.depth;
    attr.sendCqEventId = cq.sqEvent;
    attr.recvCqEventId = cq.rqEvent;
    attr.sendChannel = cq.sendChannel;
    attr.recvChannel = cq.recvChannel;
    attr.srqContext = cq.srqContext;

    HCCL_DEBUG("ra create cq: send_cq_depth[%d] recv_cq_depth[%d] send_cq_event_id[%d], recv_cq_event_id[%d]",
               attr.sendCqDepth, attr.recvCqDepth, attr.sendCqEventId, attr.recvCqEventId);
    s32 ret = RaCqCreate(rdmaHandle, &attr);
    CHK_PRT_RET(ret != 0,
                HCCL_ERROR("[HrtRaCreateCq] errNo[0x%016llx] RaCqCreate fail. "
                           "return[%d], params: rdmaHandle[%p]",
                           HCCL_ERROR_CODE(HCCL_E_NETWORK), ret, rdmaHandle),
                HCCL_E_NETWORK);
    if (cq.sq == nullptr || cq.rq == nullptr || cq.context == nullptr) {
        HCCL_ERROR("[HrtRaCreateCq] cq member[sq:%p, rq:%p, context:%p] is nullptr", cq.sq, cq.rq, cq.context);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}
// ra_cq_destory
HcclResult HrtRaDestroyCq(RdmaHandle rdmaHandle, CqInfo& cq)
{
    struct CqAttr attr;
    attr.qpContext = &cq.context;
    attr.ibSendCq = &cq.sq;
    attr.ibRecvCq = &cq.rq;
    s32 ret = RaCqDestroy(rdmaHandle, &attr);
    CHK_PRT_RET(ret != 0,
                HCCL_ERROR("[HrtRaDestroyCq] errNo[0x%016llx] RaCqDestroy failed, call interface error. "
                           "return[%d], params: rdmaHandle[%p]",
                           HCCL_ERROR_CODE(HCCL_E_NETWORK), ret, rdmaHandle),
                HCCL_E_NETWORK);
    return HCCL_SUCCESS;
}

// ra_normal_qp_create
HcclResult HrtRaNormalQpCreate(RdmaHandle rdmaHandle, QpInfo& qp)
{
    struct ibv_qp_init_attr ibQpAttr;
    CHK_SAFETY_FUNC_RET(memset_s(&ibQpAttr, sizeof(ibv_qp_init_attr), 0, sizeof(ibv_qp_init_attr)));
    ibQpAttr.qp_context= qp.context;
    ibQpAttr.send_cq = qp.sendCq;
    ibQpAttr.recv_cq = qp.recvCq;
    ibQpAttr.srq = qp.srq;
    ibQpAttr.qp_type = IBV_QPT_RC;
    ibQpAttr.cap.max_inline_data = MAX_INLINE_DATA;
    ibQpAttr.cap.max_send_wr = qp.attr.maxWr;
    ibQpAttr.cap.max_send_sge = qp.attr.maxSendSge;
    ibQpAttr.cap.max_recv_wr = (qp.srq == nullptr ? qp.attr.maxWr : 0);
    ibQpAttr.cap.max_recv_sge = (qp.srq == nullptr ? qp.attr.maxRecvSge : 0);
    s32 ret = RaNormalQpCreate(rdmaHandle, &ibQpAttr, &(qp.qpHandle), reinterpret_cast<void **>(&(qp.qp)));
    CHK_PRT_RET(ret != 0, HCCL_ERROR("[Create][NormalQp]errNo[0x%016llx] RaNormalQpCreate fail. return[%d]",\
        HCCL_ERROR_CODE(HCCL_E_NETWORK), ret), HCCL_E_NETWORK);
    return HCCL_SUCCESS;
}

HcclResult HrtRaNormalQpDestroy(QpHandle qpHandle)
{
    CHK_PTR_NULL(qpHandle);
    s32 ret = RaNormalQpDestroy(qpHandle);
    CHK_PRT_RET(ret != 0, HCCL_ERROR("[Destroy][NormalQp]errNo[0x%016llx] ra destroy normal qp fail. return[%d]",\
        HCCL_ERROR_CODE(HCCL_E_NETWORK), ret), HCCL_E_NETWORK);
    return HCCL_SUCCESS;
}
} // namespace Hccl
