/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcomm_adapter_hccp.h"

#include "log.h"
#include "orion_adpt_utils.h"

#include "hccp_async.h"
#include "hccp_async_ctx.h"

namespace hcomm {

HcclResult IpAddressToHccpEid(const Hccl::IpAddress &ipAddr, Eid &eid)
{
    HCCL_INFO("EID ipAddr[%s]", ipAddr.Describe().c_str());
    int32_t sRet = memcpy_s(eid.raw, sizeof(eid.raw), ipAddr.GetEid().raw, sizeof(ipAddr.GetEid().raw));
    if (sRet != EOK) {
        HCCL_ERROR("[%s] memcpy failed[%d].", __func__, sRet);
        return HcclResult::HCCL_E_MEMORY;
    }
    HCCL_INFO("[IpAddressToHccpEid] hccpEid.in6.subnetPrefix[%016llx], hccpEid.in6.interfaceId[%016llx]",
              eid.in6.subnet_prefix, eid.in6.interface_id);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult IpAddressToReverseHccpEid(const Hccl::IpAddress &ipAddr, Eid &eid)
{
    HCCL_INFO("EID ipAddr[%s]", ipAddr.Describe().c_str());
    int32_t sRet = memcpy_s(eid.raw, sizeof(eid.raw),
        ipAddr.GetReverseEid().raw, sizeof(ipAddr.GetReverseEid().raw));
    if (sRet != EOK) {
        HCCL_ERROR("[%s] memcpy failed[%d].", __func__, sRet);
        return HcclResult::HCCL_E_MEMORY;
    }
    HCCL_INFO("[IpAddressToHccpEid] hccpEid.in6.subnetPrefix[%016llx], hccpEid.in6.interfaceId[%016llx]",
              eid.in6.subnet_prefix, eid.in6.interface_id);
    return HcclResult::HCCL_SUCCESS;
}

inline Hccl::IpAddress HccpEidToIpAddress(Eid& hccpEid)
{
    Hccl::Eid eid{};
    HCCL_INFO("[HccpEidToIpAddress] hccpEid.in6.subnetPrefix[%016llx], hccpEid.in6.interfaceId[%016llx]",
              hccpEid.in6.subnet_prefix, hccpEid.in6.interface_id);
    s32 sRet = memcpy_s(eid.raw, sizeof(eid.raw), hccpEid.raw, sizeof(hccpEid.raw));
    if (sRet != EOK) {
        HCCL_ERROR("failed to change eid to ip");
        return Hccl::IpAddress{}; // 暂时不处理
    }
    return Hccl::IpAddress(eid);
}

HcclResult RaGetDevEidInfos(const RaInfo &raInfo, std::vector<DevEidInfo> &devEidInfos)
{
    uint32_t num = 0;
    int32_t ret = ra_get_dev_eid_info_num(raInfo, &num);
    if (ret != 0) {
        HCCL_ERROR("call ra_get_dev_eid_info_num failed, error code =%d.", ret);
        return HcclResult::HCCL_E_NETWORK;
    }

    struct dev_eid_info infoList[num] = {};
    ret = ra_get_dev_eid_info_list(raInfo, infoList, &num);
    if (ret != 0) {
        HCCL_ERROR("call ra_get_dev_eid_info_list failed, error code =%d.", ret);
        return HcclResult::HCCL_E_NETWORK;
    }

    devEidInfos.resize(num);
    for (uint32_t i = 0; i < num; i++) {
        devEidInfos[i].name = (infoList[i].name);
        Hccl::IpAddress ipAddr = HccpEidToIpAddress(infoList[i].eid);
        CHK_RET(IpAddressToCommAddr(ipAddr, devEidInfos[i].commAddr));
        devEidInfos[i].type = infoList[i].type;
        devEidInfos[i].eidIndex = infoList[i].eid_index;
        devEidInfos[i].dieId = infoList[i].die_id;
        devEidInfos[i].chipId = infoList[i].chip_id;
        devEidInfos[i].funcId = infoList[i].func_id;
    }

    return HcclResult::HCCL_SUCCESS;
}

RequestResult HccpGetAsyncReqResult(RequestHandle &reqHandle)
{
    if (reqHandle == 0) {
        HCCL_ERROR("[%s] failed, reqHandle is 0.", __func__);
        return RequestResult::INVALID_PARA;
    }

    int reqResult = 0;
    int32_t ret = RaGetAsyncReqResult(reinterpret_cast<void *>(reqHandle), &reqResult);
    // 返回 OTHERS_EAGAIN 代表查询到异步任务未完成，需要重新查询，此时保留handle
    if (ret == OTHERS_EAGAIN) {
        return RequestResult::NOT_COMPLETED;
    }

    // 返回码非0代表调用查询接口失败，当前仅入参错误时触发
    if (ret != 0) {
        HCCL_ERROR("[%s] failed to get asynchronous request result[%d], "
            "reqhandle[%llx].", __func__, ret, reqHandle);
        return RequestResult::GET_REQ_RESULT_FAILED;
    }

    RequestHandle tmpReqHandle = reqHandle;
    // 返回码为 0 时，reqResult为异步任务完成结果，0代表成功，其他值代表失败
    // SOCK_EAGAIN 为 socket 类执行结果，代表 socket 接口失败需要重试
    if (reqResult == SOCK_EAGAIN) {
        return RequestResult::SOCK_E_AGAIN;
    }

    if (reqResult != 0) {
        HCCL_ERROR("[%s] failed, the asynchronous request "
            "error[%d], reqhandle[%llx].", __func__, reqResult, tmpReqHandle);
        return RequestResult::ASYNC_REQUEST_FAILED;
    }

    return RequestResult::COMPLETED;
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

constexpr uint8_t  RNR_RETRY = 7;
constexpr uint32_t RQ_DEPTH  = 256;

HcclResult HccpUbCreateJetty(const CtxHandle ctxHandle, const HrtRaUbCreateJettyParam &in, HrtRaUbJettyCreatedOutParam &out)
{
    struct qp_create_attr attr{};
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
    } else if (in.jettyMode == HrtJettyMode::CCU_CCUM_CACHE) {
        attr.ub.token_value                   = in.tokenValue;
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 1;
        attr.ub.ext_mode.sq.buff_size         = in.sqBufSize;
        attr.ub.ext_mode.sq.buff_va           = in.sqBufVa;
    } else if (in.jettyMode == HrtJettyMode::DEV_USED) {
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 1;
        attr.ub.ext_mode.sq.buff_size         = in.sqBufSize;
        attr.ub.ext_mode.sq.buff_va           = in.sqBufVa;
    }

    // 其他Mode暂时不需要额外更新特定字段
    HCCL_INFO("Create jetty, input params: attr.ub.jetty_id[%u], attr.rq_depth[%u], "
        "attr.sq_depth[%u], attr.transport_mode[%d], attr.ub.mode[%d], "
        "attr.ub.ext_mode.sqebb_num[%u], attr.ub.ext_mode.sq.buff_va[%llx], "
        "attr.ub.ext_mode.sq.buff_size[%u], attr.ub.ext_mode.pi_type[%u].",
        attr.ub.jetty_id, attr.rq_depth, attr.sq_depth, attr.transport_mode,
        attr.ub.mode, attr.ub.ext_mode.sqebb_num, attr.ub.ext_mode.sq.buff_va,
        attr.ub.ext_mode.sq.buff_size, attr.ub.ext_mode.pi_type);

    struct qp_create_info info {};
    void *qpHandle = nullptr;
    int32_t ret = ra_ctx_qp_create(ctxHandle, &attr, &info, &qpHandle);
    if (ret != 0) {
        HCCL_ERROR("[%s] failed, ctxHandle[%p] jetty_id[%u] jetty_mode[%s] "
            "sq_depth[%u] sq.buff_va[%llx] sq.buff_size[%u].", __func__,
            ctxHandle, attr.ub.jetty_id, in.jettyMode.Describe().c_str(),
            attr.sq_depth, attr.ub.ext_mode.sq.buff_va,
            attr.ub.ext_mode.sq.buff_size);
        return HcclResult::HCCL_E_NETWORK;
    }

    // 适配URMA，直接组装WQE的TOKENID需要进行移位，包括CCU与AICPU
    constexpr u32 URMA_TOKEN_ID_RIGHT_SHIFT = 8;

    out.handle    = reinterpret_cast<JettyHandle>(qpHandle);
    out.id        = info.ub.id;
    out.uasid     = info.ub.uasid;
    out.jettyVa   = info.va;
    out.dbVa      = info.ub.db_addr;
    out.dbTokenId = info.ub.db_token_id >> URMA_TOKEN_ID_RIGHT_SHIFT;

    int32_t sRet = memcpy_s(out.key, sizeof(out.key), info.key.value, info.key.size);
    if (sRet != 0) {
        HCCL_ERROR("[%s] failed, memcpy failed[%d].", __func__, sRet);
        return HcclResult::HCCL_E_MEMORY;
    }
    out.keySize = info.key.size;
    attr.ub.token_value = 0; // 清理栈中的敏感信息
    HCCL_INFO("[%s], output params: out.id[%u], out.dbVa[%llx]", __func__, out.id, out.dbVa);

    return HcclResult::HCCL_SUCCESS;
}

HcclResult HccpUbCreateJettyAsync(const CtxHandle ctxhandle, const HrtRaUbCreateJettyParam &in,
    std::vector<char> &out, void *&jettyHandle, RequestHandle &reqHandle)
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
    } else if (in.jettyMode == HrtJettyMode::CCU_CCUM_CACHE) {
        attr.ub.token_value                   = in.tokenValue;
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 1;
        attr.ub.ext_mode.sq.buff_size         = in.sqBufSize;
        attr.ub.ext_mode.sq.buff_va           = in.sqBufVa;
    } else if (in.jettyMode == HrtJettyMode::DEV_USED) {
        attr.ub.ext_mode.cstm_flag.bs.sq_cstm = 1;
        attr.ub.ext_mode.sq.buff_size         = in.sqBufSize;
        attr.ub.ext_mode.sq.buff_va           = in.sqBufVa;
    }

