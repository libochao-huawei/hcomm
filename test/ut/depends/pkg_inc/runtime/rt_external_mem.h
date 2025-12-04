/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description: mem.h
 * Create: 2020-01-01
 */

#ifndef CCE_RUNTIME_RT_EXTERNAL_MEM_H
#define CCE_RUNTIME_RT_EXTERNAL_MEM_H

#include <stddef.h>
#include "rt_external_base.h"
#include "rt_external_stars_define.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tagInitFlowGwInfo {
    const char_t *groupName;
    uint64_t schedPolicy;
    uint64_t reschedInterval;
    char_t rsv[128];
} rtInitFlowGwInfo_t;
/**
 * @ingroup rt_mem_queue
 * @brief init flow gateway
 * @param [in] devId   the logical device id
 * @param [in] initInfo   Initialization parameters
 * @return RT_ERROR_NONE for ok
 */
RTS_API rtError_t rtMemQueueInitFlowGw(int32_t devId, const rtInitFlowGwInfo_t * const initInfo);

/**
 * @ingroup rt_mem_queue
 * @brief destroy mbuf queue init
 * @param [in] devId   the logical device id
 * @return RT_ERROR_NONE for ok
 */
RTS_API rtError_t rtMemQueueInit(int32_t devId);

typedef enum tagBuffGetCmdType {
    RT_BUFF_GET_MBUF_TIMEOUT_INFO = 0,
    RT_BUFF_GET_MBUF_USE_INFO = 1,
    RT_BUFF_GET_MBUF_TYPE_INFO = 2,
    RT_BUFF_GET_MBUF_BUILD_INFO = 3,
    RT_BUFF_GET_MAX
} rtBuffGetCmdType;

typedef struct tagBuffBuildInfo {
    uint32_t status;  /* 0: buff unbuild   1: buff build */
} rtBuffBuildInfo;

RTS_API rtError_t rtBuffGetInfo(rtBuffGetCmdType type, const void * const inBuff, uint32_t inLen,
    void * const outBuff, uint32_t * const outLen);


typedef void *rtMbufPtr_t;

/**
* @ingroup rt_mem_queue
* @brief alloc buff
* @param [out] memBuf: buff addr alloced
* @param [in]  size: The amount of memory space requested
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufAlloc(rtMbufPtr_t *memBuf, uint64_t size);

/**
* @ingroup rt_mem_queue
* @brief alloc buff
* @param [out] memBuf: buff addr alloced
* @param [in]  size: The amount of memory space requested
* @param [in]  flag: Huge page flag(bit0~31: mem type, bit32~bit35: devid, bit36~63: resv)
* @param [in]  grpId: group id
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufAllocEx(rtMbufPtr_t *memBuf, uint64_t size, uint64_t flag, int32_t grpId);
/**
* @ingroup rt_mem_queue
* @brief free buff
* @param [in] memBuf: buff addr to be freed
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufFree(rtMbufPtr_t memBuf);

/**
* @ingroup rt_mem_queue
* @brief set Data len of Mbuf
* @param [in] memBuf: Mbuf addr
* @param [in] len: data len
* @return   RT_ERROR_NONE for success, others for fail
*/
RTS_API rtError_t rtMbufSetDataLen(rtMbufPtr_t memBuf, uint64_t len);

/**
* @ingroup rt_mem_queue
* @brief set Data len of Mbuf
* @param [in] memBuf: Mbuf addr
* @param [out] len: data len
* @return   RT_ERROR_NONE for success, others for fail
*/
RTS_API rtError_t rtMbufGetDataLen(rtMbufPtr_t memBuf, uint64_t *len);

