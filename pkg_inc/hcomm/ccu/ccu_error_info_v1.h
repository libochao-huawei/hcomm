/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_ERROR_INFO_V1_H
#define CCU_ERROR_INFO_V1_H
#include <cstdint>
#include "enum_factory.h"
#include "ccu_rep_type_v1.h"

namespace hcomm {
constexpr uint32_t MISSION_STATUS_MSG_LEN = 64;
constexpr uint32_t WAIT_SIGNAL_CHANNEL_SIZE = 16;
constexpr uint32_t BUF_REDUCE_ID_SIZE = 8;

MAKE_ENUM(CcuErrorType, DEFAULT, MISSION, WAIT_SIGNAL, TRANS_MEM, BUF_TRANS_MEM, BUF_REDUCE, LOOP, LOOP_GROUP)

struct CcuErrorInfo {
    // 根据不同的typeId解析不同的union类型
    CcuErrorType type;

    // 出异常的Rep类型
    CcuRep::CcuRepType repType;

    // CCU任务执行的DieId
    uint8_t dieId;

    // CCU任务执行的MissionId
    uint8_t missionId;

    // Error所在的指令Id
    uint16_t instrId;

    union {
        struct {
            char missionError[MISSION_STATUS_MSG_LEN];
        } mission;

        struct {
            uint16_t signalId;
            uint16_t signalMask;
            uint16_t signalValue;
            uint16_t paramId;
            uint64_t paramValue;
            uint16_t channelId[WAIT_SIGNAL_CHANNEL_SIZE];
        } waitSignal;

        struct {
            uint64_t locAddr;
            uint64_t locToken;
            uint64_t rmtAddr;
            uint64_t rmtToken;
            uint64_t len;
            uint16_t signalId;
            uint16_t signalMask;
            uint16_t channelId;
            uint16_t dataType;
            uint16_t opType;
        } transMem;

        struct {
            uint16_t bufId;
            uint64_t addr;
            uint64_t token;
            uint64_t len;
            uint16_t signalId;
            uint16_t signalMask;
            uint16_t channelId;
        } bufTransMem;

        struct {
            uint16_t bufIds[BUF_REDUCE_ID_SIZE];
            uint16_t count;
            uint16_t dataType;
            uint16_t outputDataType;
            uint16_t opType;
            uint16_t signalId;
            uint16_t signalMask;
            uint16_t xnIdLength;
        } bufReduce;

        struct {
            uint16_t startInstrId;
            uint16_t endInstrId;
            uint16_t loopEngineId;
            uint16_t loopCnt;           // 该Loop需要循环执行的次数
            uint16_t loopCurrentCnt;    // 该Loop已经循环次数
            uint32_t addrStride;
        } loop;

        struct {
            uint16_t startLoopInsId;
            uint16_t loopInsCnt;
            uint16_t expandOffset;
            uint16_t expandCnt;
        } loopGroup;
    } msg;

    void SetBaseInfo(CcuRep::CcuRepType ccuRepType, uint8_t ccuDieId, uint8_t ccuMissionId, uint16_t insId)
    {
        this->repType = ccuRepType;
        this->dieId = ccuDieId;
        this->missionId = ccuMissionId;
        this->instrId = insId;
    }
};

struct ErrorInfoBase {
    int32_t  deviceId;
    uint8_t  dieId;
    uint8_t  missionId;
    uint16_t currentInsId;
    uint16_t status;
};



struct CcuMissionContext {
    union {
        uint16_t value;
        uint16_t taskId;
    } part0;

    union {
        uint16_t value;
        uint16_t streamId;
    } part1;

    union {
        uint16_t value;
        struct {
            uint16_t taskKill : 1;
            uint16_t dieId : 2;
            uint16_t status : 13; // Status [12:0]
        };
    } part2;

    union {
        uint16_t value;
        struct {
            uint16_t status : 3; // Status [15:13]
            uint16_t counter : 8;
            uint16_t denyCnt : 5;
        };
    } part3;

    union {
        uint16_t value;
        struct {
            uint16_t denyCnt : 5;
            uint16_t currentIns : 11; // Current Ins [10:0]
        };
    } part4;

    union {
        uint16_t value;
        struct {
            uint16_t currentIns : 5; // Current Ins [15:11]
            uint16_t endIns : 11;
        };
    } part5;

    union {
        uint16_t value;
        struct {
            uint16_t endIns : 5;
            uint16_t startIns : 11;
        };
    } part6;

    union {
        uint16_t value;
        struct {
            uint16_t startIns : 5;
            uint16_t profileEn : 1;
            uint16_t missionVld : 1;
            uint16_t reserved : 9;
        };
    } part7;

    uint16_t reserved[24]; // part 8-31

    uint16_t GetStatus() const
    {
        return (part3.status << 13) | (part2.status);   // part3.status为[15:13]位
    }

    uint16_t GetCurrentIns() const
    {
        return (part5.currentIns << 11) | (part4.currentIns);   // part5.currentIns为[15:11]位
    }

    uint16_t GetStartIns() const
    {
        return (part7.startIns << 11) | (part6.startIns);    // part7.startIns[15:11]位
    }

    uint16_t GetEndIns() const
    {
        return (part6.endIns << 11) | (part5.endIns);     // part6.endIns[15:11]位
    }
};

constexpr u32 CCU_COSTOM_ARGS_LEN = 32;
struct ParaCcu {
    u8  dieId;
    u8  missionId;
    u8  execMissionId;
    u32 instrId;
    u64 costumArgs[CCU_COSTOM_ARGS_LEN];
    u64 executeId;
    u64 ccuKernelHandle{0};
};

union LoopGroupXn {
    uint64_t value;
    struct {
        uint64_t reservedLow : 41;
        uint64_t loopInsCnt : 7;
        uint64_t expandOffset : 7;
        uint64_t expandCnt : 7;
        uint64_t reservedHigh : 2;
    };
};

}; // namespace hcomm

#endif // _CCU_REPRESENTATION_TYPE_H