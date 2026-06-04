/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOM_H
#define HCOM_H

#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <functional>
#include <vector>
#include <unordered_map>
#include <map>
#include "workflow.h"
#include "dtype_common.h"
#include "hccl/hccl_rank_graph.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

namespace hccl {
    // 该结构体已在ge定义，此处定义为在hccl内使用。
    struct HcclDumpInfo {
        u32 task_id;
        u32 stream_id;
        u32 sub_task_type;  // 0 SDMA\ 1 AI CORE
        void* output_addr;  // if sdma: dst
        uint64_t output_size;
        void* input_addr;   // if sdma: src
        uint64_t input_size;
    };
}  // namespace hccl

// profiling状态
enum class HcomProfilingMode {
    PROFILING_CLOSE = 0,
    PROFILING_OPEN = 1,
    PROFILING_RESERVED
};

typedef struct HcomInitConfig {
    char* algo;
    char* execTimeOut;
    u8 deterministic;

    HcomInitConfig() : algo(nullptr), execTimeOut(nullptr), deterministic(0) {}
} HcomInitConfig;

typedef struct HcomOpParamDef {
    char *group;  // 通信域groupName
    char *opType;  // 算子类型
    HcclDataType dataType; // 数据类型
	HcclReduceOp reduceOp; // 规约类型
    u8 geDeterministic;      // 是否为确定性计算
    u32 aivCoreLimit; // aiv核数限制

    char *socVersion; // soc字符串，用于查询devType
    char *rankTable;
	u32 *groupList;  // groupList解析结果
	u32 groupListSize;    // groupList的大小
	u64 count; // 数据量
    u64 rankSize;

    struct {
        HcclDataType sendType;
        HcclDataType recvType;
        void* sendCounts;
        void* recvCounts;
        void* sendDispls;
        void* recvDispls;
        void* sendCountMatrix;
    } All2AllDataDes;
    union {
        uint8_t reserved[128]; // 预留扩展字段，长度为128字节
        // 可在此处添加新的结构体及其成员
    };

    HcomOpParamDef() : group(nullptr), opType(nullptr),
        dataType(HcclDataType::HCCL_DATA_TYPE_RESERVED), reduceOp(HcclReduceOp::HCCL_REDUCE_RESERVED),
        geDeterministic(0), aivCoreLimit(0), socVersion(nullptr), rankTable(nullptr), groupList(nullptr),
        groupListSize(0), count(0), rankSize(0),
        All2AllDataDes{ HcclDataType::HCCL_DATA_TYPE_RESERVED, HcclDataType::HCCL_DATA_TYPE_RESERVED,
                        nullptr, nullptr, nullptr, nullptr, nullptr } {}
} HcomOpParam;

typedef struct HcomResResponseDef {
    u64 streamNum;
    u64 taskNum;
    u64 opMemSize;

    HcomResResponseDef() : streamNum(0), taskNum(0), opMemSize(0) {}
} HcomResResponse;

constexpr u32 ALLTOALLV_RANK_MAX_NUM = 256; // 受notify数量限制，全连接组网alltoallv最多支持256p 分级alltoallv可以做到512
constexpr u32 ALLTOALLVC_RANK_MAX_NUM = 256; // 受notify数量限制，全连接组网alltoallvc最多支持256p 分级alltoallv可以做到512
constexpr u32 CCL_OP_TAG_MAX_LEN = 512;
constexpr u32 ALG_NAME_MAX_LEN = 256; // 最大的group name 长度

enum class CommNumHcom {
    COMM_VALUE_DEFAULT = 0, // 默认值为图模式
    COMM_VALUE_RESERVED
};

