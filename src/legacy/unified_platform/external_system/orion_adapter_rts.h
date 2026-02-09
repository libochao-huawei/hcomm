/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_ADAPTER_RTS_H
#define HCCLV2_ADAPTER_RTS_H

#include <string>
#include <unordered_map>
#include "acl/acl_rt.h"
#include "types.h"
#include "const_val.h"
#include "dev_type.h"
#include "rt_external.h"
#include "rt_external_kernel.h"

namespace Hccl {
#ifdef CCL_FWK_LLT
typedef void *aclrtCntNotify;
#define ACL_NOTIFY_DEFAULT          0x00000000U
#define  ACL_ERROR_RT_FEATURE_NOT_SUPPORT        207000 // feature not support
#define ACL_NOTIFY_DEVICE_USE_ONLY  0x00000001U
#endif

using HcclRtStream = void*;
using RtNotify_t = void*;
using RtEvent_t = void*;
using RtCntNotify_t = void*;

constexpr u32 RTS_IPC_MEM_NAME_LEN       = 65;
constexpr u32 RTS_IPC_MEM_ALIGNMENT_BYTE = 32;
constexpr u32 CHIP_VERSION_MAX_LEN       = 32;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagRtMemcpyKind {
    RT_MEMCPY_HOST_TO_HOST = 0,  // host to host
    RT_MEMCPY_HOST_TO_DEVICE,    // host to device
    RT_MEMCPY_DEVICE_TO_HOST,    // device to host
    RT_MEMCPY_DEVICE_TO_DEVICE,  // device to device, 1P && P2P
    RT_MEMCPY_MANAGED,           // managed memory
    RT_MEMCPY_ADDR_DEVICE_TO_DEVICE,
    RT_MEMCPY_HOST_TO_DEVICE_EX, // host  to device ex (only used for 8 bytes)
    RT_MEMCPY_DEVICE_TO_HOST_EX, // device to host ex
    RT_MEMCPY_DEFAULT,           // auto infer copy dir
    RT_MEMCPY_RESERVED,
} rtMemcpyKind_t;
typedef enum rtKernelType {
    KERNEL_TYPE_CCE = 0,
    KERNEL_TYPE_FWK = 1,
    KERNEL_TYPE_AICPU = 2,
    KERNEL_TYPE_AICPU_CUSTOM = 4,
    KERNEL_TYPE_AICPU_KFC = 5,
    KERNEL_TYPE_CUSTOM_KFC = 6,
    KERNEL_TYPE_HWTS = 10,
    KERNEL_TYPE_RESERVED = 99,
} rtKernelType_t;

typedef struct tagRtCcuTaskGroup {
    uint32_t taskNum;
    rtCcuTaskInfo_t ccuTaskInfo[FUSION_SUB_TASK_MAX_CCU_NUM];
} rtCcuTaskGroup_t;

typedef struct tagRtDevBinary {
    uint32_t magic;    // magic number
    uint32_t version;  // version of binary
    const void *data;  // binary data
    uint64_t length;   // binary length
} rtDevBinary_t;
extern rtError_t rtFunctionRegister(void *binHandle, const void *stubFunc, const char_t *stubName, const void *kernelInfoExt,
                             uint32_t funcMode);
extern rtError_t rtDevBinaryRegister(const rtDevBinary_t *bin, void **hdl);
extern rtError_t rtKernelLaunchWithFlagV2(const void *stubFunc, uint32_t numBlocks, rtArgsEx_t *argsInfo,
                                   rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo);
extern rtError_t rtWriteValue(rtWriteValueInfo_t * const info, rtStream_t const stm);
/* 3-8包不支持的接口 
* aclrtCntNotifyWaitWithTimeout —— rtsCntNotifyWaitWithTimeout
* aclrtCntNotifyRecord —— rtsCntNotifyRecord
* aclrtCntNotifyDestroy —— rtCntNotifyDestroy
* aclrtCntNotifyCreate —— rtCntNotifyCreateServer
* aclrtGetPhyDevIdByLogicDevId —— rtsGetPhyDevIdByLogicDevId
* aclrtSetDeviceTaskAbortCallback —— rtsSetDeviceTaskAbortCallback
* aclrtCntNotifyGetId —— rtsCntNotifyGetId
* aclrtMallocWithCfg —— rtsMalloc
*/
typedef enum {
    RT_CNT_NOTIFY_WAIT_LESS_MODE = 0x0U,
    RT_CNT_NOTIFY_WAIT_EQUAL_MODE = 0x1U,
    RT_CNT_NOTIFY_WAIT_BIGGER_MODE = 0x2U,
    RT_CNT_NOTIFY_WAIT_BIGGER_OR_EQUAL_MODE = 0x3U,
    RT_CNT_NOTIFY_WAIT_EQUAL_WITH_BITMASK_MODE = 0x4U,
    RT_CNT_NOTIFY_WAIT_MODE_MAX
} rtCntNotifyWaitMode;
typedef struct {
    rtCntNotifyWaitMode mode;
    uint32_t value;
    uint32_t timeout;
    bool isClear;
    uint8_t rev[3U];
} rtCntNotifyWaitInfo_t;
typedef enum {
    RT_CNT_NOTIFY_RECORD_SET_VALUE_MODE = 0x0U,
    RT_CNT_NOTIFY_RECORD_ADD_MODE = 0x1U,
    RT_CNT_NOTIFY_RECORD_BIT_OR_MODE = 0x2U,

    RT_CNT_NOTIFY_RECORD_BIT_AND_MODE = 0x4U,
    RT_CNT_NOTIFY_RECORD_MODE_MAX
} rtCntNotifyRecordMode;

typedef struct {
    rtCntNotifyRecordMode mode;
    uint32_t value;
} rtCntNotifyRecordInfo_t;
using rtCntNotify_t =  void*;
extern rtError_t rtsCntNotifyWaitWithTimeout(rtCntNotify_t cntNotify, rtStream_t stm,
                                      rtCntNotifyWaitInfo_t *info);
extern rtError_t rtsCntNotifyRecord(rtCntNotify_t cntNotify, rtStream_t stm, rtCntNotifyRecordInfo_t *info);
extern rtError_t rtCntNotifyDestroy(rtCntNotify_t const inCntNotify);
extern rtError_t rtCntNotifyCreateServer(rtCntNotify_t * const cntNotify, uint64_t flags);
extern rtError_t rtsGetPhyDevIdByLogicDevId(int32_t logicDevId, int32_t * const phyDevId);
typedef enum {
    RT_DEVICE_TASK_ABORT_PRE = 0,
    RT_DEVICE_TASK_ABORT_POST
} rtDeviceTaskAbortStage;

typedef int32_t (*rtsDeviceTaskAbortCallback)(uint32_t devId, rtDeviceTaskAbortStage stage, uint32_t timeout, void *args);
extern rtError_t rtsSetDeviceTaskAbortCallback(const char_t *regName, rtsDeviceTaskAbortCallback callback, void *args);
extern rtError_t rtsCntNotifyGetId(rtCntNotify_t cntNotify, uint32_t *notifyId);


#define RT_MEMORY_DEFAULT (0x0U)   // default memory on device
#define RT_MEMORY_HBM (0x2U)       // HBM memory on device
#define RT_MEMORY_RDMA_HBM (0x3U)  // RDMA-HBM memory on device
#define RT_MEMORY_DDR (0x4U)       // DDR memory on device
#define RT_MEMORY_POLICY_HUGE_PAGE_ONLY (0x800U)     // Malloc mem only use huge page, 0x1U << 11U
extern rtError_t rtMalloc(void **devPtr, uint64_t size, rtMemType_t type, const uint16_t moduleId);
extern rtError_t rtStreamCreateWithFlags(rtStream_t *stm, int32_t priority, uint32_t flags);
extern rtError_t rtMemPrefetchToDevice(void *devPtr, uint64_t len, int32_t devId);

typedef enum tagRtMemoryType {
    RT_MEMORY_TYPE_HOST = 1,
    RT_MEMORY_TYPE_DEVICE = 2,
    RT_MEMORY_TYPE_SVM = 3,
    RT_MEMORY_TYPE_DVPP = 4,
    RT_MEMORY_TYPE_USER = 5 // by user malloc, unkown memory
} rtMemoryType_t;

typedef enum {
    RT_MEMORY_LOC_HOST = 0,
    RT_MEMORY_LOC_DEVICE,
    RT_MEMORY_LOC_UNREGISTERED,
    RT_MEMORY_LOC_MANAGED,
    RT_MEMORY_LOC_MAX,
} rtMemLocationType;

typedef struct tagRtPointerAttributes {
    rtMemoryType_t memoryType;  // host memory or device memory
    rtMemLocationType locationType;
    uint32_t deviceID;          // device ID
    uint32_t pageSize;
} rtPointerAttributes_t;
extern rtError_t rtIpcCloseMemory(const void *ptr);
extern rtError_t rtIpcDestroyMemoryName(const char_t *name);
extern rtError_t rtPointerGetAttributes(rtPointerAttributes_t *attributes, const void *ptr);
// DPU
typedef enum tagRtXpuDevType {
    RT_DEV_TYPE_DPU = 0,
    RT_DEV_TYPE_REV
} rtXpuDevType;
extern rtError_t rtResetXpuDevice(rtXpuDevType devType, const uint32_t devId);
extern rtError_t rtSetXpuDevice(rtXpuDevType devType, const uint32_t devId);
#ifdef __cplusplus
}
#endif
struct MsprofHcclInfo {
    uint64_t itemId;
    uint64_t cclTag;
    uint64_t groupName;
    uint32_t localRank;
    uint32_t remoteRank;
    uint32_t rankSize;
    uint32_t workFlowMode;
    uint32_t planeID;
    uint32_t ctxId;
    uint64_t notifyID;
    uint32_t stage;
    uint32_t role; // role {0: dst, 1:src}
    double durationEstimated;
    uint64_t srcAddr;
    uint64_t dstAddr;
    uint64_t dataSize; // bytes
    uint32_t opType; // {0: sum, 1: mul, 2: max, 3: min}
    uint32_t dataType; // data type {0: INT8, 1: INT16, 2: INT32, 3: FP16, 4:FP32, 5:INT64, 6:UINT64}
    uint32_t linkType; // link type {0: 'OnChip', 1: 'HCCS', 2: 'PCIe', 3: 'RoCE'}
    uint32_t transportType; // transport type {0: SDMA, 1: RDMA, 2:LOCAL}
    uint32_t rdmaType; // RDMA type {0: RDMASendNotify, 1:RDMASendPayload}
    uint32_t reserve2;
#ifdef __cplusplus
    MsprofHcclInfo() : role(0xFFFFFFFF), srcAddr(0xFFFFFFFF), dstAddr(0xFFFFFFFF),
        dataSize(0xFFFFFFFF), opType(0xFFFFFFFF), 
        dataType(0xFFFFFFFF), linkType(0xFFFFFFFF),
        transportType(0xFFFFFFFF), rdmaType(0xFFFFFFFF)
    {
    }
#endif
};
struct ProfilingDeviceCommResInfo {
    uint64_t groupName; // 通信域
    uint32_t rankSize; // 通信域内rank总数
    uint32_t rankId; // 当前device rankId，通信域内编号
    uint32_t usrRankId; // 当前device rankId，全局编号
    uint32_t aicpuKfcStreamId; // MC2中launch aicpu kfc算子的stream
    uint32_t commStreamSize; // 当前device侧使用的通信stream数量
    uint32_t commStreamIds[8]; // 具体streamId
    uint32_t reserve;
};
typedef struct {
    uint64_t va;
    uint64_t size;
    uint32_t tokenId;
    uint32_t tokenValue;
} rtMemUbTokenInfo;
constexpr uint32_t RT_STREAM_FAST_LAUNCH = 0x200U;
constexpr uint32_t RT_STREAM_FAST_SYNC = 0x400U;
constexpr uint32_t RT_STREAM_CP_PROCESS_USE = 0x800U; // RT_STREAM_CP_PROCESS_USE does not support OR with other flags
constexpr uint32_t RT_NOTIFY_FLAG_DOWNLOAD_TO_DEV = 0x01U; // RT_NOTIFY_FLAG_DOWNLOAD_TO_DEV does not support OR with other flags
constexpr uint64_t RT_NOTIFY_FLAG_DEFAULT = 0x00U;
enum class HcclRtMemcpyKind {
    HCCL_RT_MEMCPY_KIND_HOST_TO_HOST = 0, /**< host to host */
    HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE,   /**< host to device */
    HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST,   /**< device to host */
    HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE, /**< device to device */
    HCCL_RT_MEMCPY_ADDR_DEVICE_TO_DEVICE, /**< Level-2 address copy, device to device */
    HCCL_RT_MEMCPY_KIND_RESERVED,
};
s32     HrtGetDevicePhyIdByIndex(s32 deviceLogicId);
DevType HrtGetDeviceType();
s32     HrtDeviceGetBareTgid();
void    HrtGetSocVer(std::string &socName);
s32     HrtGetDevice();
// 非主线程使用rts添加task情况下，需要先使用该函数通知RTS，将线程和 device logic id绑定
void                  HrtSetDevice(s32 deviceLogicId);
void                  HrtResetDevice(s32 deviceLogicId);
u32                   HrtGetDeviceCount();