/**
* @ingroup rt_mem_queue
* @brief get Data addr of Mbuf
* @param [in] memBuf: Mbuf addr
* @param [out] buf: Mbuf data addr
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufGetBuffAddr(rtMbufPtr_t memBuf, void **buf);

/**
* @ingroup rt_mem_queue
* @brief get total Buffer size of Mbuf
* @param [in] memBuf: Mbuf addr
* @param [out] totalSize: total buffer size of Mbuf
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufGetBuffSize(rtMbufPtr_t memBuf, uint64_t *totalSize);

/**
* @ingroup rt_mem_queue
* @brief Get the address and length of its user_data from the specified Mbuf
* @param [in] memBuf: Mbuf addr
* @param [out] priv: address of its user_data
* @param [out]  size: length of its user_data
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufGetPrivInfo(rtMbufPtr_t memBuf,  void **priv, uint64_t *size);

/**
* @ingroup rt_mem_queue
* @brief copy buf ref
* @param [in] memBuf: src buff addr
* @param [out] newMemBuf: des buff addr
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufCopyBufRef(rtMbufPtr_t memBuf, rtMbufPtr_t *newMemBuf);

/**
* @ingroup rt_mem_queue
* @brief get mbuffer
* @param [in] mbufPtr: buff addr alloced
* @param [out]  buff: The buffer of mbuPtr
* @param [in]  size: The amount of memory space of buffer
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtBuffGet(const rtMbufPtr_t mbufPtr, void *buff, const uint64_t size);

/**
* @ingroup rt_mem_queue
* @brief free buff
* @param [in]  buff: The buff id the shared memory pointer applied by calling halBuffAlloc and halBuffAllocByPool
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtBuffFree(void *buff);

/**
* @ingroup rt_mem_queue
* @brief alloc buff
* @param [out] mbufPtr: buff addr alloced
* @param [in]  buff: The buff must be the shared memory pointer applied by calling halBuffAlloc and halBuffAllocByPool
* @param [in]  size: The amount of memory space requested
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufBuild(void *buff, const uint64_t size, rtMbufPtr_t *mbufPtr);

/**
* @ingroup rt_mem_queue
* @brief free the head of mbufPtr
* @param [in] mbufPtr: buff addr alloced
* @param [out]  buff: The buffer of mbuPtr
* @param [out]  size: The amount of memory space of buffer
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufUnBuild(const rtMbufPtr_t mbufPtr, void **buff, uint64_t *size);

/**
* @ingroup rt_mem_queue
* @brief put mbuffer
* @param [in] mbufPtr: buff addr alloced
* @param [out]  buff: The buffer of mbuPtr
* @param [out]  size: The amount of memory space of buffer
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtBuffPut(const rtMbufPtr_t mbufPtr, void *buff);

#define RT_MEM_BUFF_MAX_CFG_NUM 64

typedef struct {
    uint32_t cfgId;    // cfg id, start from 0
    uint32_t totalSize;  // one zone total size
    uint32_t blkSize;  // blk size, 2^n (0, 2M]
    uint32_t maxBufSize; // max size can alloc from zone
    uint32_t pageType;  // page type, small page / huge page
    int32_t elasticEnable; // elastic enable
    int32_t elasticRate;
    int32_t elasticRateMax;
    int32_t elasticHighLevel;
    int32_t elasticLowLevel;
} rtMemZoneCfg_t;

typedef struct {
    rtMemZoneCfg_t cfg[RT_MEM_BUFF_MAX_CFG_NUM];
}rtMemBuffCfg_t;

/**
* @ingroup rt_mem_queue
* @brief device buff init
* @param [in] cfg, init cfg
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtMbufInit(rtMemBuffCfg_t *cfg);

/**
* @ingroup rt_mem_queue
* @brief alloc buff
* @param [out]  buff: The buff id the shared memory pointer applied by calling halBuffAlloc and halBuffAllocByPool
* @param [in]  size: The amount of memory space requested
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtBuffAlloc(const uint64_t size, void **buff);

/**
* @ingroup rt_mem_queue
* @brief determine whether buff id is the shared memory pointer applied by calling halBuffAlloc and halBuffAllocByPool
* @param [in]  buff: The buff id the shared memory pointer applied by calling halBuffAlloc and halBuffAllocByPool
* @param [in]  size: The amount of memory space requested
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtBuffConfirm(void *buff, const uint64_t size);

#define RT_DEV_PROCESS_CP1 0
#define RT_DEV_PROCESS_CP2 1
#define RT_DEV_PROCESS_DEV_ONLY 2
#define RT_DEV_PROCESS_QS 3
#define RT_DEV_PROCESS_SIGN_LENGTH 49

typedef struct tagBindHostpidInfo {
    int32_t hostPid;
    uint32_t vfid;
    uint32_t chipId;
    int32_t cpType; // type of custom-process, see RT_DEV_PROCESS_XXX
} rtBindHostpidInfo_t;

/**
* @ingroup rt_mem_queue
* @brief  query device proccess id
* @param [in] info: see struct rtBindHostpidInfo_t
* @param [out] devPid: device proccess id
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtQueryDevPid(rtBindHostpidInfo_t *info, int32_t *devPid);

#define RT_MQ_EVENT_QS_MSG 27 // same as driver's

#define RT_MQ_SCHED_PRIORITY_LEVEL0 0 // same as driver's
#define RT_MQ_SCHED_PRIORITY_LEVEL1 1
#define RT_MQ_SCHED_PRIORITY_LEVEL2 2
#define RT_MQ_SCHED_PRIORITY_LEVEL3 3
#define RT_MQ_SCHED_PRIORITY_LEVEL4 4
#define RT_MQ_SCHED_PRIORITY_LEVEL5 5
#define RT_MQ_SCHED_PRIORITY_LEVEL6 6
#define RT_MQ_SCHED_PRIORITY_LEVEL7 7

/* Events can be released between different systems. This parameter specifies the destination type of events
   to be released. The destination type is defined based on the CPU type of the destination system. */
