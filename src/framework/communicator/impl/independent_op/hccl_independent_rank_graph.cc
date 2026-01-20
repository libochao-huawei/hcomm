/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api.h"
#include "log.h"
#include "hccl_comm_pub.h"
#include "independent_op.h"
#include <string>
#include "param_check_pub.h"
#include "hccl_comm.h"
#include "hccl_inner.h"
#if defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)
#include "op_base_v2.h"
#endif

using namespace hccl;

#ifndef CCL_KERNEL_AICPU
HcclResult HcclGetRankGraph(HcclComm comm, GraphType type, void **graph, uint32_t *len)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(graph);
    CHK_PTR_NULL(len);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetRankGraph(type, graph, len);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to HcclGetRankGraph ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], len[%u]", __func__, hcclComm->GetIdentifier().c_str(), *len);
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetLinks(HcclComm comm, uint32_t netLayer, uint32_t srcRank, uint32_t dstRank,
    CommLink **links, uint32_t *linkNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(links);
    CHK_PTR_NULL(linkNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetLinksV2(comm, netLayer, srcRank, dstRank, links, linkNum), socNamePtr);
#endif
    if (srcRank == dstRank) {
        HCCL_ERROR("[%s] srcRank[%u] and dstRank[%u] is same", __func__, srcRank, dstRank);
        return HCCL_E_PARA;
    }
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HCCL_RUN_INFO("Entry-%s: comm[%s], netLayer%u], srcRank[%u], dstRank[%u]", __func__,
        hcclComm->GetIdentifier().c_str(), netLayer, srcRank, dstRank);
    HcclResult ret = hcclComm->GetLinks(netLayer, srcRank, dstRank, links, linkNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to get links for netLayer[%d], srcRank[%u], dstRank[%u]] ret[%d]",
            __func__, netLayer, srcRank, dstRank, ret);
        return ret;
    }
    HCCL_INFO("[%s] success: comm[%s] linkNum[%u]",  __func__, hcclComm->GetIdentifier().c_str(), *linkNum);
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetLayers(HcclComm comm, uint32_t **netLayers, uint32_t *netLayerNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(netLayers);
    CHK_PTR_NULL(netLayerNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetNetLayersV2(comm, netLayers, netLayerNum), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetNetLayers(netLayers, netLayerNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to GetCommNetLayers ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], netLayerNum size[%u]", __func__, hcclComm->GetIdentifier().c_str(), *netLayerNum);
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetTopoTypeByLayer(HcclComm comm, uint32_t netLayer, CommTopo *topoType)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(topoType);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(
        [&]() -> HcclResult {
            uint32_t uintTopoType = 0;
            CHK_RET(HcclGetInstTopoTypeByNetLayerV2(comm, netLayer, &uintTopoType));
            *topoType = static_cast<CommTopo>(uintTopoType);
            return HCCL_SUCCESS;
        }(), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetInstTopoTypeByNetLayer(netLayer, topoType);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], [%d]", __func__, hcclComm->GetIdentifier().c_str(), *topoType);
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetRankSizeByLayer(HcclComm comm, uint32_t netLayer, uint32_t *rankNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(rankNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetInstSizeByNetLayerV2(comm, netLayer, rankNum), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetInstSizeByNetLayer(netLayer, rankNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], rankNum[%u]", __func__, hcclComm->GetIdentifier().c_str(), *rankNum);
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetRanksByLayer(HcclComm comm, uint32_t netLayer, uint32_t **ranks, uint32_t *rankNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(rankNum);
    CHK_PTR_NULL(ranks);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetInstRanksByNetLayerV2(comm, netLayer, ranks, rankNum), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetInstRanksByNetLayer(netLayer, ranks, rankNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], rankNum[%u]", __func__, hcclComm->GetIdentifier().c_str(), *rankNum);
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetInstSizeListByLayer(HcclComm comm, uint32_t netLayer, uint32_t **instSizeList, uint32_t *listSize)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(instSizeList);
    CHK_PTR_NULL(listSize);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetInstSizeListByNetLayerV2(comm, netLayer, instSizeList, listSize), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetInstSizeListByNetLayer(netLayer, instSizeList, listSize);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], listSize[%u]", __func__, hcclComm->GetIdentifier().c_str(), *listSize);
    return HCCL_SUCCESS;
}

HcclResult HcclGetTopoInstsByLayer(HcclComm comm, uint32_t netLayer, uint32_t **topoInsts, uint32_t *topoInstNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(topoInsts);
    CHK_PTR_NULL(topoInstNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetTopoInstsByLayerV2(comm, netLayer, topoInsts, topoInstNum), socNamePtr);
#endif
    return HCCL_SUCCESS;
}

HcclResult HcclGetTopoType(HcclComm comm, uint32_t netLayer, uint32_t topoInstId, CommTopo *topoType)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(topoType);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetTopoTypeV2(comm, netLayer, topoInstId, topoType), socNamePtr);
#endif
    return HCCL_SUCCESS;
}

HcclResult HcclGetRanksByTopoInst(HcclComm comm, uint32_t netLayer, uint32_t topoInstId, uint32_t **ranks, uint32_t *rankNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(ranks);
    CHK_PTR_NULL(rankNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetRanksByTopoInstV2(comm, netLayer, topoInstId, ranks, rankNum), socNamePtr);
#endif
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetEndpointNum(HcclComm comm, uint32_t layer, uint32_t topoInstId, uint32_t *num)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(num);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclRankGraphGetEndpointNumV2(comm, layer, topoInstId, num), socNamePtr);
#endif
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetEndpointDesc(HcclComm comm, uint32_t layer, uint32_t topoInstId, uint32_t *descNum, EndpointDesc *endpointDesc)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(descNum);
    CHK_PTR_NULL(endpointDesc);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclRankGraphGetEndpointDescV2(comm, layer, topoInstId, descNum, endpointDesc), socNamePtr);
