/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
/*
 * 该特性代码不涉及开源
 */

#ifndef INTERFACE_HCCL_H
#define INTERFACE_HCCL_H

#include "acl/acl_base.h"
#include "stream_pub.h"
#include "hccl/base.h"

#define GID_LENGTH 16

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AscendQPInfoDef {
    uint32_t qpn;
    uint32_t gidIdx;
    uint8_t gid[GID_LENGTH];
    uint32_t psn;
    uint32_t sq_depth;
    uint32_t rq_depth;
    uint32_t scq_depth;
    uint32_t rcq_depth;
    uint64_t reserved[32];
} AscendQPInfo;

typedef struct AscendQPQosDef {
    uint32_t tc;
    uint32_t sl;
} AscendQPQos;

/**
* @brief 异构场景RDMA设备初始化接口
*/
HcclResult hcclAscendRdmaInit();

/**
* @brief 异构场景RDMA设备解初始化接口
*/
HcclResult hcclAscendRdmaDeInit();

/**
* @brief 异构场景QP创建接口
* @param ascendQPInfo (out): 本地QP信息。
*/
HcclResult hcclCreateAscendQP(AscendQPInfo* ascendQPInfo);

/**
* @brief 异构场景QP带参数创建接口
* @param ascendQPInfo (in & out): 本地QP信息和参数配置。
*/
HcclResult hcclCreateAscendQPWithAttr(AscendQPInfo* ascendQPInfo);

/**
* @brief 异构场景QP状态迁移
* @param localQPInfo (in): 本地QP信息。
* @param remoteQPInfo (in): 远端QP信息。
*/
HcclResult hcclModifyAscendQP(AscendQPInfo* localQPInfo, AscendQPInfo* remoteQPInfo);

/**
* @brief 异构场景QP状态迁移扩展，支持用户参数配置qpQos，如tc、sl
* @param localQPInfo (in): 本端QP信息。
* @param remoteQPInfo (in): 远端QP信息。
* @param qpQos (in): QP qos信息。
*/
HcclResult hcclModifyAscendQPEx(AscendQPInfo* localQPInfo, AscendQPInfo* remoteQPInfo, AscendQPQos* qpQos);

/**
* @brief 异构场景QP销毁接口
* @param ascendQPInfo (in): 本端待销毁QP信息。
*/
HcclResult hcclDestroyAscendQP(AscendQPInfo* ascendQPInfo);

typedef struct AscendMrInfoDef {
    uint64_t addr;  // in: starting address of mr
    uint64_t size;  // in: size of mr
    uint32_t key;   // out: local addr access key
} AscendMrInfo;

/**
* @brief 异构场景数据内存申请
* @param ptr (in): 基地址
* @param size (in): 长度
*/
HcclResult hcclAllocWindowMem(void **ptr, uint64_t size);

/**
* @brief 异构场景数据内存释放
* @param ptr (in): 基地址
*/
HcclResult hcclFreeWindowMem(void *ptr);

/**
* @brief 异构场景同步内存申请
* @param ptr (in): 基地址
*/
HcclResult hcclAllocSyncMem(int32_t **ptr);

/**
* @brief 异构场景同步内存释放
* @param ptr (in): 基地址
*/
HcclResult hcclFreeSyncMem(int32_t *ptr);

/**
* @brief 异构场景内存（数据内存和同步内存）注册
* @param memInfo (in\out): 注册内存信息
*/
HcclResult hcclRegisterMem(AscendMrInfo* memInfo);

/**
* @brief 异构场景内存（数据内存和同步内存）解注册
* @param memInfo (in): 注册内存信息
*/
HcclResult hcclDeRegisterMem(AscendMrInfo* memInfo);

typedef struct AscendSendRecvInfoDef {
    AscendQPInfo* localQPinfo;
    AscendMrInfo* localWindowMem;
    AscendMrInfo* remoteWindowMem;
    AscendMrInfo* localSyncMemPrepare;
    AscendMrInfo* localSyncMemDone;
    AscendMrInfo* localSyncMemAck;
    AscendMrInfo* remoteSyncMemPrepare;
    AscendMrInfo* remoteSyncMemDone;
    AscendMrInfo* remoteSyncMemAck;
    uint32_t immData = 0; // 默认为0， 不发送立即数，当imma不为0时，支持按照immData作为立即数发送
} AscendSendRecvInfo;

typedef struct AscendSendRecvLinkInfoDef {
    AscendQPInfo* localQPinfo;
    AscendMrInfo* localSyncMemPrepare;
    AscendMrInfo* localSyncMemDone;
    AscendMrInfo* localSyncMemAck;
    AscendMrInfo* remoteSyncMemPrepare;
    AscendMrInfo* remoteSyncMemDone;
    AscendMrInfo* remoteSyncMemAck;
    uint32_t immData = 0; // 默认为0， 不发送立即数，当imma不为0时，支持按照immData作为立即数发送
    uint32_t wqePerDoorbell; // 下发多少个wr之后敲一次doorbell，必须小于等于300
} AscendSendRecvLinkInfo;