HcclResult HrtGetDeviceInfo(uint32_t deviceLogicId, int32_t moduleType, int32_t infoType, int64_t &val);
HcclResult HrtGetMainboardId(uint32_t deviceLogicId, HcclMainboardId &hcclMainboardId);

void *HrtDevBinaryRegister(const rtDevBinary_t *bin);

aclrtStream HrtStreamCreateWithFlags(int32_t priority, uint32_t flags);
void       HrtStreamDestroy(aclrtStream ptr);
void       HrtStreamSetMode(HcclRtStream streamPtr, const uint64_t stmMode);
u64        HrtStreamGetMode(HcclRtStream const ptr);
void       HcclStreamSynchronize(HcclRtStream ptr);
s32        HrtGetStreamId(aclrtStream ptr);
void       HrtStreamActive(aclrtStream activeStream, aclrtStream stream);

void                 *HrtMalloc(u64 size, rtMemType_t memType);
void                  HrtFree(void *devPtr);
void                  HrtMemcpy(void *dst, uint64_t destMax, const void *src, uint64_t count, rtMemcpyKind_t kind);
void                  HrtMemset(void *dst, uint64_t destMax, uint64_t count);
void                  HrtIpcSetMemoryName(void *ptr, char_t *name, u64 ptrMaxLen, u32 nameMaxLen);
void                  HrtIpcDestroyMemoryName(const char_t *name);
void                 *HrtIpcOpenMemory(const char_t *name);
void                  HrtIpcCloseMemory(const void *ptr);
void                  HrtIpcSetMemoryPid(const char_t *name, int pid);
rtPointerAttributes_t HrtPointerGetAttributes(const void *ptr);
void                  PrintMemoryAttr(const void *memAddr);
void                  HrtDevMemAlignWithPage(void *ptr, u64 size, void *&ipcPtr, u64 &ipcSize, u64 &ipcOff);
HcclResult            HrtMemPrefetchToDevice(void *devPtr, uint64_t len);

