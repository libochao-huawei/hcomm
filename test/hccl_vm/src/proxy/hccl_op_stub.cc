/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "acl/acl_rt.h"
#include "aiv_kernel/aiv_mode_stub/aiv_mode_stub_base.h"
#include "dtype_common.h"
#include "hccl/hccl_types.h"
#include "sim_common_defs.h"
#include "hccl_proxy_common.h"
#include "hccl_rank_graph.h"
#include "store_sim_store_pub.h"
#include "sim_log.h"
#include "store_sim_comm_pool_policy.h"
#include "store_sim_device_memory_manager.h"
#include "store_sim_memory_manager.h"
#include "store_sim_run_mode.h"
#include "db_sim_op_db_ops.h"
#include "db_sim_runner_ops.h"
 
void print_stacktrace() {
    const int max_frames = 64;
    void* buffer[max_frames];
 
    int nptrs = backtrace(buffer, max_frames);
    char** strings = backtrace_symbols(buffer, nptrs);
 
    if (strings == nullptr) {
        perror("backtrace_symbols");
        return;
    }
 
    fprintf(stderr, "==== Stack trace ====\n");
    for (int i = 0; i < nptrs; i++) {
        fprintf(stderr, "%s\n", strings[i]);
    }
    fprintf(stderr, "=====================\n");
 
    free(strings);
}
 
extern "C" uint8_t GetOpExpansionMode();

namespace {
    constexpr uint32_t INVALID_AIV_LAUNCH_INDEX = std::numeric_limits<uint32_t>::max();
    std::atomic<uint32_t> g_aivLaunchIndex{0};
    thread_local uint32_t g_currentAivLaunchIndex = INVALID_AIV_LAUNCH_INDEX;

    static std::vector<uint8_t> BuildOpDetailsV1(uint64_t count,
                                             uint16_t dataType, uint32_t reduceOpType, uint32_t reduceOp,
                                             HcclCMDType opType) {
        OpDetails details;
        std::memset(&details, 0, sizeof(details));
        details.opType = static_cast<uint16_t>(opType);
        details.dataType = dataType;
        details.reduceType = reduceOp;
        
        details.opV1.count = count;
        details.opV1.dataType = dataType;
        details.opV1.reduceOpType = reduceOpType;

        std::vector<uint8_t> blob(sizeof(OpDetails));
        std::memcpy(blob.data(), &details, sizeof(OpDetails));
        return blob;
    }

    static std::vector<uint8_t> BuildOpDetailsV2(uint64_t sendCount, uint16_t sendDataType,
                                             uint64_t recvCount, uint16_t recvDataType, uint16_t extInfo,
                                             HcclCMDType opType, uint16_t dataType, uint32_t reduceOp) {
        OpDetails details;
        std::memset(&details, 0, sizeof(details));
        details.opType = static_cast<uint16_t>(opType);
        details.dataType = dataType;
        details.reduceType = reduceOp;
        
        details.opV2.sendCount = sendCount;
        details.opV2.sendDataType = sendDataType;
        details.opV2.recvCount = recvCount;
        details.opV2.recvDataType = recvDataType;
        details.opV2.extInfo = extInfo;

        std::vector<uint8_t> blob(sizeof(OpDetails));
        std::memcpy(blob.data(), &details, sizeof(OpDetails));
        return blob;
    }

    static uint32_t GetDeviceType()
    {
       DevType devType;
       auto ret = hrtGetDeviceType(devType);
       if(ret != HcclResult::HCCL_SUCCESS){
         return ret;
       }
       return static_cast<uint32_t>(devType);
    }

    int RecordOpDbInfo(HcclCMDType cmdType,
        uint32_t rankId, uint64_t streamId,
        const void* inputBuf, uint32_t inputSize,
        const void* outputBuf, uint32_t outputSize,
        const std::vector<uint8_t>& details,
        uint32_t root,
        uint32_t rankSize,
        uint32_t srcRank,
        uint32_t dstRank,
        const std::vector<uint8_t>& extInfo = {})
    {
        (void) cmdType;
        sim::OpDetailTab opDetailTab;
        opDetailTab.id = 0;
        opDetailTab.pid = getpid();
        opDetailTab.rankId = rankId;
        opDetailTab.opIter = 0;
        opDetailTab.syncIter = 0;
        opDetailTab.streamId = streamId;
        opDetailTab.root = root;
        opDetailTab.opExpansionMode = GetOpExpansionMode();
        opDetailTab.devType = GetDeviceType();
        opDetailTab.rankSize = rankSize;
        opDetailTab.srcRank = srcRank;
        opDetailTab.dstRank = dstRank;
        opDetailTab.opDetail = details;
        opDetailTab.opExtInfo = extInfo;

        sim::OpMemInfoTab opMemInfoTab;
        opMemInfoTab.id = 0;
        opMemInfoTab.inputAddr = reinterpret_cast<uint64_t>(inputBuf);
        opMemInfoTab.inputSize = inputSize;
        opMemInfoTab.outputAddr = reinterpret_cast<uint64_t>(outputBuf);
        opMemInfoTab.outputSize = outputSize;
        opMemInfoTab.cclAddr = 0;
        opMemInfoTab.cclSize = 0;

        int ret = sim::InsertOpDetailAndMem(opDetailTab, opMemInfoTab);
        if (ret != 0) {
            HCCL_VM_ERROR("[RecordOpDbInfo] insert op detail+mem failed");
            return -1;
        }
        return 0;
    }
}

using namespace HcclSim;
namespace fs = std::filesystem;
 