#endif
    return HCCL_SUCCESS;
}

HcclResult HcclRankGraphGetEndpointInfo(HcclComm comm, uint32_t rankId, const EndpointDesc *endpointDesc, EndpointAttr endpointAttr, uint32_t infoLen, void *info)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(endpointDesc);
    CHK_PTR_NULL(info);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclRankGraphGetEndpointInfoV2(comm, rankId, endpointDesc, endpointAttr, infoLen, info));
#endif
    return HCCL_SUCCESS;
}

HcclResult HcclGetHeterogMode(HcclComm comm, HcclHeterogMode *mode)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(mode);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    HCCLV2_FUNC_RUN(HcclGetHeterogModeV2(comm, mode));
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->GetHeterogMode(mode);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], mode[%u]", __func__, hcclComm->GetIdentifier().c_str(), *mode);
    return HCCL_SUCCESS;
}

#endif

HcclResult HcclGetRankSize(HcclComm comm, uint32_t *rankSize)
{
    // 入参合法性校验
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(rankSize);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetRankSizeV2(comm, rankSize), socNamePtr);
#endif
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    u32 tmpRankSize = INVALID_VALUE_RANKSIZE;
    CHK_RET(hcclComm->GetRankSize(tmpRankSize));
    *rankSize = tmpRankSize;
    /* 关键状态记录 */
    HCCL_INFO("HcclGetRankSize success, rankSizePtr[%p], rankSize[%u]", rankSize, tmpRankSize);
    return HCCL_SUCCESS;
}

HcclResult HcclGetRankId(HcclComm comm, uint32_t *rank)
{
    // 入参合法性校验
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(rank);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetRankIdV2(comm, rank), socNamePtr);
#endif

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    u32 tmpRankId = INVALID_VALUE_RANKID;
    CHK_RET(hcclComm->GetUserRank(tmpRankId));
    *rank = tmpRankId;
    /* 关键状态记录 */
    HCCL_INFO("HcclGetRankId success, rankIdPtr[%p], rankId[%u]", rank, tmpRankId);
    return HCCL_SUCCESS;
}

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
HcclResult CommGetNetLayers(HcclComm comm, uint32_t **netLayers, uint32_t *netLayerNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(netLayers);
    CHK_PTR_NULL(netLayerNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetNetLayersV2(comm, netLayers, netLayerNum), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->CommGetNetLayers(netLayers, netLayerNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to GetCommNetLayers ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], netLayerNum size[%u]", __func__, hcclComm->GetIdentifier().c_str(), *netLayerNum);
    return HCCL_SUCCESS;
}

HcclResult CommGetInstTopoTypeByNetLayer(HcclComm comm, uint32_t netLayer, uint32_t *topoType)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(topoType);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetInstTopoTypeByNetLayerV2(comm, netLayer, topoType), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->CommGetInstTopoTypeByNetLayer(netLayer, topoType);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], [%d]", __func__, hcclComm->GetIdentifier().c_str(), *topoType);
    return HCCL_SUCCESS;
}

HcclResult CommGetInstSizeByNetLayer(HcclComm comm, uint32_t netLayer, uint32_t *rankNum)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(rankNum);
#if (defined (OPEN_BUILD_PROJECT) && defined (ORION_MODE)) && (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
    const char *socNamePtr = aclrtGetSocName();
    CHK_PTR_NULL(socNamePtr);
    HCCLV2_FUNC_RUN(HcclGetInstSizeByNetLayerV2(comm, netLayer, rankNum), socNamePtr);
#endif
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    HcclResult ret = hcclComm->CommGetInstSizeByNetLayer(netLayer, rankNum);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed, ret[%d]", __func__, ret);
        return ret;
    }
    HCCL_RUN_INFO("[%s] success, group[%s], rankNum[%u]", __func__, hcclComm->GetIdentifier().c_str(), *rankNum);
    return HCCL_SUCCESS;
}
#ifdef __cplusplus
}
#endif // __cplusplus