void *HrtMallocHost(u64 size);
void  HrtFreeHost(void *hostPtr);

// rts notify manager api
aclrtNotify HrtNotifyCreate(s32 deviceLogicId);
aclrtNotify HrtNotifyCreateWithFlag(u32 devId, u32 flag);
void       HrtNotifyDestroy(RtNotify_t ptr);
void       HrtIpcSetNotifyName(RtNotify_t ptr, char_t *name, uint32_t len);

u32        HrtGetNotifyID(RtNotify_t notifyHandle);
u64        HrtNotifyGetAddr(RtNotify_t notifyHandle);
void       HrtSetIpcNotifyPid(aclrtNotify notify, int32_t pid);
RtNotify_t HrtIpcOpenNotify(const char_t *name);
RtNotify_t HrtIpcOpenNotifyWithFlag(const char_t *name, uint32_t flags);
u32       HrtNotifyGetOffset(RtNotify_t ptr);

// rts notify task api
void HrtNotifyWaitWithTimeOut(RtNotify_t notifyPtr, aclrtStream streamPtr, uint32_t timeOut);
void HrtNotifyRecord(RtNotify_t notifyPtr, aclrtStream streamPtr);

// rts memcpy task api
void HrtMemAsyncCopy(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtMemcpyKind kind,
                     aclrtStream streamPtr);

