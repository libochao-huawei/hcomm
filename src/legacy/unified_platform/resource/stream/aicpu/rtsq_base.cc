/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rtsq_base.h"
#include "ascend_hal.h"
#include "log.h"
#include "drv_api_exception.h"
#include "exception_util.h"
#include <unordered_map>
namespace Hccl {

RtsqBase::RtsqBase(u32 devPhyId, u32 streamId, u32 sqId) : devPhyId_(devPhyId), streamId_(streamId), sqId_(sqId)
{
    auto ret = drvGetLocalDevIDByHostDevID(devPhyId_, &localDevId_);
    if (ret != DRV_ERROR_NONE) {
        std::string formatStr = StringFormat(
            "RtsqBase::%s call drvGetLocalDevIDByHostDevID failed, devPhyId %u, ret %d", __func__, devPhyId_, ret);
        THROW<DrvApiException>(formatStr);
    }

    sqHead_     = QuerySqHead();
    sqTail_     = QuerySqTail();
    sqDepth_    = QuerySqDepth();
    sqBaseAddr_ = QuerySqBaseAddr();
    HCCL_INFO("%s, %s", __func__, GetHwSqDescribe().c_str());
}

void RtsqBase::Reset()
{
    sqHead_  = QuerySqHead();
    sqTail_  = QuerySqTail();
    sqDepth_ = QuerySqDepth();
    sqBaseAddr_ = QuerySqBaseAddr();
    if (SetTaskIdBySqeId() != HCCL_SUCCESS) {
        taskId_ = 0;
    }
}

std::string RtsqBase::GetHwSqDescribe()
{
    return StringFormat("devPhyId=%u, localDevId=%u, streamId=%u, sqId=%u, sqDepth=%u, sqBaseAddr=0x%llx, "
                        "currentHead=%u, currentTail=%u, cqeStatus=%u, taskId=%u",
                        devPhyId_, localDevId_, streamId_, sqId_, sqDepth_, sqBaseAddr_, QuerySqHead(), QuerySqTail(), QueryCqeStatus(),
                        taskId_);
}

u32 RtsqBase::QuerySqStatusByType(QueryDrvSqCqPtopType givenType)
{
    const std::unordered_map<QueryDrvSqCqPtopType, drvSqCqPropType_t, std::EnumClassHash> DrvSqCqPtopTypeMap = {
        {QueryDrvSqCqPtopType::HEAD, drvSqCqPropType_t::DRV_SQCQ_PROP_SQ_HEAD},
        {QueryDrvSqCqPtopType::TAIL, drvSqCqPropType_t::DRV_SQCQ_PROP_SQ_TAIL},
        {QueryDrvSqCqPtopType::DEPTH, drvSqCqPropType_t::DRV_SQCQ_PROP_SQ_DEPTH},
        {QueryDrvSqCqPtopType::CQE_STATUS, drvSqCqPropType_t::DRV_SQCQ_PROP_SQ_CQE_STATUS},
    };
    halSqCqQueryInfo  queryInfo;
    drvSqCqPropType_t type = DrvSqCqPtopTypeMap.at(givenType);
    queryInfo.tsId         = 0;
    queryInfo.sqId         = sqId_;
    queryInfo.cqId         = 0;
    queryInfo.type         = DRV_NORMAL_TYPE;
    queryInfo.prop         = type;
    drvError_t ret = halSqCqQuery(localDevId_, &queryInfo);
    if (ret != 0) {
        std::string formatStr = StringFormat("RtsqBase::%s call halSqCqQuery failed, localDevId %u, ret %d, givenType=%s",
                                             __func__, localDevId_, ret, givenType.Describe().c_str());
        THROW<DrvApiException>(formatStr);
    }

    return queryInfo.value[0];
}

u64 RtsqBase::QuerySqBaseAddr()
{
    halSqCqQueryInfo queryInfo;
    queryInfo.tsId = 0;
    queryInfo.sqId = sqId_;
    queryInfo.cqId = 0;
    queryInfo.type = DRV_NORMAL_TYPE;
    queryInfo.prop = DRV_SQCQ_PROP_SQ_BASE;
    drvError_t ret = halSqCqQuery(localDevId_, &queryInfo);
    if (ret != 0) {
        std::string formatStr
            = StringFormat("RtsqBase::%s call halSqCqQuery failed, localDevId %u, ret %d", __func__, localDevId_, ret);
        THROW<DrvApiException>(formatStr);
    }
    HCCL_INFO("RtsqBase::%s end", __func__);

    // 参照 driver API，BaseAddress为64bit，由两个32bit拼接而成，高32bit为 value[1], 低32bit为value[0]
    return ((static_cast<u64>(queryInfo.value[1])) << 32) | queryInfo.value[0];
}

u32 RtsqBase::QuerySqHead()
{
    return QuerySqStatusByType(QueryDrvSqCqPtopType::HEAD);
}

u32 RtsqBase::QuerySqTail()
{
    return QuerySqStatusByType(QueryDrvSqCqPtopType::TAIL);
}

u32 RtsqBase::QuerySqDepth()
{
    return QuerySqStatusByType(QueryDrvSqCqPtopType::DEPTH);
}

u32 RtsqBase::QueryCqeStatus()
{
    return QuerySqStatusByType(QueryDrvSqCqPtopType::CQE_STATUS);
}

void RtsqBase::ConfigSqStatusByType(ConfigDrvSqCqPtopType givenType, u32 value)
{
    const std::unordered_map<ConfigDrvSqCqPtopType, drvSqCqPropType_t, std::EnumClassHash> ConfigDrvSqCqPtopTypeMap
        = {{ConfigDrvSqCqPtopType::TAIL, drvSqCqPropType_t::DRV_SQCQ_PROP_SQ_TAIL},
           {ConfigDrvSqCqPtopType::DISABLE_TO_ENABLE, drvSqCqPropType_t::DRV_SQCQ_PROP_SQ_DISABLE_TO_ENABLE}};
    halSqCqConfigInfo configInfo;
    configInfo.tsId     = 0;
    configInfo.sqId     = sqId_;
    configInfo.cqId     = 0;
    configInfo.type     = DRV_NORMAL_TYPE;
    configInfo.prop     = ConfigDrvSqCqPtopTypeMap.at(givenType);
    configInfo.value[0] = value;

    HCCL_INFO("RtsqBase::%s start, givenType=%s", __func__, givenType.Describe().c_str());
    drvError_t ret = halSqCqConfig(localDevId_, &configInfo);
    if (ret != 0) {
        std::string formatStr
            = StringFormat("RtsqBase::%s call halSqCqConfig failed, localDevId %u, ret %d", __func__, localDevId_, ret);
        THROW<DrvApiException>(formatStr);
    }
    HCCL_INFO("RtsqBase::%s end", __func__);
}

void RtsqBase::ConfigSqTail(u32 value)
{
    HCCL_INFO("RtsqBase::%s, value=%u", __func__, value);
    ConfigSqStatusByType(ConfigDrvSqCqPtopType::TAIL, value);
}

void RtsqBase::ConfigDisableToEnable(u32 value)
{
    HCCL_INFO("RtsqBase::%s, value=%u", __func__, value);
    ConfigSqStatusByType(ConfigDrvSqCqPtopType::DISABLE_TO_ENABLE, value);
}

HcclResult RtsqBase::SetTaskIdBySqeId()
{
    if (UNLIKELY(aicpu::GetSqeId == nullptr)) {
        HCCL_WARNING("[RtsqBase][SetTaskIdBySqeId] aicpu::GetSqeId is nullptr.");
        return HCCL_E_INTERNAL;
    }
    u32 taskIdEnd;
    aicpu::GetSqeId(1, taskId_, taskIdEnd);
    return HCCL_SUCCESS;
}

} // namespace Hccl