/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_tbe_task.h"
#include "tbe_vector_reduce.h"
#include "tbe_crack_cleared.h"

static uint32_t gInitCount[TBE_MAX_MODULE_DEVICE_NUM]{0};
static TbeReduce::TbeVectorReduce g_TbeVectorReduce[TBE_MAX_MODULE_DEVICE_NUM];
static TbeReduce::TbeCrackCleard g_TbeCrackCleard[TBE_MAX_MODULE_DEVICE_NUM];

bool checkDeviceLogicId(int32_t deviceLogicId)
{
    if (deviceLogicId < 0 || (static_cast<u32>(deviceLogicId) >= TBE_MAX_MODULE_DEVICE_NUM)) {
        HCCL_ERROR("check deviceLogicId[%d] fail.", deviceLogicId);
        return false;
    }
    return true;
}

HcclResult HcclTbeTaskInit(int32_t deviceLogicId)
{
    CHK_PRT_RET(!checkDeviceLogicId(deviceLogicId), HCCL_ERROR("deviceLogicId param fail."), HCCL_E_PARA);
    if (gInitCount[deviceLogicId] > 0) {
        HCCL_INFO("tbe task has init. count[%u]", gInitCount[deviceLogicId]);
        gInitCount[deviceLogicId]++;
        return HCCL_SUCCESS;
    }
    CHK_RET(g_TbeVectorReduce[deviceLogicId].Init());
    CHK_RET(g_TbeCrackCleard[deviceLogicId].CrackInit());
    gInitCount[deviceLogicId]++;
    HCCL_RUN_INFO("tbe task init success.deviceLogicId[%d]", deviceLogicId);
    return HCCL_SUCCESS;
}

HcclResult HcclTbeTaskDeInit(int32_t deviceLogicId)
{
    CHK_PRT_RET(!checkDeviceLogicId(deviceLogicId), HCCL_ERROR("deviceLogicId param fail."), HCCL_E_PARA);
    if (gInitCount[deviceLogicId] > 0) {
        gInitCount[deviceLogicId]--;
    }
    if (gInitCount[deviceLogicId] == 0) {
        CHK_RET(g_TbeVectorReduce[deviceLogicId].DeInit());
        CHK_RET(g_TbeCrackCleard[deviceLogicId].CrackDeInit());
    }
    HCCL_RUN_INFO("tbe task deinit success.deviceLogicId[%d]", deviceLogicId);
    return HCCL_SUCCESS;
}

HcclResult HcclTbeMemClean(int64_t addrList[], int64_t sizeList[], uint32_t count,
    aclrtStream stream, int32_t deviceLogicId)
{
    CHK_PTR_NULL(addrList);
    CHK_PTR_NULL(sizeList);
    CHK_PTR_NULL(stream);
    CHK_PRT_RET(count == 0, HCCL_INFO("count is zero."), HCCL_SUCCESS);
    CHK_PRT_RET(!checkDeviceLogicId(deviceLogicId), HCCL_ERROR("deviceLogicId param fail."), HCCL_E_PARA);
    for (uint32_t i = 0; i < count; i++) {
        CHK_PRT_RET(addrList[i] == 0, HCCL_ERROR("addr i=[%u] is invalid.", i), HCCL_E_PARA);
    }
    std::vector<int64_t> crackAddr(addrList, addrList + count);
    std::vector<int64_t> crackSize(sizeList, sizeList + count);
    CHK_RET(g_TbeCrackCleard[deviceLogicId].Run(crackAddr, crackSize, stream));
    HCCL_RUN_INFO("tbe mem clean.deviceLogicId[%d]", deviceLogicId);
    return HCCL_SUCCESS;
}

HcclResult HcclTbeReduce(const TbeReduceParam *param, aclrtStream stream,
    void *overflowAddrs[], uint32_t overflowCount, int32_t deviceLogicId)
{
    CHK_PTR_NULL(param);
    CHK_PTR_NULL(overflowAddrs);
    CHK_PTR_NULL(stream);
    CHK_PRT_RET(!checkDeviceLogicId(deviceLogicId), HCCL_ERROR("deviceLogicId param fail."), HCCL_E_PARA);
    if (overflowCount > 0) {
        std::vector<void *> globalWorkSpaceAddr(overflowAddrs, overflowAddrs + overflowCount);
        CHK_RET(g_TbeVectorReduce[deviceLogicId].SetGlobalWorkSpace(globalWorkSpaceAddr));
    }
    CHK_RET(g_TbeVectorReduce[deviceLogicId].Run(param->src1, param->src2, param->count, param->dataType,
        param->redOp, stream, param->dst));
    return HCCL_SUCCESS;
}

HcclResult HcclGetVectorBlockSize(uint32_t *blockSize, int32_t deviceLogicId)
{
    CHK_PTR_NULL(blockSize);
    CHK_PRT_RET(!checkDeviceLogicId(deviceLogicId), HCCL_ERROR("deviceLogicId param fail."), HCCL_E_PARA);
    u32 getBlockSize = 0;
    CHK_RET(g_TbeVectorReduce[deviceLogicId].GetVectorBlockSize(getBlockSize));
    *blockSize = getBlockSize;
    return HCCL_SUCCESS;
}

HcclResult HcclTbeReduceGenArgs(const TbeReduceParam *param, aclrtStream stream,
    void *overflowAddrs[], uint32_t overflowCount, TbeReduceArg *args, int32_t deviceLogicId)
{
    CHK_PTR_NULL(param);
    CHK_PTR_NULL(stream);
    CHK_PTR_NULL(overflowAddrs);
    CHK_PTR_NULL(args);
    CHK_PRT_RET(!checkDeviceLogicId(deviceLogicId), HCCL_ERROR("deviceLogicId param fail."), HCCL_E_PARA);
    if (overflowCount > 0) {
        std::vector<void *> globalWorkSpaceAddr(overflowAddrs, overflowAddrs + overflowCount);
        CHK_RET(g_TbeVectorReduce[deviceLogicId].SetGlobalWorkSpace(globalWorkSpaceAddr));
    }
    CHK_RET(g_TbeVectorReduce[deviceLogicId].PrepareVectorReduce(param->src1, param->src2, param->count, param->dataType,
        param->redOp, param->dst, stream, args->addrListDevMem, args->argsHandle, args->funcAddr, args->blockDim));
    return HCCL_SUCCESS;
}