// rts reduce task api
void HrtReduceAsync(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtReduceKind kind,
                    aclDataType type, aclrtStream streamPtr);

// rts rdma task
void HrtRDMASend(u32 qpn, u32 wqeIndex, aclrtStream streamPtr); // 910A offload
void HrtRDMADBSend(uint32_t dbindex, uint64_t dbinfo,
                   aclrtStream streamPtr); // 910A opbase and 910A2/910A3

void HrtKernelLaunchWithFlagV2(const void *stubFunc, uint32_t numBlocks, rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc,
    rtStream_t stream, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo);
void HrtFunctionRegister(void *binHandle, const void *stubFunc, const char *stubName, const void *devFunc,
                                uint32_t funcMode);
void HrtAicpuKernelLaunchExWithArgs(uint32_t kernelType, const char *opName, uint32_t numBlocks,
                                    const rtAicpuArgsEx_t *argsInfo, rtSmDesc_t * const smDesc, const rtStream_t stream,
                                    uint32_t flags);

// rts task exception api
void HrtRegTaskFailCallbackByModule(aclrtExceptionInfoCallback callback);

// 添加任一task后可获取得到 taskId, streamId
void HrtGetTaskIdAndStreamID(u32 &taskId, u32 &streamId);
u64  HrtGetRdmaDoorbellAddr(s32 deviceLogicId, u32 dbIndex);
u32  HrtStreamGetSqId(const aclrtStream ptr);
u32  HrtStreamGetCqId(const aclrtStream ptr);

