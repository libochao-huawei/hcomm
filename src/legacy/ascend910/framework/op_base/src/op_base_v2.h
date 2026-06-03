/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OP_BASE_V2_H
#define OP_BASE_V2_H

#include <functional>
#include "hccl_types.h"
#include "task_param.h"

namespace Hccl {
using ProfCallback = std::function<HcclResult(const TaskParam&, uint64_t)>;
}

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

HcclResult HcclCommDestroyV2(HcclComm comm);

HcclResult __attribute__((weak, used, noinline)) HcclCommInitClusterInfoV2(const char *clusterInfo, uint32_t rank, HcclComm *comm);

HcclResult __attribute__((weak, used, noinline)) HcclCommInitClusterInfoConfigV2(
    const char *clusterInfo, uint32_t rank, HcclCommConfig *config, HcclComm *comm);

HcclResult __attribute__((weak, used, noinline)) HcclCommInitAllV2(uint32_t ndev, int32_t *devices, HcclComm *comms);

HcclResult __attribute__((weak, used, noinline)) HcclCommInitClusterInfoMemConfigV2(const char *rankTableString, uint32_t rank,
                                            HcclCommConfig *config, HcclComm *comm);

HcclResult __attribute__((weak, used, noinline)) HcclAlltoAllV2(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
    uint64_t recvCount, HcclDataType recvType, HcclComm comm, aclrtStream stream);
