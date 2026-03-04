/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_ADAPTER_HCCP_H
#define HCOMM_ADAPTER_HCCP_H

#include <vector>
#include <string>

#include "hccp.h"
#include "hccp_ctx.h"

#include "enum_factory.h"
#include "hccl_rank_graph.h"

// orion 暂时复用
#include "ip_address.h"

namespace hcomm {

// 暂时放在adapter
constexpr uint32_t URMA_EID_LEN = 16;
constexpr uint8_t UB_ERR_TIMEOUT_8S = 16; // errTimeout 16-23: 芯片配置b10, 超时8s

using Eid = HccpEid; // 使用hccp定义的union表示eid

// 当前支持编译定义，后续考虑直接使用hccp数据结构
struct DevEidInfo {
    std::string name{};
    CommAddr commAddr{};
    uint32_t eidIndex{0};
    uint32_t type{0};
    uint32_t dieId{0};
    uint32_t chipId{0};
    uint32_t funcId{0};
};

HcclResult IpAddressToHccpEid(const Hccl::IpAddress &ipAddr, Eid &eid);
HcclResult IpAddressToReverseHccpEid(const Hccl::IpAddress &ipAddr, Eid &eid);

HcclResult RaGetDevEidInfos(const RaInfo &raInfo, std::vector<DevEidInfo> &devEidInfos);

using RequestHandle = u64;
MAKE_ENUM(RequestResult,
    COMPLETED,
    NOT_COMPLETED, SOCK_E_AGAIN,
    INVALID_PARA,
    GET_REQ_RESULT_FAILED, ASYNC_REQUEST_FAILED);

RequestResult HccpGetAsyncReqResult(RequestHandle &reqHandle);

using CtxHandle         = void *;
using JettyHandle       = void *;
using TargetJettyHandle = void *;
using JfcHandle         = void *;
using TokenIdHandle     = void *;

MAKE_ENUM(HrtTransportMode, RM, RC);
// STANDARD: URMA标准CreateJetty
// HOST_OFFLOAD: HOST侧展开下沉算子，需要指定sqeBbNum
// HOST_OPBASE: Host展开单算子，需要指定sqeBbNum,
// DEV_USED: 在Dev的APICPU展开算子，STARS不能使用UB DirectWQE的task，可以使用UB DbSend task，不需要指定sqeBbNum
// CACHE_LOCK_DWQE: 该模式下，      STARS仅能使用UB DirectWQE的task，不能使用UB DbSend task,，需要指定sqeBbNum
// CCU_CCUM_CACHE: 不需要指定sqeBbNum
MAKE_ENUM(HrtJettyMode, STANDARD, HOST_OFFLOAD, HOST_OPBASE, DEV_USED, CACHE_LOCK_DWQE, CCU_CCUM_CACHE);
MAKE_ENUM(HrtUbJfcMode, NORMAL, STARS_POLL, CCU_POLL);
using HrtRaUbCreateJettyParam = struct HrtRaUbJettyCreateParamDef {
    JfcHandle sjfcHandle{nullptr};
    JfcHandle rjfcHandle{nullptr};

    // CCU的DB需要注册，填写tokenValue
    u32 tokenValue{0};
    TokenIdHandle tokenIdHandle{0};

    HrtJettyMode jettyMode{HrtJettyMode::STANDARD};

    // 如果jettyId为0，则代表UB自行申请jetty,如果jettyId不为0，则代表使用预留jetty id
    // [1024, 1024 +127]为ccuJetty预留的id
    // [1024 + 192, 1024 + 192 + 4K - 1]为starsJetty预留的id
    u32 jettyId{0};

    // 指定内存，需要填写的参数，CCU类型 和 DEV_USED类型需要填写
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
};

HcclResult HccpUbCreateJetty(const CtxHandle ctxhandle, const HrtRaUbCreateJettyParam &in,
    HrtRaUbJettyCreatedOutParam &out);

HcclResult HccpUbCreateJettyAsync(const CtxHandle ctxhandle, const HrtRaUbCreateJettyParam &in,
    std::vector<char> &out, void *&jettyHandle, RequestHandle &reqHandle);

using HrtRaUbJettyImportedOutParam = struct HrtRaUbJettyImportedOutParamDef {
    TargetJettyHandle handle{0};
    u64               targetJettyVa{0};
    u32               tpn{0};
};

MAKE_ENUM(TpProtocol, CTP, RTP);

struct JettyImportCfg {
    u64 localTpHandle{0};
    u64 remoteTpHandle{0};
    u64 localTag{0};  // tag是hccp预留字段，暂不需要赋值
    u32 localPsn{0};
    u32 remotePsn{0};
    TpProtocol protocol{TpProtocol::INVALID};
};

HcclResult HccpUbTpImportJetty(const CtxHandle ctxHandle, u8 *key, const u32 keyLen,
    const u32 tokenValue, const JettyImportCfg &jettyImportCfg,
    HrtRaUbJettyImportedOutParam &out);

using HccpUbJettyImportedInParam = struct HccpUbJettyImportedInParamDef {
    u8 *key{nullptr};
    u32 keyLen{0};
    u32 tokenValue{0};
    JettyImportCfg jettyImportCfg{};
};

HcclResult HccpUbTpImportJettyAsync(const CtxHandle ctxHandle,
    const HccpUbJettyImportedInParam &in, std::vector<char> &out,
    void *&remQpHandle, RequestHandle &reqHandle);

} // namespace hcomm
#endif // HCOMM_ADAPTER_HCCP_H