// 对rts结构体打桩，联调用，待RTS接口上线后，删除掉
struct HrtUbDbDetailInfo {
    u16 functionId;
    u16 dieId;
    u16 rsv;
    u16 jettyId;
    u16 piValue;
};

struct HrtUbDbInfo {
    u8                dbNum;
    u8                wrCqe;
    HrtUbDbDetailInfo info[2];
};

struct HrtUbWqeInfo {
    u16 wrCqe;
    u16 functionId;
    u16 dieId;
    u16 wqeSize;
    u16 jettyId;
    u8 *wqe;
    u16 wqePtrLen;
};

constexpr u32 DWQE_SIZE_64  = 64;
constexpr u32 DWQE_SIZE_128 = 128;

void HrtUbDbSend(const HrtUbDbInfo &info, aclrtStream streamPtr);

void HrtUbDirectSend(const HrtUbWqeInfo &info, aclrtStream streamPtr);

aclrtCntNotify HrtCntNotifyCreate(u32 deviceId);

u32 HrtGetCntNotifyId(const aclrtCntNotify inCntNotify);

void HrtCntNotifyDestroy(const aclrtCntNotify inCntNotify);

MAKE_ENUM(HrtCntNotifyRecordMode, WRITE_BIT, STORE)
void HrtCntNotifyRecord(const aclrtCntNotify inCntNotify, const aclrtStream streamPtr, HrtCntNotifyRecordMode mode, u32 value);
MAKE_ENUM(HrtCntNotifyWaitMode, EQUAL, BITMAP)
void HrtCntNotifyWaitWithTimeOut(const aclrtCntNotify inCntNotify, const aclrtStream streamPtr, HrtCntNotifyWaitMode mode, u32 value,
                                 u32 timeout, bool isClear = true);

void HrtCcuLaunch(rtCcuTaskInfo_t &taskInfo, aclrtStream const streamPtr);
void HrtUbDevQueryInfo(rtUbDevQueryCmd cmd, void *devInfo);
// pair<tokendId, tokenValue>
std::pair<u32, u32> HrtUbDevQueryToken(u64 addr, u64 size);
MAKE_ENUM(HrtDevResProcType, PROCESS_CP1, PROCESS_HCCP)
MAKE_ENUM(HrtDevResType, RES_TYPE_STARS_NOTIFY_RECORD, RES_TYPE_CCU_CKE, RES_TYPE_CCU_XN,
          RES_TYPE_STARS_CNT_NOTIFY_BIT_WR)
struct HrtDevResInfo {
    u32               dieId{0}; // for ccu res need set devId, for others set 0
    HrtDevResProcType procType{HrtDevResProcType::PROCESS_CP1};
    HrtDevResType     resType{HrtDevResType::RES_TYPE_STARS_NOTIFY_RECORD};
    u32               resId{0};
    u32               flag{0};
};

struct HrtDevResAddrInfo {
    u64 address{0};
    u32 len{0};
};

HrtDevResAddrInfo HrtGetDevResAddress(const HrtDevResInfo &devResInfo);
void              HrtReleaseDevResAddress(const HrtDevResInfo &devResInfo);

MAKE_ENUM(HrtEventStatus, EVENT_INIT, EVENT_RECORDED)
aclrtEvent      HrtEventCreateWithFlag(u32 flag);
void           HrtEventDestroy(RtEvent_t eventPtr);
void           HrtEventRecord(RtEvent_t eventPtr, aclrtStream streamPtr);
HrtEventStatus HrtEventQueryStatus(RtEvent_t eventPtr);

void HrtWriteValue(u64 addr, u32 piVal, const aclrtStream streamPtr);
void HrtDeviceAbortRegCallBack(rtsDeviceTaskAbortCallback callback, void *args);
} // namespace Hccl

#endif // HCCL_ADAPTER_RTS_H