HcclResult __attribute__((weak, used, noinline)) HcclAlltoAllVV2(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
    const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType, HcclComm comm,
    aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclCreateSubCommConfigV2(HcclComm *comm, uint32_t rankNum, uint32_t *rankIds, uint64_t subCommId,
    uint32_t subCommRankId, HcclCommConfig *config, HcclComm *subComm);

HcclResult __attribute__((weak, used, noinline)) HcclGetRankIdV2(HcclComm comm, uint32_t *rank);

HcclResult __attribute__((weak, used, noinline)) HcclGetRootInfoV2(HcclRootInfo *rootInfo);

HcclResult __attribute__((weak, used, noinline)) HcclGetCommNameV2(HcclComm commHandle, char *commName);

HcclResult __attribute__((weak, used, noinline)) HcclCommInitRootInfoV2(
    uint32_t nRanks, const HcclRootInfo *rootInfo, uint32_t rank, HcclComm *comm, std::string &identifier);

HcclResult __attribute__((weak, used, noinline)) HcclCommInitRootInfoConfigV2(uint32_t nRanks, const HcclRootInfo *rootInfo,
    uint32_t rank, const HcclCommConfig *config, HcclComm *comm);

HcclResult __attribute__((weak, used, noinline)) HcclGetRankSize(HcclComm comm, uint32_t *rankSize);

HcclResult __attribute__((weak, used, noinline)) HcclAlltoAllVCV2(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType,
    const void *recvBuf, HcclDataType recvType, HcclComm comm, rtStream_t stream);

HcclResult __attribute__((weak, used, noinline)) HcclReduceV2(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
    uint32_t root, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclAllReduceV2(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
    HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclBroadcastV2(void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm,
    aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclGetTopoDescV2();

HcclResult __attribute__((weak, used, noinline)) HcclScatterV2(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, uint32_t root,
    HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclCommSuspend(HcclComm comm);

HcclResult __attribute__((weak, used, noinline)) HcclReduceScatterV2(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
    HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclReduceScatterVV2(void *sendBuf, void *sendCounts, void *sendDispls, void *recvBuf,
    uint64_t recvCount, HcclDataType dataType, HcclReduceOp op, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclAllGatherV2(
    void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclAllGatherVV2(void *sendBuf, uint64_t sendCount, void *recvBuf, void *recvCounts,
    void *recvDispls, HcclDataType dataType, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclSendV2(
    void *sendBuf, uint64_t count, HcclDataType dataType, uint32_t destRank, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclRecvV2(
    void *recvBuf, uint64_t count, HcclDataType dataType, uint32_t srcRank, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclBatchSendRecvV2(HcclSendRecvItem *sendRecvInfo, uint32_t itemNum, HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclGetRankSizeV2(HcclComm comm, uint32_t *rankSize);

HcclResult __attribute__((weak, used, noinline)) HcclAllocComResourceByTilingV2(HcclComm comm, void *stream, void *mc2Tiling, void **commContext);

HcclResult __attribute__((weak, used, noinline)) HcclCommSuspendV2(HcclComm comm);

HcclResult __attribute__((weak, used, noinline)) HcclCommResumeV2(HcclComm comm);

HcclResult __attribute__((weak, used, noinline)) HcclCommResumeImplV2(HcclComm comm);

HcclResult __attribute__((weak, used, noinline)) HcclGetCommAsyncErrorV2();

HcclResult __attribute__((weak, used, noinline)) HcclGetRawCommHandle(const char *commName, HcclComm *commHandle);

HcclResult __attribute__((weak, used, noinline)) HcclSetConfigV2(HcclConfig config, HcclConfigValue configValue);

HcclResult __attribute__((weak, used, noinline)) HcclGetConfigV2(HcclConfig config, HcclConfigValue *configValue);

HcclResult __attribute__((weak, used, noinline)) HcclBarrierV2(HcclComm comm, aclrtStream stream);

HcclResult __attribute__((weak, used, noinline)) HcclGetHeterogModeV2(HcclComm comm, HcclHeterogMode *mode);

HcclResult __attribute__((weak, used, noinline)) HcclGetRankGraphV2(HcclComm *comm, void **rankGraph);

HcclResult __attribute__((weak, used, noinline)) HcclGetCclBuffer(HcclComm comm, uintptr_t &cclBufferAddr, size_t &cclBufferSize, HcclMemType &cclBufferMemType);

HcclResult __attribute__((weak, used, noinline)) HcclCommWorkingDevNicSetV2(HcclComm comm, uint32_t *ranks, bool *useBackup, uint32_t nRanks);
 
HcclResult __attribute__((weak, used, noinline)) HcclCommSetMemoryRangeV2(HcclComm comm, void *baseVirPtr, size_t size, size_t alignment, uint64_t flags);
 
HcclResult __attribute__((weak, used, noinline)) HcclCommUnsetMemoryRangeV2(HcclComm comm, void *baseVirPtr);
 
HcclResult __attribute__((weak, used, noinline)) HcclCommActivateCommMemoryV2(HcclComm comm, void *virPtr, size_t size, size_t offset, void* handle, uint64_t flags);
 
HcclResult __attribute__((weak, used, noinline)) HcclCommDeactivateCommMemoryV2(HcclComm comm, void *virPtr);

HcclResult __attribute__((weak, used, noinline)) HcommFlushV2();

uint32_t __attribute__((weak, used, noinline)) HcclGetCommConfigCapabilityV2();
#if (!defined (HCCD)) && (!defined (CCL_KERNEL_AICPU))
HcclResult __attribute__((weak, used, noinline)) HcclGetCcuTaskInfoLegacy(HcclComm comm, void *tilingData, void *ccuTaskGroup);

HcclResult __attribute__((weak, used, noinline)) HcclGetNetLayersV2(HcclComm comm, uint32_t **netLayers, uint32_t *netLayerNum);

HcclResult __attribute__((weak, used, noinline)) HcclGetInstSizeByNetLayerV2(HcclComm comm, uint32_t netLayer, uint32_t *rankNum);

HcclResult __attribute__((weak, used, noinline)) HcclGetInstTopoTypeByNetLayerV2(HcclComm comm, uint32_t netLayer, uint32_t *topoType);

HcclResult __attribute__((weak, used, noinline)) CommGetCCLBufSizeCfgV2(HcclComm comm, uint64_t *cclBufSize);

HcclResult __attribute__((weak, used, noinline)) HcclGetInstRanksByNetLayerV2(HcclComm comm, uint32_t netLayer, uint32_t **ranks, uint32_t *rankNum);

HcclResult __attribute__((weak, used, noinline)) HcclGetInstSizeListByNetLayerV2(HcclComm comm, uint32_t netLayer, uint32_t **instSizeList, uint32_t *listSize);

HcclResult __attribute__((weak, used, noinline)) HcclGetLinksV2(HcclComm comm, uint32_t netLayer, uint32_t srcRank, uint32_t dstRank, CommLink **linkList, uint32_t *listSize);

HcclResult __attribute__((weak, used, noinline)) HcclGetTopoInstsByLayerV2(HcclComm comm, uint32_t netLayer, uint32_t **topoInsts, uint32_t *topoInstNum);

HcclResult __attribute__((weak, used, noinline)) HcclGetTopoTypeV2(HcclComm comm, uint32_t netLayer, uint32_t topoInstId, CommTopo *topoType);

HcclResult __attribute__((weak, used, noinline)) HcclGetRanksByTopoInstV2(HcclComm comm, uint32_t netLayer, uint32_t topoInstId, uint32_t **ranks, uint32_t *rankNum);

HcclResult __attribute__((weak, used, noinline)) HcclRankGraphGetEndpointNumV2(HcclComm comm, uint32_t layer, uint32_t topoInstId, uint32_t *num);

HcclResult __attribute__((weak, used, noinline)) HcclRankGraphGetEndpointDescV2(HcclComm comm, uint32_t layer, uint32_t topoInstId, uint32_t *descNum, EndpointDesc *endpointDesc);

HcclResult __attribute__((weak, used, noinline)) HcclRankGraphGetEndpointInfoV2(HcclComm comm, uint32_t rankId, const EndpointDesc *endpointDesc, EndpointAttr endpointAttr, uint32_t infoLen, void *info);

HcclResult __attribute__((weak, used, noinline)) HcclGetOpArgsV2(void **opArgs);
 	 
HcclResult __attribute__((weak, used, noinline)) HcclFreeOpArgsV2(void *opArgs);

HcclResult __attribute__((weak, used, noinline)) HcclSetOpSrcDataTypeV2(void *opArgs, uint8_t srcDataType);

HcclResult __attribute__((weak, used, noinline)) HcclSetOpDstDataTypeV2(void *opArgs, uint8_t dstDataType);

HcclResult __attribute__((weak, used, noinline)) HcclSetOpReduceTypeV2(void *opArgs, uint32_t reduceType);

HcclResult __attribute__((weak, used, noinline)) HcclSetOpCountV2(void *opArgs, uint64_t count);

HcclResult __attribute__((weak, used, noinline)) HcclSetOpAlgConfigV2(void *opArgs, char *algConfig);

HcclResult __attribute__((weak, used, noinline)) HcclSetOpCommEngineV2(void *opArgs, uint8_t commEngine);

HcclResult __attribute__((weak, used, noinline)) HcclCommResPrepareV2(HcclComm comm, char *opName, void *opArgs, void **addr);

HcclResult __attribute__((weak, used, noinline)) HcclDevMemAcquireV2(HcclComm comm, const char *memTag, uint64_t *size, void **addr, bool *newCreated);

HcclResult __attribute__((weak, used, noinline)) HcclGetHcclBufferV2(HcclComm comm, void **addr, uint64_t *size);

HcclResult __attribute__((weak, used, noinline)) HcclGetRemoteIpcHcclBufV2(HcclComm comm, uint64_t remoteRank, void **addr, uint64_t *size);

HcclResult __attribute__((weak, used, noinline)) HcclGetAicpuOpStreamAndNotifyV2(HcclComm comm, rtStream_t *opstream, u8 aicpuNotifyNum, void **aicpuNotify);

typedef int32_t(Callback)(uint64_t, int32_t);
HcclResult __attribute__((weak, used, noinline)) HcclTaskRegisterV2(HcclComm comm, const char *msgTag, Callback cb);
HcclResult __attribute__((weak, used, noinline)) HcclTaskUnRegisterV2(HcclComm comm, const char *msgTag);
HcclResult __attribute__((weak, used, noinline)) HcclTaskRegisterProfV2(HcclComm comm, Hccl::ProfCallback profCallback);
HcclResult __attribute__((weak, used, noinline)) HcclGetDpuSteamIdV2(HcclComm comm, u32 &dpuStreamId);
#endif
#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // OP_BASE_V2_H