#define RT_MQ_DST_ENGINE_ACPU_DEVICE 0            // device AICPU, same as driver's
#define RT_MQ_DST_ENGINE_ACPU_HOST 1              // Host AICPU
#define RT_MQ_DST_ENGINE_CCPU_DEVICE 2           // device CtrlCPU
#define RT_MQ_DST_ENGINE_CCPU_HOST 3             // Host CtrlCPU
#define RT_MQ_DST_ENGINE_DCPU_DEVICE 4          // device DataCPU
#define RT_MQ_DST_ENGINE_TS_CPU 5                 // device TS CPU
#define RT_MQ_DST_ENGINE_DVPP_CPU 6               // device DVPP CPU

#define RT_MQ_SCHED_EVENT_QS_MSG 25 // same as driver's EVENT_QS_MSG
#define RT_MQ_SCHED_EVENT_DRV_CUSTOM_MSG 56  // drvier's custom msg event

/* When the destination engine is AICPU, select a policy.
   ONLY: The command is executed only on the local AICPU.
   FIRST: The local AICPU is preferentially executed. If the local AICPU is busy, the remote AICPU can be used. */
#define RT_SCHEDULE_POLICY_ONLY 0 // same as driver's schedule_policy
#define RT_SCHEDULE_POLICY_FIRST 1 // same as driver's schedule_policy


typedef struct tagEschedEventSummary {
    int32_t pid; // dst PID
    uint32_t grpId;
    int32_t eventId; // only RT_MQ_SCHED_EVENT_QS_MSG is supported
    uint32_t subeventId;
    uint32_t msgLen;
    char_t *msg;
    uint32_t dstEngine; // dst system cpu type
    int32_t policy; // RT_SCHEDULE_POLICY_ONLY or RT_SCHEDULE_POLICY_FIRST
} rtEschedEventSummary_t;

typedef struct tagEschedEventReply {
    char_t *buf;
    uint32_t bufLen;
    uint32_t replyLen; // output, ack msg len, same with msgLen in halEschedAckEvent
} rtEschedEventReply_t;