/* hccl算子类型 */
const std::string HCCL_KERNEL_OP_TYPE_BROADCAST = "HcomBroadcast";
const std::string HCCL_KERNEL_OP_TYPE_SCATTER = "HcomScatter";
const std::string HCCL_KERNEL_OP_TYPE_ALLREDUCE = "HcomAllReduce";
const std::string HCCL_KERNEL_OP_TYPE_ALLGATHER = "HcomAllGather";
const std::string HCCL_KERNEL_OP_TYPE_ALLGATHERV = "HcomAllGatherV";
const std::string HCCL_KERNEL_OP_TYPE_REDUCESCATTER = "HcomReduceScatter";
const std::string HCCL_KERNEL_OP_TYPE_SEND = "HcomSend";
const std::string HCCL_KERNEL_OP_TYPE_RECEIVE = "HcomReceive";
const std::string HCCL_KERNEL_OP_TYPE_REDUCE = "HcomReduce";
const std::string HCCL_KERNEL_OP_TYPE_ALLTOALLV = "HcomAllToAllV";
const std::string HCCL_KERNEL_OP_TYPE_ALLTOALLVC = "HcomAllToAllVC";
const std::string HCCL_KERNEL_OP_TYPE_GATHER_ALLTOALLV = "HcomGatherAllToAllV";
const std::string HCCL_KERNEL_OP_TYPE_ALLTOALL = "HcomAllToAll";
const std::string HCCL_KERNEL_OP_TYPE_REDUCESCATTERV = "HcomReduceScatterV";

const std::map<std::string, HcclCMDType> HCCL_OPTYPE_NAME_MAP = {
    {HCCL_KERNEL_OP_TYPE_BROADCAST, HcclCMDType::HCCL_CMD_BROADCAST},
    {HCCL_KERNEL_OP_TYPE_SCATTER, HcclCMDType::HCCL_CMD_SCATTER},
    {HCCL_KERNEL_OP_TYPE_ALLREDUCE, HcclCMDType::HCCL_CMD_ALLREDUCE},
    {HCCL_KERNEL_OP_TYPE_REDUCE, HcclCMDType::HCCL_CMD_REDUCE},
    {HCCL_KERNEL_OP_TYPE_SEND, HcclCMDType::HCCL_CMD_SEND},
    {HCCL_KERNEL_OP_TYPE_RECEIVE, HcclCMDType::HCCL_CMD_RECEIVE},
    {HCCL_KERNEL_OP_TYPE_ALLGATHER, HcclCMDType::HCCL_CMD_ALLGATHER},
    {HCCL_KERNEL_OP_TYPE_ALLGATHERV, HcclCMDType::HCCL_CMD_ALLGATHER_V},
    {HCCL_KERNEL_OP_TYPE_REDUCESCATTER, HcclCMDType::HCCL_CMD_REDUCE_SCATTER},
    {HCCL_KERNEL_OP_TYPE_REDUCESCATTERV, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V},
    {HCCL_KERNEL_OP_TYPE_ALLTOALLV, HcclCMDType::HCCL_CMD_ALLTOALLV},
    {HCCL_KERNEL_OP_TYPE_ALLTOALLVC, HcclCMDType::HCCL_CMD_ALLTOALLVC},
    {HCCL_KERNEL_OP_TYPE_ALLTOALL, HcclCMDType::HCCL_CMD_ALLTOALL},
};

using HcclRtStream = void *;
using rtStream_t = void *;

/**
 * @brief Get the rank number in the group.
 *
 * @param group A string identifying the group name.
 * @param rankSize A pointer identifying the rank number.
 * @return HcclResult
 */
HCCL_API HcclResult HcomGetRankSize(const char *group, u32 *rankSize);

/**
 * @brief Get the rank id of this rank.
 *
 * @param group A string identifying the group name.
 * @param rankId A pointer identifying the rank id.
 * @return HcclResult
 */
HCCL_API HcclResult HcomGetRankId(const char *group, u32 *rankId);

/**
 * @brief Create group.
 *
 * @param group A string identifying the group name.
 * @param rankNum An integer(u32) identifying the number of ranks in the group.
 * @param rankIds A list identifying the ranks in the group.
 * @return HcclResult
 */
HCCL_API HcclResult HcomCreateGroup(const char *group, u32 rankNum, u32 *rankIds);

/**
 * @brief Destroy group
 *
 * @param group A string identifying the group name.
 * @return HcclResult
 */
HCCL_API HcclResult HcomDestroyGroup(const char *group);

/**
 * @brief optimizer offload CPU-side hcom init.
 *
 * @param rankTable A string identifying the rank table.
 * @param rankId An integer(u32) identifying the number of rank id.
 * @return HcclResult
 */
extern HCCL_API HcclResult HcomInitByRankTable(const char *rankTable, uint32_t rankId);

/**
 * @brief optimizer offload CPU-side hcom destroy.
 *
 * @return HcclResult
 */