    // 其他Mode暂时不需要额外更新特定字段
    HCCL_INFO("Create jetty, input params: attr.ub.jetty_id[%u], attr.rq_depth[%u], "
              "attr.sq_depth[%u], attr.transport_mode[%d], attr.ub.mode[%d], "
              "attr.ub.ext_mode.sqebb_num[%u], attr.ub.ext_mode.sq.buff_va[%llx], "
              "attr.ub.ext_mode.sq.buff_size[%u], attr.ub.ext_mode.pi_type[%u], priority[%u].",
              attr.ub.jetty_id, attr.rq_depth, attr.sq_depth, attr.transport_mode, attr.ub.mode,
              attr.ub.ext_mode.sqebb_num, attr.ub.ext_mode.sq.buff_va, attr.ub.ext_mode.sq.buff_size,
              attr.ub.ext_mode.pi_type, attr.ub.priority);

    void *raReqHandle = nullptr;
    out.resize(sizeof(qp_create_info));
    s32 ret = ra_ctx_qp_create_async(ctxhandle, &attr, reinterpret_cast<qp_create_info *>(out.data()),
        &jettyHandle, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[%s] failed, call interface error[%d], raReqHandle[%p], "
            "ctxHanlde[%p].", __func__, ret, raReqHandle, ctxhandle);
        return HcclResult::HCCL_E_NETWORK;
    }
    attr.ub.token_value = 0; // 清理栈中的token信息
    HCCL_INFO("[%s] ok, get handle[%llu].", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    reqHandle = reinterpret_cast<RequestHandle>(raReqHandle);
    return HcclResult::HCCL_SUCCESS;
}

