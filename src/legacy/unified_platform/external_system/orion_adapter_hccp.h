/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_ADAPTER_HCCP_H
#define HCCLV2_ADAPTER_HCCP_H
#include <vector>
#include <unordered_set>
#include "hccp_common.h"
#include "ip_address.h"
#include "data_type.h"
#include "reduce_op.h"
#include "hccp_tlv.h"
#include <mutex>
#include "hccp_async_ctx.h"

namespace Hccl {
using namespace std;

/// 与 `Hccl::UB_QOS_DEFAULT`（legacy/framework/env_config/env_config.h）及 Next `EnvConfig::UB_QOS_DEFAULT` 数值一致；
/// 本头文件不 include env_config，避免经 base_config.h 拉入 dma_mode.h 导致 platform 等目标缺头编译失败。
constexpr u32 kRaUbGetTpInfoParamDefaultQos = 4U;

/// 单次向管控面查询 TP 列表条数上限（异步接口传入/返回 num；与 buffer 中 HccpTpInfo 条数一致）。
constexpr uint32_t TP_HANDLE_REQUEST_NUM = 8U;

constexpr u32 DEFAULT_INIT_PHY_ID  = 0;
constexpr u32 DEFAULT_INIT_NIC_POS = 0;
constexpr u32 DEFAULT_HDC_TYPE     = 6;
constexpr u32 PID_HDC_TYPE         = 18;

constexpr u32 SOCKET_NOT_CONNECTED   = 0;
constexpr u32 SOCKET_CONNECTED       = 1;
constexpr u32 SOCKET_CONNECT_TIMEOUT = 2;
constexpr u32 SOCKET_CONNECTING      = 3;

constexpr u32 UB_WQE_SIZE_64  = 64;
constexpr u32 UB_WQE_SIZE_128 = 128;

// QP CQ default attr
constexpr u32 DEFAULT_OPBASE_MAX_SEND_WR = 32768;
constexpr u32 DEFAULT_OFFLINE_MAX_SEND_WR = 128;
constexpr u32 DEFAULT_MAX_RECV_WR = 128;
constexpr u32 DEFAULT_MAX_SEND_SGE = 1;
constexpr u32 DEFAULT_MAX_RECV_SGE = 1;
constexpr u32 DEFAULT_MAX_SEND_CQ_DEPTH = 32768;
constexpr u32 DEFAULT_MAX_RECV_CQ_DEPTH = 128;
constexpr u32 DEFAULT_MAX_INLINE_DATA = 32;
constexpr u32 HETEROG_OFFLINE_EXT_MAX_SEND_WR = 512;

// 适配URMA，直接组装WQE的TOKENID需要进行移位，包括CCU与AICPU
constexpr u32 URMA_TOKEN_ID_RIGHT_SHIFT = 8;

// HCCL 默认无效端口号
constexpr u32 HCCL_INVALID_PORT = 65536;

using RdmaHandle = void *;
using QpHandle   = void *;

using SocketHandle = void *;
using FdHandle     = void *;

using MrHandle = void *;

MAKE_ENUM(HrtNetworkMode, PEER, HDC)
enum DeviceIdType {
    DEVICE_ID_TYPE_PHY_ID = 0,
    DEVICE_ID_TYPE_SDID
};


inline s32 EnvLinkTimeoutGet();

struct HRaInitConfig {
    HrtNetworkMode mode;
    uint32_t       phyId;
    uint32_t       hdcType{DEFAULT_HDC_TYPE};
};

void HrtRaInit(HRaInitConfig &cfg);
void HrtRaDeInit(HRaInitConfig &cfg);

struct HRaTlvInitConfig  {
    HrtNetworkMode mode;
    uint32_t       phyId;
    int32_t       version;
};

void* HrtRaTlvInit(HRaTlvInitConfig &cfg);
HcclResult HrtRaTlvRequest(void* tlv_handle, u32 tlv_module_type, u32 tlv_ccu_msg_type);
void HrtRaTlvDeInit(void* tlv_handle);

u32 HrtRaGetInterfaceVersion(u32 phyId, u32 interfaceOpcode);

enum class TlsStatus : int{
    UNKNOWN = -1, // 不支持查询
    DISABLE = 0, //  未使能
    ENABLE,      //  使能
};

HcclResult HrtRaGetTlsStatus(struct RaInfo *info, TlsStatus &tlsStatus);

struct RaInterface {
    uint32_t  phyId;
    IpAddress address;
};

SocketHandle HrtRaSocketInit(HrtNetworkMode netMode, RaInterface &in);
void         HrtRaSocketDeInit(SocketHandle socketHandle);

struct RaSocketListenParam {
    SocketHandle socketHandle; /**< socket handle */
    unsigned int port;         /**< Socket listening port number */
    RaSocketListenParam(SocketHandle handle, u32 port) : socketHandle(handle), port(port)
    {
    }
};

using QpConfig = struct QpConfigDef {
    IpAddress selfIp;
    IpAddress peerIp;
    u32 maxWr;
    u32 maxSendSge;
    u32 maxRecvSge;
    s32 sqEvent;
    s32 rqEvent;

    QpConfigDef(IpAddress &selfIp, IpAddress &peerIp, u32 maxWr, u32 maxSendSge,
        u32 maxRecvSge, s32 sqEvent, s32 rqEvent)
        : selfIp(selfIp),
          peerIp(peerIp),
          maxWr(maxWr),
          maxSendSge(maxSendSge),
          maxRecvSge(maxRecvSge),
          sqEvent(sqEvent),
          rqEvent(rqEvent)
    {}
    QpConfigDef(u32 maxWr, u32 maxSendSge, u32 maxRecvSge, s32 sqEvent, s32 rqEvent)
        : maxWr(maxWr), maxSendSge(maxSendSge), maxRecvSge(maxRecvSge), sqEvent(sqEvent), rqEvent(rqEvent)
    {}
    QpConfigDef() : maxWr(0), maxSendSge(0), maxRecvSge(0), sqEvent(0), rqEvent(0) {}
};

using QpInfo = struct QpInfoDef {
    QpConfig attr;
    RdmaHandle rdmaHandle;
    QpHandle qpHandle;
    struct ibv_qp* qp;
    void* context;
    struct ibv_cq* sendCq;
    struct ibv_cq* recvCq;
    struct ibv_srq *srq;
    struct ibv_cq* srqCq;
    void *srqContext;
    struct ibv_comp_channel *sendChannel;
    struct ibv_comp_channel *recvChannel;
    s32 flag = 0;
    s32 qpMode = 0;
    u32 trafficClass = 0;
    u32 serviceLevel = 0;
    u32 retryCnt = 0;
    u32 retryInterval = 0;
    QpInfoDef() : rdmaHandle(nullptr), qpHandle(nullptr), qp(nullptr), context(nullptr), sendCq(nullptr),
        recvCq(nullptr), srq(nullptr), srqCq(nullptr), srqContext(nullptr),
        sendChannel(nullptr), recvChannel(nullptr), trafficClass(HCCL_COMM_TRAFFIC_CLASS_CONFIG_NOT_SET),
        serviceLevel(HCCL_COMM_SERVICE_LEVEL_CONFIG_NOT_SET) {}
    QpInfoDef(QpConfig attr, RdmaHandle rdmaHandle, QpHandle qpHandle, struct ibv_qp* qp, void* context,
              struct ibv_cq* sendCq, struct ibv_cq* recvCq, struct ibv_srq *srq, struct ibv_cq* srqCq,
              void *srqContext = nullptr, struct ibv_comp_channel *sendChannel = nullptr,
              struct ibv_comp_channel *recvChannel = nullptr, u32 tc = HCCL_COMM_TRAFFIC_CLASS_CONFIG_NOT_SET,
              u32 sl = HCCL_COMM_SERVICE_LEVEL_CONFIG_NOT_SET)
        : attr(attr), rdmaHandle(rdmaHandle), qpHandle(qpHandle), qp(qp), context(context), sendCq(sendCq),
        recvCq(recvCq), srq(srq), srqCq(srqCq), srqContext(srqContext),
        sendChannel(sendChannel), recvChannel(recvChannel), trafficClass(tc), serviceLevel(sl) {}
};

using CqInfo = struct CqInfoDef {
    struct ibv_cq* sq;
    struct ibv_cq* rq;
    void* context;
    u32 depth;
    u32 used;
    s32 sqEvent;
    s32 rqEvent;
    void *srqContext;
    struct ibv_comp_channel *sendChannel;
    struct ibv_comp_channel *recvChannel;
    std::vector<QpInfo> qps;
    CqInfoDef() : sq(nullptr), rq(nullptr), context(nullptr), depth(0), used(0), sqEvent(-1), rqEvent(-1),
        srqContext(nullptr), sendChannel(nullptr), recvChannel(nullptr) {}
    CqInfoDef(struct ibv_cq* sq, struct ibv_cq* rq, void* context, u32 depth, s32 sqEvent, s32 rqEvent,
        void *srqContext = nullptr, struct ibv_comp_channel *sendChannel = nullptr,
        struct ibv_comp_channel *recvChannel = nullptr)
        : sq(sq), rq(rq), context(context), depth(depth), used(0), sqEvent(sqEvent),
        rqEvent(rqEvent), srqContext(srqContext), sendChannel(sendChannel), recvChannel(recvChannel) {}
};

void HrtRaSocketListenOneStart(RaSocketListenParam &in);
void HrtRaSocketListenOneStop(RaSocketListenParam &in);
bool HrtRaSocketTryListenOneStart(RaSocketListenParam &in);

void HrtRaSocketSetWhiteListStatus(u32 enable);
u32  HrtRaSocketGetWhiteListStatus();

struct RaSocketWhitelist {
    IpAddress   remoteIp;  /**< IP address of remote */
    uint32_t    connLimit; /**< limit of whilte list */
    std::string tag;
};

void HrtRaSocketWhiteListAdd(SocketHandle socketHandle, vector<RaSocketWhitelist> &wlists);
void HrtRaSocketWhiteListDel(SocketHandle socketHandle, vector<RaSocketWhitelist> &wlists);
void HrtRaSocketGetVnicIpInfos(u32 phyId, DeviceIdType deviceIdType, u32 deviceId, IpAddress &vnicIP);

struct RaSocketConnectParam {
    SocketHandle socketHandle; /**< socket handle */
    IpAddress    remoteIp;     /**< IP address of remote socket, [0-7] is reserved for vnic */
    unsigned int port;         /**< Socket listening port number */
    std::string  tag;
    RaSocketConnectParam(SocketHandle handle, IpAddress &remoteIp, u32 port, const std::string &tag)
        : socketHandle(handle), remoteIp(remoteIp), port(port), tag(tag)
    {
    }
};

struct RaSocketCloseParam {
    SocketHandle socketHandle;
    FdHandle     fdHandle;
    RaSocketCloseParam(SocketHandle socketHandle, FdHandle fdHandle) : socketHandle(socketHandle), fdHandle(fdHandle)
    {
    }
};

void HrtRaSocketConnectOne(RaSocketConnectParam &in);
void HrtRaSocketCloseOne(RaSocketCloseParam &in);

struct RaSocketGetParam {
    SocketHandle socketHandle; /**< socket handle */
    IpAddress    remoteIp;     /**< IP address of remote socket */
    std::string  tag;
    FdHandle     fdHandle;
    RaSocketGetParam(SocketHandle handle, IpAddress &remoteIp, const std::string &tag, FdHandle fdHandle)
        : socketHandle(handle), remoteIp(remoteIp), tag(tag), fdHandle(fdHandle)
    {
    }
};

struct RaSocketFdHandleParam {
    FdHandle fdHandle; /**< fd handle */
    int      status;   /**< socket status:0 not connected 1:connected 2:connect timeout 3:connecting */
    RaSocketFdHandleParam(FdHandle fdHandle, int status) : fdHandle(fdHandle), status(status)
    {
    }
};
RaSocketFdHandleParam HrtRaBlockGetOneSocket(u32 role, RaSocketGetParam &param);

void HrtRaSocketBlockSend(const FdHandle fdHandle, const void *data, u32 sendSize);
bool HrtRaSocketNonBlockSend(const FdHandle fdHandle, void *data, u64 size, u64 *sentSize);
void HrtRaSocketBlockRecv(const FdHandle fdHandle, void *data, u32 size);
HcclResult HrtRaSocketNonBlockSendHeart(const FdHandle fdHandle, void *data, u64 size, u64 *sentSize);
HcclResult HrtRaSocketNonBlockRecvHeart(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize);

vector<std::pair<std::string, IpAddress>> HrtGetHostIf(u32 devPhyId);
vector<IpAddress>                         HrtGetDeviceIp(u32 devicePhyId, NetworkMode netWorkMode = NetworkMode::NETWORK_OFFLINE);

constexpr u32 RDMA_MEM_KEY_MAX_LEN  = 64; // 最大的memKey长度
constexpr u32 RDMA_MEM_KEY_LEN_ROCE = 4;  // 暂定ROCE k的ey长度为4， 未来从HCCP新接口获取key真实长度

RdmaHandle HrtRaRdmaInit(HrtNetworkMode netMode, RaInterface &in);
void       HrtRaRdmaDeInit(RdmaHandle rdmaHandle, HrtNetworkMode netMode);

void HrtRaGetNotifyBaseAddr(RdmaHandle rdmaHandle, u64 *va, u64 *size);

constexpr s32 QP_FLAG_RC          = 0; // flag: 0 = RC, 1= UD，其它预留
constexpr s32 OFFLINE_QP_MODE     = 1; // 下沉模式的QP(80)
constexpr s32 OPBASE_QP_MODE      = 2; // 单算子模式的QP(80)
constexpr s32 OFFLINE_QP_MODE_EXT = 3; // 下沉模式(81)QP
constexpr s32 OPBASE_QP_MODE_EXT  = 4; // 单算子模式(81)的QP

QpHandle HrtRaQpCreate(RdmaHandle rdmaHandle, int flag, int qpMode);

void HrtRaQpDestroy(QpHandle qpHandle);
void HrtRaQpConnectAsync(QpHandle qpHandle, FdHandle fdHandle);
int  HrtGetRaQpStatus(QpHandle qpHandle);

struct RaMrInfo {
    void              *addr;   /**< starting address of mr */
    unsigned long long size;   /**< size of mr */
    int                access; /**< access of mr, reference to ra_access_flags */
    unsigned int       lkey;   /**< local addr access key */
};

void HrtRaMrReg(QpHandle qpHandle, RaMrInfo &info);
void HrtRaMrDereg(QpHandle qpHandle, RaMrInfo &info);

struct HRaSendWr {
    uint64_t locAddr;   /**< address of buf */
    uint32_t len;       /**< len of buf */
    uint64_t rmtAddr;   /**< destination address */
    uint32_t op;        /**< operations of RDMA supported:RDMA_WRITE:0 */
    int      sendFlag; /**< reference to ra_send_flags */
    HRaSendWr(uint64_t locAddr, uint32_t len, uint64_t rmtAddr, uint32_t op, int sendFlag)
        : locAddr(locAddr), len(len), rmtAddr(rmtAddr), op(op), sendFlag(sendFlag)
    {
    }
};

struct RaSendWrResp {
    unsigned int  sqIndex;  /**< index of sq */
    unsigned int  wqeIndex; /**< index of wqe */
    unsigned int  dbIndex;  /**< index of db */
    unsigned long dbInfo;   /**< db content */
    RaSendWrResp(unsigned int sqIndex, unsigned int wqeIndex, unsigned int dbIndex, unsigned long dbInfo)
        : sqIndex(sqIndex), wqeIndex(wqeIndex), dbIndex(dbIndex), dbInfo(dbInfo)
    {
    }
};

RaSendWrResp HrtRaSendOneWr(QpHandle qpHandle, HRaSendWr &in);

string HrtRaGetKeyDescribe(const u8 *key, u32 len);

using LocMemHandle      = u64;
using RemMemHandle      = u64;
using JfcHandle         = u64;
using JettyHandle       = u64;
using TargetJettyHandle = u64;
using NotifyHandle      = u64;
using TokenIdHandle     = u64;

using HrtRaUbCtxInitParam = struct HrtRaUbCtxInitParamDef {
    HrtNetworkMode   mode;
    u32              phyId;
    const IpAddress &addr;
    HrtRaUbCtxInitParamDef(HrtNetworkMode mode, u32 phyId, const IpAddress &addr) : mode(mode), phyId(phyId), addr(addr)
    {
    }
};

RdmaHandle HrtRaUbCtxInit(const HrtRaUbCtxInitParam &in);

void HrtRaUbCtxDestroy(RdmaHandle handle);

std::pair<TokenIdHandle, uint32_t> RaUbAllocTokenIdHandle(RdmaHandle handle);
void RaUbFreeTokenIdHandle(RdmaHandle handle, TokenIdHandle tokenIdHandle);

using HrtRaUbLocMemRegParam = struct HrtRaUbLocalMemRegParamDef {
    u64 addr;
    u64 size;
    u32 tokenValue;
    TokenIdHandle tokenIdHandle;
    u32 nonPin{1}; // 1: 寄存器(notify和cntNotify、CCU), 0: Memory(rtMalloc)
    HrtRaUbLocalMemRegParamDef(u64 addr, u64 size, u32 tokenValue, TokenIdHandle tokenIdHandle, u32 nonPin = 1)
        : addr(addr), size(size), tokenValue(tokenValue), tokenIdHandle(tokenIdHandle), nonPin(nonPin)
    {
    }
};

constexpr u32 HRT_UB_MEM_KEY_MAX_LEN = 64; // UB 最大的memKey长度

using HrtRaUbLocalMemRegOutParam = struct HrtRaUbLocMemHandleParamDef {
    LocMemHandle handle{0};
    u8           key[HRT_UB_MEM_KEY_MAX_LEN]{};
    u32          tokenId{0};
    u64          targetSegVa{0};
    u32          keySize{0};
};

HrtRaUbLocalMemRegOutParam HrtRaUbLocalMemReg(RdmaHandle handle, const HrtRaUbLocMemRegParam &in);

std::pair<u64, u64> BufAlign(u64 addr, u64 size);

void HrtRaUbLocalMemUnreg(RdmaHandle rdmaHandle, LocMemHandle lmemHandle);

using HrtRaUbRemMemImportedOutParam = struct HrtRaUbRemMemHandleParamDef {
    RemMemHandle handle{0};
    u64          targetSegVa{0};
};

HrtRaUbRemMemImportedOutParam HrtRaUbRemoteMemImport(RdmaHandle handle, u8 *key, u32 keyLen, u32 tokenValue);

void HrtRaUbRemoteMemUnimport(RdmaHandle rdmaHandle, RemMemHandle rmemHandle);

MAKE_ENUM(HrtUbJfcMode, NORMAL, STARS_POLL, CCU_POLL, USER_CTL)
 	 
struct CqCreateInfo {
    uint64_t va;
    uint32_t id;
    uint64_t bufAddr;
    uint32_t cqeSize;
    uint32_t cqDepth;
    uint64_t swdbAddr;
};

JfcHandle HrtRaUbCreateJfc(RdmaHandle handle, CqCreateInfo& cqInfo, HrtUbJfcMode mode);

JfcHandle HrtRaUbCreateJfcUserCtl(RdmaHandle handle, CqCreateInfo& cqInfo);

void HrtRaUbDestroyJfc(RdmaHandle handle, JfcHandle jfcHandle);

MAKE_ENUM(HrtTransportMode, RM, RC)
MAKE_ENUM(TpProtocol, CTP, TP, UBOE);

// STANDARD: URMA标准CreateJetty
// HOST_OFFLOAD: HOST侧展开下沉算子，需要指定sqeBbNum
// HOST_OPBASE: Host展开单算子，需要指定sqeBbNum,
// DEV_USED: 在Dev的APICPU展开算子，STARS不能使用UB DirectWQE的task，可以使用UB DbSend task，不需要指定sqeBbNum
// CACHE_LOCK_DWQE: 该模式下，      STARS仅能使用UB DirectWQE的task，不能使用UB DbSend task,，需要指定sqeBbNum
// CCU_CCUM_CACHE: 不需要指定sqeBbNum
MAKE_ENUM(HrtJettyMode, STANDARD, HOST_OFFLOAD, HOST_OPBASE, DEV_USED, CACHE_LOCK_DWQE, CCU_CCUM_CACHE)
using HrtRaUbCreateJettyParam = struct HrtRaUbJettyCreateParamDef {
    JfcHandle sjfcHandle{0};
    JfcHandle rjfcHandle{0};

    // CCU的DB需要注册，填写tokenValue
    u32 tokenValue{0};
    TokenIdHandle tokenIdHandle{0};

    HrtJettyMode jettyMode{HrtJettyMode::STANDARD};

    // 如果jettyId为0，则代表UB自行申请jetty,如果jettyId不为0，则代表使用预留jetty id
    // [1024, 1024 +127]为ccuJetty预留的id
    // [1024 + 192, 1024 + 192 + 4K - 1]为starsJetty预留的id
    u32 jettyId{0};

    // 指定内存，需要填写的参数，CCU类型需要填写,即HrtJettyMode::CCU_CCUM_CACHE
    u64 sqBufVa{0};
    u32 sqBufSize{0};
    // 指定sqeBB资源起始id，当前预留
    u32 sqeBufIndex{0};

    // HOST_OFFLOAD / HOST_OPBASE / CACHE_LOCK_DWQE 类型的Jetty ，需要指定WQEBB的数目
    // STADARD 类型Jetty，该参数代表SQ深度
    u32              sqDepth{0};
    /// UB Jetty priority（低 4bit）；GetQpCreateAttr 写入 attr.ub.priority
    u8               qos{4};
    u32              rqDepth{64};
    HrtTransportMode transMode{HrtTransportMode::RM}; // 仅能使用RM模式的Jetty

    HrtRaUbJettyCreateParamDef() {}

    HrtRaUbJettyCreateParamDef(JfcHandle sjfcHandle, JfcHandle rjfcHandle,
        u32 tokenValue, TokenIdHandle tokenIdHandle, HrtJettyMode jettyMode,
        u32 jettyId, u64 sqBufVa, u32 sqBufSize, u32 sqeBufIndex, u32 sqDepth)
        : sjfcHandle(sjfcHandle), rjfcHandle(rjfcHandle), tokenValue(tokenValue),
          tokenIdHandle(tokenIdHandle), jettyMode(jettyMode), jettyId(jettyId),
          sqBufVa(sqBufVa), sqBufSize(sqBufSize), sqeBufIndex(sqeBufIndex), sqDepth(sqDepth)
    {
    }
};

constexpr u32 HRT_UB_QP_KEY_MAX_LEN = 64; // UB 最大的QpKey长度

using HrtRaUbJettyCreatedOutParam = struct HrtRaUbJettyCreatedOutParamDef {
    JettyHandle handle{0};
    u8          key[HRT_UB_QP_KEY_MAX_LEN]{0};
    u64         jettyVa{0};
    u32         uasid{0};
    u32         id{0};
    u32         keySize{0};
    u64         dbVa{0};
    u32         dbTokenId{0};
    uint64_t    sqBuffVa{0}; // 适配HCCP修改，jettybufva由HCCP提供，不再由HCCL分配
};

HrtRaUbJettyCreatedOutParam HrtRaUbCreateJetty(RdmaHandle handle, const HrtRaUbCreateJettyParam &in);

void HrtRaUbDestroyJetty(JettyHandle jettyHandle);

struct JettyImportCfg {
    u64 localTpHandle{0};
    u64 remoteTpHandle{0};
    u64 localTag{0};  // tag是hccp预留字段，暂不需要赋值
    u32 localPsn{0};
    u32 remotePsn{0};
    TpProtocol protocol{TpProtocol::INVALID};
};

using HrtRaUbJettyImportedInParam = struct HrtRaUbJettyImportedInParamDef {
    u8 *key{nullptr};
    u32 keyLen{0};
    u32 tokenValue{0};
    JettyImportCfg jettyImportCfg{};
};

using HrtRaUbJettyImportedOutParam = struct HrtRaUbJettyImportedOutParamDef {
    TargetJettyHandle handle{0};
    u64               targetJettyVa{0};
    u32               tpn{0};
};

HrtRaUbJettyImportedOutParam RaUbImportJetty(RdmaHandle handle, u8 *key, u32 keyLen, u32 tokenValue);
HrtRaUbJettyImportedOutParam RaUbTpImportJetty(RdmaHandle handle, u8 *key, u32 keyLen,
    u32 tokenValue, const JettyImportCfg &jettyImportCfg);

void HrtRaUbUnimportJetty(RdmaHandle handle, TargetJettyHandle targetJettyHandle);

void HrtRaUbJettyBind(JettyHandle jettyHandle, TargetJettyHandle targetJettyHandle);

void HrtRaUbJettyUnbind(JettyHandle jettyHandle);

// 参照hccp做opcode定义
MAKE_ENUM(HrtUbSendWrOpCode, WRITE, WRITE_WITH_NOTIFY, READ, NOP)

using HrtRaUbSendWrReqParam = struct HrtRaUbSendWrParamDef {
    HrtUbSendWrOpCode opcode;
    bool              cqeEn{true};

    bool inlineFlag{false};
    u8  *inlineData;

    bool     inlineReduceFlag{false};
    DataType dataType;
    ReduceOp reduceOp;

    u64          notifyData;
    u64          notifyAddr;
    NotifyHandle notifyHandle;

    u64 localAddr;
    u64 remoteAddr;
    u32 size;

    LocMemHandle lmemHandle;
    RemMemHandle rmemHandle;

    TargetJettyHandle handle; // valid when RM mode
};

using HrtRaUbSendWrRespParam = struct HrtRaUbDbInfoParamDef {
    u32 jettyId{0};
    u32 funcId{0};
    u32 dieId{0};
    u32 piVal{0};
    u8  dwqe[128]{0};
    u32 dwqeSize{0};
};

HrtRaUbSendWrRespParam HrtRaUbPostSend(JettyHandle jettyHandle, HrtRaUbSendWrReqParam &in);
void                   HrtRaUbPostNops(JettyHandle jettyHandle, JettyHandle remoteJettyHandle, const u32 numNop);

std::pair<uint32_t, uint32_t> HraGetDieAndFuncId(RdmaHandle handle);
bool HraGetRtpEnable(RdmaHandle handle);

struct HRaInfo {
    HrtNetworkMode mode;
    uint32_t       phyId;
    HRaInfo(HrtNetworkMode mode, uint32_t phyId) : mode(mode), phyId(phyId)
    {
    }
};

void HrtRaCustomChannel(const HRaInfo &raInfo, void *customIn, void *customOut);

void RaUbUpdateCi(JettyHandle jettyHandle, u32 ci);

struct HrtDevEidInfo {
#ifdef HCCL_ALG_ANALYZER_DAVID
    std::string portId{0};
#endif
    std::string name{0};
    IpAddress   ipAddress{0};
    uint32_t eidIndex{0};
    uint32_t type{0};
    uint32_t dieId{0};
    uint32_t chipId{0};
    uint32_t funcId{0};
    uint32_t devFeature{0};
};
std::vector<HrtDevEidInfo> HrtRaGetDevEidInfoList(const HRaInfo &raInfo);

RaSocketFdHandleParam RaGetOneSocket(u32 role, RaSocketGetParam &param);

using RequestHandle = u64;

MAKE_ENUM(ReqHandleResult, COMPLETED, NOT_COMPLETED, SOCK_E_AGAIN, INVALID_PARA);

ReqHandleResult HrtRaGetAsyncReqResult(RequestHandle &reqHandle);

RequestHandle RaSocketConnectOneAsync(RaSocketConnectParam &in);
RequestHandle RaSocketCloseOneAsync(RaSocketCloseParam &in);
RequestHandle RaSocketListenOneStartAsync(RaSocketListenParam &in);
RequestHandle RaSocketListenOneStopAsync(RaSocketListenParam &in);

RequestHandle HrtRaSocketSendAsync(const FdHandle fdHandle, const void *data, u32 size, unsigned long long &sentSize);
RequestHandle HrtRaSocketRecvAsync(const FdHandle fdHandle, void *data, u32 size, unsigned long long &recvSize);

RequestHandle RaUbLocalMemRegAsync(RdmaHandle handle, const HrtRaUbLocMemRegParam &in,
    vector<char_t> &out, void* &lmemHandle);
RequestHandle RaUbLocalMemUnregAsync(RdmaHandle rdmaHandle, LocMemHandle lmemHandle);

RequestHandle RaUbCreateJettyAsync(const RdmaHandle handle, const HrtRaUbCreateJettyParam &in,
    vector<char_t> &out, void *&jettyHandle);
RequestHandle RaUbDestroyJettyAsync(void* jettyHandle);

using RaUbGetTpInfoParam = struct RaUbGetTpInfoParamDef {
    IpAddress locAddr{};
    IpAddress rmtAddr{};
    TpProtocol tpProtocol{TpProtocol::CTP};
    /// 与 Next TpMgr 一致：参与 SL→jetty priority 映射（0–7）；默认见 kRaUbGetTpInfoParamDefaultQos
    uint32_t qos{kRaUbGetTpInfoParamDefaultQos};
    uint32_t slLevelCount{0U};
    bool loopFirstTpLowestSl{false};
    /// 与 Next `GetTpInfoParam::ccuLoopbackGetTpInfo` 对齐：标识 CCU 设备环回 GetTpInfo（便于日志/后续分支）
    bool ccuLoopbackGetTpInfo{false};

    explicit RaUbGetTpInfoParamDef() = default;
    RaUbGetTpInfoParamDef(const IpAddress &locAddr, const IpAddress &rmtAddr, TpProtocol tpProtocol)
        : locAddr(locAddr), rmtAddr(rmtAddr), tpProtocol(tpProtocol) {}

    std::string Describe() const {
        return StringFormat(
            "RaUbGetTpInfoParam[locAddr=%s, rmtAddr=%s, tpProtocol=%s, qos=%u, loopFirstTpLowestSl=%d, ccuLoop=%d]",
            locAddr.Describe().c_str(), rmtAddr.Describe().c_str(), tpProtocol.Describe().c_str(),
            static_cast<unsigned>(qos & 0xFFU), static_cast<int>(loopFirstTpLowestSl),
            static_cast<int>(ccuLoopbackGetTpInfo));
    }
};

RequestHandle RaUbGetTpInfoAsync(const RdmaHandle rdmaHandle, const RaUbGetTpInfoParam &param, vector<char_t> &out, uint32_t &num);

void RaUbGetTpInfo(const RdmaHandle rdmaHandle, const RaUbGetTpInfoParam &param, vector<char_t> &out, uint32_t &num);

RequestHandle RaUbImportJettyAsync(const RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void* &remQpHandle);
RequestHandle RaUbTpImportJettyAsync(const RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void *&remQpHandle);
RequestHandle RaUbUnimportJettyAsync(void* targetJettyHandle);

struct SocketEventInfo {
    u32 event;
    FdHandle fdHandle;
};

HcclResult HrtRaWaitEventHandle(int event_handle, std::vector<SocketEventInfo> &event_infos, int timeout,
                          unsigned int maxevents, u32 &events_num);
void HrtRaGetSecRandom(u32 *value, u32 &devPhyId);

HcclResult HrtRaCreateQpWithCq(RdmaHandle rdmaHandle, s32 sqEvent, s32 rqEvent,
    void *sendChannel, void *recvChannel, QpInfo &info, bool isHdcMode);
HcclResult HrtRaDestroyQpWithCq(const QpInfo& info, bool isHdcMode);
HcclResult HrtRaCreateCq(RdmaHandle rdmaHandle, CqInfo& cq);
HcclResult HrtRaDestroyCq(RdmaHandle rdmaHandle, CqInfo& cq);
HcclResult ConstructQpDefaultAttrs(s32 qpMode, struct qp_ext_attrs &attrs, bool isWorkFlowLib);
HcclResult HrtRaNormalQpCreate(RdmaHandle rdmaHandle, QpInfo& qp);
HcclResult HrtRaNormalQpDestroy(QpHandle qpHandle);
  
MAKE_ENUM(AuxInfoInType, AUX_INFO_IN_TYPE_CQE, AUX_INFO_IN_TYPE_AE, AUX_INFO_IN_TYPE_MAX);
struct AuxInfoIn {
    AuxInfoInType auxInfoInType;
    union {
        struct {
            uint32_t status;
            uint8_t sR;
        } cqe;
        struct {
            uint32_t eventType;
        } ae;
    };
    u8 resv[7];
};

constexpr u32 MAX_AUX_INFO_NUM = 256;
struct AuxInfoOut {
    uint32_t auxInfoTypes[MAX_AUX_INFO_NUM];
    uint32_t auxInfoValues[MAX_AUX_INFO_NUM];
    uint32_t auxInfoNum{0};
};
HcclResult RaGetAuxInfo(const RdmaHandle rdmaHandle, AuxInfoIn auxInfoIn, AuxInfoOut &auxInfoOut);

MAKE_ENUM(JettyStatus, RESET, READY, SUSPENDED, ERROR);
constexpr u32 MAX_JETTY_QUERY_NUM = 128;
HcclResult RaBatchQueryJettyStatus(const std::vector<JettyHandle> &jettyHandles, std::vector<JettyStatus> &jettyAttrs, u32 &num);

struct ConnJettyInfo {
    RdmaHandle rdmaHandle{nullptr};
    JettyHandle remoteJetty{0};
    JettyHandle localJetty{0};
};

struct BatchDeleteJettyInfo {
    std::unordered_map<RdmaHandle, std::unordered_set<JettyHandle>> unimportJettyList;
    std::unordered_map<RdmaHandle, std::unordered_set<JettyHandle>> deleteJettyList;
};
constexpr u32 MAX_DELETE_JETTY_NUMS = 768;
HcclResult HrtRaCtxQpDestoryBatch(const RdmaHandle handle, const std::unordered_set<JettyHandle> &jettyHandles, std::vector<JettyHandle> &failJettyHandles);

enum CcuMemTypeBitmap : uint64_t {
    CCU_MEMTYPE_INVALID = 0,
    CCU_MEMTYPE_INS = 1ULL << 0,
    CCU_MEMTYPE_GSA = 1ULL << 1,
    CCU_MEMTYPE_XN = 1ULL << 2,
    CCU_MEMTYPE_CKE = 1ULL << 3,
    CCU_MEMTYPE_LOOP_CKE = 1ULL << 4,
    CCU_MEMTYPE_PFE = 1ULL << 5,
    CCU_MEMTYPE_CHN = 1ULL << 6,
    CCU_MEMTYPE_JETTY_CTX = 1ULL << 7,
    CCU_MEMTYPE_MISSION_CTX = 1ULL << 8,
    CCU_MEMTYPE_LOOP_CTX = 1ULL << 9,
    CCU_MEMTYPE_MISSION_SQE = 1ULL << 10,
    CCU_MEMTYPE_CQE_BLOCK0 = 1ULL << 11,
    CCU_MEMTYPE_CQE_BLOCK1 = 1ULL << 12,
    CCU_MEMTYPE_CQE_BLOCK2 = 1ULL << 13,
    CCU_MEMTYPE_WQEBB = 1ULL << 14,
    CCU_MEMTYPE_MS_BLOCK0 = 1ULL << 32,
    CCU_MEMTYPE_MS_BLOCK1 = 1ULL << 33,
    CCU_MEMTYPE_MS_BLOCK2 = 1ULL << 34,
    CCU_MEMTYPE_MS_BLOCK3 = 1ULL << 35
};

struct CcuMemInfo {
 	CcuMemTypeBitmap memType{CcuMemTypeBitmap::CCU_MEMTYPE_INVALID};
 	uint64_t memVa{0};
 	uint32_t memSize{0};
 	uint32_t resv[1];
};

void HrtSetMemInfoList(struct CcuMemInfo *memInfoList, uint32_t count, struct ccu_mem_info *recvMemList);
HcclResult HrtGetCcuMemInfo(void* tlv_handle, uint32_t udieIdx, uint64_t memTypeBitmap, struct CcuMemInfo *memInfoList, uint32_t count);

HcclResult HrtRaGetEidByIp(RdmaHandle handle, const vector<IpAddress>& ipV4AddrList, vector<IpAddress>& eidAddrList);

HcclResult HrtRaSetTpAttrAsync(RdmaHandle handle, uint64_t tpHandle, uint32_t attrBitmap, TpAttr& attr, RequestHandle& reqHandle);
HcclResult HrtRaGetTpAttrAsync(u32 phyId, RdmaHandle handle, uint64_t tpHandle, uint32_t& attrBitmap, TpAttr& attr, RequestHandle& reqHandle);

constexpr u32 GET_UBOE_FLAG_ENABLE_OPCODE = 57;
constexpr u32 GET_UBOE_FLAG_ENABLE_VERSION = 2;
constexpr u32 UBOE_DEV_FLAG_RIGHT_SHIFT = 19;

HcclResult HrtGetUboeFlagEnable(const u32 devPhyId);

inline bool HrtCheckUboeSupported(const u32 devFeature)
{
    // 设备特性位掩码, 右移取UBOE标志位, 值为1表示支持
    return (devFeature >> UBOE_DEV_FLAG_RIGHT_SHIFT) & 1;
}

} // namespace Hccl
#endif // HCCLV2_ADAPTER_HCCP_H