extern HCCL_API HcclResult HcomDestroy(void);

extern HCCL_API HcclResult HcomGetCommHandleByGroup(const char *group, HcclComm *commHandle);

HCCL_API HcclResult HcomGetGroupNameByOpBase(s64 opBaseHcom, char **groupname);
HCCL_API HcclResult GetGroupNameByOpBaseHcom(s64 opBaseHcom, char **groupname);

HCCL_API HcclResult HcomCreateComResourceByComm(HcclComm comm, u32 streamMode, bool isOpbaseMode,
    void** commContext, bool isMC2 = false);

HCCL_API void HcomTopoInfoRegCallback(HcclResult (*p1)(const char *, uint32_t), void (*p2)(const char *));

HCCL_API HcclWorkflowMode HcomGetWorkflowMode();

HCCL_API HcclResult HcomSetWorkflowMode(HcclWorkflowMode mode);

HCCL_API HcclResult HcomCalcOpOnline(HcomOpParam *hcomOpParam, HcomResResponse *hcomResResponse);

HCCL_API HcclResult HcomCalcOpResOffline(HcomOpParam *hcomOpParam, HcomResResponse *hcomResResponse);

HCCL_API HcclResult HcomGetMemType(const char *group, const char *socVersion, bool isMalloc, u32 *memType, bool *isTsMem,
    bool withoutImplCompile = false, bool level2Address = false);

HCCL_API HcclResult HcomGetBandWidthPerNPU(u32 level, float *bandWidth);

HCCL_API HcclResult HcomGetServerNumAndDeviceNumPerServer(u32 *serverNum, u32 *deviceNumPerServer, u32 *deviceNumPerAggregation);

HCCL_API bool HcomGetSecAddrCopyFlag(const char *socVersion);

HCCL_API HcclResult HcomInitByString(const char *rankTableM, const char *identify,
    WorkMode commWorkMode = WorkMode::HCCL_MODE_NORMAL, HcomInitConfig *initConfig = nullptr);

HCCL_API HcclResult HcomInitByMasterInfo(const char *masterIp, const char *masterPort,
    const char *masterDeviceId, const char *rankSize, const char *rankIp, HcomInitConfig *initConfig = nullptr);

HCCL_API HcclResult HcomCreateCommCCLbuffer(const char *group);

HCCL_API HcclResult HcomGetInCCLbuffer(const char *group, void** buffer, u64 *size);

HCCL_API HcclResult HcomGetOutCCLbuffer(const char *group, void** buffer, u64 *size);

HCCL_API void HcomSetLaunchKernelMode(bool state);

HCCL_API HcclResult HcomGetAicpuOpStreamNotify(const char *group, HcclRtStream *opStream, u8 aicpuNotifyNum, void** aicpuNotify);

HCCL_API HcclResult HcomMc2AiCpuStreamAllocAndGet(const char *group, u32 streamMode, rtStream_t *aiCpuStream);

HCCL_API void HcomSetDumpDebugMode(const bool dumpDebug);

HCCL_API HcclResult HcomGetAlgorithm(u32 level, char** algo);

HCCL_API HcclResult HcomGetAlgExecParam(const char *tag, const char *group, u64 count, void *inputPtr, void *outputPtr,
    HcclCMDType opType, bool clearEnable, HcclDataType dataType, HcclReduceOp op, 
    void **commContext, u64 *len, u32 aivCoreLimit);

HCCL_API void HcomSetAutoTuneMode(bool autoTuneMode);

HCCL_API DevType HcomGetDeviceType();

HCCL_API HcclResult HcomSetProfilingMode(HcomProfilingMode profilingMode, const char *profilingOption);

HCCL_API HcclResult HcomGetSplitStrategy(const char *group, const struct model_feature *feature,
    u32 **segmentIdxPtr, u32 *len, bool *configured, GradSplitForceMode force = GradSplitForceMode::FORCE_NONE,
    OriginalGraphShapeType shapeType = OriginalGraphShapeType::KNOWN_SHAPE);

HCCL_API bool HcomFindGroup(const char *group);

#define TEMP_WEAK_DEF 1

#define HCOM_SELECT_ALG_POINTER_MODE
HCCL_API HcclResult HcomSelectAlg(s64 comm, const char *group, u64 count, void* counts,
    HcclDataType dataType, HcclReduceOp op, HcclCMDType opType, int32_t aivCoreLimit,
    bool *ifAiv, char *algName);

