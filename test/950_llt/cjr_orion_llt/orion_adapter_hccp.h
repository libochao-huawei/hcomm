/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: orion adapter hccp header file
 * 本头文件不包含 hccp头文件
 * Author: zhangqingwei
 * Create: 2024-02-04
 */

#ifndef HCCLV2_ADAPTER_HCCP_H
#define HCCLV2_ADAPTER_HCCP_H
#include <vector>
#include "ip_address.h"
#include "data_type.h"
#include "reduce_op.h"
#include "hccp_tlv.h"

namespace Hccl {
using namespace std;
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

// 适配URMA，直接组装WQE的TOKENID需要进行移位，包括CCU与AICPU
constexpr u32 URMA_TOKEN_ID_RIGHT_SHIFT = 8;

// HCCL 默认无效端口号
constexpr u32 HCCL_INVALID_PORT = 65536;

using RdmaHandle = void *;
using QpHandle   = void *;

using SocketHandle = void *;
using FdHandle     = void *;

MAKE_ENUM(HrtNetworkMode, PEER, HDC)

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
void HrtRaTlvRequest(void* tlv_handle, u32 tlv_module_type, u32 tlv_ccu_msg_type);
void HrtRaTlvDeInit(void* tlv_handle);

u32 HrtRaGetInterfaceVersion(u32 phyId, u32 interfaceOpcode);

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

vector<std::pair<std::string, IpAddress>> HrtGetHostIf(u32 devPhyId);
vector<IpAddress>                         HrtGetDeviceIp(u32 devicePhyId);

constexpr u32 RDMA_MEM_KEY_MAX_LEN  = 64; // 最大的memKey长度
constexpr u32 RDMA_MEM_KEY_LEN_ROCE = 4;  // 暂定ROCE k的ey长度为4， 未来从HCCP新接口获取key真实长度

RdmaHandle HrtRaRdmaInit(HrtNetworkMode netMode, RaInterface &in);
void       HrtRaRdmaDeInit(RdmaHandle rdmaHandle);

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

MAKE_ENUM(HrtUbJfcMode, NORMAL, STARS_POLL, CCU_POLL)

JfcHandle HrtRaUbCreateJfc(RdmaHandle handle, HrtUbJfcMode mode);

void HrtRaUbDestroyJfc(RdmaHandle handle, JfcHandle jfcHandle);

MAKE_ENUM(HrtTransportMode, RM, RC)
MAKE_ENUM(TpProtocol, CTP, TP);

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

RequestHandle HrtRaSocketSendAsync(const FdHandle fdHandle, const void *data, u32 size,
    unsigned long long &sentSize);
RequestHandle HrtRaSocketRecvAsync(const FdHandle fdHandle, void *data, u32 size,
    unsigned long long &recvSize);

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

    explicit RaUbGetTpInfoParamDef() = default;
    RaUbGetTpInfoParamDef(const IpAddress &locAddr, const IpAddress &rmtAddr, TpProtocol tpProtocol)
        : locAddr(locAddr), rmtAddr(rmtAddr), tpProtocol(tpProtocol){};

    std::string Describe() const {
        return StringFormat("RaUbGetTpInfoParam[locAddr=%s, rmtAddr=%s, tpProtocol=%s]",
            locAddr.Describe().c_str(), rmtAddr.Describe().c_str(),
            tpProtocol.Describe().c_str());
    }
};

RequestHandle RaUbGetTpInfoAsync(const RdmaHandle rdmaHandle, const RaUbGetTpInfoParam &param, vector<char_t> &out, uint32_t &num);

RequestHandle RaUbImportJettyAsync(const RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void* &remQpHandle);
RequestHandle RaUbTpImportJettyAsync(const RdmaHandle rdmaHandle, const HrtRaUbJettyImportedInParam &in,
    vector<char_t> &out, void *&remQpHandle);
RequestHandle RaUbUnimportJettyAsync(void* targetJettyHandle);

struct SocketEventInfo {
    u32 event;
    FdHandle fdHandle;
};

void HrtRaWaitEventHandle(int event_handle, std::vector<SocketEventInfo> &event_infos, int timeout,
                          unsigned int maxevents, u32 &events_num);
void HrtRaGetSecRandom(u32 *value, u32 &devPhyId);
} // namespace Hccl
#endif // HCCLV2_ADAPTER_HCCP_H
