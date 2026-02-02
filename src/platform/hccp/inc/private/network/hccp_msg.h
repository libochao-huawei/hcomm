/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: hccp_msg.h
 * Create: 2025-04-22
 */

#ifndef TS_HCCP_MSG_H
#define TS_HCCP_MSG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#pragma pack(push)
#pragma pack (1)
typedef struct TagTsCcuTaskInfo {
    uint8_t udieId;
    uint8_t rsv;
    uint16_t missionId;
} TsCcuTaskInfoT;

typedef struct TagTsUbTaskInfo {
    uint8_t udieId;
    uint8_t functionId;
    uint16_t jettyId;
} TsUbTaskInfoT;

typedef struct TagTsCcuTaskReport {
    uint8_t num;
    uint8_t rsv1[3];
    TsCcuTaskInfoT array[8];
    uint8_t rsv2[60]; // rsv 60B
} TsCcuTaskReportT;

typedef struct TagTsUbTaskReport {
    uint8_t num;
    uint8_t rsv1[3];
    TsUbTaskInfoT array[4];
    uint8_t rsv2[76]; // rsv 76B
} TsUbTaskReportT;

typedef struct TagTsHccpMsg {
    // head 32B
    int32_t hccpPid;  // apm_query_slave_tgid_by_master
    uint32_t hostPid;
    uint8_t cmdType;   // 0:ub force kill; 1:ccu force kill
    uint8_t vfId;
    uint16_t sqId;
    uint16_t isAppExit;
    uint16_t sqeType;
    uint8_t appFlag;
    uint8_t intrType;
    uint16_t modelId;
    uint8_t res2[12];
    // info 96B
    union {
        TsUbTaskReportT ubTaskInfo;
        TsCcuTaskReportT ccuTaskInfo;
    } u;
} TsHccpMsg;

typedef enum TagTsHccpSubEventId {
    TOPIC_SEND_KILL_MSG = 1U,
    TOPIC_KILL_DONE_MSG,
} TsHccpSubEventId;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TS_HCCP_MSG_H */