static HcclResult ImportJetty(const CtxHandle ctxHandle, u8 *key,
    const u32 keyLen, const u32 tokenValue, const jetty_import_exp_cfg &cfg,
    const jetty_import_mode mode, const TpProtocol protocol,
    HrtRaUbJettyImportedOutParam &out)
{
    if (mode == jetty_import_mode::JETTY_IMPORT_MODE_NORMAL) {
        HCCL_ERROR("[%s] currently not support JETTY_IMPORT_MODE_NORMAL.",
            __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    if (protocol != TpProtocol::RTP && protocol != TpProtocol::CTP) {
        HCCL_ERROR("[%s] failed, tp protocol[%s] is not expected.",
        __func__, protocol.Describe().c_str());
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    struct qp_import_info_t info {};
    int res = memcpy_s(info.in.key.value, sizeof(info.in.key.value), key, keyLen);
    if (res != 0) {
        HCCL_ERROR("[%s] memcpy_s failed, ret = %d", __func__, res);
        return HcclResult::HCCL_E_MEMORY;
    }
    info.in.key.size = keyLen;

    info.in.ub.mode = mode;
    info.in.ub.token_value = tokenValue;
    info.in.ub.policy = jetty_grp_policy::JETTY_GRP_POLICY_RR;
    info.in.ub.type = target_type::TARGET_TYPE_JETTY;

    info.in.ub.flag.value = 0;
    info.in.ub.flag.bs.token_policy = TOKEN_POLICY_PLAIN_TEXT;

    info.in.ub.exp_import_cfg = cfg;
    // tp_type: 0->RTP, 1->CTP
    info.in.ub.tp_type = protocol == TpProtocol::RTP ? 0 : 1;

    void *remQpHandle = nullptr;
    int32_t ret = ra_ctx_qp_import(ctxHandle, &info, &remQpHandle);
    if (ret != 0) {
        HCCL_ERROR("[%s] failed, ctxHandle[%p] loc tp handle[%llx] "
            "rmt tp handle[%llx] loc tag[%llu] loc psn[%u] rmt psn[%u]"
            "protocol[%s].", __func__, ctxHandle, cfg.tp_handle, cfg.peer_tp_handle,
            cfg.tag, cfg.tx_psn, cfg.rx_psn);
        return HcclResult::HCCL_E_NETWORK;
    }

    out.handle        = reinterpret_cast<TargetJettyHandle>(remQpHandle);
    out.targetJettyVa = info.out.ub.tjetty_handle;
    out.tpn           = info.out.ub.tpn;
    info.in.ub.token_value = 0; // 清理栈中的敏感信息
    return HcclResult::HCCL_SUCCESS;
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

HcclResult HccpUbTpImportJetty(const CtxHandle ctxHandle, u8 *key, const u32 keyLen,
    const u32 tokenValue, const JettyImportCfg &jettyImportCfg,
    HrtRaUbJettyImportedOutParam &out)
{
    struct jetty_import_exp_cfg cfg = GetTpImportCfg(jettyImportCfg);
    const auto mode = jetty_import_mode::JETTY_IMPORT_MODE_EXP;
    return ImportJetty(ctxHandle, key, keyLen, tokenValue,
        cfg, mode, jettyImportCfg.protocol, out);
}

static HcclResult ImportJettyAsync(CtxHandle ctxHandle, const HccpUbJettyImportedInParam &in,
    std::vector<char> &out, void *&remQpHandle, const jetty_import_exp_cfg &cfg, jetty_import_mode mode,
    TpProtocol protocol, RequestHandle &reqHandle)
{
    if (mode == jetty_import_mode::JETTY_IMPORT_MODE_NORMAL) {
        HCCL_ERROR("[%s] currently not support JETTY_IMPORT_MODE_NORMAL.",
            __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    out.resize(sizeof(qp_import_info_t));
    struct qp_import_info_t *info = reinterpret_cast<qp_import_info_t *>(out.data());

    s32 ret = memcpy_s(info->in.key.value, sizeof(info->in.key.value), in.key, in.keyLen);
    if (ret != 0) {
        HCCL_ERROR("[%s] memcpy_s failed, ret=%d.", __func__, ret);
        return HcclResult::HCCL_E_MEMORY;
    }

    info->in.key.size = in.keyLen;
    info->in.ub.mode = mode;
    info->in.ub.token_value = in.tokenValue;
    info->in.ub.policy = jetty_grp_policy::JETTY_GRP_POLICY_RR;
    info->in.ub.type = target_type::TARGET_TYPE_JETTY;

    info->in.ub.flag.value = 0;
    info->in.ub.flag.bs.token_policy = TOKEN_POLICY_PLAIN_TEXT;

    info->in.ub.exp_import_cfg = cfg;

    if (protocol != TpProtocol::RTP && protocol != TpProtocol::CTP) {
        HCCL_ERROR("[%s] failed, tp protocol[%s] is not expected, %s.",
        __func__, protocol.Describe().c_str());
        return HcclResult::HCCL_E_PARA;
    }
    // tp_type: 0->RTP, 1->CTP
    info->in.ub.tp_type = protocol == TpProtocol::RTP ? 0 : 1;

    void *raReqHandle = nullptr;
    ret = ra_ctx_qp_import_async(ctxHandle, info, &remQpHandle, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "ctxHandle[%p].", __func__, ret, raReqHandle, ctxHandle);
        return HcclResult::HCCL_E_NETWORK;
    }
    info->in.ub.token_value = 0;
    HCCL_INFO("[%s] ok, get handle[%llu]", __func__, reinterpret_cast<RequestHandle>(raReqHandle));
    reqHandle = reinterpret_cast<RequestHandle>(raReqHandle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult HccpUbTpImportJettyAsync(const CtxHandle ctxHandle, const HccpUbJettyImportedInParam &in,
    std::vector<char> &out, void *&remQpHandle, RequestHandle &reqHandle)
{
    struct jetty_import_exp_cfg cfg = GetTpImportCfg(in.jettyImportCfg);
    const auto mode = jetty_import_mode::JETTY_IMPORT_MODE_EXP;
    return ImportJettyAsync(ctxHandle, in, out, remQpHandle,
        cfg, mode, in.jettyImportCfg.protocol, reqHandle);
}

} // namespace hcomm