/**
* @brief 异构场景点对点通信接口-发送
* @param sendBuf (in): 发送内存基地址
* @param count (in): 数据个数
* @param dataType (in): 数据类型
* @param sendRecvInfo (in): QP\window内存\同步内存信息
* @param stream (in): 异步执行stream
*/
HcclResult HcclSendByAscendQP(void* sendBuf, uint64_t count, HcclDataType dataType,
    AscendSendRecvInfo* sendRecvInfo, aclrtStream stream);

/**
* @brief 异构场景点对点通信接口-接收
* @param recvBuf (out): 接收内存基地址
* @param count (in): 数据个数
* @param dataType (in): 数据类型
* @param sendRecvInfo (in): QP\window内存\同步内存信息
* @param stream (in): 异步执行stream
*/
HcclResult HcclRecvByAscendQP(void* recvBuf, uint64_t count, HcclDataType dataType,
    AscendSendRecvInfo* sendRecvInfo, aclrtStream stream);

struct HcclErrCqeInfo {
    uint32_t status;
    uint32_t qpn;
    struct timeval time;
};

/**
* @brief 异构场景QP状态迁移接口，并支持按照QP粒度设置RDMA qos，包括sl、tc。
* @param localQPInfo (in): 本地QP信息。
* @param remoteQPInfo (in):对端QP信息。
* @param remoteQPInfo (in):对本端QP配置Qos，包含tc，sl值。tc值有效范围[0，255]，且必须是4的倍数。sl值有效范围[0，7]。
*/
HcclResult HcclGetCqeErrInfoList(struct HcclErrCqeInfo *infoList, uint32_t *num);

/**
* @brief 按照QP粒度获取RDMA error cqe 信息。
* @param qpn (in): qp编号
* @param infoList (out): error cqe 数组指针。
* @param num (in/out):获取error cqe的个数，不大于128。初始值配置必须大于0。
*/
HcclResult HcclGetCqeErrInfoListByQpn(uint32_t qpn, struct HcclErrCqeInfo *infoList, uint32_t *num);

/**
* @brief 异构场景单边通信接口，put数据到对端。
* @param num (in):要发送的数据组数
* @param count(in):向对端发送的数据的数量
* @param dataType(in):对端接收数据的类型
* @param sendRecvInfo: AscendSendRecvLinkInfo指针，包含的信息有本端的qp信息，本端和对端的syncmem信息
* @param stream：异步执行stream
*/
HcclResult HcclPutByAscendQP(void* putBuf, uint64_t count, HcclDataType dataType,
    AscendSendRecvInfo* sendRecvInfo, aclrtStream stream);

/**
* @brief 异构场景单边通信接口，批量put数据到对端。
* @param num (in):要发送的数据组数
* @param putMRList(in):向对端发送的数据的mr信息的数组指针
* @param remoteMRList(in):对端接收数据的mr信息的数组指针
* @param sendRecvLinkInfo: AscendSendRecvLinkInfo指针，包含的信息有本端的qp信息，本端和对端的syncmem信息和要发送的立即数以及敲doorbell的频次
* @param stream：异步执行stream
*/
HcclResult HcclBatchPutMRByAscendQP(unsigned int num, AscendMrInfo* putMRList, AscendMrInfo* remoteMRList,
    AscendSendRecvLinkInfo* sendRecvLinkInfo, aclrtStream stream);

HcclResult HcclWaitPutMRByAscendQP(AscendSendRecvLinkInfo* sendRecvLinkInfo, aclrtStream stream);

HcclResult HcclWaitPutMRDoWait(AscendSendRecvLinkInfo* sendRecvLinkInfo, aclrtStream stream);

HcclResult HcclWaitPutMRDoRecord(AscendSendRecvLinkInfo* sendRecvLinkInfo, aclrtStream stream);

HcclResult hcclGetSyncMemRegKey(AscendMrInfo* memInfo);

typedef struct AscendSendLinkInfoDef {
    AscendQPInfo* localQPinfo; // 本端的QP内存信息
    AscendMrInfo* remoteNotifyValueMem; // 内存值为1的远端内存
    AscendMrInfo* localSyncMemAck; // 本端的notifywait
    uint32_t wqePerDoorbell; // 下发多少个wr之后敲一次doorbell，必须小于等于300
} AscendSendLinkInfo;

HcclResult HcclOneSideBatchPutByAscendQP(unsigned int num, AscendMrInfo* putMRList, AscendMrInfo* remoteMRList,
    AscendSendLinkInfo* sendlinkInfo, aclrtStream stream);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif