/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu context header file
 * Create: 2025-02-18
 */

#ifndef HCOMM_CCU_CONTEXT_RESOURCE_H
#define HCOMM_CCU_CONTEXT_RESOURCE_H

#include <vector>
#include <array>
#include <unordered_map>

#include "ccu_datatype_v1.h"

namespace hcomm { 

struct CcuRepResource {
    std::array<std::vector<CcuRep::CcuBuf>, CCU_MAX_IODIE_NUM>  ccubufs;
    std::array<std::vector<CcuRep::CcuBuf>, CCU_MAX_IODIE_NUM>  blockCcubufs;
    std::array<std::vector<CcuRep::Executor>, CCU_MAX_IODIE_NUM>   executor;
    std::array<std::vector<CcuRep::Executor>, CCU_MAX_IODIE_NUM>   blockExecutor;
    std::array<std::vector<CcuRep::CompletedEvent>, CCU_MAX_IODIE_NUM> completedEvent;
    std::array<std::vector<CcuRep::CompletedEvent>, CCU_MAX_IODIE_NUM> blockCompletedEvent;
    std::array<std::vector<CcuRep::Address>, CCU_MAX_IODIE_NUM>    address;
    std::array<std::vector<CcuRep::Variable>, CCU_MAX_IODIE_NUM>   continuousVariable;
    std::array<std::vector<CcuRep::Variable>, CCU_MAX_IODIE_NUM>   variable;
    std::array<std::vector<CcuRep::LocalNotify>, CCU_MAX_IODIE_NUM> localNotify;
};

// Context共享资源
struct CcuSharedResource {
    std::unordered_map<std::string, CcuRep::Variable>   sharedVar;
    std::unordered_map<std::string, CcuRep::LocalNotify> sharedNotify;
};

}; // namespace hcomm

#endif // HCOMM_CCU_CONTEXT_RESOURCE_H