/**
* @ingroup rt_mem_queue
* @brief  Commit the event to a specific process
* @param [in] devId: logic devid
* @param [in] evt: event summary info
* @param [out] ack: event reply info
* @return RT_ERROR_NONE for ok
*/
RTS_API rtError_t rtEschedSubmitEventSync(int32_t devId, rtEschedEventSummary_t *evt,
                                          rtEschedEventReply_t *ack);

#define RT_MEM_GRP_NAME_LEN 32  // it must be same as driver define BUFF_GRP_NAME_LEN
#define RT_MEM_CACHE_MAX_NUM 1024  // it must be same as driver define BUFF_CACHE_MAX_NUM

// mem group
typedef struct {
    uint64_t maxMemSize; // max buf size in grp, in KB. = 0 means no limit
    uint32_t cacheAllocFlag;
    uint32_t addGrpTimeout;
    int32_t rsv[RT_MEM_GRP_NAME_LEN - 2];
} rtMemGrpConfig_t;

/**
* @ingroup rt_mem_queue
* @brief create mem group
* @attention null
* @param [in] name, group name
* @param [in] cfg, group cfg
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtMemGrpCreate(const char_t *name, const rtMemGrpConfig_t *cfg);

typedef struct {
    uint64_t memSize;
    uint32_t memFlag;
    int32_t rsv[RT_MEM_CACHE_MAX_NUM];
} rtMemGrpCacheAllocPara;

/**
* @ingroup rt_mem_queue
* @brief alloc mem group cache
* @attention null
* @param [in] name, group name
* @param [in] devId, device id
* @param [in] para, mem group cache alloc para
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtMemGrpCacheAlloc(const char_t *name, int32_t devId, const rtMemGrpCacheAllocPara *para);

typedef struct {
    uint32_t admin : 1;     // admin permission, can add other proc to grp
    uint32_t read : 1;     // read only permission
    uint32_t write : 1;    // read and write permission
    uint32_t alloc : 1;    // alloc permission (have read and write permission)
    uint32_t rsv : 28;
} rtMemGrpShareAttr_t;

/**
* @ingroup rt_mem_queue
* @brief add process to group
* @param [in] name, group name
* @param [in] pid, process id
* @param [in] attr, process permission in group
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtMemGrpAddProc(const char_t *name, int32_t pid, const rtMemGrpShareAttr_t *attr);

/**
* @ingroup rt_mem_queue
* @brief attach proccess to check permission in group
* @param [in] name, group name
* @param [in] timeout, time out ms
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtMemGrpAttach(const char_t *name, int32_t timeout);

typedef enum tagGroupQueryCmdType {
    RT_MEM_GRP_QUERY_GROUP,                  /* not support */
    RT_MEM_GRP_QUERY_GROUPS_OF_PROCESS,      /* query process all grp */
    RT_MEM_GRP_QUERY_GROUP_ID,               /* query grp ID by grp name */
    RT_MEM_GRP_QUERY_GROUP_ADDR_INFO,        /* query group addr info */
    RT_MEM_GRP_QUERY_CMD_MAX                 /* not support */
} rtGroupQueryCmdType;

typedef struct {
    int32_t pid;
} rtMemGrpQueryByProc_t; // cmd: RT_MEM_GRP_QUERY_GROUPS_OF_PROCESS

typedef struct {
    char grpName[RT_MEM_GRP_NAME_LEN];
} rtMemGrpQueryGroupId_t; // cmd: RT_MEM_GRP_QUERY_GROUP_ID

typedef struct {
    char grpName[RT_MEM_GRP_NAME_LEN];
    uint32_t devId;
} rtMemGrpQueryGroupAddrPara_t; /* cmd: RT_MEM_GRP_QUERY_GROUP_ADDR_INFO */

typedef struct {
    int32_t cmd;  // value range: rtGroupQueryCmdType
    union {
        rtMemGrpQueryByProc_t grpQueryByProc; // cmd: RT_MEM_GRP_QUERY_GROUPS_OF_PROCESS
        rtMemGrpQueryGroupId_t grpQueryGroupId; // cmd: RT_MEM_GRP_QUERY_GROUP_ID
        rtMemGrpQueryGroupAddrPara_t grpQueryGroupAddrPara; // cmd: RT_MEM_GRP_QUERY_GROUP_ADDR_INFO
    };
} rtMemGrpQueryInput_t;

typedef struct {
    char_t groupName[RT_MEM_GRP_NAME_LEN];  // group name
    rtMemGrpShareAttr_t attr; // process in group attribute
} rtMemGrpOfProc_t; // cmd: RT_MEM_GRP_QUERY_GROUPS_OF_PROCESS

typedef struct {
    int32_t groupId; // group id
} rtMemGrpQueryGroupIdInfo_t; // cmd: RT_MEM_GRP_QUERY_GROUP_ID

typedef struct {
    uint64_t addr; /* cache memory addr */
    uint64_t size; /* cache memory size */
} rtMemGrpQueryGroupAddrInfo_t; /* cmd: RT_MEM_GRP_QUERY_GROUP_ADDR_INFO */

typedef struct {
    size_t maxNum; // max number of result
    size_t resultNum; // if the number of results exceeds 'maxNum', only 'maxNum' results are filled in buffer
    union {
        rtMemGrpOfProc_t *groupsOfProc; // cmd: RT_MEM_GRP_QUERY_GROUPS_OF_PROCESS
        rtMemGrpQueryGroupIdInfo_t *groupIdInfo; // cmd: RT_MEM_GRP_QUERY_GROUP_ID
        rtMemGrpQueryGroupAddrInfo_t *groupAddrInfo; // cmd: RT_MEM_GRP_QUERY_GROUP_ADDR_INFO
    };
} rtMemGrpQueryOutput_t;

/**
* @ingroup rt_mem_queue
* @brief buff group query
* @param [in] input, query input
* @param [in|out] output, query output
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtMemGrpQuery(rtMemGrpQueryInput_t * const input, rtMemGrpQueryOutput_t *output);

/**
* @ingroup rt_mem_queue
* @brief buff group query
* @param [in] devId, cdevice id
* @param [in] name, group name
* @param [out] qid, queue id
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtMemQueueGetQidByName(int32_t devId, const char_t *name, uint32_t *qId);

/**
* @ingroup rt_mem_queue
* @brief esched attach device
* @param [in] devId, device id
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedAttachDevice(int32_t devId);

/**
* @ingroup rt_mem_queue
* @brief esched dettach device
* @param [in] devId, device id
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedDettachDevice(int32_t devId);

/**
* @ingroup rt_mem_queue
* @brief esched wait event
* @param [in] devId, device id
* @param [in] grpId, group id
* @param [in] threadId, thread id
* @param [in] timeout
* @param [in] evt
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedWaitEvent(int32_t devId, uint32_t grpId, uint32_t threadId,
                                    int32_t timeout, rtEschedEventSummary_t *evt);

/**
* @ingroup rtQueueSubscribe
* @brief queue subscribe
* @param [in] devId, device id
* @param [in] qid, queue id
* @param [in] groupId, group id
* @param [in] type

* @return   0 for success, others for fail
*/
RTS_API rtError_t rtQueueSubscribe(int32_t devId, uint32_t qId, uint32_t groupId, int32_t type);

typedef enum rtEventIdType {
    RT_EVENT_RANDOM_KERNEL,      /* Random operator event */
    RT_EVENT_DVPP_MSG,           /* operator events commited by DVPP */
    RT_EVENT_FR_MSG,             /* operator events commited by Feature retrieves */
    RT_EVENT_TS_HWTS_KERNEL,     /* operator events commited by ts/hwts */
    RT_EVENT_AICPU_MSG,          /* aicpu activates its own stream events */
    RT_EVENT_TS_CTRL_MSG,        /* controls message events of TS */
    RT_EVENT_QUEUE_ENQUEUE,      /* entry event of Queue(consumer) */
    RT_EVENT_QUEUE_FULL_TO_NOT_FULL,   /* full to non-full events of Queue(producers) */
    RT_EVENT_QUEUE_EMPTY_TO_NOT_EMPTY,   /* empty to non-empty event of Queue(consumer) */
    RT_EVENT_TDT_ENQUEUE,        /* data entry event of TDT */
    RT_EVENT_TIMER,              /* ros timer */
    RT_EVENT_HCFI_SCHED_MSG,     /* scheduling events of HCFI */
    RT_EVENT_HCFI_EXEC_MSG,      /* performs the event of HCFI */
    RT_EVENT_ROS_MSG_LEVEL0,
    RT_EVENT_ROS_MSG_LEVEL1,
    RT_EVENT_ROS_MSG_LEVEL2,
    RT_EVENT_ACPU_MSG_TYPE0,
    RT_EVENT_ACPU_MSG_TYPE1,
    RT_EVENT_ACPU_MSG_TYPE2,
    RT_EVENT_CCPU_CTRL_MSG,
    RT_EVENT_SPLIT_KERNEL,
    RT_EVENT_DVPP_MPI_MSG,
    RT_EVENT_CDQ_MSG,
    /* Add a new event here */
    RT_EVENT_TEST,               /* Reserve for test */
    RT_EVENT_MAX_NUM
} rtEventIdType_t;

/**
* @ingroup rtEschedAckEvent
* @brief esched ack event
* @param [in] devId, device id
* @param [in] evtId, event type
* @param [in] subEvtId, sub event type
* @param [in] msg, message info
* @param [in] len, message length
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedAckEvent(int32_t devId, rtEventIdType_t evtId,
                                   uint32_t subEvtId, char_t *msg, uint32_t len);

/**
* @ingroup rtQueueSubF2NFEvent
* @brief full to not full event
* @param [in] devId, device id
* @param [in] qid, queue id
* @param [in] groupId, group id
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtQueueSubF2NFEvent(int32_t devId, uint32_t qId, uint32_t groupId);

typedef enum rtGroupType {
    /* Bound to a AICPU, multiple threads can be woken up simultaneously within a group */
    RT_GRP_TYPE_BIND_DP_CPU = 1,
    RT_GRP_TYPE_BIND_CP_CPU,             /* Bind to the control CPU */
    RT_GRP_TYPE_BIND_DP_CPU_EXCLUSIVE    /* Bound to a AICPU, intra-group threads are mutex awakened */
} rtGroupType_t;

/**
* @ingroup rt_mem_queue
* @brief esched create group
* @param [in] devId, device id
* @param [in] grpId, group id
* @param [in] type, group type
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedCreateGrp(int32_t devId, uint32_t grpId, rtGroupType_t type);

/**
* @ingroup rt_mem_queue
* @brief esched submit event
* @param [in] devId, device id
* @param [in] evt
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedSubmitEvent(int32_t devId, rtEschedEventSummary_t *evt);

/**
* @ingroup rt_mem_queue
* @brief esched submit event
* @param [in] devId, device id
* @param [in] grpId, group id
* @param [in] threadId, thread id
* @param [in] eventBitmap
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedSubscribeEvent(int32_t devId, uint32_t grpId, uint32_t threadId, uint64_t eventBitmap);

// EschedQueryInfo group
#define EVENT_MAX_GRP_NAME_LEN   16
typedef enum tagEschedQueryType {
    RT_QUERY_TYPE_LOCAL_GRP_ID,
    RT_QUERY_TYPE_REMOTE_GRP_ID,
    RT_QUERY_TYPE_MAX
} rtEschedQueryType;

typedef struct tagEschedInputInfo {
    void *inBuff;
    unsigned int inLen;
} rtEschedInputInfo;

typedef struct tagEschedOutputInfo {
    void *outBuff;
    unsigned int outLen;
} rtEschedOutputInfo;

typedef struct tagEschedQueryGidInput {
    int pid;
    char grpName[EVENT_MAX_GRP_NAME_LEN];
} rtEschedQueryGidInput;

typedef struct tagEschedQueryGidOutput {
    unsigned int grpId;
} rtEschedQueryGidOutput;

/**
* @ingroup rtEschedQueryInfo
* @brief  query esched info, such as grpid.
* @param [in] devId: logic devid
* @param [in] type: query info type
* @param [in] inPut: Input the corresponding data structure based on the type.
* @param [out] outPut: OutPut the corresponding data structure based on the type.
* @return   0 for success, others for fail
*/
RTS_API rtError_t rtEschedQueryInfo(const uint32_t devId, const rtEschedQueryType type,
    rtEschedInputInfo *inPut, rtEschedOutputInfo *outPut);

