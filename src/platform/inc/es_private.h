/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ES_PRIVATE_H
#define ES_PRIVATE_H

#include <climits>
#include "private_types.h"
#include "hccl_common.h"

constexpr s64 HCCL_SUB_STREAM_ES_LOOKUP = 3;  // embadding service场景下，lookup算子流水化执行需要3条从流
#define MEMBER_OFFSET(type, member) ((u64)(&(((type *)nullptr)->member)))

using MemoryStartAndSize = struct MemoryStartAndSizeDef {
    void *startAddr;
    u64 size;
    MemoryStartAndSizeDef() : startAddr(nullptr), size(0) {}
};

const std::string HCCL_KERNEL_OP_TYPE_REMOTE_LOOKUP = "HcomRemoteLookup";
const std::string HCCL_KERNEL_OP_TYPE_COLL_REMOTE_UPDATE = "HcomCollRemoteUpdate";
const std::string HCCL_KERNEL_OP_TYPE_COLL_REMOTE_LOOKUP = "HcomCollRemoteLookup";
const std::string HCCL_KERNEL_OP_TYPE_GATHER = "HcomGather";
const std::string HCCL_KERNEL_OP_TYPE_COLL_REMOTE_LOOKUP_PAIRED = "HcomCollRemoteLookupPaired";
const std::string HCCL_KERNEL_OP_TYPE_COLL_REMOTE_LOOKUP_UNIQUED_PAIRED = "HcomCollRemoteLookupUniquedAndPaired";
const std::string HCCL_KERNEL_OP_TYPE_COLL_REMOTE_UPDATE_PAIRED = "HcomCollRemoteUpdatePaired";

// ZEROCOPY
constexpr s32 ZERO_COPY_USED = 1; // zerocopy使用标识
constexpr s32 ZERO_COPY_UNUSED = 0; // zerocopy未使用标识

constexpr u32 SERVICE_CANCEL_SIGNAL = 0xFFFFFFFF;

struct HcclRdmaSignalInfo {
    u32 type = INVALID_UINT;
    s32 mrRegFlag = INVALID_INT;
    void *notifyAddr;
    u64 len = INVALID_U64;
    hccl::MemType memType = hccl::MEM_TYPE_RESERVED;
    u32 lkey = INVALID_UINT;
};

// Ebadding Service aicpu接口参数声明 start
#ifdef CCL_LLT
constexpr s32 ES_PIPELINE_THRESHOLD = 1000; // key num大于65536就选择流水执行
#else
constexpr s32 ES_PIPELINE_THRESHOLD = INT_MAX; // key num大于65536就选择流水执行
#endif
constexpr s32 ES_PIPELINE_NOFITY_COUNT = 3; // lookup流水执行时，4条流分别执行4个kernel，每个cube使用3个notify做同步


// PS数量规格当前256
constexpr u32 HDCS_RANK_SIZE = 256;

struct HdcsTransportInfo {
    hccl::HcclHeterogCommType commType{};
    s32 deviceLogicId{};
    void *inputShmMem{};
    u64 inputMemSize{};
    void *outputShmMem{};
    u64 outputMemSize{};
    void *envelopMem{};
    u32 envelopeSize{};
    u32 rank{};
    s32 tag{};
};

struct HdcsEmbeddingServiceParam {
    s32 tag = 0;
    u32 devId = 0;
    void *keys = nullptr;
    u64 keyMaxNum = 0;
    u32 tableId{};
    HcclDataType keyType = HCCL_DATA_TYPE_RESERVED;
    s64 *keyNumInput{};
    s32 *uniqueIndices{};
    s32 *keyCount{};
    void *values = nullptr;
    void *indices{};
    void *numUniqued{};
    void *psSeg{};
    void *psSegNum{};
    u64 valueCount = 0;
    HcclDataType valuesType = HCCL_DATA_TYPE_RESERVED;
    char groupName[GROUP_NAME_MAX_LEN] = {0};

    void *tableIdAddr = nullptr;
    s32 insertFlag{};
    s32 valueItemSize{};

    void *keyTransferMem = nullptr;
    u64 keyTransferMemSize = 0;
    void *valueTransferMem = nullptr;
    u64 valueTransferMemSize = 0;
    void *rdmaEnveInfosTransferMem{};
    u64 rdmaEnveInfosTransferMemSize{};
    u64 workerspaceMemSize{};
    s32 cubeIndex{ 0 };
    u64 pipelineKeyNum = 0;
    void *errorMsg{};
    s32 intZerocpyFlag{};
    s32 outZerocpyFlag{};
    void *globalStepAddr{ nullptr };

    bool usePipeline{};
    // pairedMode为true，表示采用了Paired接口
    bool pairedMode{ false };
    bool enableKeyCounter{};
    bool disableUnique{};
    bool uniqued{ false };
    bool haveRdmaConn{ false };
};

struct RemoteLookupPubParams {
    s64 *keys = nullptr;
    u64 keyMaxNum = 0;
    s32 *tableId{ nullptr };
    s32 valueItemSize;
    s32 maxEmbeddingDim;
    std::string uniqueTag;
    void *value = nullptr;
};
struct HdcsCsInitPara {
    u32 workerList[HDCS_RANK_SIZE];
    u32 psList[HDCS_RANK_SIZE];
    u32 psSize;
    u32 workerSize;
    char rankTableM[HETEROG_RANK_TABLE_MAX_LEN] = {0};
    u32 rank;
    char groupName[GROUP_NAME_MAX_LEN] = {0};
    void *devicePid; // output
    bool isInference = false;
    bool enableEntryLog = false;
};

struct HdcsRegTransportPara {
    char groupName[GROUP_NAME_MAX_LEN]{};
    s32 transportNum{};
    s32 pcieTransportNum{};
    HdcsTransportInfo transportInfos[HDCS_RANK_SIZE]{};
};

// lookup send key、recv value、recover value、reset unique handle公用这个参数结构体
struct HdcsLookupPipelineParams {
    s32 tag = 0;
    char groupName[GROUP_NAME_MAX_LEN] = {0};
    // lookup的具体参数在aicpu侧的hdcs impl实例对象中存储，通过cubeIndex访问，当前不考虑同一个通信域内lookup并行场景
    s32 cubeIndex{ 0 };
    bool usePipeline{ false };
    bool intZerocpyFlag{ false };
    void *tableIdAddr{};
};

struct PsBufferInfo {
    s64 offset{};
    s64 count{};
};

struct RdmaBuffer {
    u64 addr;
    u64 buffKey;
};

// 当前先按8字节填充方式，共16字节
constexpr u32 ES_KEY_AND_COUNTER_MEM_BYTES_SIZE = SIZE_TABLE[HCCL_DATA_TYPE_INT128];
constexpr u32 ES_KEY_MEM_BYTES_SIZE = SIZE_TABLE[HCCL_DATA_TYPE_INT64];
constexpr u32 ES_KEY_COUNTER_MEM_BYTES_SIZE = ES_KEY_AND_COUNTER_MEM_BYTES_SIZE - ES_KEY_MEM_BYTES_SIZE;

constexpr u32 ES_FLAGS_ENABLE_COUNTER = 1;

constexpr u32 ES_MAX_PS_NUM = HDCS_RANK_SIZE;

template <typename T>
T Align(T value, T alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

constexpr u32 IPC_MEM_ALIGNMENT_BYTE = 32;
#endif /* ES_PRIVATE_H */
