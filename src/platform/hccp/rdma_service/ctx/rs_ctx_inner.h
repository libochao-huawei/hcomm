/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rdma_server context external interface declaration
 * Create: 2025-04-15
 */

#ifndef RS_CTX_INNER_H
#define RS_CTX_INNER_H

#include <pthread.h>
#include <urma_types.h>
#include "hccp_ctx.h"
#include "rs_inner.h"
#include "rs_list.h"

#define DEV_INDEX_DIEID_OFFSET 8U
#define DEV_INDEX_CNT_OFFSET 16U
#define DEV_INDEX_UE_INFO_MASK 0x0000FFFFUL
#define CI_ADDR_TWO_BYTES 2
#define CI_ADDR_BUFFER_ALIGN_4K_PAGE_SIZE 4096U

struct RsUbDevCb {
    struct rs_cb *rscb;
    unsigned int phyId;
    unsigned int eidIndex;
    union HccpEid eid;
    urma_context_t *urmaCtx;
    urma_device_t *urmaDev;
    unsigned int index;
    struct DevBaseAttr devAttr;

    unsigned int cqeErrCnt;
    pthread_mutex_t cqeErrCntMutex;

    pthread_mutex_t mutex;
    unsigned int asyncEventCnt;
    unsigned int jfceCnt;
    unsigned int jfcCnt;
    unsigned int jettyCnt;
    unsigned int rjettyCnt;
    unsigned int tokenIdCnt;
    unsigned int lsegCnt;
    unsigned int rsegCnt;
    struct RsListHead asyncEventList;
    struct RsListHead jfceList;
    struct RsListHead jfcList;
    struct RsListHead jettyList;
    struct RsListHead rjettyList;
    struct RsListHead tokenIdList;
    struct RsListHead lsegList;
    struct RsListHead rsegList;
    struct RsListHead list;
};

struct RsCtxAsyncEventCb {
    struct RsUbDevCb *devCb;
    urma_async_event_t asyncEvent;
    struct RsListHead list;
};

struct RsCtxJfceCb {
    struct RsUbDevCb *devCb;
    uint64_t jfceAddr; // urma_jfce_t *
    union DataPlaneCstmFlag dataPlaneFlag;
    struct RsListHead list;
};

struct RsCtxJfcCb {
    struct RsUbDevCb *devCb;
    uint64_t jfcAddr;
    enum JfcMode jfcType;
    struct RsListHead list;
    struct {
        bool valid;
        uint32_t cqeFlag;
    } ccuExCfg;
};

struct RsCrErrInfo {
    pthread_mutex_t mutex;
    struct CrErrInfo info;
};

struct RsCtxJettyCb {
    struct RsUbDevCb *devCb;
    urma_jetty_t *jetty;
    urma_jfr_t *jfr;
    int jettyMode;
    uint32_t jettyId;
    int transportMode;
    unsigned int state;
    unsigned int txDepth;
    unsigned int rxDepth;
    urma_jetty_flag_t flag;
    urma_jfs_flag_t jfsFlag;
    uint64_t tokenIdAddr; /**< NULL means unspecified */
    unsigned int tokenValue;
    uint8_t priority;
    uint8_t rnrRetry;
    uint8_t errTimeout;
    union {
        struct {
            struct JettyQueCfgEx sq;
            bool piType;
            union CstmJfsFlag cstmFlag;
            uint32_t sqebbNum;
        } extMode;
        struct {
            bool lockFlag;
            uint32_t sqeBufIdx;
        } taCacheMode;
    };
    uint64_t sqBuffVa;
    uint64_t dbAddr;
    uint32_t dbTokenId;
    uint64_t dbSegHandle;
    pthread_mutex_t mutex;
    uint32_t lastPi;
    uint64_t ciAddr;
    struct RsCrErrInfo crErrInfo;
    struct RsListHead list;
};

struct RsTokenIdCb {
    struct RsUbDevCb *devCb;
    urma_token_id_t *tokenId;
    struct RsListHead list;
};

struct RsCtxRemJettyCb {
    struct RsUbDevCb *devCb;
    urma_target_jetty_t *tjetty;
    struct QpKey jettyKey;
    enum JettyImportMode mode;
    unsigned int tokenValue;
    enum JettyGrpPolicy policy;
    enum TargetType type;
    union ImportJettyFlag flag;
    uint32_t tpType;
    struct JettyImportExpCfg expImportCfg;
    unsigned int state;
    struct RsListHead list;
};

struct RsSegInfo {
    uint64_t addr;
    uint64_t len;
    urma_seg_t seg;
};

struct RsSegCb {
    struct RsUbDevCb *devCb;

    struct RsSegInfo segInfo;
    uint32_t state;

    urma_token_t tokenValue;
    urma_target_seg_t *segment;

    struct RsListHead list;
};

STATIC inline uint32_t RsGenerateUeInfo(uint32_t dieId, uint32_t funcId)
{
    return (dieId << DEV_INDEX_DIEID_OFFSET) | funcId;
}

STATIC inline uint32_t RsGenerateDevIndex(uint32_t devCnt, uint32_t dieId, uint32_t funcId)
{
    return (devCnt << DEV_INDEX_CNT_OFFSET) | RsGenerateUeInfo(dieId, funcId);
}

#endif // RS_CTX_INNER_H