#ifdef __cplusplus
extern "C" {
#endif

uint8_t GetOpExpansionMode()
{
    sim::SimOpExpansionMode mode = sim::SimOpExpansionMode::SIM_OP_EXPANSION_MODE_RESERVED;
    const char *expanEnv = std::getenv("HCCL_OP_EXPANSION_MODE");
    if (expanEnv == nullptr) {
        printf("[GetOpExpansionMode] HCCL_OP_EXPANSION_MODE env is not set, use default value: %d\n", mode);
        return static_cast<uint8_t>(mode);
    }

    std::string modeStr(expanEnv);
    if (modeStr == "AI_CPU") {
        mode = sim::SimOpExpansionMode::SIM_OP_EXPANSION_MODE_AICPU;
    } else if (modeStr == "AIV") {
        mode = sim::SimOpExpansionMode::SIM_OP_EXPANSION_MODE_AIV;
    } else if (modeStr == "CCU_SCHED" || modeStr == "CCU_MS") {
        mode = sim::SimOpExpansionMode::SIM_OP_EXPANSION_MODE_CCU;
    }

    printf("[GetOpExpansionMode] HCCL_OP_EXPANSION_MODE = %s\n", expanEnv);
    return static_cast<uint8_t>(mode);
}
 
HcclResult HcclAlltoAll(const void *sendBuf, uint64_t sendCount, HcclDataType sendType, const void *recvBuf,
    uint64_t recvCount, HcclDataType recvType, HcclComm comm, aclrtStream stream)
{
    printf("HcclAlltoAll called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  sendCount = %lu\n", sendCount);
    printf("  recvCount = %lu\n", recvCount);
    printf("  sendType = %d\n", sendType);
    printf("  recvType = %d\n", recvType);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    uint32_t inDataSize = 0;
    if (sim::GetDataTypeSize(sendType, inDataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t outDataSize = 0;
    if (sim::GetDataTypeSize(recvType, outDataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t inputSize = static_cast<uint32_t>(inDataSize * sendCount * rankSize);
    uint32_t outputSize = static_cast<uint32_t>(outDataSize * recvCount * rankSize);

    // Use BuildOpDetailsV2 for AlltoAll
    auto alltoallDetails = BuildOpDetailsV2(
        sendCount, sendType, recvCount, recvType, 0, HcclCMDType::HCCL_CMD_ALLTOALL, sendType, 0);
    std::vector<uint8_t> alltoallExtInfo;
    uint32_t alltoallCount = rankSize * rankSize;
    alltoallExtInfo.resize(sizeof(uint32_t) + alltoallCount * sizeof(uint64_t));
    std::memcpy(alltoallExtInfo.data(), &alltoallCount, sizeof(uint32_t));
    for (uint32_t i = 0; i < alltoallCount; i++) {
        std::memcpy(alltoallExtInfo.data() + sizeof(uint32_t) + i * sizeof(uint64_t), &sendCount, sizeof(uint64_t));
    }
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_ALLTOALL, curRank, reinterpret_cast<uint64_t>(stream),
                   sendBuf, inputSize, recvBuf, outputSize, alltoallDetails,
                   0, rankSize, curRank, curRank, alltoallExtInfo) != 0) {
        HCCL_VM_ERROR("[HcclAlltoAll] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }
    printf("HcclAlltoAll get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);

    using HcclAlltoAllFunc = HcclResult (*)(
        const void *, uint64_t, HcclDataType, const void *, uint64_t, HcclDataType, HcclComm, aclrtStream);
    HcclAlltoAllFunc hcclAlltoAllFunc = reinterpret_cast<HcclAlltoAllFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclAlltoAllFunc != nullptr) {
        return hcclAlltoAllFunc(sendBuf, sendCount, sendType, recvBuf, recvCount, recvType, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclAlltoAllV(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
                         const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
                         HcclComm comm, aclrtStream stream)
{
    printf("HcclAlltoAllV called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  sendType = %d\n", sendType);
    printf("  recvType = %d\n", recvType);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    // auto inDataSize = DATA_TYPE_SIZE_MAP.at(sendType);
    // auto outDataSize = DATA_TYPE_SIZE_MAP.at(recvType);
    uint32_t inDataSize = 0;
    if (sim::GetDataTypeSize(sendType, inDataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t outDataSize = 0;
    if (sim::GetDataTypeSize(recvType, outDataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t inCountTotal = 0;
    uint32_t outCountTotal = 0;
    for (uint32_t rank = 0; rank < rankSize; rank++) {
        inCountTotal += ((uint64_t*)sendCounts)[rank];
        outCountTotal += ((uint64_t*)recvCounts)[rank];
    }
    uint32_t inputSize = static_cast<uint32_t>(inDataSize * inCountTotal);
    uint32_t outputSize = static_cast<uint32_t>(outDataSize * outCountTotal);

    auto alltoallvDetails = BuildOpDetailsV2(
        static_cast<uint64_t>(inCountTotal), sendType, static_cast<uint64_t>(outCountTotal), recvType, 0,
        HcclCMDType::HCCL_CMD_ALLTOALLV, sendType, 0);

    std::vector<uint64_t> sendCountMatrix(rankSize * rankSize, 0);
    for (uint32_t j = 0; j < rankSize; j++) {
        sendCountMatrix[curRank * rankSize + j] = ((uint64_t*)sendCounts)[j];
    }

    for (uint32_t j = 0; j < rankSize; j++) {
        sendCountMatrix[j * rankSize + curRank] = ((uint64_t*)recvCounts)[j];
    }

    std::vector<uint8_t> alltoallvExtInfo;
    uint32_t alltoallvCount = rankSize * rankSize;
    alltoallvExtInfo.resize(sizeof(uint32_t) + alltoallvCount * sizeof(uint64_t));
    std::memcpy(alltoallvExtInfo.data(), &alltoallvCount, sizeof(uint32_t));
    std::memcpy(alltoallvExtInfo.data() + sizeof(uint32_t), sendCountMatrix.data(), alltoallvCount * sizeof(uint64_t));
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_ALLTOALLV, curRank, reinterpret_cast<uint64_t>(stream),
                   sendBuf, inputSize, recvBuf, outputSize, alltoallvDetails,
                   0, rankSize, curRank, curRank, alltoallvExtInfo) != 0) {
        HCCL_VM_ERROR("[HcclAlltoAllV] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }

    printf("HcclAlltoAllV get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);
 
    using HcclAlltoAllVFunc = HcclResult (*)(const void *,
        const void *,
        const void *,
        HcclDataType,
        const void *,
        const void *,
        const void *,
        HcclDataType,
        HcclComm,
        aclrtStream);
    HcclAlltoAllVFunc hcclAlltoAllVFunc = reinterpret_cast<HcclAlltoAllVFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclAlltoAllVFunc != nullptr) {
        return hcclAlltoAllVFunc(
            sendBuf, sendCounts, sdispls, sendType, recvBuf, recvCounts, rdispls, recvType, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclAllGather(void *sendBuf, void *recvBuf, uint64_t sendCount, HcclDataType dataType,
    HcclComm comm, aclrtStream stream)
{
    printf("HcclAllGather called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  sendCount = %lu\n", sendCount);
    printf("  dataType = %d\n", dataType);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    // auto dataSize = DATA_TYPE_SIZE_MAP.at(dataType);
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t inputSize = static_cast<uint32_t>(dataSize * sendCount);
    uint32_t outputSize = static_cast<uint32_t>(dataSize * sendCount * rankSize);

    // Use BuildOpDetailsV1 for AllGather
    auto allGatherDetails = BuildOpDetailsV1(
        sendCount, dataType, 0, 0, HcclCMDType::HCCL_CMD_ALLGATHER);
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_ALLGATHER, curRank, reinterpret_cast<uint64_t>(stream),
                   sendBuf, inputSize, recvBuf, outputSize, allGatherDetails,
                   0, rankSize, curRank, curRank) != 0) {
        HCCL_VM_ERROR("[HcclAllGather] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }
    
    printf("HcclAllGather get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);
 
    using HcclAllGatherFunc = HcclResult (*)(void *, void *, uint64_t, HcclDataType, HcclComm, aclrtStream);
    HcclAllGatherFunc hcclAllGatherFunc = reinterpret_cast<HcclAllGatherFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclAllGatherFunc != nullptr) {
        return hcclAllGatherFunc(sendBuf, recvBuf, sendCount, dataType, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclBroadcast(
    void *buf, uint64_t count, HcclDataType dataType, uint32_t root, HcclComm comm, aclrtStream stream)
{
    printf("HcclBroadcast called with parameters:\n");
    printf("  buf = %p\n", buf);
    printf("  count = %lu\n", count);
    printf("  dataType = %d\n", dataType);
    printf("  root = %u\n", root);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    // auto dataSize = DATA_TYPE_SIZE_MAP.at(dataType);
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t size = static_cast<uint32_t>(dataSize * count);

    // Use BuildOpDetailsV1 for Broadcast
    auto broadcastDetails = BuildOpDetailsV1(
        count, dataType, 0, 0, HcclCMDType::HCCL_CMD_BROADCAST);
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_BROADCAST, curRank, reinterpret_cast<uint64_t>(stream),
                   buf, size, buf, size, broadcastDetails,
                   root, rankSize, curRank, curRank) != 0) {
        HCCL_VM_ERROR("[HcclBroadcast] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }

    printf("HcclBroadcast get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);
 
    using HcclBroadcastFunc = HcclResult (*)(void *, uint64_t, HcclDataType, uint32_t, HcclComm, aclrtStream);
    HcclBroadcastFunc hcclBroadcastFunc = reinterpret_cast<HcclBroadcastFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclBroadcastFunc != nullptr) {
        return hcclBroadcastFunc(buf, count, dataType, root, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclAllReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
    HcclComm comm, aclrtStream stream)
{
    printf("HcclAllReduce called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  count = %lu\n", count);
    printf("  dataType = %d\n", dataType);
    printf("  op = %d\n", op);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    // auto dataSize = DATA_TYPE_SIZE_MAP.at(dataType);
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t size = static_cast<uint32_t>(dataSize * count);
    // Use BuildOpDetailsV1 for AllReduce
    auto allReduceDetails = BuildOpDetailsV1(
        count, dataType, static_cast<uint32_t>(op), static_cast<uint32_t>(op), HcclCMDType::HCCL_CMD_ALLREDUCE);
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_ALLREDUCE, curRank, reinterpret_cast<uint64_t>(stream),
                   sendBuf, size, recvBuf, size, allReduceDetails,
                   0, rankSize, curRank, curRank) != 0) {
        HCCL_VM_ERROR("[HcclAllReduce] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }
    printf("HcclAllReduce get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);

    using HcclAddreduceFunc = HcclResult (*)(void *, void *, uint64_t, HcclDataType, HcclReduceOp, HcclComm, aclrtStream);
    HcclAddreduceFunc hcclAddreduceFunc = reinterpret_cast<HcclAddreduceFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclAddreduceFunc != nullptr) {
        return hcclAddreduceFunc(sendBuf, recvBuf, count, dataType, op, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, uint32_t root,
    HcclComm comm, aclrtStream stream)
{
    printf("HcclScatter called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  recvCount = %lu\n", recvCount);
    printf("  dataType = %d\n", dataType);
    printf("  root = %u\n", root);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    // auto dataSize = DATA_TYPE_SIZE_MAP.at(dataType);
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t inputValueSize = 0;
    if (curRank == root) {
        inputValueSize = static_cast<uint32_t>(dataSize * recvCount * rankSize);
    }
    uint32_t outputValueSize = static_cast<uint32_t>(dataSize * recvCount);
 
    const void* inputPtr = (curRank == root) ? sendBuf : nullptr;
    // Use BuildOpDetailsV1 for Scatter
    auto scatterDetails = BuildOpDetailsV1(
        recvCount, dataType, 0, 0, HcclCMDType::HCCL_CMD_SCATTER);
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_SCATTER, curRank, reinterpret_cast<uint64_t>(stream),
                   inputPtr, inputValueSize, recvBuf, outputValueSize, scatterDetails,
                   root, rankSize, curRank, curRank) != 0) {
        HCCL_VM_ERROR("[HcclScatter] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }
    
    printf("HcclScatter get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);
 
    using HcclScatterFunc = HcclResult (*)(void *, void *, uint64_t, HcclDataType, uint32_t, HcclComm, aclrtStream);
    HcclScatterFunc hcclScatterFunc = reinterpret_cast<HcclScatterFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclScatterFunc != nullptr) {
        return hcclScatterFunc(sendBuf, recvBuf, recvCount, dataType, root, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclGetHcclBuffer(HcclComm comm, void **buffer, uint64_t *size)
{
    printf("HcclGetHcclBufferNew called with parameters: buffer= %p, %lu\n", *buffer, *size);
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
 
    using HcclGetHcclBufferFunc = HcclResult (*)(HcclComm, void**, uint64_t*);
    auto hcclGetHcclBufferFunc = reinterpret_cast<HcclGetHcclBufferFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclGetHcclBufferFunc != nullptr) {
        auto ret = hcclGetHcclBufferFunc(comm, buffer, size);
        sim::UpdateOpMemCclBuffer(reinterpret_cast<uint64_t>(*buffer), *size);
        printf("HcclGetHcclBufferNew get rank%u ccl buffer= %p, %lx\n", curRank, *buffer, *size);
        return ret;
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclReduce(void *sendBuf, void *recvBuf, uint64_t count, HcclDataType dataType, HcclReduceOp op,
    uint32_t root, HcclComm comm, aclrtStream stream)
{
    printf("HcclReduce called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  count = %lu\n", count);
    printf("  dataType = %d\n", dataType);
    printf("  reduce op = %u\n", static_cast<int>(op));
    printf("  root = %d\n", root);
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    // auto dataSize = DATA_TYPE_SIZE_MAP.at(dataType);
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t size = static_cast<uint32_t>(dataSize * count);

    // Use BuildOpDetailsV1 for Reduce
    auto reduceDetails = BuildOpDetailsV1(
        count, dataType, static_cast<uint32_t>(op), static_cast<uint32_t>(op), HcclCMDType::HCCL_CMD_REDUCE);
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_REDUCE, curRank, reinterpret_cast<uint64_t>(stream),
                   sendBuf, size, recvBuf, size, reduceDetails,
                   root, rankSize, curRank, curRank) != 0) {
        HCCL_VM_ERROR("[HcclReduce] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }

    printf("HcclReduce get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);
 
    using HcclReduceFunc =
        HcclResult (*)(void *, void *, uint64_t, HcclDataType, HcclReduceOp, uint32_t, HcclComm, aclrtStream);
    auto hcclReduceFunc = reinterpret_cast<HcclReduceFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclReduceFunc != nullptr) {
        return hcclReduceFunc(sendBuf, recvBuf, count, dataType, op, root, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult HcclReduceScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, HcclReduceOp op,
    HcclComm comm, aclrtStream stream)
{
    printf("HcclReduceScatter called with parameters:\n");
    printf("  sendBuf = %p\n", sendBuf);
    printf("  recvBuf = %p\n", recvBuf);
    printf("  recvCount = %lu\n", recvCount);
    printf("  dataType = %d\n", dataType);
    printf("  reduce op = %u\n", static_cast<int>(op));
    printf("  comm = %p\n", comm);
    printf("  stream = %p\n", stream);
 
    uint32_t curRank = (uint32_t)sim::GetCurrRankId();
    uint32_t rankSize = sim::GetRankSize();
    // 注册input、output buffer
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    uint32_t inputSize = static_cast<uint32_t>(dataSize * recvCount * rankSize);
    uint32_t outputSize = static_cast<uint32_t>(dataSize * recvCount);

    // Use BuildOpDetailsV1 for ReduceScatter
    auto reduceScatterDetails = BuildOpDetailsV1(
        recvCount, dataType, static_cast<uint32_t>(op), static_cast<uint32_t>(op), HcclCMDType::HCCL_CMD_REDUCE_SCATTER);
    if (RecordOpDbInfo(HcclCMDType::HCCL_CMD_REDUCE_SCATTER, curRank, reinterpret_cast<uint64_t>(stream),
                   sendBuf, inputSize, recvBuf, outputSize, reduceScatterDetails,
                   0, rankSize, curRank, curRank) != 0) {
        HCCL_VM_ERROR("[HcclReduceScatter] record op db info failed");
        return HcclResult::HCCL_E_PARA;
    }

    printf("HcclReduceScatter get op info: allRank= %u, curRank= %u.\n", rankSize, curRank);

    using HcclReduceScatterFunc = HcclResult (*)(void *, void *, uint64_t, HcclDataType, HcclReduceOp, HcclComm, aclrtStream);
    auto hcclReduceScatterFunc = reinterpret_cast<HcclReduceScatterFunc>(dlsym(RTLD_NEXT, __func__));
    if (hcclReduceScatterFunc != nullptr) {
        return hcclReduceScatterFunc(sendBuf, recvBuf, recvCount, dataType, op, comm, stream);
    } else {
        printf("[ERROR] dlsym %s failed\n", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}
 
HcclResult _Z18GetRunSideIsDeviceRb(bool &isDeviceSide)
{
    isDeviceSide = false;
    return HcclResult::HCCL_SUCCESS;
}
 
#ifdef __cplusplus
}
#endif  // __cplusplus

extern "C" bool GetPtrNameByVirPtr(void *virPtr, uint32_t &offset, sim::PhyMemBlock &phyMem);

namespace ops_hccl {
constexpr uint32_t AIV_STUB_MAX_RANK_SIZE = 128;
constexpr uint32_t AIV_STUB_MAX_NUM_BLOCKS = 48;
constexpr int32_t AIV_STUB_TOPO_LEN = 128;
constexpr uint64_t AIV_STUB_GM_IN_TABLE_OFFSET = 0;
constexpr uint64_t AIV_STUB_GM_OUT_TABLE_OFFSET = 16 * 1024;
constexpr uint64_t AIV_STUB_TOPO_OFFSET = 32 * 1024;
constexpr uint64_t AIV_STUB_FLAG1_OFFSET = 1 * 1024 * 1024;
constexpr uint64_t AIV_STUB_FLAG2_OFFSET = 5 * 1024 * 1024;
constexpr uint64_t AIV_STUB_TAG_CLEAR_OFFSET = 512 * 1024;
constexpr uint64_t AIV_STUB_BASE_FLAG_OFFSET = 9 * 1024 * 1024;
constexpr uint64_t AIV_STUB_FLAG_EMPTY_OFFSET = 10 * 1024 * 1024;
constexpr uint64_t AIV_STUB_GM_OUT_PING_OFFSET = 18 * 1024 * 1024;
constexpr uint64_t AIV_STUB_GM_OUT_PONG_OFFSET = 34 * 1024 * 1024;
constexpr uint64_t AIV_STUB_COMM_INFO_SIZE = 65 * 1024 * 1024;
constexpr uint64_t AIV_STUB_UB_ALIGN_SIZE = 32;
constexpr uint64_t AIV_STUB_FLAG_SLOT_SIZE = 128;
constexpr uint32_t AIV_STUB_FLAG_SLOT_PRINT_NUM = 16;
constexpr uint32_t AIV_STUB_TAG_PRINT_NUM = 16;

enum class KernelArgsType {
    ARGS_TYPE_SERVER = 0,
    ARGS_TYPE_TWO_SHOT = 1,
    ARGS_TYPE_DEFAULT
};

HcclCMDType g_currentAivCmdType = HcclCMDType::HCCL_CMD_MAX;
KernelArgsType g_currentAivArgsType = KernelArgsType::ARGS_TYPE_SERVER;

struct ExtraArgs {
    uint64_t sendCounts[AIV_STUB_MAX_RANK_SIZE] = {};
    uint64_t sendDispls[AIV_STUB_MAX_RANK_SIZE] = {};
    uint64_t recvCounts[AIV_STUB_MAX_RANK_SIZE] = {};
    uint64_t recvDispls[AIV_STUB_MAX_RANK_SIZE] = {};
};

struct OpCounterInfo {
    uint64_t headCountMem = 0;
    uint64_t tailCountMem = 0;
    uint64_t addOneMem = 0;
    uint32_t memSize = 0;
    bool isEnableCounter = false;
};

struct AivOpArgs {
    HcclCMDType cmdType = HcclCMDType::HCCL_CMD_MAX;
    std::string comm = {};
    HcclComm hcclComm = nullptr;
    uint32_t numBlocks = AIV_STUB_MAX_NUM_BLOCKS;
    void *stream = nullptr;
    uint64_t beginTime = 0;
    OpCounterInfo counter = {};
    void *buffersIn = nullptr;
    uint64_t input = 0;
    uint64_t output = 0;
    uint32_t rank = 0;
    uint32_t sendRecvRemoteRank = 0;
    uint32_t rankSize = 0;
    uint64_t xRankSize = 0;
    uint64_t yRankSize = 0;
    uint64_t zRankSize = 0;
    uint64_t count = 0;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    HcclReduceOp op = HcclReduceOp::HCCL_REDUCE_SUM;
    uint32_t root = 0;
    uint32_t sliceId = 0;
    uint64_t inputSliceStride = 0;
    uint64_t outputSliceStride = 0;
    uint64_t repeatNum = 0;
    uint64_t inputRepeatStride = 0;
    uint64_t outputRepeatStride = 0;
    bool isOpBase = false;
    ExtraArgs extraArgs = {};
    uint64_t topo_[AIV_STUB_TOPO_LEN] = {0};
    KernelArgsType argsType = KernelArgsType::ARGS_TYPE_SERVER;
};

struct AivKernelArgs {
    const void *buffersIn = nullptr;
    uint64_t input = 0;
    uint64_t output = 0;
    uint32_t rank = 0;
    uint32_t sendRecvRemoteRank = 0;
    uint32_t rankSize = 0;
    uint64_t xRankSize = 0;
    uint64_t yRankSize = 0;
    uint64_t zRankSize = 0;
    uint64_t len = 0;
    uint32_t dataType = 0;
    uint32_t reduceOp = 0;
    uint32_t root = 0;
    uint32_t tag = 0;
    uint64_t inputSliceStride = 0;
    uint64_t outputSliceStride = 0;
    uint64_t repeatNum = 0;
    uint64_t inputRepeatStride = 0;
    uint64_t outputRepeatStride = 0;
    uint32_t numBlocks = 0;
    bool isOpBase = false;
    const void *headCountMem = nullptr;
    const void *tailCountMem = nullptr;
    const void *addOneMem = nullptr;
    uint32_t counterMemSize = 0;
    bool isEnableCounter = false;
};

struct AivExtraKernelArgs {
    const void *buffersIn = nullptr;
    uint64_t input = 0;
    uint64_t output = 0;
    uint32_t rank = 0;
    uint32_t sendRecvRemoteRank = 0;
    uint32_t rankSize = 0;
    uint64_t xRankSize = 0;
    uint64_t yRankSize = 0;
    uint64_t zRankSize = 0;
    uint64_t len = 0;
    uint32_t dataType = 0;
    uint32_t reduceOp = 0;
    uint32_t root = 0;
    uint32_t tag = 0;
    uint64_t inputSliceStride = 0;
    uint64_t outputSliceStride = 0;
    uint64_t repeatNum = 0;
    uint64_t inputRepeatStride = 0;
    uint64_t outputRepeatStride = 0;
    uint32_t numBlocks = 0;
    bool isOpBase = false;
    const void *headCountMem = nullptr;
    const void *tailCountMem = nullptr;
    const void *addOneMem = nullptr;
    uint32_t counterMemSize = 0;
    bool isEnableCounter = false;
    ExtraArgs extraArgs = {};
};

struct AivHostLaunchArgs {
    const void *buffersIn = nullptr;
    uint64_t input = 0;
    uint64_t output = 0;
    uint32_t rank = 0;
    uint32_t sendRecvRemoteRank = 0;
    uint32_t rankSize = 0;
    uint64_t xRankSize = 0;
    uint64_t yRankSize = 0;
    uint64_t zRankSize = 0;
    uint64_t len = 0;
    uint32_t dataType = 0;
    uint32_t reduceOp = 0;
    uint32_t root = 0;
    uint32_t tag = 0;
    uint64_t inputSliceStride = 0;
    uint64_t outputSliceStride = 0;
    uint64_t repeatNum = 0;
    uint64_t inputRepeatStride = 0;
    uint64_t outputRepeatStride = 0;
    uint32_t numBlocks = 0;
    bool isOpBase = false;
    const void *headCountMem = nullptr;
    const void *tailCountMem = nullptr;
    const void *addOneMem = nullptr;
    uint32_t counterMemSize = 0;
    bool isEnableCounter = false;
    ExtraArgs extraArgs = {};
    bool hasExtraArgs = false;
};

struct CheckerFuncHandleView {
    std::string funcName {};
    std::string kernelName {};
};

using AivOpKernelFunc = void (*)(
    uint8_t *buffIn,
    uint64_t input,
    uint64_t output,
    uint32_t rank,
    uint32_t sendRecvRemoteRank,
    uint32_t rankSize,
    uint64_t xRankSize,
    uint64_t yRankSize,
    uint64_t zRankSize,
    uint64_t len,
    uint32_t dataType,
    uint32_t reduceOp,
    uint32_t root,
    uint32_t sliceId,
    uint64_t inputSliceStride,
    uint64_t outputSliceStride,
    uint64_t repeatNum,
    uint64_t inputRepeatStride,
    uint64_t outputRepeatStride,
    uint32_t numBlocks,
    bool isOpBase,
    uint8_t *headCountMem,
    uint8_t *tailCountMem,
    uint8_t *addOneMem,
    uint32_t counterMemSize,
    bool isEnableCounter);
using AivExtraOpKernelFunc = void (*)(
    uint8_t *buffIn,
    uint64_t input,
    uint64_t output,
    uint32_t rank,
    uint32_t sendRecvRemoteRank,
    uint32_t rankSize,
    uint64_t xRankSize,
    uint64_t yRankSize,
    uint64_t zRankSize,
    uint64_t len,
    uint32_t dataType,
    uint32_t reduceOp,
    uint32_t root,
    uint32_t sliceId,
    uint64_t inputSliceStride,
    uint64_t outputSliceStride,
    uint64_t repeatNum,
    uint64_t inputRepeatStride,
    uint64_t outputRepeatStride,
    uint32_t numBlocks,
    bool isOpBase,
    uint8_t *headCountMem,
    uint8_t *tailCountMem,
    uint8_t *addOneMem,
    uint32_t counterMemSize,
    bool isEnableCounter,
    ExtraArgs extraArgs);
using AivEnvInitFunc = void (*)(
    uint32_t rankId,
    size_t blockNum,
    const void *buffIn,
    uint32_t rankSize,
    uint64_t input,
    uint64_t inputSize,
    uint64_t output,
    uint64_t outputSize,
    uint64_t inputGlobalOffsetBase,
    uint64_t outputGlobalOffsetBase,
    uint64_t cclBufferSize,
    uint64_t flagBufferSize,
    AivSim::AivOpParam opParam);
using AivSetBlockIdxFunc = void (*)(int64_t blockIdx);
using AivDumpTasksFunc = void (*)(uint32_t launchIndex);

constexpr const char *AIV_STUB_ENV_INIT_SYMBOL = "aiv_env_init";
constexpr const char *AIV_STUB_SET_BLOCK_IDX_SYMBOL = "aiv_set_block_idx";
constexpr const char *AIV_STUB_DUMP_TASKS_SYMBOL = "aiv_dump_tasks";
constexpr const char *AIV_STUB_SO_NAME = "libhccl_aiv_kernel.so";
constexpr uint64_t INVALID_MEMORY_LAYOUT_SIZE = static_cast<uint64_t>(-1);

struct ResolvedHostPtrHandle {
    const uint8_t *hostPtr = nullptr;
    sim::PhyMemBlock phyMem = {};
    bool needRelease = false;
};

struct ResolvedKernelLaunchArgs {
    AivHostLaunchArgs args = {};
    ResolvedHostPtrHandle buffersInHandle = {};
};

struct OpMemInfoMatchInfo {
    uint64_t offsetInLayout = 0;
    uint64_t totalSize = 0;
};

struct CclOpMemInfoCacheEntry {
    uint64_t cclAddr = 0;
    uint64_t cclSize = 0;
    uint32_t opMemId = 0;
    uint32_t opDetailId = 0;
    bool valid = false;
};

struct CclOpMemInfoCache {
    bool initialized = false;
    uint32_t rankSize = 0;
    std::vector<CclOpMemInfoCacheEntry> entries;
};

static CclOpMemInfoCache g_cclOpMemInfoCache;

static ResolvedHostPtrHandle ResolveHostPtr(const void *devPtr)
{
    ResolvedHostPtrHandle handle {};
    if (devPtr == nullptr) {
        return handle;
    }

    sim::PhyMemBlock phyMem {};
    uint32_t offset = 0;
    if (!::GetPtrNameByVirPtr(const_cast<void *>(devPtr), offset, phyMem)) {
        return handle;
    }

    handle.phyMem = phyMem;
    auto *devStartPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(devPtr) - offset);
    auto *hostBasePtr =
        static_cast<const uint8_t *>(sim::DeviceMemoryManager::GetInstance().GetHostPtrByDevPtr(devStartPtr));
    if (hostBasePtr == nullptr) {
        hostBasePtr = static_cast<const uint8_t *>(
            sim::DeviceMemoryManager::GetInstance().AcquirePhyMem(phyMem.name, phyMem.device_id, phyMem.size));
        if (hostBasePtr == nullptr) {
            return handle;
        }
        handle.phyMem = phyMem;
        handle.needRelease = true;
    }
    handle.hostPtr = hostBasePtr + offset;
    return handle;
}

static void ReleaseHostPtr(ResolvedHostPtrHandle &handle)
{
    if (!handle.needRelease) {
        return;
    }

    // needRelease 仅在走 AcquirePhyMem 时置位；按 size 判定是否走了复用区（与申请同一判据）
    if (sim::CommPoolPolicy::ShouldRedirect(handle.phyMem.size, sim::IsCheckOnlyMode())) {
        sim::MemoryManager::GetInstance().ReleaseMemByName(sim::CommPoolPolicy::kPoolName);
    } else {
        sim::DeviceMemoryManager::GetInstance().ReleasePhyMem(handle.phyMem.name, handle.phyMem.device_id);
    }
    handle.hostPtr = nullptr;
    handle.phyMem = {};
    handle.needRelease = false;
}

static void DumpAivExtraArgs(const ExtraArgs &extraArgs)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-ExecuteKernelLaunch] extraArgs:\n";
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "  extraArgs.sendCounts[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.sendCounts[i]) << '\n';
    }
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "  extraArgs.sendDispls[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.sendDispls[i]) << '\n';
    }
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "  extraArgs.recvCounts[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.recvCounts[i]) << '\n';
    }
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "  extraArgs.recvDispls[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.recvDispls[i]) << '\n';
    }
    HCCL_VM_DEBUG("{}", oss.str());
}

static void DumpAivTopo(const uint64_t topo[AIV_STUB_TOPO_LEN])
{
    std::ostringstream oss;
    oss << "[virtual-aiv-ExecuteKernelLaunch] topo:\n";
    for (uint32_t i = 0; i < static_cast<uint32_t>(AIV_STUB_TOPO_LEN); ++i) {
        oss << "  opArgs.topo_[" << i << "] = " << static_cast<unsigned long long>(topo[i]) << '\n';
    }
    HCCL_VM_DEBUG("{}", oss.str());
}

static void DumpFlagSlots(std::ostringstream &oss, const uint8_t *flagBase, uint64_t byteOffset, uint32_t slotCount,
    const char *label)
{
    const uint64_t slotBase = byteOffset / AIV_STUB_UB_ALIGN_SIZE;
    const uint64_t slotStride = AIV_STUB_FLAG_SLOT_SIZE / AIV_STUB_UB_ALIGN_SIZE;
    oss << "    " << label
        << " byteOffset=0x" << std::hex << static_cast<unsigned long long>(byteOffset)
        << std::dec << " slotBase=" << static_cast<unsigned long long>(slotBase)
        << " slotStride=" << static_cast<unsigned long long>(slotStride)
        << " slotCount=" << slotCount << '\n';
    for (uint32_t i = 0; i < slotCount; ++i) {
        const uint64_t slotIndex = slotBase + static_cast<uint64_t>(i) * slotStride;
        const auto *slotHead = reinterpret_cast<const int32_t *>(
            flagBase + byteOffset + static_cast<uint64_t>(i) * AIV_STUB_FLAG_SLOT_SIZE);
        oss << "      slot[" << static_cast<unsigned long long>(slotIndex)
            << "] addr=" << static_cast<const void *>(slotHead)
            << " words=[" << slotHead[0] << ", " << slotHead[1] << ", " << slotHead[2] << ", " << slotHead[3]
            << "]\n";
    }
}

static void DumpBuffersInParsedDeviceView(const void *buffersInDev, uint32_t rankSize, uint32_t numBlocks)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-ExecuteKernelLaunch] buffersIn-parse:\n";
    oss << "  [buffersIn-parse] aivCommInfoPtr base(dev) = " << buffersInDev << '\n';
    if (buffersInDev == nullptr) {
        oss << "  [buffersIn-parse] buffersIn is null, skip device-side parse dump.\n";
        HCCL_VM_DEBUG("{}", oss.str());
        return;
    }

    ResolvedHostPtrHandle buffersInHandle = ResolveHostPtr(buffersInDev);
    auto *buffersInHost = buffersInHandle.hostPtr;
    oss << "  [buffersIn-parse] aivCommInfoPtr base(host) = " << static_cast<const void *>(buffersInHost) << '\n';
    if (buffersInHost == nullptr) {
        oss << "  [buffersIn-parse] failed to translate device ptr to host ptr.\n";
        HCCL_VM_DEBUG("{}", oss.str());
        return;
    }

    const uint32_t parsedRankSize =
        (rankSize < AIV_STUB_MAX_RANK_SIZE) ? rankSize : AIV_STUB_MAX_RANK_SIZE;
    const auto *gmInTable = reinterpret_cast<const uint64_t *>(buffersInHost + AIV_STUB_GM_IN_TABLE_OFFSET);
    const auto *gmOutTable = reinterpret_cast<const uint64_t *>(buffersInHost + AIV_STUB_GM_OUT_TABLE_OFFSET);
    const auto *topoTable = reinterpret_cast<const uint64_t *>(buffersInHost + AIV_STUB_TOPO_OFFSET);
    const auto *flag1Base = buffersInHost + AIV_STUB_FLAG1_OFFSET;
    const auto *tagTable = reinterpret_cast<const int32_t *>(buffersInHost + AIV_STUB_TAG_CLEAR_OFFSET);
    const auto *emptyClearTable = reinterpret_cast<const int32_t *>(buffersInHost + AIV_STUB_FLAG_EMPTY_OFFSET);
    const uint64_t nonPingpongBaseFlagOffset = AIV_STUB_BASE_FLAG_OFFSET - AIV_STUB_FLAG1_OFFSET;

    oss << "  [buffersIn-parse] device-side parsed results from buffersIn (not a raw buffersIn pointer dump):\n";
    oss << "    AIV comm layout: commInfoSize=0x" << std::hex
        << static_cast<unsigned long long>(AIV_STUB_COMM_INFO_SIZE)
        << ", GM_OUT_TABLE=0x" << static_cast<unsigned long long>(AIV_STUB_GM_OUT_TABLE_OFFSET)
        << ", TOPO=0x" << static_cast<unsigned long long>(AIV_STUB_TOPO_OFFSET)
        << ", TAG/CLEAR=0x" << static_cast<unsigned long long>(AIV_STUB_TAG_CLEAR_OFFSET)
        << ", FLAG1=0x" << static_cast<unsigned long long>(AIV_STUB_FLAG1_OFFSET)
        << ", FLAG2=0x" << static_cast<unsigned long long>(AIV_STUB_FLAG2_OFFSET)
        << ", BASE_FLAG=0x" << static_cast<unsigned long long>(AIV_STUB_BASE_FLAG_OFFSET)
        << ", EMPTY_CLEAR=0x" << static_cast<unsigned long long>(AIV_STUB_FLAG_EMPTY_OFFSET)
        << ", PING=0x" << static_cast<unsigned long long>(AIV_STUB_GM_OUT_PING_OFFSET)
        << ", PONG=0x" << static_cast<unsigned long long>(AIV_STUB_GM_OUT_PONG_OFFSET)
        << std::dec << '\n';
    oss << "    Compatibility note: not support pingpong yet; AllGather pingpong data"
        << " slots are dumped for layout visibility only.\n";
    oss << "    Legacy host DFX flag offset remains 0x" << std::hex
        << static_cast<unsigned long long>(40 * 1024)
        << ", but device-side non-pingpong GM_OUT uses FLAG1_OFFSET(0x"
        << static_cast<unsigned long long>(AIV_STUB_FLAG1_OFFSET) << ").\n"
        << std::dec;
    oss << "    GM_IN parsed entries count=" << parsedRankSize
        << " from +0x" << std::hex << static_cast<unsigned long long>(AIV_STUB_GM_IN_TABLE_OFFSET) << std::dec
        << '\n';
    for (uint32_t i = 0; i < parsedRankSize; ++i) {
        oss << "      GM_IN[" << i << "] dev=0x" << std::hex << static_cast<unsigned long long>(gmInTable[i])
            << std::dec << '\n';
    }
    if (parsedRankSize == 0) {
        oss << "      rankSize is 0, device would not populate GM_IN[].\n";
    }

    oss << "    GM_OUT parsed entries count=" << parsedRankSize
        << " from (+0x" << std::hex << static_cast<unsigned long long>(AIV_STUB_GM_OUT_TABLE_OFFSET)
        << " table) + FLAG1_OFFSET(0x" << static_cast<unsigned long long>(AIV_STUB_FLAG1_OFFSET) << ')'
        << std::dec << '\n';
    for (uint32_t i = 0; i < parsedRankSize; ++i) {
        const uint64_t commInfoDev = gmOutTable[i];
        const uint64_t flagDev = (commInfoDev == 0) ? 0 : (commInfoDev + AIV_STUB_FLAG1_OFFSET);
        oss << "      GM_OUT[" << i << "] dev=0x" << std::hex << static_cast<unsigned long long>(flagDev)
            << " (src commInfoDev=0x" << std::hex << static_cast<unsigned long long>(commInfoDev)
            << std::dec << ")\n";
    }
    if (parsedRankSize == 0) {
        oss << "      rankSize is 0, device would not populate GM_OUT[].\n";
    }

    oss << "    TOPO_ parsed entries [0.." << static_cast<unsigned>(AIV_STUB_TOPO_LEN - 1)
        << "] from +0x" << std::hex << static_cast<unsigned long long>(AIV_STUB_TOPO_OFFSET) << std::dec << '\n';
    for (uint32_t i = 0; i < static_cast<uint32_t>(AIV_STUB_TOPO_LEN); ++i) {
        oss << "      TOPO_[" << i << "] = " << static_cast<unsigned long long>(topoTable[i]) << '\n';
    }

    const uint32_t barrierSlotPrintNum =
        (rankSize < AIV_STUB_MAX_RANK_SIZE) ? rankSize : AIV_STUB_MAX_RANK_SIZE;
    oss << "  [buffersIn-parse] follow-up work areas derived from the same base:\n";
    oss << "    FLAG1 non-pingpong GM_OUT @ +0x" << std::hex
        << static_cast<unsigned long long>(AIV_STUB_FLAG1_OFFSET)
        << ", flagSlotSize=" << std::dec << AIV_STUB_FLAG_SLOT_SIZE
        << ", baseFlagOffset relative to GM_OUT=0x" << std::hex
        << static_cast<unsigned long long>(nonPingpongBaseFlagOffset)
        << ", absoluteBaseFlag=0x" << static_cast<unsigned long long>(AIV_STUB_BASE_FLAG_OFFSET)
        << std::dec << '\n';
    oss << "    FLAG2 pingpong alt @ +0x" << std::hex << static_cast<unsigned long long>(AIV_STUB_FLAG2_OFFSET)
        << ", PING data @ +0x" << static_cast<unsigned long long>(AIV_STUB_GM_OUT_PING_OFFSET)
        << ", PONG data @ +0x" << static_cast<unsigned long long>(AIV_STUB_GM_OUT_PONG_OFFSET)
        << std::dec << '\n';
    oss << "    FLAG1 bytes=[0x0, 0x"
        << std::hex << static_cast<unsigned long long>(AIV_STUB_FLAG_EMPTY_OFFSET - AIV_STUB_FLAG1_OFFSET) << ')'
        << ", flagSlotSize=" << std::dec << AIV_STUB_FLAG_SLOT_SIZE
        << std::dec << '\n';
    DumpFlagSlots(oss, flag1Base, 0, AIV_STUB_FLAG_SLOT_PRINT_NUM, "FLAG1 operator slots[0..15]");
    DumpFlagSlots(oss, flag1Base, nonPingpongBaseFlagOffset, barrierSlotPrintNum,
        "BASE_FLAG_OFFSET - FLAG1_OFFSET barrier slots");

    oss << "    TAG/CLEAR ints[0.." << (AIV_STUB_TAG_PRINT_NUM - 1)
        << "] @ +0x" << std::hex << static_cast<unsigned long long>(AIV_STUB_TAG_CLEAR_OFFSET) << std::dec << '\n';
    oss << "      non-pingpong model currently keeps tag_ fixed at 1; real HCCL reads/writes this slot.\n";
    for (uint32_t i = 0; i < AIV_STUB_TAG_PRINT_NUM; ++i) {
        oss << "      TAG_CLEAR[" << i << "] = " << tagTable[i] << '\n';
    }
    oss << "    EMPTY_CLEAR ints[0.." << (AIV_STUB_TAG_PRINT_NUM - 1)
        << "] @ +0x" << std::hex << static_cast<unsigned long long>(AIV_STUB_FLAG_EMPTY_OFFSET) << std::dec
        << '\n';
    for (uint32_t i = 0; i < AIV_STUB_TAG_PRINT_NUM; ++i) {
        oss << "      EMPTY_CLEAR[" << i << "] = " << emptyClearTable[i] << '\n';
    }

    HCCL_VM_DEBUG("{}", oss.str());
    ReleaseHostPtr(buffersInHandle);
}

static void DumpLaunchKernelCfg(const aclrtLaunchKernelCfg *cfg)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-aclrtLaunchKernelWithHostArgs] cfg:\n";
    oss << "  cfg = " << cfg << '\n';
    if (cfg == nullptr) {
        HCCL_VM_DEBUG("{}", oss.str());
        return;
    }

    oss << "  cfg->attrs = " << cfg->attrs << '\n';
    oss << "  cfg->numAttrs = " << cfg->numAttrs << '\n';
    for (size_t i = 0; i < cfg->numAttrs; ++i) {
        const auto &attr = cfg->attrs[i];
        oss << "    cfg->attrs[" << i << "].id = " << static_cast<int>(attr.id) << '\n';
        switch (attr.id) {
            case ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE:
                oss << "      schemMode = " << static_cast<unsigned>(attr.value.schemMode) << '\n';
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE:
                oss << "      dynUBufSize = " << attr.value.dynUBufSize << '\n';
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE:
                oss << "      engineType = " << static_cast<unsigned>(attr.value.engineType) << '\n';
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET:
                oss << "      blockDimOffset = " << attr.value.blockDimOffset << '\n';
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH:
                oss << "      isBlockTaskPrefetch = " << static_cast<unsigned>(attr.value.isBlockTaskPrefetch) << '\n';
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP:
                oss << "      isDataDump = " << static_cast<unsigned>(attr.value.isDataDump) << '\n';
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT:
                oss << "      timeout = " << static_cast<unsigned>(attr.value.timeout) << "(s)\n";
                break;
            case ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT_US:
                oss << "      timeoutUs.low = " << attr.value.timeoutUs.timeoutLow << '\n';
                oss << "      timeoutUs.high = " << attr.value.timeoutUs.timeoutHigh << '\n';
                break;
            default:
                oss << "      raw rsv = ["
                    << attr.value.rsv[0] << ", "
                    << attr.value.rsv[1] << ", "
                    << attr.value.rsv[2] << ", "
                    << attr.value.rsv[3] << "]\n";
                break;
        }
    }
    HCCL_VM_DEBUG("{}", oss.str());
}

static void DumpPlaceHolderArray(const aclrtPlaceHolderInfo *placeHolderArray, size_t placeHolderNum)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-aclrtLaunchKernelWithHostArgs] placeHolderArray:\n";
    oss << "  placeHolderArray = " << placeHolderArray << '\n';
    oss << "  placeHolderNum = " << placeHolderNum << '\n';
    if (placeHolderArray == nullptr) {
        HCCL_VM_DEBUG("{}", oss.str());
        return;
    }

    for (size_t i = 0; i < placeHolderNum; ++i) {
        oss << "    placeHolderArray[" << i << "].addrOffset = " << placeHolderArray[i].addrOffset << '\n';
        oss << "    placeHolderArray[" << i << "].dataOffset = " << placeHolderArray[i].dataOffset << '\n';
    }
    HCCL_VM_DEBUG("{}", oss.str());
}

static bool IsAivExtraArgsCmdType(HcclCMDType cmdType)
{
    return cmdType == HcclCMDType::HCCL_CMD_ALLTOALLV;
}

static void CopyCommonKernelArgsToHostArgs(const AivKernelArgs &src, AivHostLaunchArgs &dst)
{
    dst.buffersIn = src.buffersIn;
    dst.input = src.input;
    dst.output = src.output;
    dst.rank = src.rank;
    dst.sendRecvRemoteRank = src.sendRecvRemoteRank;
    dst.rankSize = src.rankSize;
    dst.xRankSize = src.xRankSize;
    dst.yRankSize = src.yRankSize;
    dst.zRankSize = src.zRankSize;
    dst.len = src.len;
    dst.dataType = src.dataType;
    dst.reduceOp = src.reduceOp;
    dst.root = src.root;
    dst.tag = src.tag;
    dst.inputSliceStride = src.inputSliceStride;
    dst.outputSliceStride = src.outputSliceStride;
    dst.repeatNum = src.repeatNum;
    dst.inputRepeatStride = src.inputRepeatStride;
    dst.outputRepeatStride = src.outputRepeatStride;
    dst.numBlocks = src.numBlocks;
    dst.isOpBase = src.isOpBase;
    dst.headCountMem = src.headCountMem;
    dst.tailCountMem = src.tailCountMem;
    dst.addOneMem = src.addOneMem;
    dst.counterMemSize = src.counterMemSize;
    dst.isEnableCounter = src.isEnableCounter;
}

static void CopyCommonKernelArgsToHostArgs(const AivExtraKernelArgs &src, AivHostLaunchArgs &dst)
{
    dst.buffersIn = src.buffersIn;
    dst.input = src.input;
    dst.output = src.output;
    dst.rank = src.rank;
    dst.sendRecvRemoteRank = src.sendRecvRemoteRank;
    dst.rankSize = src.rankSize;
    dst.xRankSize = src.xRankSize;
    dst.yRankSize = src.yRankSize;
    dst.zRankSize = src.zRankSize;
    dst.len = src.len;
    dst.dataType = src.dataType;
    dst.reduceOp = src.reduceOp;
    dst.root = src.root;
    dst.tag = src.tag;
    dst.inputSliceStride = src.inputSliceStride;
    dst.outputSliceStride = src.outputSliceStride;
    dst.repeatNum = src.repeatNum;
    dst.inputRepeatStride = src.inputRepeatStride;
    dst.outputRepeatStride = src.outputRepeatStride;
    dst.numBlocks = src.numBlocks;
    dst.isOpBase = src.isOpBase;
    dst.headCountMem = src.headCountMem;
    dst.tailCountMem = src.tailCountMem;
    dst.addOneMem = src.addOneMem;
    dst.counterMemSize = src.counterMemSize;
    dst.isEnableCounter = src.isEnableCounter;
}

static bool ParseAivHostLaunchArgs(
    const void *hostArgs, size_t argsSize, HcclCMDType cmdType, AivHostLaunchArgs &parsedArgs)
{
    parsedArgs = {};
    if (hostArgs == nullptr) {
        HCCL_VM_ERROR("[virtual-aiv-aclrtLaunchKernelWithHostArgs] hostArgs is null, can not virtual execute kernel.");
        return false;
    }

    if (IsAivExtraArgsCmdType(cmdType)) {
        if (argsSize < sizeof(AivExtraKernelArgs)) {
            HCCL_VM_ERROR(
                "[virtual-aiv-aclrtLaunchKernelWithHostArgs] argsSize({}) < sizeof(AivExtraKernelArgs)({}) "
                "for cmdType={}, stop virtual execution.",
                argsSize,
                sizeof(AivExtraKernelArgs),
                static_cast<int>(cmdType));
            return false;
        }
        const auto &rawArgs = *static_cast<const AivExtraKernelArgs *>(hostArgs);
        CopyCommonKernelArgsToHostArgs(rawArgs, parsedArgs);
        parsedArgs.extraArgs = rawArgs.extraArgs;
        parsedArgs.hasExtraArgs = true;
        return true;
    }

    if (argsSize < sizeof(AivKernelArgs)) {
        HCCL_VM_ERROR(
            "[virtual-aiv-aclrtLaunchKernelWithHostArgs] argsSize({}) < sizeof(AivKernelArgs)({}) "
            "for cmdType={}, stop virtual execution.",
            argsSize,
            sizeof(AivKernelArgs),
            static_cast<int>(cmdType));
        return false;
    }
    const auto &rawArgs = *static_cast<const AivKernelArgs *>(hostArgs);
    CopyCommonKernelArgsToHostArgs(rawArgs, parsedArgs);
    parsedArgs.hasExtraArgs = false;
    return true;
}

static void DumpHostLaunchArgs(const AivHostLaunchArgs *hostArgs, size_t argsSize)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-aclrtLaunchKernelWithHostArgs] hostArgs:\n";
    oss << "  hostArgs = " << hostArgs << '\n';
    oss << "  argsSize = " << argsSize << '\n';
    oss << "  sizeof(AivKernelArgs) = " << sizeof(AivKernelArgs) << '\n';
    oss << "  sizeof(AivExtraKernelArgs) = " << sizeof(AivExtraKernelArgs) << '\n';
    oss << "  sizeof(AivHostLaunchArgs normalized) = " << sizeof(AivHostLaunchArgs) << '\n';
    if (hostArgs == nullptr) {
        HCCL_VM_DEBUG("{}", oss.str());
        return;
    }

    oss << "  hostArgs.buffersIn = " << hostArgs->buffersIn << '\n';
    oss << "  hostArgs.input = 0x" << std::hex << static_cast<unsigned long long>(hostArgs->input) << std::dec << '\n';
    oss << "  hostArgs.output = 0x" << std::hex << static_cast<unsigned long long>(hostArgs->output) << std::dec << '\n';
    oss << "  hostArgs.rank = " << hostArgs->rank << '\n';
    oss << "  hostArgs.sendRecvRemoteRank = " << hostArgs->sendRecvRemoteRank << '\n';
    oss << "  hostArgs.rankSize = " << hostArgs->rankSize << '\n';
    oss << "  hostArgs.xRankSize = " << static_cast<unsigned long long>(hostArgs->xRankSize) << '\n';
    oss << "  hostArgs.yRankSize = " << static_cast<unsigned long long>(hostArgs->yRankSize) << '\n';
    oss << "  hostArgs.zRankSize = " << static_cast<unsigned long long>(hostArgs->zRankSize) << '\n';
    oss << "  hostArgs.len = " << static_cast<unsigned long long>(hostArgs->len) << '\n';
    oss << "  hostArgs.dataType = " << hostArgs->dataType << '\n';
    oss << "  hostArgs.reduceOp = " << hostArgs->reduceOp << '\n';
    oss << "  hostArgs.root = " << hostArgs->root << '\n';
    oss << "  hostArgs.tag = " << hostArgs->tag << '\n';
    oss << "  hostArgs.inputSliceStride = " << static_cast<unsigned long long>(hostArgs->inputSliceStride) << '\n';
    oss << "  hostArgs.outputSliceStride = " << static_cast<unsigned long long>(hostArgs->outputSliceStride) << '\n';
    oss << "  hostArgs.repeatNum = " << static_cast<unsigned long long>(hostArgs->repeatNum) << '\n';
    oss << "  hostArgs.inputRepeatStride = " << static_cast<unsigned long long>(hostArgs->inputRepeatStride) << '\n';
    oss << "  hostArgs.outputRepeatStride = " << static_cast<unsigned long long>(hostArgs->outputRepeatStride) << '\n';
    oss << "  hostArgs.numBlocks = " << hostArgs->numBlocks << '\n';
    oss << "  hostArgs.isOpBase = " << static_cast<int>(hostArgs->isOpBase) << '\n';
    oss << "  hostArgs.headCountMem = " << hostArgs->headCountMem << '\n';
    oss << "  hostArgs.tailCountMem = " << hostArgs->tailCountMem << '\n';
    oss << "  hostArgs.addOneMem = " << hostArgs->addOneMem << '\n';
    oss << "  hostArgs.counterMemSize = " << hostArgs->counterMemSize << '\n';
    oss << "  hostArgs.isEnableCounter = " << static_cast<int>(hostArgs->isEnableCounter) << '\n';
    oss << "  hostArgs.hasExtraArgs = " << static_cast<int>(hostArgs->hasExtraArgs) << '\n';
    HCCL_VM_DEBUG("{}", oss.str());
    if (hostArgs->hasExtraArgs) {
        DumpAivExtraArgs(hostArgs->extraArgs);
    }
}

static const char *GetAivWideKernelTypeSuffix(HcclDataType dataType)
{
    switch (dataType) {
        case HcclDataType::HCCL_DATA_TYPE_FP16:
            return "half";
        case HcclDataType::HCCL_DATA_TYPE_INT16:
            return "int16_t";
        case HcclDataType::HCCL_DATA_TYPE_UINT16:
            return "uint16_t";
        case HcclDataType::HCCL_DATA_TYPE_FP32:
            return "float";
        case HcclDataType::HCCL_DATA_TYPE_FP64:
            return "uint64_t";
        case HcclDataType::HCCL_DATA_TYPE_INT32:
            return "int32_t";
        case HcclDataType::HCCL_DATA_TYPE_UINT32:
            return "uint32_t";
        case HcclDataType::HCCL_DATA_TYPE_INT8:
            return "int8_t";
        case HcclDataType::HCCL_DATA_TYPE_UINT8:
            return "uint8_t";
        case HcclDataType::HCCL_DATA_TYPE_BFP16:
            return "bfloat16_t";
        case HcclDataType::HCCL_DATA_TYPE_INT64:
            return "int64_t";
        case HcclDataType::HCCL_DATA_TYPE_UINT64:
            return "uint64_t";
        case HcclDataType::HCCL_DATA_TYPE_HIF8:
            return "hifloat8_t";
        case HcclDataType::HCCL_DATA_TYPE_FP8E4M3:
            return "fp8_e4m3fn_t";
        case HcclDataType::HCCL_DATA_TYPE_FP8E5M2:
            return "fp8_e5m2_t";
        case HcclDataType::HCCL_DATA_TYPE_FP8E8M0:
            return "fp8_e8m0_t";
        default:
            return nullptr;
    }
}

static const char *GetAivReduceKernelTypeSuffix(HcclDataType dataType)
{
    switch (dataType) {
        case HcclDataType::HCCL_DATA_TYPE_FP16:
            return "half";
        case HcclDataType::HCCL_DATA_TYPE_INT16:
            return "int16_t";
        case HcclDataType::HCCL_DATA_TYPE_FP32:
            return "float";
        case HcclDataType::HCCL_DATA_TYPE_INT32:
            return "int32_t";
        case HcclDataType::HCCL_DATA_TYPE_INT8:
            return "int8_t";
        case HcclDataType::HCCL_DATA_TYPE_BFP16:
            return "bfloat16_t";
        case HcclDataType::HCCL_DATA_TYPE_INT64:
            return "int64_t";
        default:
            return nullptr;
    }
}

static std::string BuildAivKernelName(const char *kernelPrefix, const char *typeSuffix)
{
    if (kernelPrefix == nullptr || typeSuffix == nullptr) {
        return {};
    }

    return std::string(kernelPrefix) + typeSuffix;
}

static bool IsUnsupportedFallbackAivCmdType(HcclCMDType cmdType)
{
    switch (cmdType) {
        case HcclCMDType::HCCL_CMD_ALLGATHER_V:
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V:
        case HcclCMDType::HCCL_CMD_ALLTOALLVC:
            return true;
        default:
            return false;
    }
}

static std::string GetFallbackAivKernelName(
    HcclCMDType cmdType, HcclDataType dataType, KernelArgsType argsType)
{
    switch (cmdType) {
        case HcclCMDType::HCCL_CMD_ALLGATHER:
            return BuildAivKernelName("aiv_all_gather_", GetAivWideKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_ALLREDUCE:
            return BuildAivKernelName(
                argsType == KernelArgsType::ARGS_TYPE_TWO_SHOT ?
                    "aiv_allreduce_mesh1d_twoshot_" :
                    "aiv_allreduce_",
                GetAivReduceKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER:
            return BuildAivKernelName("aiv_reduce_scatter_", GetAivReduceKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_BROADCAST:
            return BuildAivKernelName("aiv_broadcast_", GetAivWideKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_REDUCE:
            return BuildAivKernelName("aiv_reduce_", GetAivReduceKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_SCATTER:
            return BuildAivKernelName("aiv_scatter_", GetAivWideKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_ALLTOALL:
            return BuildAivKernelName("aiv_alltoall_", GetAivWideKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_ALLTOALLV:
            return BuildAivKernelName("aiv_alltoallv_", GetAivWideKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_SEND:
            return BuildAivKernelName("aiv_send_", GetAivWideKernelTypeSuffix(dataType));
        case HcclCMDType::HCCL_CMD_RECEIVE:
            return BuildAivKernelName("aiv_recv_", GetAivWideKernelTypeSuffix(dataType));
        default:
            return {};
    }
}

static std::string InferKernelNameFromFuncHandle(aclrtFuncHandle funcHandle)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-aclrtLaunchKernelWithHostArgs] InferKernelNameFromFuncHandle:\n";
    oss << "  funcHandle = " << funcHandle << '\n';
    if (funcHandle == nullptr) {
        HCCL_VM_DEBUG("{}", oss.str());
        return {};
    }

    char funcNameBuffer[256] = {0};
    aclError funcNameRet = aclrtGetFunctionName(funcHandle, sizeof(funcNameBuffer), funcNameBuffer);
    oss << "  aclrtGetFunctionName(funcHandle) ret = " << static_cast<int>(funcNameRet) << '\n';
    oss << "  aclrtGetFunctionName(funcHandle) funcName = "
        << (funcNameBuffer[0] == '\0' ? "<empty>" : funcNameBuffer) << '\n';

    std::string resolvedKernelName;

    if (funcNameRet == ACL_SUCCESS) {
        const auto *funcHandleView = reinterpret_cast<const CheckerFuncHandleView *>(funcHandle);
        oss << "  [InferKernelNameFromFuncHandle] current checker aclrtBinaryGetFunction stores funcHandle as "
            << "sim::FuncHandle*.\n";
        oss << "  [InferKernelNameFromFuncHandle] funcHandle->funcName = "
            << (funcHandleView->funcName.empty() ? "<empty>" : funcHandleView->funcName.c_str()) << '\n';
        oss << "  [InferKernelNameFromFuncHandle] funcHandle->kernelName = "
            << (funcHandleView->kernelName.empty() ? "<empty>" : funcHandleView->kernelName.c_str()) << '\n';
        if (!funcHandleView->kernelName.empty()) {
            resolvedKernelName = funcHandleView->kernelName;
        } else if (!funcHandleView->funcName.empty()) {
            resolvedKernelName = funcHandleView->funcName;
        }
    }

    if (resolvedKernelName.empty()) {
        resolvedKernelName = std::string(funcNameBuffer);
    }
    oss << "  resolvedKernelName = " << (resolvedKernelName.empty() ? "<empty>" : resolvedKernelName.c_str()) << '\n';
    HCCL_VM_DEBUG("{}", oss.str());
    return resolvedKernelName;
}

static std::string GetAivLibraryPath(const std::string &soName, const std::string &kernelName)
{
    const char *kernelNameCStr = kernelName.empty() ? "<empty>" : kernelName.c_str();
    const char *installDirEnv = std::getenv("HCCL_VM_INSTALL_DIR");
    if (installDirEnv == nullptr || installDirEnv[0] == '\0') {
        HCCL_VM_ERROR("[virtual-aiv] env HCCL_VM_INSTALL_DIR is not set, can not locate {} for kernel {}",
            soName, kernelNameCStr);
        return {};
    }

    std::error_code ec;
    const fs::path installDir = fs::path(installDirEnv);
    const fs::path soPath = installDir / "lib" / "x86_64" / soName;
    if (!fs::exists(soPath, ec)) {
        if (ec) {
            HCCL_VM_ERROR("[virtual-aiv] failed to stat aiv library path, kernel={}, HCCL_VM_INSTALL_DIR={}, so={}, err={}",
                kernelNameCStr, installDir.string(), soPath.string(), ec.message());
            return {};
        }
        HCCL_VM_ERROR("[virtual-aiv] missing aiv stub shared library, kernel={}, HCCL_VM_INSTALL_DIR={}, expectedSo={}",
            kernelNameCStr, installDir.string(), soPath.string());
        return {};
    }

    return soPath.string();
}

static ResolvedKernelLaunchArgs PrepareResolvedKernelLaunchArgs(const AivHostLaunchArgs &rawArgs)
{
    ResolvedKernelLaunchArgs resolvedArgs {};
    resolvedArgs.args = rawArgs;
    resolvedArgs.buffersInHandle = ResolveHostPtr(rawArgs.buffersIn);
    if (resolvedArgs.buffersInHandle.hostPtr == nullptr && rawArgs.buffersIn != nullptr) {
        HCCL_VM_ERROR(
            "[virtual-aiv-VirtualExecuteAivKernel] buffersIn translation failed, translated host ptr is null, "
            "raw={:p}",
            rawArgs.buffersIn);
    }
    resolvedArgs.args.buffersIn = resolvedArgs.buffersInHandle.hostPtr;
    return resolvedArgs;
}

enum class OpMemInfoLookupStatus {
    RESOLVED,
    MISSING,
    INVALID
};

static bool TryMatchOpMemInfoRange(
    uint64_t queryVirtualAddr,
    uint64_t baseAddr,
    uint64_t size,
    OpMemInfoMatchInfo &matchInfo)
{
    if (size == 0 || queryVirtualAddr < baseAddr) {
        return false;
    }

    const uint64_t offsetInLayout = queryVirtualAddr - baseAddr;
    if (offsetInLayout >= size) {
        return false;
    }

    matchInfo.offsetInLayout = offsetInLayout;
    matchInfo.totalSize = size;
    return true;
}

static bool StartsWith(const std::string &value, const char *prefix)
{
    return value.rfind(prefix, 0) == 0;
}

static bool IsAivBroadcastKernel(const std::string &kernelName)
{
    return StartsWith(kernelName, "aiv_broadcast_");
}

static bool IsAivScatterKernel(const std::string &kernelName)
{
    return StartsWith(kernelName, "aiv_scatter_");
}

static OpMemInfoLookupStatus LookupOpMemInfoByVirtualAddr(
    uint32_t rankId,
    uint64_t queryVirtualAddr,
    BufferType bufType,
    OpMemInfoMatchInfo &matchInfo)
{
    matchInfo = {};
    if (queryVirtualAddr == 0) {
        return OpMemInfoLookupStatus::RESOLVED;
    }

    sim::OpMemInfoTab opMemInfo {};
    if (sim::QueryCurrentOpMemInfoByRank(rankId, opMemInfo) != 0) {
        return OpMemInfoLookupStatus::MISSING;
    }

    uint64_t baseAddr = 0;
    uint64_t size = 0;
    if (bufType == BufferType::INPUT) {
        baseAddr = opMemInfo.inputAddr;
        size = opMemInfo.inputSize;
    } else if (bufType == BufferType::OUTPUT) {
        baseAddr = opMemInfo.outputAddr;
        size = opMemInfo.outputSize;
    } else if (bufType == BufferType::CCL) {
        baseAddr = opMemInfo.cclAddr;
        size = opMemInfo.cclSize;
    } else {
        HCCL_VM_ERROR("[virtual-aiv] unsupported opMemInfo buffer type, rankId={}, baseAddr=0x{:x}, bufType={}",
            rankId, queryVirtualAddr, static_cast<uint32_t>(bufType));
        return OpMemInfoLookupStatus::INVALID;
    }

    if (!TryMatchOpMemInfoRange(queryVirtualAddr, baseAddr, size, matchInfo)) {
        HCCL_VM_DEBUG(
            "[virtual-aiv] queryAddr=0x{:x} did not match opMemInfo range. "
            "rankId={}, bufType={}, opMemBase=0x{:x}, opMemSize={}",
            queryVirtualAddr,
            rankId,
            static_cast<uint32_t>(bufType),
            baseAddr,
            size);
        return OpMemInfoLookupStatus::MISSING;
    }

    return OpMemInfoLookupStatus::RESOLVED;
}

static void InitCclOpMemInfoCache(uint32_t rankSize)
{
    if (g_cclOpMemInfoCache.initialized) {
        return;
    }

    g_cclOpMemInfoCache.initialized = true;
    g_cclOpMemInfoCache.rankSize = rankSize;
    g_cclOpMemInfoCache.entries.assign(rankSize, CclOpMemInfoCacheEntry {});

    for (uint32_t rankId = 0; rankId < rankSize; ++rankId) {
        sim::OpMemInfoTab opMemInfo {};
        if (sim::QueryCurrentOpMemInfoByRank(rankId, opMemInfo) != 0) {
            HCCL_VM_ERROR("[virtual-aiv] failed to cache CCL opMemInfo, rankId={}", rankId);
            continue;
        }

        auto &entry = g_cclOpMemInfoCache.entries[rankId];
        entry.cclAddr = opMemInfo.cclAddr;
        entry.cclSize = opMemInfo.cclSize;
        entry.opMemId = opMemInfo.id;
        entry.opDetailId = opMemInfo.opDetailId;
        entry.valid = opMemInfo.cclAddr != 0 && opMemInfo.cclSize != 0;
        HCCL_VM_INFO(
            "[virtual-aiv] cache CCL opMemInfo, rankId={}, opMemId={}, opDetailId={}, cclAddr=0x{:x}, cclSize={}",
            rankId,
            entry.opMemId,
            entry.opDetailId,
            entry.cclAddr,
            entry.cclSize);
    }
}

static OpMemInfoLookupStatus LookupCachedCclOpMemInfoByVirtualAddr(
    uint32_t rankId,
    uint64_t queryVirtualAddr,
    uint32_t rankSize,
    OpMemInfoMatchInfo &matchInfo)
{
    matchInfo = {};
    if (queryVirtualAddr == 0) {
        return OpMemInfoLookupStatus::RESOLVED;
    }

    InitCclOpMemInfoCache(rankSize);

    if (rankId >= g_cclOpMemInfoCache.entries.size()) {
        HCCL_VM_ERROR(
            "[virtual-aiv] cached CCL opMemInfo rank out of range, rankId={}, cachedRankSize={}, currentRankSize={}",
            rankId,
            g_cclOpMemInfoCache.rankSize,
            rankSize);
        return OpMemInfoLookupStatus::INVALID;
    }

    if (g_cclOpMemInfoCache.rankSize != rankSize) {
        HCCL_VM_ERROR(
            "[virtual-aiv] cached CCL opMemInfo rankSize mismatch, cachedRankSize={}, currentRankSize={}",
            g_cclOpMemInfoCache.rankSize,
            rankSize);
        return OpMemInfoLookupStatus::INVALID;
    }

    const auto &entry = g_cclOpMemInfoCache.entries[rankId];
    if (!entry.valid) {
        HCCL_VM_ERROR(
            "[virtual-aiv] cached CCL opMemInfo is invalid, rankId={}, opMemId={}, opDetailId={}, "
            "cclAddr=0x{:x}, cclSize={}",
            rankId,
            entry.opMemId,
            entry.opDetailId,
            entry.cclAddr,
            entry.cclSize);
        return OpMemInfoLookupStatus::MISSING;
    }

    if (!TryMatchOpMemInfoRange(queryVirtualAddr, entry.cclAddr, entry.cclSize, matchInfo)) {
        HCCL_VM_DEBUG(
            "[virtual-aiv] queryAddr=0x{:x} did not match cached CCL opMemInfo range. "
            "rankId={}, opMemId={}, opDetailId={}, cclAddr=0x{:x}, cclSize={}",
            queryVirtualAddr,
            rankId,
            entry.opMemId,
            entry.opDetailId,
            entry.cclAddr,
            entry.cclSize);
        return OpMemInfoLookupStatus::MISSING;
    }

    return OpMemInfoLookupStatus::RESOLVED;
}

static void ResolveVirtualAivBufferSizes(const std::string &kernelName,
    ResolvedKernelLaunchArgs &resolvedArgs,
    uint64_t &inputSize,
    uint64_t &outputSize,
    uint64_t &inputGlobalOffsetBase,
    uint64_t &outputGlobalOffsetBase,
    uint64_t &cclBufferSize,
    uint64_t &flagBufferSize)
{
    inputGlobalOffsetBase = 0;
    outputGlobalOffsetBase = 0;
    const uint64_t inputAddr = resolvedArgs.args.input;
    OpMemInfoMatchInfo inputMatchInfo {};
    const OpMemInfoLookupStatus inputStatus = LookupOpMemInfoByVirtualAddr(
        resolvedArgs.args.rank, inputAddr, BufferType::INPUT, inputMatchInfo);
    if (inputStatus == OpMemInfoLookupStatus::MISSING) {
        if (IsAivScatterKernel(kernelName) && resolvedArgs.args.rank != resolvedArgs.args.root) {
            HCCL_VM_INFO("[virtual-aiv] skip missing input opMemInfo for kernel {}, rank={}, root={}, baseAddr=0x{:x}",
                kernelName, resolvedArgs.args.rank, resolvedArgs.args.root, inputAddr);
            resolvedArgs.args.input = 0;
            inputSize = 0;
        } else {
            HCCL_VM_ERROR(
                "[virtual-aiv] expected exactly one opMemInfo range, rankId={}, baseAddr=0x{:x}, bufType={}, matchedCount={}",
                resolvedArgs.args.rank,
                inputAddr,
                static_cast<uint32_t>(BufferType::INPUT),
                0);
            inputSize = INVALID_MEMORY_LAYOUT_SIZE;
        }
    } else if (inputStatus == OpMemInfoLookupStatus::INVALID) {
        inputSize = INVALID_MEMORY_LAYOUT_SIZE;
    } else {
        inputSize = inputMatchInfo.totalSize;
        inputGlobalOffsetBase = inputMatchInfo.offsetInLayout;
    }

    const uint64_t outputAddr = resolvedArgs.args.output;
    OpMemInfoMatchInfo outputMatchInfo {};
    const OpMemInfoLookupStatus outputStatus = LookupOpMemInfoByVirtualAddr(
        resolvedArgs.args.rank, outputAddr, BufferType::OUTPUT, outputMatchInfo);
    if (outputStatus == OpMemInfoLookupStatus::MISSING) {
        if (IsAivBroadcastKernel(kernelName)) {
            HCCL_VM_INFO("[virtual-aiv] skip missing output opMemInfo for kernel {}, rank={}, baseAddr=0x{:x}",
                kernelName, resolvedArgs.args.rank, outputAddr);
            resolvedArgs.args.output = 0;
            outputSize = 0;
        } else {
            HCCL_VM_ERROR(
                "[virtual-aiv] expected exactly one opMemInfo range, rankId={}, baseAddr=0x{:x}, bufType={}, matchedCount={}",
                resolvedArgs.args.rank,
                outputAddr,
                static_cast<uint32_t>(BufferType::OUTPUT),
                0);
            outputSize = INVALID_MEMORY_LAYOUT_SIZE;
        }
    } else if (outputStatus == OpMemInfoLookupStatus::INVALID) {
        outputSize = INVALID_MEMORY_LAYOUT_SIZE;
    } else {
        outputSize = outputMatchInfo.totalSize;
        outputGlobalOffsetBase = outputMatchInfo.offsetInLayout;
    }

    cclBufferSize = 0;
    flagBufferSize = AIV_STUB_COMM_INFO_SIZE - AIV_STUB_FLAG1_OFFSET;

    if (resolvedArgs.args.buffersIn == nullptr) {
        return;
    }

    const auto *ipcBufferGlobal = static_cast<const uint64_t *>(resolvedArgs.args.buffersIn);
    for (uint32_t i = 0; i < resolvedArgs.args.rankSize; ++i) {
        if (cclBufferSize == 0) {
            const uint64_t cclBufferAddr = ipcBufferGlobal[i];
            OpMemInfoMatchInfo cclMatchInfo {};
            const OpMemInfoLookupStatus finalCclBufferStatus =
                LookupCachedCclOpMemInfoByVirtualAddr(
                    i,
                    cclBufferAddr,
                    resolvedArgs.args.rankSize,
                    cclMatchInfo);
            if (finalCclBufferStatus == OpMemInfoLookupStatus::MISSING) {
                HCCL_VM_ERROR(
                    "[virtual-aiv] expected exactly one opMemInfo range, rankId={}, baseAddr=0x{:x}, bufType={}, matchedCount={}",
                    i,
                    cclBufferAddr,
                    static_cast<uint32_t>(BufferType::CCL),
                    0);
                cclBufferSize = INVALID_MEMORY_LAYOUT_SIZE;
            } else if (finalCclBufferStatus == OpMemInfoLookupStatus::INVALID) {
                cclBufferSize = INVALID_MEMORY_LAYOUT_SIZE;
            } else {
                cclBufferSize = cclMatchInfo.totalSize;
            }
        }
        if (cclBufferSize != 0) {
            break;
        }
    }
}

static void DumpVirtualKernelExtraArgsWithSource(std::ostringstream &oss, const ExtraArgs &extraArgs)
{
    oss << "    kernelFunc.extraArgs <- aclrtLaunchKernelWithHostArgs(hostArgs->extraArgs)\n";
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "      kernelFunc.extraArgs.sendCounts[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.sendCounts[i])
            << " <- hostArgs->extraArgs.sendCounts[" << i << "]\n";
    }
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "      kernelFunc.extraArgs.sendDispls[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.sendDispls[i])
            << " <- hostArgs->extraArgs.sendDispls[" << i << "]\n";
    }
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "      kernelFunc.extraArgs.recvCounts[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.recvCounts[i])
            << " <- hostArgs->extraArgs.recvCounts[" << i << "]\n";
    }
    for (uint32_t i = 0; i < AIV_STUB_MAX_RANK_SIZE; ++i) {
        oss << "      kernelFunc.extraArgs.recvDispls[" << i << "] = "
            << static_cast<unsigned long long>(extraArgs.recvDispls[i])
            << " <- hostArgs->extraArgs.recvDispls[" << i << "]\n";
    }
}

static void DumpVirtualKernelFuncArgs(
    const std::string &kernelName, uint32_t numBlocks, const AivHostLaunchArgs &rawArgs,
    const ResolvedKernelLaunchArgs &resolvedArgs)
{
    std::ostringstream oss;
    oss << "[virtual-aiv-VirtualExecuteAivKernel] kernelFunc shared args:\n";
    oss << "    launchContext.kernelName = " << kernelName
        << " <- aclrtLaunchKernelWithHostArgs(funcHandle)\n";
    oss << "    launchContext.numBlocks = " << numBlocks
        << " <- aclrtLaunchKernelWithHostArgs(numBlocks)\n";
    oss << "    kernelFunc.buffIn = " << resolvedArgs.args.buffersIn
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->buffersIn=" << rawArgs.buffersIn
        << "), checker translates only buffersIn to host address\n";
    oss << "    kernelFunc.input = 0x" << std::hex << static_cast<unsigned long long>(resolvedArgs.args.input)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->input=0x"
        << static_cast<unsigned long long>(rawArgs.input)
        << "), checker keeps raw input address unchanged\n" << std::dec;
    oss << "    kernelFunc.output = 0x" << std::hex << static_cast<unsigned long long>(resolvedArgs.args.output)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->output=0x"
        << static_cast<unsigned long long>(rawArgs.output)
        << "), checker keeps raw output address unchanged\n" << std::dec;
    oss << "    kernelFunc.rank = " << resolvedArgs.args.rank
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->rank)\n";
    oss << "    kernelFunc.sendRecvRemoteRank = " << resolvedArgs.args.sendRecvRemoteRank
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->sendRecvRemoteRank)\n";
    oss << "    kernelFunc.rankSize = " << resolvedArgs.args.rankSize
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->rankSize)\n";
    oss << "    kernelFunc.xRankSize = " << static_cast<unsigned long long>(resolvedArgs.args.xRankSize)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->xRankSize)\n";
    oss << "    kernelFunc.yRankSize = " << static_cast<unsigned long long>(resolvedArgs.args.yRankSize)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->yRankSize)\n";
    oss << "    kernelFunc.zRankSize = " << static_cast<unsigned long long>(resolvedArgs.args.zRankSize)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->zRankSize)\n";
    oss << "    kernelFunc.len = " << static_cast<unsigned long long>(resolvedArgs.args.len)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->len)\n";
    oss << "    kernelFunc.dataType = " << resolvedArgs.args.dataType
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->dataType)\n";
    oss << "    kernelFunc.reduceOp = " << resolvedArgs.args.reduceOp
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->reduceOp)\n";
    oss << "    kernelFunc.root = " << resolvedArgs.args.root
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->root)\n";
    oss << "    kernelFunc.sliceId = " << resolvedArgs.args.tag
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->tag)\n";
    oss << "    kernelFunc.inputSliceStride = "
        << static_cast<unsigned long long>(resolvedArgs.args.inputSliceStride)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->inputSliceStride)\n";
    oss << "    kernelFunc.outputSliceStride = "
        << static_cast<unsigned long long>(resolvedArgs.args.outputSliceStride)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->outputSliceStride)\n";
    oss << "    kernelFunc.repeatNum = " << static_cast<unsigned long long>(resolvedArgs.args.repeatNum)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->repeatNum)\n";
    oss << "    kernelFunc.inputRepeatStride = "
        << static_cast<unsigned long long>(resolvedArgs.args.inputRepeatStride)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->inputRepeatStride)\n";
    oss << "    kernelFunc.outputRepeatStride = "
        << static_cast<unsigned long long>(resolvedArgs.args.outputRepeatStride)
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->outputRepeatStride)\n";
    oss << "    kernelFunc.numBlocks = " << resolvedArgs.args.numBlocks
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->numBlocks)\n";
    oss << "    kernelFunc.isOpBase = " << (resolvedArgs.args.isOpBase ? "true" : "false")
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->isOpBase)\n";
    oss << "    kernelFunc.headCountMem = " << resolvedArgs.args.headCountMem
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->headCountMem=" << rawArgs.headCountMem
        << "), checker keeps raw headCountMem pointer unchanged\n";
    oss << "    kernelFunc.tailCountMem = " << resolvedArgs.args.tailCountMem
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->tailCountMem=" << rawArgs.tailCountMem
        << "), checker keeps raw tailCountMem pointer unchanged\n";
    oss << "    kernelFunc.addOneMem = " << resolvedArgs.args.addOneMem
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->addOneMem=" << rawArgs.addOneMem
        << "), checker keeps raw addOneMem pointer unchanged\n";
    oss << "    kernelFunc.counterMemSize = " << resolvedArgs.args.counterMemSize
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->counterMemSize)\n";
    oss << "    kernelFunc.isEnableCounter = " << (resolvedArgs.args.isEnableCounter ? "true" : "false")
        << " <- aclrtLaunchKernelWithHostArgs(hostArgs->isEnableCounter)\n";
    if (resolvedArgs.args.hasExtraArgs) {
        DumpVirtualKernelExtraArgsWithSource(oss, resolvedArgs.args.extraArgs);
    } else {
        oss << "    kernelFunc.extraArgs <- not present in aclrtLaunchKernelWithHostArgs host args\n";
    }
    HCCL_VM_DEBUG("{}", oss.str());
}

struct VirtualAivLibrary {
    void *handle = nullptr;
    std::string soName {};
    std::string soPath {};
    AivEnvInitFunc envInit = nullptr;
    AivSetBlockIdxFunc setBlockIdx = nullptr;
    AivDumpTasksFunc dumpTasks = nullptr;
};

static void CloseVirtualAivLibrary(VirtualAivLibrary &lib)
{
    if (lib.handle != nullptr) {
        dlclose(lib.handle);
        lib.handle = nullptr;
    }

    lib.envInit = nullptr;
    lib.setBlockIdx = nullptr;
    lib.dumpTasks = nullptr;
}

static VirtualAivLibrary LoadVirtualAivLibrary(const std::string &soName, const std::string &kernelName)
{
    VirtualAivLibrary lib {};
    lib.soName = soName;
    if (lib.soName.empty()) {
        HCCL_VM_ERROR("[virtual-aiv] empty soName for kernel {}",
            kernelName.empty() ? "<empty>" : kernelName.c_str());
        return lib;
    }

    lib.soPath = GetAivLibraryPath(lib.soName, kernelName);
    if (lib.soPath.empty()) {
        return lib;
    }

    dlerror();
    lib.handle = dlopen(lib.soPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    const char *dlopenErr = dlerror();
    if (lib.handle == nullptr || dlopenErr != nullptr) {
        HCCL_VM_ERROR("[virtual-aiv] dlopen {} failed, err = {}",
            lib.soPath, dlopenErr == nullptr ? "unknown" : dlopenErr);
        CloseVirtualAivLibrary(lib);
        return lib;
    }

    dlerror();
    lib.envInit = reinterpret_cast<AivEnvInitFunc>(dlsym(lib.handle, AIV_STUB_ENV_INIT_SYMBOL));
    const char *envInitErr = dlerror();
    if (lib.envInit == nullptr || envInitErr != nullptr) {
        HCCL_VM_ERROR("[virtual-aiv] dlsym {} from {} failed, err = {}",
            AIV_STUB_ENV_INIT_SYMBOL, lib.soPath, envInitErr == nullptr ? "unknown" : envInitErr);
        lib.envInit = nullptr;
    }

    dlerror();
    lib.setBlockIdx =
        reinterpret_cast<AivSetBlockIdxFunc>(dlsym(lib.handle, AIV_STUB_SET_BLOCK_IDX_SYMBOL));
    const char *setBlockIdxErr = dlerror();
    if (lib.setBlockIdx == nullptr || setBlockIdxErr != nullptr) {
        HCCL_VM_ERROR("[virtual-aiv] dlsym {} from {} failed, err = {}",
            AIV_STUB_SET_BLOCK_IDX_SYMBOL, lib.soPath, setBlockIdxErr == nullptr ? "unknown" : setBlockIdxErr);
        lib.setBlockIdx = nullptr;
    }

    dlerror();
    lib.dumpTasks = reinterpret_cast<AivDumpTasksFunc>(dlsym(lib.handle, AIV_STUB_DUMP_TASKS_SYMBOL));
    const char *dumpTasksErr = dlerror();
    if (lib.dumpTasks == nullptr || dumpTasksErr != nullptr) {
        HCCL_VM_ERROR("[virtual-aiv] dlsym {} from {} failed, err = {}",
            AIV_STUB_DUMP_TASKS_SYMBOL, lib.soPath, dumpTasksErr == nullptr ? "unknown" : dumpTasksErr);
        lib.dumpTasks = nullptr;
    }

    return lib;
}

static aclError VirtualExecuteAivKernel(
    const std::string &kernelName,
    const std::string &soName,
    uint32_t numBlocks,
    const AivHostLaunchArgs &rawArgs,
    uint32_t launchIndex)
{
    VirtualAivLibrary lib = LoadVirtualAivLibrary(soName, kernelName);
    if (lib.handle == nullptr || lib.envInit == nullptr || lib.setBlockIdx == nullptr) {
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }
    if (kernelName.empty()) {
        HCCL_VM_ERROR("[virtual-aiv-VirtualExecuteAivKernel] empty kernelName, can not virtual execute AIV kernel.");
        CloseVirtualAivLibrary(lib);
        return ACL_ERROR_INVALID_PARAM;
    }

    dlerror();
    void *kernelSymbol = dlsym(lib.handle, kernelName.c_str());
    const char *kernelErr = dlerror();
    if (kernelSymbol == nullptr || kernelErr != nullptr) {
        HCCL_VM_ERROR("[virtual-aiv] dlsym {} from {} failed, err = {}",
            kernelName, lib.soPath, kernelErr == nullptr ? "unknown" : kernelErr);
        CloseVirtualAivLibrary(lib);
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    ResolvedKernelLaunchArgs resolvedArgs = PrepareResolvedKernelLaunchArgs(rawArgs);
    uint64_t inputSize = 0;
    uint64_t outputSize = 0;
    uint64_t inputGlobalOffsetBase = 0;
    uint64_t outputGlobalOffsetBase = 0;
    uint64_t cclBufferSize = 0;
    uint64_t flagBufferSize = 0;
    ResolveVirtualAivBufferSizes(
        kernelName,
        resolvedArgs,
        inputSize,
        outputSize,
        inputGlobalOffsetBase,
        outputGlobalOffsetBase,
        cclBufferSize,
        flagBufferSize);
    DumpVirtualKernelFuncArgs(kernelName, numBlocks, rawArgs, resolvedArgs);
    if (inputSize == INVALID_MEMORY_LAYOUT_SIZE ||
        outputSize == INVALID_MEMORY_LAYOUT_SIZE ||
        cclBufferSize == INVALID_MEMORY_LAYOUT_SIZE) {
        HCCL_VM_ERROR("[virtual-aiv] failed to resolve opMemInfo size for kernel {}, rank={}",
            kernelName,
            resolvedArgs.args.rank);
        ReleaseHostPtr(resolvedArgs.buffersInHandle);
        CloseVirtualAivLibrary(lib);
        return ACL_ERROR_INVALID_PARAM;
    }

    AivSim::AivOpParam curOpParam {};
    curOpParam.dataType = resolvedArgs.args.dataType;
    curOpParam.len = resolvedArgs.args.len;
    curOpParam.reduceOp = resolvedArgs.args.reduceOp;
    curOpParam.root = resolvedArgs.args.root;
    curOpParam.sliceId = resolvedArgs.args.tag;
    curOpParam.inputStride = resolvedArgs.args.inputSliceStride;
    curOpParam.outputStride = resolvedArgs.args.outputSliceStride;
    size_t kernelNameCopyLen = kernelName.size();
    if (kernelNameCopyLen >= AivSim::AIV_OP_KERNEL_NAME_MAX_LEN) {
        kernelNameCopyLen = AivSim::AIV_OP_KERNEL_NAME_MAX_LEN - 1;
    }
    std::memcpy(curOpParam.kernelName, kernelName.data(), kernelNameCopyLen);
    curOpParam.kernelName[kernelNameCopyLen] = '\0';
    HCCL_VM_DEBUG(
        "[virtual-aiv-VirtualExecuteAivKernel] aiv_env_init and curOp:\n"
        "  rank = {}\n"
        "  blockNum = {}\n"
        "  buffIn = {:p}\n"
        "  rankSize = {}\n"
        "  input = 0x{:x}\n"
        "  inputSize = {}\n"
        "  output = 0x{:x}\n"
        "  outputSize = {}\n"
        "  inputGlobalOffsetBase = {}\n"
        "  outputGlobalOffsetBase = {}\n"
        "  cclBufferSize = {}\n"
        "  flagBufferSize = {}\n"
        "  curOp.dataType = {}\n"
        "  curOp.len = {}\n"
        "  curOp.reduceOp = {}\n"
        "  curOp.root = {}\n"
        "  curOp.sliceId = {}\n"
        "  curOp.inputStride = {}\n"
        "  curOp.outputStride = {}\n"
        "  curOp.kernelName = {}",
        resolvedArgs.args.rank,
        numBlocks,
        resolvedArgs.args.buffersIn,
        resolvedArgs.args.rankSize,
        static_cast<unsigned long long>(resolvedArgs.args.input),
        static_cast<unsigned long long>(inputSize),
        static_cast<unsigned long long>(resolvedArgs.args.output),
        static_cast<unsigned long long>(outputSize),
        static_cast<unsigned long long>(inputGlobalOffsetBase),
        static_cast<unsigned long long>(outputGlobalOffsetBase),
        static_cast<unsigned long long>(cclBufferSize),
        static_cast<unsigned long long>(flagBufferSize),
        curOpParam.dataType,
        static_cast<unsigned long long>(curOpParam.len),
        curOpParam.reduceOp,
        curOpParam.root,
        curOpParam.sliceId,
        static_cast<unsigned long long>(curOpParam.inputStride),
        static_cast<unsigned long long>(curOpParam.outputStride),
        curOpParam.kernelName);
    lib.envInit(
        resolvedArgs.args.rank,
        numBlocks,
        resolvedArgs.args.buffersIn,
        resolvedArgs.args.rankSize,
        resolvedArgs.args.input,
        inputSize,
        resolvedArgs.args.output,
        outputSize,
        inputGlobalOffsetBase,
        outputGlobalOffsetBase,
        cclBufferSize,
        flagBufferSize,
        curOpParam);

    if (numBlocks == 0) {
        HCCL_VM_DEBUG("[virtual-aiv-VirtualExecuteAivKernel] numBlocks is 0, skip kernel invocation.");
        ReleaseHostPtr(resolvedArgs.buffersInHandle);
        CloseVirtualAivLibrary(lib);
        return ACL_SUCCESS;
    }

    for (uint32_t blockIdx = 0; blockIdx < numBlocks; ++blockIdx) {
        HCCL_VM_DEBUG(
            "[virtual-aiv-VirtualExecuteAivKernel] launch kernel {}, blockIdx={} <- "
            "aclrtLaunchKernelWithHostArgs(numBlocks) loop index; shared kernelFunc args are printed above",
            kernelName,
            blockIdx);
        lib.setBlockIdx(static_cast<int64_t>(blockIdx));
        if (resolvedArgs.args.hasExtraArgs) {
            auto kernelFunc = reinterpret_cast<AivExtraOpKernelFunc>(kernelSymbol);
            kernelFunc(
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.buffersIn)),
                resolvedArgs.args.input,
                resolvedArgs.args.output,
                resolvedArgs.args.rank,
                resolvedArgs.args.sendRecvRemoteRank,
                resolvedArgs.args.rankSize,
                resolvedArgs.args.xRankSize,
                resolvedArgs.args.yRankSize,
                resolvedArgs.args.zRankSize,
                resolvedArgs.args.len,
                resolvedArgs.args.dataType,
                resolvedArgs.args.reduceOp,
                resolvedArgs.args.root,
                resolvedArgs.args.tag,
                resolvedArgs.args.inputSliceStride,
                resolvedArgs.args.outputSliceStride,
                resolvedArgs.args.repeatNum,
                resolvedArgs.args.inputRepeatStride,
                resolvedArgs.args.outputRepeatStride,
                resolvedArgs.args.numBlocks,
                resolvedArgs.args.isOpBase,
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.headCountMem)),
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.tailCountMem)),
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.addOneMem)),
                resolvedArgs.args.counterMemSize,
                resolvedArgs.args.isEnableCounter,
                resolvedArgs.args.extraArgs);
        } else {
            auto kernelFunc = reinterpret_cast<AivOpKernelFunc>(kernelSymbol);
            kernelFunc(
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.buffersIn)),
                resolvedArgs.args.input,
                resolvedArgs.args.output,
                resolvedArgs.args.rank,
                resolvedArgs.args.sendRecvRemoteRank,
                resolvedArgs.args.rankSize,
                resolvedArgs.args.xRankSize,
                resolvedArgs.args.yRankSize,
                resolvedArgs.args.zRankSize,
                resolvedArgs.args.len,
                resolvedArgs.args.dataType,
                resolvedArgs.args.reduceOp,
                resolvedArgs.args.root,
                resolvedArgs.args.tag,
                resolvedArgs.args.inputSliceStride,
                resolvedArgs.args.outputSliceStride,
                resolvedArgs.args.repeatNum,
                resolvedArgs.args.inputRepeatStride,
                resolvedArgs.args.outputRepeatStride,
                resolvedArgs.args.numBlocks,
                resolvedArgs.args.isOpBase,
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.headCountMem)),
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.tailCountMem)),
                const_cast<uint8_t *>(static_cast<const uint8_t *>(resolvedArgs.args.addOneMem)),
                resolvedArgs.args.counterMemSize,
                resolvedArgs.args.isEnableCounter);
        }
    }
    if (lib.dumpTasks != nullptr) {
        lib.dumpTasks(launchIndex);
    }

    ReleaseHostPtr(resolvedArgs.buffersInHandle);
    CloseVirtualAivLibrary(lib);
    return ACL_SUCCESS;
}