typedef enum tagRtDebugMemoryType {
    RT_MEM_TYPE_L0A = 1,
    RT_MEM_TYPE_L0B = 2,
    RT_MEM_TYPE_L0C = 3,
    RT_MEM_TYPE_UB = 4,
    RT_MEM_TYPE_L1 = 5,
    RT_MEM_TYPE_DCACHE = 10,
    RT_MEM_TYPE_ICACHE = 11,
    RT_MEM_TYPE_REGISTER = 101,
    RT_MEM_TYPE_MAX,
} rtDebugMemoryType_t;

typedef struct tagRtDebugMemoryParam {
    uint8_t coreType; // aic/aiv
    uint8_t reserve;
    uint16_t coreId;
    rtDebugMemoryType_t debugMemType;
    uint32_t elementSize;
    uint32_t reserved;
    uint64_t srcAddr;
    uint64_t dstAddr;  // host addr
    uint64_t memLen;
} rtDebugMemoryParam_t;

/**
 * @ingroup dvrt_mem
 * @brief read mem info while holding the core
 * @param [in] param
 * @return RT_ERROR_NONE for ok, errno for failed
 * @return RT_ERROR_INVALID_VALUE for error input
 */
RTS_API rtError_t rtDebugReadAICore(rtDebugMemoryParam_t *const param);

/**
 * @ingroup dvrt_mem
 * @brief HCCL Async memory cpy
 * @param [in] sqIndex sq index
 * @param [in] wqeIndex moudle index
 * @param [in] stm asynchronized task stream
 * @return RT_ERROR_NONE for ok
 * @return RT_ERROR_INVALID_VALUE for error input
 * @return RT_ERROR_DRV_ERR for driver error
 */
RTS_API rtError_t rtRDMASend(uint32_t sqIndex, uint32_t wqeIndex, rtStream_t stm);

/**
 * @ingroup dvrt_mem
 * @brief HCCL Async memory cpy
 * @param [in] dbindex single device 0
 * @param [in] dbinfo doorbell info
 * @param [in] stm asynchronized task stream
 * @return RT_ERROR_NONE for ok
 * @return RT_ERROR_INVALID_VALUE for error input
 * @return RT_ERROR_DRV_ERR for driver error
 */
RTS_API rtError_t rtRDMADBSend(uint32_t dbIndex, uint64_t dbInfo, rtStream_t stm);

typedef struct tagUbDbDetailInfo {
    uint16_t functionId : 7;
    uint16_t dieId : 1;
    uint16_t rsv : 8;
    uint16_t jettyId;
    uint16_t piValue;
} rtUbDbDetailInfo_t;

typedef struct tagUbDbInfo {
    uint8_t dbNum;
    uint8_t wrCqe;
    rtUbDbDetailInfo_t info[4];
} rtUbDbInfo_t;

typedef struct tagUbWqeInfo {
    uint16_t wrCqe : 1;
    uint16_t functionId : 7;
    uint16_t dieId : 1;
    uint16_t wqeSize : 1;
    uint16_t rsv : 6;
    uint16_t jettyId;
    uint8_t *wqe;
    uint16_t wqePtrLen;
} rtUbWqeInfo_t;

/**
 * @ingroup rt_stars
 * @brief ub doorbell send
 * @param [in] dbSendInfo       dbSendInfo input
 * @param [in] stm              stm: stream handle
 * @return RT_ERROR_NONE for ok, others failed
 */
RTS_API rtError_t rtUbDbSend(rtUbDbInfo_t *dbInfo,  rtStream_t stm);
 
/**
 * @ingroup rt_stars
 * @brief ub direct wqe send
 * @param [in] wqeInfo          wqeInfo input
 * @param [in] stm              stm: stream handle
 * @return RT_ERROR_NONE for ok, others failed
 */
RTS_API rtError_t rtUbDirectSend(rtUbWqeInfo_t *wqeInfo, rtStream_t stm);

typedef enum {
    RT_MEM_MALLOC_HUGE_FIRST,
    RT_MEM_MALLOC_HUGE_ONLY,
    RT_MEM_MALLOC_NORMAL_ONLY,
    RT_MEM_MALLOC_HUGE_FIRST_P2P,
    RT_MEM_MALLOC_HUGE_ONLY_P2P,
    RT_MEM_MALLOC_NORMAL_ONLY_P2P,
    RT_MEM_MALLOC_HUGE1G_ONLY,
    RT_MEM_MALLOC_HUGE1G_ONLY_P2P,
    RT_MEM_TYPE_LOW_BAND_WIDTH = 0x0100,            // DDR type -> RT_MEMORY_DDR
    RT_MEM_TYPE_HIGH_BAND_WIDTH = 0x1000,           // HBM type -> RT_MEMORY_HBM

    RT_MEM_ACCESS_USER_SPACE_READONLY = 0x100000,   // use for dvpp
} rtMallocPolicy;

typedef enum { 
    RT_MEM_ADVISE_NONE = 0,
    RT_MEM_ADVISE_DVPP,
    RT_MEM_ADVISE_TS,
    RT_MEM_ADVISE_CACHED,
} rtMallocAdvise;

typedef enum {
    RT_MEM_MALLOC_ATTR_RSV = 0,
    RT_MEM_MALLOC_ATTR_MODULE_ID,   // 申请内存的模块id
    RT_MEM_MALLOC_ATTR_DEVICE_ID,   // 指定deviceId申请内存
    RT_MEM_MALLOC_ATTR_MAX
} rtMallocAttr;

typedef union {
    uint16_t moduleId;  // 默认不配置时，为RUNTIME_ID
    uint32_t deviceId;  // 默认不配置时，为ctx的deviceId
    uint8_t rsv[8];     // 预留8字节
} rtMallocAttrValue;

typedef struct {
    rtMallocAttr attr;
    rtMallocAttrValue value;
} rtMallocAttribute_t;

typedef struct {
    rtMallocAttribute_t *attrs;
    size_t numAttrs;
} rtMallocConfig_t;

typedef struct {    // use for rtMallocAttrValue
    uint16_t moduleId;
    uint32_t deviceId;
} rtConfigValue_t;

/**
 * @ingroup rts_mem
 * @brief alloc device memory
 * @param [in|out] devPtr   memory pointer
 * @param [in] size         memory size
 * @param [in] policy       memory policy
 * @param [in] advise       memory advise, such as TS,DVPP
 * @param [in] cfg   memory attributes config, such ModuleId, DeviceId
 * @return RT_ERROR_NONE for ok
 * @return RT_ERROR_INVALID_VALUE for error input
 */
RTS_API rtError_t rtMemAlloc(void **devPtr, uint64_t size, rtMallocPolicy policy, rtMallocAdvise advise, rtMallocConfig_t *cfg);

#if defined(__cplusplus)
}
#endif

#endif  // CCE_RUNTIME_RT_EXTERNAL_MEM_H