HCCL_API HcclResult HcomCalcAivCoreNum(const char *group, HcclCMDType opType, u64 count, void* counts, HcclDataType dataType,
    int32_t aivCoreLimit, char *algName, u32 *numBlocks);

HCCL_API HcclResult HcomSetWorkspaceResource(const char *tag, const char *group, rtStream_t *stream,
    s32 len, void *memPtr, u64 maxSize);

HCCL_API HcclResult HcomSetGlobalWorkSpace(const char *group, void **globalWorkSpaceAddr, u32 len);

HCCL_API HcclResult HcomSetAivCoreLimit(const char *group, u32 aivCoreLimit);

HCCL_API HcclResult HcomReleaseSubComms();

HCCL_API HcclResult HcomUnloadTask(const char *group, const char *tag);

HCCL_API HcclResult HcomClearAivSyncBuf(const char *group, bool aivClearEnable);

HCCL_API HcclResult HcomSetAttachedStream(const char *group, u32 graphId, const rtStream_t *stream, s32 len);

HCCL_API HcclResult HcomSupportDeterministicOptim(const char *group, bool *isDeterministicOptim);

HCCL_API HcclResult HcomTbeMemClean(int64_t addrList[], int64_t sizeList[], uint32_t count,
    rtStream_t stream, int32_t deviceLogicId);

HCCL_API HcclResult HcomGetInitStatus(bool *initiated);
HCCL_API HcclResult HcomAllGather(const char *tag, void *inputPtr, void *outputPtr, u64 inputCount,
    HcclDataType dataType, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomAllGatherV(const char *tag, const void *sendBuf, u64 sendCount, const void *recvBuf,
    const void *recvCounts, const void *rdispls, HcclDataType dataType, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomAllReduce(const char *tag, void *inputPtr, void *outputPtr, u64 count,
    HcclDataType dataType, HcclReduceOp op, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomReduce(const char *tag, void *inputPtr, void *outputPtr, u64 count, HcclDataType dataType,
    HcclReduceOp op, u32 root, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomBroadcast(const char *tag, void *ptr, u64 count, HcclDataType dataType, u32 root,
    const char *group, rtStream_t stream);
HCCL_API HcclResult HcomReduceScatter(const char *tag, void *inputPtr, void *outputPtr, u64 count,
    HcclDataType dataType, HcclReduceOp op, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomReduceScatterV(const char *tag, void *sendBuf, const void *sendCounts, const void *sdispls,
    void *recvBuf, u64 recvCount, HcclDataType dataType, HcclReduceOp op, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomSend(const char *tag, void *inputPtr, u64 count, HcclDataType dataType,
    u32 destRank, u32 srTag, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomReceive(const char *tag, void *outputPtr, u64 count, HcclDataType dataType,
    u32 srcRank, u32 srTag, const char *group, rtStream_t stream);
HCCL_API HcclResult HcomAlltoAllV(const void *sendBuf, const void *sendCounts, const void *sdispls, HcclDataType sendType,
    const void *recvBuf, const void *recvCounts, const void *rdispls, HcclDataType recvType,
    const char *group, rtStream_t stream, const char *tag);
HCCL_API HcclResult HcomAlltoAllVC(const void *sendBuf, const void *sendCountMatrix, HcclDataType sendType,
    const void *recvBuf, HcclDataType recvType, const char *group, rtStream_t stream, const char *tag);
HCCL_API HcclResult HcomAllToAll(const void *sendBuf, u64 sendCount, HcclDataType sendType,
                        const void *recvBuf, u64 recvCount, HcclDataType recvType,
                        const char *group, rtStream_t stream, const char *tag);
HCCL_API HcclResult HcomGenerateCclOpTag(const char *opType, s64 hcomComm, const char *group, char *sTag);
HCCL_API HcclResult HcomGetCommCCLBufferSize(const char *group, uint64_t &size);
HCCL_API HcclResult HcomGetL0TopoTypeEx(const char *group, CommTopo *topoType, uint32_t flag);
HCCL_API HcclResult HcomGetRankSizeEx(const char *group, uint32_t *rankSize, uint32_t flag);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCOM_H