HcclResult ExecuteKernelLaunch(const AivOpArgs &opArgs)
{
    HCCL_VM_DEBUG(
        "[virtual-aiv-ExecuteKernelLaunch] called with parameters:\n"
        "  cmdType = {}\n"
        "  comm = {}\n"
        "  hcclComm = {:p}\n"
        "  numBlocks = {}\n"
        "  stream = {:p}\n"
        "  beginTime = {}\n"
        "  counter.headCountMem = 0x{:x}\n"
        "  counter.tailCountMem = 0x{:x}\n"
        "  counter.addOneMem = 0x{:x}\n"
        "  counter.memSize = {}\n"
        "  counter.isEnableCounter = {}\n"
        "  input = 0x{:x}\n"
        "  output = 0x{:x}\n"
        "  rank = {}\n"
        "  sendRecvRemoteRank = {}\n"
        "  rankSize = {}\n"
        "  xRankSize = {}\n"
        "  yRankSize = {}\n"
        "  zRankSize = {}\n"
        "  count = {}\n"
        "  dataType = {}\n"
        "  op = {}\n"
        "  root = {}\n"
        "  sliceId = {}\n"
        "  inputSliceStride = {}\n"
        "  outputSliceStride = {}\n"
        "  repeatNum = {}\n"
        "  inputRepeatStride = {}\n"
        "  outputRepeatStride = {}\n"
        "  isOpBase = {}\n"
        "  argsType = {}\n"
        "  buffersIn(base aivCommInfoPtr, raw pointer only; parsed results below) = {:p}",
        static_cast<int>(opArgs.cmdType),
        opArgs.comm,
        opArgs.hcclComm,
        opArgs.numBlocks,
        opArgs.stream,
        static_cast<unsigned long long>(opArgs.beginTime),
        static_cast<unsigned long long>(opArgs.counter.headCountMem),
        static_cast<unsigned long long>(opArgs.counter.tailCountMem),
        static_cast<unsigned long long>(opArgs.counter.addOneMem),
        opArgs.counter.memSize,
        static_cast<int>(opArgs.counter.isEnableCounter),
        static_cast<unsigned long long>(opArgs.input),
        static_cast<unsigned long long>(opArgs.output),
        opArgs.rank,
        opArgs.sendRecvRemoteRank,
        opArgs.rankSize,
        static_cast<unsigned long long>(opArgs.xRankSize),
        static_cast<unsigned long long>(opArgs.yRankSize),
        static_cast<unsigned long long>(opArgs.zRankSize),
        static_cast<unsigned long long>(opArgs.count),
        static_cast<int>(opArgs.dataType),
        static_cast<int>(opArgs.op),
        opArgs.root,
        opArgs.sliceId,
        static_cast<unsigned long long>(opArgs.inputSliceStride),
        static_cast<unsigned long long>(opArgs.outputSliceStride),
        static_cast<unsigned long long>(opArgs.repeatNum),
        static_cast<unsigned long long>(opArgs.inputRepeatStride),
        static_cast<unsigned long long>(opArgs.outputRepeatStride),
        static_cast<int>(opArgs.isOpBase),
        static_cast<int>(opArgs.argsType),
        opArgs.buffersIn);
    if (IsAivExtraArgsCmdType(opArgs.cmdType)) {
        DumpAivExtraArgs(opArgs.extraArgs);
    } else {
        HCCL_VM_DEBUG("[virtual-aiv-ExecuteKernelLaunch] extraArgs are not used for cmdType={}.",
            static_cast<int>(opArgs.cmdType));
    }
    DumpAivTopo(opArgs.topo_);
    DumpBuffersInParsedDeviceView(opArgs.buffersIn, opArgs.rankSize, opArgs.numBlocks);

    HcclTaskMetaData taskMetaData{};
    taskMetaData.taskType = HccLTaskMetaType::AIV_GRAPH;
    taskMetaData.commId = 0;
    taskMetaData.rankId = static_cast<uint32_t>(sim::GetCurrRankId());
    taskMetaData.jettyId = 0;
    if (opArgs.stream != nullptr) {
        taskMetaData.streamId = sim::GetCurrentStreamId(reinterpret_cast<uint64_t>(opArgs.stream));
    }

    const uint32_t launchIndex = g_aivLaunchIndex.fetch_add(1, std::memory_order_relaxed);
    taskMetaData.taskData.aiv.launchIdx = static_cast<uint64_t>(launchIndex);
    uint32_t unusedIndex = 0;
    auto insertRet = InsertTaskToCollection(&taskMetaData, &unusedIndex);
    if (insertRet != HcclVmResult::HCCL_SIM_SUCCESS) {
        HCCL_VM_ERROR("[virtual-aiv-ExecuteKernelLaunch] failed to insert AIV launch task, ret={}, rankId={}",
            static_cast<uint32_t>(insertRet),
            taskMetaData.rankId);
        return HcclResult::HCCL_E_INTERNAL;
    }
    const HcclCMDType prevCmdType = g_currentAivCmdType;
    const KernelArgsType prevArgsType = g_currentAivArgsType;
    g_currentAivCmdType = opArgs.cmdType;
    g_currentAivArgsType = opArgs.argsType;
    g_currentAivLaunchIndex = launchIndex;
    HCCL_VM_INFO("[virtual-aiv-ExecuteKernelLaunch] inserted AIV launch task, rankId={}, launchIndex={}, streamId={}",
        taskMetaData.rankId,
        launchIndex,
        taskMetaData.streamId);

    using ExecuteKernelLaunchFunc = HcclResult (*)(const AivOpArgs &);
    constexpr const char *executeKernelLaunchSymbol = "_ZN8ops_hccl19ExecuteKernelLaunchERKNS_9AivOpArgsE";

    dlerror();
    auto executeKernelLaunchFunc =
        reinterpret_cast<ExecuteKernelLaunchFunc>(dlsym(RTLD_NEXT, executeKernelLaunchSymbol));
    const char *dlsymErr = dlerror();
    if (executeKernelLaunchFunc == nullptr || dlsymErr != nullptr) {
        g_currentAivCmdType = prevCmdType;
        g_currentAivArgsType = prevArgsType;
        g_currentAivLaunchIndex = INVALID_AIV_LAUNCH_INDEX;
        HCCL_VM_ERROR("[virtual-aiv-ExecuteKernelLaunch] dlsym {} failed, err = {}", executeKernelLaunchSymbol,
            dlsymErr == nullptr ? "unknown" : dlsymErr);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    HcclResult ret = executeKernelLaunchFunc(opArgs);
    g_currentAivCmdType = prevCmdType;
    g_currentAivArgsType = prevArgsType;
    g_currentAivLaunchIndex = INVALID_AIV_LAUNCH_INDEX;
    HCCL_VM_INFO("[virtual-aiv-ExecuteKernelLaunch] returned {}", static_cast<int>(ret));
    return ret;
}
}

extern "C" aclError aclrtLaunchKernelWithHostArgs(aclrtFuncHandle funcHandle, uint32_t numBlocks,
    aclrtStream stream, aclrtLaunchKernelCfg *cfg, void *hostArgs, size_t argsSize,
    aclrtPlaceHolderInfo *placeHolderArray, size_t placeHolderNum)
{
    (void) cfg;
    HCCL_VM_DEBUG(
        "[virtual-aiv-aclrtLaunchKernelWithHostArgs] called with parameters:\n"
        "  funcHandle = {:p}\n"
        "  numBlocks = {}\n"
        "  stream = {:p}\n"
        "  hostArgs = {:p}\n"
        "  argsSize = {}\n"
        "  placeHolderArray = {:p}\n"
        "  placeHolderNum = {}\n"
        "  currentCmdType = {}\n"
        "  currentArgsType = {}\n"
        "  currentLaunchIndex = {}",
        funcHandle,
        numBlocks,
        stream,
        hostArgs,
        argsSize,
        static_cast<const void *>(placeHolderArray),
        placeHolderNum,
        static_cast<int>(ops_hccl::g_currentAivCmdType),
        static_cast<int>(ops_hccl::g_currentAivArgsType),
        g_currentAivLaunchIndex);
    ops_hccl::DumpLaunchKernelCfg(cfg);
    ops_hccl::DumpPlaceHolderArray(placeHolderArray, placeHolderNum);

    ops_hccl::AivHostLaunchArgs parsedHostArgs {};
    if (!ops_hccl::ParseAivHostLaunchArgs(
        hostArgs, argsSize, ops_hccl::g_currentAivCmdType, parsedHostArgs)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    ops_hccl::DumpHostLaunchArgs(&parsedHostArgs, argsSize);
    const std::string inferredKernelName = ops_hccl::InferKernelNameFromFuncHandle(funcHandle);
    const bool isUnsupportedFallbackCmdType =
        ops_hccl::IsUnsupportedFallbackAivCmdType(ops_hccl::g_currentAivCmdType);
    const std::string fallbackKernelName = ops_hccl::GetFallbackAivKernelName(
        ops_hccl::g_currentAivCmdType,
        static_cast<HcclDataType>(parsedHostArgs.dataType),
        ops_hccl::g_currentAivArgsType);

    std::string kernelName = inferredKernelName;
    if (!kernelName.empty()) {
        HCCL_VM_INFO(
            "[virtual-aiv-aclrtLaunchKernelWithHostArgs] use inferred kernelName from funcHandle = {} "
            "(cmdType={}, dataType={}, argsType={}, fallbackKernelName={})",
            kernelName,
            static_cast<int>(ops_hccl::g_currentAivCmdType),
            parsedHostArgs.dataType,
            static_cast<int>(ops_hccl::g_currentAivArgsType),
            fallbackKernelName.empty() ? "<empty>" : fallbackKernelName.c_str());
    } else {
        if (isUnsupportedFallbackCmdType) {
            HCCL_VM_ERROR(
                "[virtual-aiv-aclrtLaunchKernelWithHostArgs] cmdType={} fallback AIV kernel is not supported currently.",
                static_cast<int>(ops_hccl::g_currentAivCmdType));
            return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
        }
        kernelName = fallbackKernelName;
        if (!kernelName.empty()) {
            HCCL_VM_INFO(
                "[virtual-aiv-aclrtLaunchKernelWithHostArgs] fallback kernelName by cmdType/dataType/argsType = {} "
                "(cmdType={}, dataType={}, argsType={}, inferredKernelName=<empty>)",
                kernelName,
                static_cast<int>(ops_hccl::g_currentAivCmdType),
                parsedHostArgs.dataType,
                static_cast<int>(ops_hccl::g_currentAivArgsType));
        }
    }

    if (kernelName.empty()) {
        HCCL_VM_ERROR(
            "[virtual-aiv-aclrtLaunchKernelWithHostArgs] failed to resolve kernelName from funcHandle/hostArgs, "
            "cmdType={}, dataType={}, argsType={}, can not virtual execute kernel.",
            static_cast<int>(ops_hccl::g_currentAivCmdType),
            parsedHostArgs.dataType,
            static_cast<int>(ops_hccl::g_currentAivArgsType));
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    const std::string soName = ops_hccl::AIV_STUB_SO_NAME;
    HCCL_VM_INFO(
        "[virtual-aiv-aclrtLaunchKernelWithHostArgs] resolved kernelName = {}, soName = {}, cmdType = {}, "
        "argsType = {}",
        kernelName,
        soName,
        static_cast<int>(ops_hccl::g_currentAivCmdType),
        static_cast<int>(ops_hccl::g_currentAivArgsType));

    const uint32_t launchIndex = g_currentAivLaunchIndex;
    if (launchIndex == INVALID_AIV_LAUNCH_INDEX) {
        HCCL_VM_ERROR(
            "[virtual-aiv-aclrtLaunchKernelWithHostArgs] invalid launchIndex={}, can not virtual execute kernel.",
            launchIndex);
        return ACL_ERROR_INTERNAL_ERROR;
    }
    aclError ret = ops_hccl::VirtualExecuteAivKernel(kernelName, soName, numBlocks, parsedHostArgs, launchIndex);
    HCCL_VM_INFO("[virtual-aiv-aclrtLaunchKernelWithHostArgs] virtual execute ret = {}", static_cast<int>(ret));
    return ret;
}
