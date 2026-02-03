/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "runtime_api_exception.h"
#include "exception_util.h"
#include "orion_adapter_rts.h"
#include "invalid_params_exception.h"
#include "log.h"
#include "acl/acl_rt.h"
#include "driver/ascend_hal.h"

using namespace std;
namespace Hccl {

static constexpr int32_t RT_NOT_SUPPORT = 207000;
HcclResult HrtThreadExchangeCaptureMode(aclmdlRICaptureMode *mode);
constexpr u32 TOKEN_ID_RIGHT_SHIF = 8; // URMA_TOKEN_ID_RIGHT_SHIF，因URMA配置原因需要右移8位

const std::unordered_map<std::string, DevType> SOC_VER_CONVERT{{"Ascend310P1", DevType::DEV_TYPE_V51_310_P1},
                                                               {"Ascend310P3", DevType::DEV_TYPE_V51_310_P3},
                                                               {"Ascend910", DevType::DEV_TYPE_910A},
                                                               {"Ascend910A", DevType::DEV_TYPE_910A},
                                                               {"Ascend910B", DevType::DEV_TYPE_910A},
                                                               {"Ascend910ProA", DevType::DEV_TYPE_910A},
                                                               {"Ascend910ProB", DevType::DEV_TYPE_910A},
                                                               {"Ascend910PremiumA", DevType::DEV_TYPE_910A},
                                                               {"Ascend910B1", DevType::DEV_TYPE_910A2},
                                                               {"Ascend910B2", DevType::DEV_TYPE_910A2},
                                                               {"Ascend910B3", DevType::DEV_TYPE_910A2},
                                                               {"Ascend910B4", DevType::DEV_TYPE_910A2},
                                                               {"Ascend910_939", DevType::DEV_TYPE_910A3},
                                                               {"Ascend910_938", DevType::DEV_TYPE_910A3},
                                                               {"Ascend910_937", DevType::DEV_TYPE_910A3},
                                                               {"nosoc", DevType::DEV_TYPE_NOSOC}};

// 添加编译宏，防止返回82类型芯片造成已有UT失效
DevType HrtGetDeviceType()
{
    std::string targetChipVerStr;
    HrtGetSocVer(targetChipVerStr);
    HCCL_INFO("[HrtGetDeviceType]targetChipVerStr = %s.", targetChipVerStr.c_str());
    if (targetChipVerStr.find("Ascend950") != std::string::npos) {
        HCCL_INFO("[HrtGetDeviceType]DeviceType = DevType::DEV_TYPE_910_95.");
        return DevType::DEV_TYPE_910_95;
    }

    auto iter = SOC_VER_CONVERT.find(targetChipVerStr);
    if (iter == SOC_VER_CONVERT.end()) {
        string msg = StringFormat("[Get][DeviceType]errNo[0x%016llx] rtGetSocVersion get "
                   "illegal chipver, chip_ver[%s], return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), targetChipVerStr.c_str(), iter->second);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return iter->second;
}

s32 HrtGetDevicePhyIdByIndex(s32 deviceLogicId)
{
    DevType deviceType = HrtGetDeviceType();
    if (deviceType == DevType::DEV_TYPE_NOSOC) {
        return 0;
    }
    HCCL_INFO("[HrtGetDevicePhyIdByIndex]deviceLogicId=%d.",deviceLogicId);

    s32 devicePhyId = 0;
    aclError ret = rtsGetPhyDevIdByLogicDevId(deviceLogicId, &devicePhyId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Get][DevicePhyId]errNo[0x%016llx] rtGet device PhyId by "
                   "index failed, return[%d], "
                   "para: devIndex[%d], phyId[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_DRV), ret, deviceLogicId, devicePhyId);
        MACRO_THROW(RuntimeApiException, msg);
    }
    HCCL_INFO("[HrtGetDevicePhyIdByIndex]devicePhyId=%d.",devicePhyId);
    return devicePhyId;
}

s32 HrtDeviceGetBareTgid()
{
    s32       pid = 0;
    aclError ret = aclrtDeviceGetBareTgid(&pid);
    HCCL_INFO("Call rtDeviceGetBareTgid, return value[%d], rtGet pid[%d].", ret, pid);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Get][BareTgid]errNo[0x%016llx] rtGet pid fail."
                   "return[%d], rtGet pid[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, pid);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return pid;
}

void HrtGetSocVer(std::string &socName)
{
    const char *socNamePtr = aclrtGetSocName();
    if (socNamePtr == nullptr) {
        HCCL_ERROR("[Get][SocVer]errNo[0x%016llx] rtGet deviceVer failed.",
                   HCCL_ERROR_CODE((HcclResult::HCCL_E_RUNTIME)));
        throw RuntimeApiException("call rtGetSocVersion failed. ");
    }

    socName = socNamePtr;
}

s32 HrtGetDevice()
{
    s32 deviceLogicId = 0;
    aclError ret = aclrtGetDevice(&deviceLogicId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Get][Device]errNo[0x%016llx] rtGet device fail."	 
                      "please make sure that device is set. return[%d], para:deviceLogicId[%d]",	 
                      HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);	 
        MACRO_THROW(RuntimeApiException, msg);
    }
    HCCL_INFO("[HrtGetDevice]deviceLogicId=%d.",deviceLogicId);
    return deviceLogicId;
}

void HrtSetDevice(s32 deviceLogicId)
{
    aclError ret = aclrtSetDevice(deviceLogicId);
    HCCL_INFO("Call rtSetDevice, return value[%d], para: device_id[%d].", ret, deviceLogicId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Set][Device]errNo[0x%016llx] rtSet device fail."
                   "return[%d], para:deviceLogicId[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtResetDevice(s32 deviceLogicId)
{
    aclError ret = aclrtResetDevice(deviceLogicId);
    HCCL_INFO("Call aclrtResetDevice, return value[%d], para: device_id[%d].", ret, deviceLogicId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Reset][Device]errNo[0x%016llx] rtReset device fail."
                   "return[%d], para: deviceLogicId[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

u32 HrtGetDeviceCount()
{
    u32       count = 0;
    aclError ret   = aclrtGetDeviceCount(&count);
    HCCL_INFO("Call rtGetDeviceCount, return value[%d], para: count[%d].", ret, count);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Get][DeviceCount]errNo[0x%016llx] rtGet device count fail."
                   "return[%d], para:count[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, count);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return count;
}

// 兜底extern, 正式适配待todo
void *HrtDevBinaryRegister(const rtDevBinary_t *bin)
{
    HCCL_INFO("[HrtDevBinaryRegister] bin magic=%u, version=%u, data=%p, length=%u.", bin->magic,
                bin->version, bin->data, bin->length);
    void     *handle = nullptr;
    rtError_t ret    = rtDevBinaryRegister(bin, &handle);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[rtDev][inaryRegister]register opBinary failed."
                   "bin magic=%u, version=%u, data=%p, length=%u, ret=%d, handle.", 
                    bin->magic, bin->version, bin->data, bin->length, ret, handle);
        MACRO_THROW(RuntimeApiException, msg);
    }

    return handle;
}
// 兜底extern, 正式适配待todo
void HrtFunctionRegister(void *binHandle, const void *stubFunc, const char *stubName, const void *devFunc,
                                uint32_t funcMode)
{
    HCCL_INFO("[HrtFunctionRegister] binHandlec=%p, stubFunc=%p, stubName=%s, devFunc=%p, funcMode=%u.", 
                binHandle, stubFunc, stubName, devFunc, funcMode);
    rtError_t ret = rtFunctionRegister(binHandle, stubFunc, stubName, devFunc, funcMode);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[HrtFunctionRegister]rt dev function register failed."
                   "binHandlec=%p, stubFunc=%p, stubName=%s, devFunc=%p, funcMode=%u, ret=%d.", 
                binHandle, stubFunc, stubName, devFunc, funcMode, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

s32 HrtGetStreamId(aclrtStream ptr)
{
    CHECK_NULLPTR(ptr, "[HrtStreamGetMode] ptr is nullptr!");
    s32       streamId;
    aclError ret = aclrtStreamGetId(ptr, &streamId);
    HCCL_INFO("Call aclrtStreamGetId, ptr[%p] return value[%d] streamId[%d].", ptr, ret, streamId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Get][StreamId]errNo[0x%016llx]."
                   "rt get stream ID fail. ptr[%p], return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ptr, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }

    return streamId;
}

u64 HrtStreamGetMode(HcclRtStream const ptr)
{
    HCCL_INFO("[HrtStreamGetMode]ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtStreamGetMode] ptr is nullptr!");
    u64 stmMode  =  0;
    s32 streamId = -1;
    aclError ret = aclrtStreamGetId(ptr, &streamId);
    HCCL_DEBUG("Call aclrtStreamGetId, return value[%d].", ret);
    aclrtStreamAttrValue value;
    ret = aclrtGetStreamAttribute(ptr, ACL_STREAM_ATTR_FAILURE_MODE, &value);
    stmMode = value.failureMode;
    HCCL_INFO("Call rtStreamGetMode return value[%d]. stmMode[%llu].", ret, stmMode);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Stream][GetMode]errNo[0x%016llx] rtStreamGetMode error."
            "ptr[%p], stmMode[%llu], streamId[%d], rtRet[%d].", 
            HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ptr, stmMode, streamId, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return static_cast<u64>(stmMode);
}

void HrtStreamSetMode(HcclRtStream streamPtr, const uint64_t stmMode)
{
    HCCL_INFO("[HrtStreamSetMode]streamPtr[%p], stmMode[%llu].", streamPtr, stmMode);
    CHECK_NULLPTR(streamPtr, "[HrtStreamSetMode] ptr is nullptr!");
    s32 streamId = -1;
    aclError ret = aclrtStreamGetId(streamPtr, &streamId);
    HCCL_DEBUG("Call aclrtStreamGetId, return value[%d].", ret);
    aclrtStreamAttrValue value;
    value.failureMode = stmMode;
    ret = aclrtSetStreamAttribute(streamPtr, ACL_STREAM_ATTR_FAILURE_MODE, &value);
    HCCL_INFO("Call rtStreamSetMode return value[%d]. stmMode[%llu].", ret, stmMode);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Stream][SetMode]errNo[0x%016llx] rtStreamSetMode error." 
            "streamPtr[%p], rtRet[%d], stmMode[%llu].", 
            HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), streamPtr, ret, stmMode);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

HcclResult HrtGetDeviceInfo(uint32_t deviceLogicId, int32_t moduleType, int32_t infoType, int64_t &val)
{
    HCCL_INFO("[HrtGetDeviceInfo]deviceLogicId[%u], moduleType[%d], infoType[%d], val[%ld].", deviceLogicId, moduleType, infoType, val);
    rtError_t ret = rtGetDeviceInfo(deviceLogicId, moduleType, infoType, reinterpret_cast<int64_t *>(&val));
    HCCL_INFO("Call HrtGetDeviceInfo return[%d]. val[%ld].", ret, val);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[HrtGetDeviceInfo]errNo[0x%016llx] rt get device info failed, "
                   "deviceLogicId=%u, moduleType=%d, infoType=%d, return=%d.",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), deviceLogicId, moduleType, infoType, ret);
        return HcclResult::HCCL_E_RUNTIME;
    }
    return HcclResult::HCCL_SUCCESS;
}

constexpr uint64_t POD_MAINBOARD = 0x0;
constexpr uint64_t A_K_SERVER_MAINBOARD = 0x1;
constexpr uint64_t A_X_SERVER_MAINBOARD = 0x2;
constexpr uint64_t PCIE_STD_MAINBOARD = 0x3;
constexpr uint64_t RSV1_MAINBOARD = 0x4;
constexpr uint64_t RSV2_MAINBOARD = 0x5;
constexpr uint64_t EQUIP_MAINBOARD = 0x6;
constexpr uint64_t EVB_MAINBOARD = 0x7;

const std::unordered_map<uint64_t, HcclMainboardId> rtMainboardIdToHcclMainboardId = {
    {POD_MAINBOARD, HcclMainboardId::MAINBOARD_POD},
    {A_K_SERVER_MAINBOARD, HcclMainboardId::MAINBOARD_A_K_SERVER},
    {A_X_SERVER_MAINBOARD, HcclMainboardId::MAINBOARD_A_X_SERVER},
    {PCIE_STD_MAINBOARD, HcclMainboardId::MAINBOARD_PCIE_STD},
    {RSV1_MAINBOARD, HcclMainboardId::MAINBOARD_RSV},
    {RSV2_MAINBOARD, HcclMainboardId::MAINBOARD_RSV},
    {EQUIP_MAINBOARD, HcclMainboardId::MAINBOARD_EQUIPMENT},
    {EVB_MAINBOARD, HcclMainboardId::MAINBOARD_EVB}
};

/*
 * 获取Mainboard ID 5-7位，输出整机形态枚举值
 * Mainboard ID描述说明
 * Mainboard ID采用了16bit，区分形态，主从，以及端口配置
 * bit[7:5] 区分整机形态(当前POD和EVB没有区分A+X或A+K)
 * {
 *  000: 天成 POD
 *  001: A+K Server
 *  010: A+X Server
 *  011: PCIE标卡
 *  100-101: RSV
 *  110: 装备
 *  111: EVB
 * }
 * bit[4:1] 整机形态细分
 * {
 *  0000-1111
 * }
 * bit[0] 主从或池化
 * {
 *  0: 主从（NPU作为某个Host的从设备，Host主控）
 *  1: 池化（NPU作为资源池，其它Host对等访问）
 * }
 */
HcclResult HrtGetMainboardId(uint32_t deviceLogicId, HcclMainboardId &hcclMainboardId)
{
    HCCL_INFO("[HrtGetMainboardId] deviceLogicId[%d], hcclMainboardId[%s].",
              deviceLogicId, hcclMainboardId.Describe().c_str());
    constexpr int32_t moduleType = DEV_MODULE_TYPE::MODULE_TYPE_SYSTEM;
    constexpr int32_t infoType = DEV_INFO_TYPE::INFO_TYPE_MAINBOARD_ID;
    constexpr uint64_t BITS_5 = 5;
    constexpr uint64_t MASK_7 = 0x7;
    int64_t val = 0;
    CHK_RET(HrtGetDeviceInfo(deviceLogicId, moduleType, infoType, val));
    HCCL_INFO("[HrtGetMainboardId] deviceLogicId[%d] val[%ld].", deviceLogicId, val);
    uint64_t mainboardId = (static_cast<uint64_t>(val) >> BITS_5) & MASK_7; // 提取val的5-7位，判断整机形态
    auto it = rtMainboardIdToHcclMainboardId.find(mainboardId);
    if (it != rtMainboardIdToHcclMainboardId.end()) {
        hcclMainboardId = it->second;
    } else {
        hcclMainboardId = HcclMainboardId::MAINBOARD_OTHERS;
    }
    HCCL_INFO("[HrtGetMainboardId] deviceLogicId[%d] mainboardId[%llu] hcclMainboardId[%s].",
              deviceLogicId, mainboardId, hcclMainboardId.Describe().c_str());
    return HcclResult::HCCL_SUCCESS;
}

aclrtStream HrtStreamCreateWithFlags(int32_t priority, uint32_t flags)
{
    HCCL_INFO("[HrtStreamCreateWithFlags] priority[%d], flags[%u].", priority, flags);
    aclrtStream ptr = nullptr;
    aclError  ret = rtStreamCreateWithFlags(&ptr, priority, flags);
    HCCL_INFO("Call rtGetStreamId return value[%d]. Params: flags[%u].", ret, flags);

    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Stream][CreateWithFlags]errNo[0x%016llx] rtStreamCreate error." 
                                "rtRet[%d], flags[%u], ptr[%p].",
                                HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, flags, ptr);
        MACRO_THROW(RuntimeApiException, msg);
    }
    CHECK_NULLPTR(ptr, "[HrtStreamCreateWithFlags] ptr is nullptr!");
    HCCL_INFO("[HrtStreamCreateWithFlags] ptr[%p]", ptr);

    return ptr;
}

void HrtStreamDestroy(aclrtStream ptr)
{
    HCCL_INFO("[HrtStreamDestroy] ptr[%p].", ptr);
    aclError ret = aclrtDestroyStreamForce(ptr);
    HCCL_INFO("Call aclrtDestroyStreamForce, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Stream][Destroy]errNo[0x%016llx] rt stream Destroy fail." 
                                "return[%d], ptr[%p].",
                                HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtStreamActive(aclrtStream activeStream, aclrtStream stream)
{
    HCCL_INFO("[HrtStreamActive] activeStream[%p], activeStream[%p].", activeStream, stream);
    aclError ret = aclrtActiveStream(activeStream, stream);
    HCCL_INFO("Call aclrtActiveStream, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Activate][Stream]errNo[0x%016llx] "
                   "rt stream active fail. return[%d], active_stream=%p, stream=%p.",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, activeStream, stream);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

inline s32 GetMsTimeFromExecTimeout()
{
    constexpr s32 HCCL_EXEC_TIME_OUT_OFFSET_S = 5; // 避免与notifywait timeout时间冲突，增加5s的偏移值
    constexpr u32 TIME_S_TO_MS                = 1000;
    s32           execTimeOut                 = 3000; // 从环境变量中获取超时时间
    s64           timeOutMs                   = (execTimeOut + HCCL_EXEC_TIME_OUT_OFFSET_S) * TIME_S_TO_MS;
    timeOutMs                                 = (timeOutMs > 0x7FFFFFFF) ? 0x7FFFFFFF : timeOutMs;
    return static_cast<s32>(static_cast<u64>(timeOutMs) & (0x7FFFFFFF));
}

void HcclStreamSynchronize(HcclRtStream ptr)
{
    HCCL_INFO("[HcclStreamSynchronize] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HcclStreamSynchronize] ptr is nullptr!");
    s32       timeout = GetMsTimeFromExecTimeout();
    aclError  ret     = aclrtSynchronizeStreamWithTimeout(ptr, timeout);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Synchronize][Stream]errNo[0x%016llx] rt "
                   "streamsynchronizewithtimeout fail. return[%d], stream[%p], timeout[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, timeout);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

/*
 *RT_MEMORY_TS：aclrtMallocForTaskScheduler
 *RT_MEMORY_DDR：ACL_MEM_TYPE_LOW_BAND_WIDTH
 *RT_MEMORY_HBM：ACL_MEM_TYPE_HIGH_BAND_WIDTH
*/
void *HrtMalloc(u64 size, rtMemType_t memType)
{
    HCCL_INFO("[HrtMalloc] size[%llu], memType[%u].", size, memType);
    void     *devPtr = nullptr;
    rtError_t ret    = rtMalloc(&devPtr, size, memType, HCCL);
    HCCL_INFO("Call rtMalloc, return value[%d] size[%llu] memType[%u], devPtr[%p], moudleId: HCCL.", 
                ret, size, memType, devPtr);

    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[Malloc][Mem]errNo[0x%016llx] rtMalloc failed."
                   "return[%d], para: devPtrAddr[%p], size[%llu], memType[%u].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, devPtr, size, memType);
        MACRO_THROW(RuntimeApiException, msg);
    }
    CHECK_NULLPTR(devPtr, "[HrtStreamCreateWithFlags] ptr is nullptr!");

    return devPtr;
}

void HrtFree(void *devPtr)
{
    HCCL_INFO("[HrtFree] devPtr[%p].", devPtr);
    CHECK_NULLPTR(devPtr, "[HrtFree] ptr is nullptr!");
    aclError ret = aclrtFree(devPtr);
    HCCL_INFO("Call aclrtFree, return value[%d], para: dev_ptr[%p].", ret, devPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[Free][Mem]errNo[0x%016llx] aclrtFree failed."
                   "return[%d], para: devPtrAddr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, devPtr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

HcclResult MemcpyKindTranslate(rtMemcpyKind_t kind, aclrtMemcpyKind *rtKind)
{
    switch (kind) {
        case rtMemcpyKind_t::RT_MEMCPY_HOST_TO_HOST: {
            *rtKind = ACL_MEMCPY_HOST_TO_HOST;
            return HCCL_SUCCESS;
        }
        case rtMemcpyKind_t::RT_MEMCPY_HOST_TO_DEVICE: {
            *rtKind = ACL_MEMCPY_HOST_TO_DEVICE;
            return HCCL_SUCCESS;
        }
        case rtMemcpyKind_t::RT_MEMCPY_DEVICE_TO_HOST: {
            *rtKind = ACL_MEMCPY_DEVICE_TO_HOST;
            return HCCL_SUCCESS;
        }
        case rtMemcpyKind_t::RT_MEMCPY_DEVICE_TO_DEVICE: {
            *rtKind = ACL_MEMCPY_DEVICE_TO_DEVICE;
            return HCCL_SUCCESS;
        }
        default: {
            HCCL_ERROR("[MemcpyKindTranslate]Not support the memory copy type[%d].", kind);
            return HCCL_E_PARA;
        }
    }
}

void HrtMemcpy(void *dst, uint64_t destMax, const void *src, uint64_t count, rtMemcpyKind_t kind)
{
    aclmdlRICaptureMode mode = aclmdlRICaptureMode::ACL_MODEL_RI_CAPTURE_MODE_RELAXED;
    HcclResult hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_WARNING("[hrtMemcpy] HrtThreadExchangeCaptureMode return [%d].", hcclRet));
    aclrtMemcpyKind rtKind = ACL_MEMCPY_DEFAULT;
    hcclRet = MemcpyKindTranslate(kind, &rtKind);
    aclError ret = aclrtMemcpy(dst, destMax, src, count, rtKind);
    HCCL_INFO("Call rtMemcpy, return value[%d]", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[SyncCopy][Mem]errNo[0x%016llx] rtMemcpy failed."
                   "return[%d], para: dstAddr[%p], destMax[%llu], srcAddr[%p], count[%llu], kind[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, src, count, kind);
        throw RuntimeApiException(StringFormat(
            "call rtMemcpy failed, dst=%p, destMax=0x%llx, src=%p, count=0x%llx, kind=%d.",
            dst, destMax, src, count, kind));
    }
    hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_WARNING("[hrtMemcpy] HrtThreadExchangeCaptureMode return [%d].", hcclRet));
}

void HrtMemset(void *dst, uint64_t destMax, uint64_t count)
{
    HCCL_INFO("[HrtMemset] dst[%p], destMax[%llu], count[%llu].", dst, destMax, count);
    CHECK_NULLPTR(dst, "[HrtMemset] dst is nullptr!");
    aclmdlRICaptureMode mode = aclmdlRICaptureMode::ACL_MODEL_RI_CAPTURE_MODE_RELAXED;
    HcclResult hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_WARNING("[hrtMemSet] HrtThreadExchangeCaptureMode return [%d].", hcclRet));
    aclError ret = aclrtMemset(dst, destMax, 0, count);

    HCCL_INFO("Call aclrtMemset, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[SyncSet][Mem]errNo[0x%016llx] aclrtMemset failed."
                   "return[%d], para: dstAddr[%p], destMax[%llu], count[%llu].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, count);
        MACRO_THROW(RuntimeApiException, msg);
    }
    hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_ERROR("[hrtMemSet] HrtThreadExchangeCaptureMode return [%d].", hcclRet));
}

void HrtIpcSetMemoryName(void *ptr, char_t *name, u64 ptrMaxLen, u32 nameMaxLen)
{
    HCCL_INFO("[HrtIpcSetMemoryName] ptr[%p], name[%s], ptrMaxLen[%llu], nameMaxLen[%u].", 
            ptr, name, ptrMaxLen, nameMaxLen);
    CHECK_NULLPTR(ptr, "[HrtIpcSetMemoryName] ptr is nullptr!");
    aclError ret = aclrtIpcMemGetExportKey(ptr, ptrMaxLen, name, nameMaxLen, 0UL);
    HCCL_INFO("Call aclrtIpcMemGetExportKey, return value[%d], para: ptr[%p], name[%s], byteCount[%llu], nameLen[%u].",
              ret, ptr, name, ptrMaxLen, nameMaxLen);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Set][IpcMemoryName]errNo[0x%016llx] rtSet Ipc Memory Name."
                   "return[%d], para: ptr[%p] byteCount[%llu], name[%s].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, ptrMaxLen, name);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtIpcDestroyMemoryName(const char_t *name)
{
    HCCL_INFO("[HrtIpcDestroyMemoryName] name[%s].", name);
    CHECK_NULLPTR(name, "[HrtIpcDestroyMemoryName] name is nullptr!");
    rtError_t ret = rtIpcDestroyMemoryName(reinterpret_cast<const char *>(name));
    HCCL_INFO("Call rtIpcDestroyMemoryName, return[%d], para: name[%s].", ret, name);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[Destroy][IpcMemoryName]errNo[0x%016llx] "
                   "rtDestroy Ipc memory name fail. return[%d], para: name[%s].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, name);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void *HrtIpcOpenMemory(const char_t *name)
{
    HCCL_INFO("[HrtIpcOpenMemory] name[%s].", name);
    CHECK_NULLPTR(name, "[HrtIpcOpenMemory] name is nullptr!");
    void     *ptr = nullptr;
    aclError ret = aclrtIpcMemImportByKey(&ptr, name, 0UL);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Open][IpcMemory]errNo[0x%016llx] "
                   "rtOpen ipc memory fail. return[%d], para: ptr[%p], name[%s].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, name);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return ptr;
}

void HrtIpcCloseMemory(const void *ptr)
{
    HCCL_INFO("[HrtIpcCloseMemory] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtIpcCloseMemory] ptr is nullptr!");
    rtError_t ret = rtIpcCloseMemory(ptr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[Close][IpcMemory]errNo[0x%016llx] "
                   "rtClose ipc memory fail, return[%d]. para: ptr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtIpcSetMemoryPid(const char_t *name, int pid)
{
    aclError ret = aclrtIpcMemSetImportPid(name, &pid, 1);
    HCCL_INFO("Call aclrtIpcMemSetImportPid, return value[%d], pid[%d], name[%s].", ret, pid, name);
    CHECK_NULLPTR(name, "[HrtIpcSetMemoryPid] name is nullptr!");
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Set][IpcMemoryPid]errNo[0x%016llx] "
                   "rtSet ipc memory pid fail. return[%d], pid[%d], name[%s].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, pid, name);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

rtPointerAttributes_t  HrtPointerGetAttributes(const void *ptr)
{
    HCCL_INFO("[HrtPointerGetAttributes] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtPointerGetAttributes] ptr is nullptr!");
    rtPointerAttributes_t ptrAttr;
    rtError_t             ret = rtPointerGetAttributes(reinterpret_cast<rtPointerAttributes_t *>(&ptrAttr), ptr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[Get][PointAttr]errNo[0x%016llx] rt get point attr failed."
                   "return[%d], para: ptrAddr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return ptrAttr;
}

void PrintMemoryAttr(const void *memAddr)
{
    HCCL_INFO("[PrintMemoryAttr] memAddr[%p].", memAddr);
    CHECK_NULLPTR(memAddr, "[PrintMemoryAttr] memAddr is nullptr!");
    if (LIKELY(!CheckInfoLogLevel())) {
        return;
    }

    rtPointerAttributes_t memAttr = HrtPointerGetAttributes(memAddr);
    HCCL_INFO("memory attributes: address[%p], page size[%u], type[%d].", memAddr, memAttr.pageSize, memAttr.memoryType);
}

void HrtDevMemAlignWithPage(void *ptr, u64 size, void *&ipcPtr, u64 &ipcSize, u64 &ipcOff)
{
    HCCL_INFO("[HrtDevMemAlignWithPage] ptr[%p], size[%llu], ipcPtr[%p], ipcSize[%llu], ipcOff[%llu].",
        ptr, size, ipcPtr, ipcSize, ipcOff);
    CHECK_NULLPTR(ptr, "[HrtDevMemAlignWithPage] ptr is nullptr!");
    CHECK_NULLPTR(ipcPtr, "[HrtDevMemAlignWithPage] ipcPtr is nullptr!");
    rtPointerAttributes_t memAttr = HrtPointerGetAttributes(ptr);

    HCCL_INFO("[HrtDevMemAlignWithPage]get pageSize[%u].", memAttr.pageSize);
    if (memAttr.pageSize == 0) {
        ipcPtr  = ptr;
        ipcSize = size;
        ipcOff  = 0;
        return;
    }

    u64 tmpPtr = reinterpret_cast<u64>(ptr);
    ipcPtr     = reinterpret_cast<void *>((reinterpret_cast<u64>(ptr)) & (~(static_cast<u64>(memAttr.pageSize) - 1)));
    ipcOff     = tmpPtr - reinterpret_cast<u64>(ipcPtr);
    ipcSize    = size + ipcOff;
}

void *HrtMallocHost(u64 size)
{
    HCCL_INFO("[HrtMallocHost] size[%llu].", size);
    void *hostPtr = nullptr;
    aclrtMallocAttrValue moduleIdValue;
    moduleIdValue.moduleId = HCCL;
    aclrtMallocAttribute attrs{.attr = ACL_RT_MEM_ATTR_MODULE_ID, .value = moduleIdValue};
    aclrtMallocConfig cfg{.attrs = &attrs, .numAttrs = 1};
    aclError ret = aclrtMallocHostWithCfg(&hostPtr, size, &cfg);
    HCCL_INFO("Call aclrtMallocHostWithCfg, return value[%d], para: hostPtr[%p], size[%llu], moudleId: HCCL.", 
                ret, hostPtr, size);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Malloc][Host]errNo[0x%016llx] rt malloc host fail. return[%d]."
                   "para: size[%llu].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, size);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return hostPtr;
}

void HrtFreeHost(void *hostPtr)
{
    HCCL_INFO("[HrtFreeHost] hostPtr[%p].", hostPtr);
    CHECK_NULLPTR(hostPtr, "[HrtFreeHost] hostPtr is nullptr!");
    aclError ret = aclrtFreeHost(hostPtr);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Free][Host]errNo[0x%016llx] rt free host fail. return[%d]."
                   "para: hostPtr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, hostPtr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

aclrtNotify HrtNotifyCreate(s32 deviceLogicId)
{
    HCCL_INFO("[HrtNotifyCreate] deviceLogicId[%d].", deviceLogicId);
    aclrtNotify ptr = nullptr;
    // aclrtCreateNotify 中通过 aclrtGetDevice 获取 deviceId，所以要求当前线程设置过 setDevice
    aclError  ret = aclrtCreateNotify(&ptr, ACL_NOTIFY_DEFAULT);
    HCCL_INFO("[HrtNotifyCreate] deviceId[%d].", deviceLogicId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Notify][Create]errNo[0x%016llx] rtNotifyCreate error."
                   "return[%d], deviceId[%d], ptr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId, ptr);
        MACRO_THROW(RuntimeApiException, msg);
    }
    HCCL_INFO("[HrtNotifyCreate] deviceId[%d], ptr[%p]", deviceLogicId, ptr);
    return ptr;
}

void HrtNotifyDestroy(RtNotify_t ptr)
{
    HCCL_INFO("[HrtNotifyDestroy] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtNotifyDestroy] ptr is nullptr!");
    aclError ret = aclrtDestroyNotify(ptr);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Notify][Destroy]errNo[0x%016llx] aclrtDestroyNotify error."
                   "ptr[%p], return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ptr, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtIpcSetNotifyName(RtNotify_t ptr, char_t *name, uint32_t len)
{
    HCCL_INFO("[HrtIpcSetNotifyName] ptr[%p], name[%s], len[%u].", ptr, name, len);
    CHECK_NULLPTR(ptr, "[HrtIpcSetNotifyName] ptr is nullptr!");
    CHECK_NULLPTR(name, "[HrtIpcSetNotifyName] name is nullptr!");
    if (HrtGetDeviceType() == DevType::DEV_TYPE_910_95) {
        return;
    }
    aclError ret = aclrtNotifyGetExportKey(ptr, name, len, 0UL);
    HCCL_INFO("Call aclrtNotifyGetExportKey, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Set][IPCNotify]errNo[0x%016llx] IPC set notify name fail."
                   "ptr[%p], name[%s], len[%u], return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ptr, name, len, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
    HCCL_INFO("[HrtIpcSetNotifyName] ptr[%p], name[%s] len[%u].", ptr, name, len);
}

u32 HrtGetNotifyID(RtNotify_t notifyHandle)
{
    HCCL_INFO("[HrtGetNotifyID] notifyHandle[%p].", notifyHandle);
    CHECK_NULLPTR(notifyHandle, "[HrtGetNotifyID] notifyHandle is nullptr!");
    u32       notifyID = 0;
    aclError  ret      = aclrtGetNotifyId(notifyHandle, &notifyID);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[HrtGetNotifyID]rt get notify id failed."
                                "notifyHandle[%p], notifyID[%u], return[%d].",
                                notifyHandle, notifyID, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return notifyID;
}

u64 HrtNotifyGetAddr(RtNotify_t notifyHandle)
{
    HCCL_INFO("[HrtNotifyGetAddr] notifyHandle[%p].", notifyHandle);
    CHECK_NULLPTR(notifyHandle, "[HrtNotifyGetAddr] notifyHandle is nullptr!");
    uint64_t  addr;
    rtError_t ret = rtGetNotifyAddress(notifyHandle, &addr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[rtGetNotifyAddress]rt get notify address failed."
                                "notifyHandle[%p], addr[%llu], return[%d].",
                                notifyHandle, addr, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return addr;
}

void HrtSetIpcNotifyPid(aclrtNotify notify, int32_t pid)
{
    HCCL_INFO("[HrtSetIpcNotifyPid] notify[%p], pid[%d].", notify, pid);
    CHECK_NULLPTR(notify, "[HrtSetIpcNotifyPid] notify is nullptr!");
    aclError ret = aclrtNotifySetImportPid(notify, &pid, 1);
    HCCL_INFO("Call rtSetIpcNotifyPid, return value[%d]. Params: pid[%d].", ret, pid);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[Set][IpcNotifyPid]errNo[0x%016llx] "
                   "rtSet ipc Notify pid fail. notify[%p], return[%d], pid[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), notify, ret, pid);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

RtNotify_t HrtIpcOpenNotify(const char_t *name)
{
    HCCL_INFO("[HrtIpcOpenNotify] name[%s].", name);
    CHECK_NULLPTR(name, "[HrtIpcOpenNotify] name is nullptr!");
    uint64_t flags = 0;
    aclrtNotify* notify = nullptr;
    aclError ret = aclrtNotifyImportByKey(notify, name, static_cast<uint64_t>(flags));
    HCCL_INFO("Call rtIpcOpenNotify, return value[%d] para: notify[%p], name[%s].", ret, notify, name);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[rt][IpcOpenNotify]errNo[0x%016llx] rt ipc notify open fail."
                   "return[%d]. para: notify[%p], name[%s], flags[%llu]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, notify, name, flags);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return notify;
}

u32 HrtNotifyGetOffset(RtNotify_t ptr)
{
    HCCL_INFO("[HrtNotifyGetOffset] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtNotifyGetOffset] ptr is nullptr!");
    uint32_t  offset = 0;
    aclError ret = aclrtGetNotifyId(ptr, &offset);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[rt][NotifyGetOffset]errNo[0x%016llx] rt ipc notify open fail."
                   "return[%d], ptr[%p], offset[%u].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, offset);
        MACRO_THROW(RuntimeApiException, msg);
    }
    return offset;
}

void HrtNotifyWaitWithTimeOut(RtNotify_t notifyPtr, aclrtStream streamPtr, uint32_t timeOut)
{
    HCCL_INFO("[HrtNotifyWaitWithTimeOut] notifyPtr[%p], streamPtr[%p], timeOut[%u].", 
                notifyPtr, streamPtr, timeOut);
    CHECK_NULLPTR(notifyPtr, "[HrtNotifyWaitWithTimeOut] notifyPtr is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtNotifyWaitWithTimeOut] streamPtr is nullptr!");
    aclError ret = aclrtWaitAndResetNotify(notifyPtr, streamPtr, timeOut);
    HCCL_INFO("Call aclrtWaitAndResetNotify, return value[%d]", ret);
    if (ret != ACL_SUCCESS) {
        throw RuntimeApiException(
            StringFormat("call aclrtWaitAndResetNotify failed, notifyPtr=%p, streamPtr=%p, return=%d", notifyPtr, streamPtr, ret));
        ;
    }
}

void HrtNotifyRecord(RtNotify_t notifyPtr, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtNotifyRecord] notifyPtr[%p], streamPtr[%p].", notifyPtr, streamPtr);
    CHECK_NULLPTR(notifyPtr, "[HrtNotifyRecord] notifyPtr is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtNotifyRecord] streamPtr is nullptr!");
    aclError ret = aclrtRecordNotify(notifyPtr, streamPtr);
    HCCL_INFO("Call aclrtRecordNotify, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        throw RuntimeApiException(
            StringFormat("call HrtNotifyRecord failed, notifyPtr=%p, streamPtr=%p, return=%d", notifyPtr, streamPtr, ret));
    }
}

void HrtMemAsyncCopy(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtMemcpyKind kind,
                     aclrtStream streamPtr)
{
    HCCL_INFO("[HrtMemAsyncCopy] dst[%p], destMax[%llu], src[%p], count[%llu], kind[%d], streamPtr[%p].", 
                dst, destMax, src, count, kind, streamPtr);
    CHECK_NULLPTR(dst, "[HrtMemAsyncCopy] dst is nullptr!");
    CHECK_NULLPTR(src, "[HrtMemAsyncCopy] src is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtMemAsyncCopy] streamPtr is nullptr!");
    aclError ret = aclrtMemcpyAsync(dst, destMax, src, count, kind, streamPtr);
    HCCL_DEBUG("Call aclrtMemcpyAsync, return value[%d], para: dstAddr[%p], destMax[%llu], "
               "srcAddr[%p], count[%llu], rtKind[%d].", ret, dst, destMax, src, count, kind);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[AsyncCopy][Mem]errNo[0x%016llx] rt memory async copy failed, "
                   "return[%d], para: dstAddr[%p], destMax[%llu], srcAddr[%p], count[%llu], kind[%d], stream[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, src, count, kind, streamPtr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtReduceAsync(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtReduceKind kind,
                    aclDataType type, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtReduceAsync] dst[%p], destMax[%llu], src[%p], count[%llu], kind[%d], type[%d], streamPtr[%p].", 
                dst, destMax, src, count, kind, type, streamPtr);
    CHECK_NULLPTR(dst, "[HrtReduceAsync] dst is nullptr!");
    CHECK_NULLPTR(src, "[HrtReduceAsync] src is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtReduceAsync] streamPtr is nullptr!");
    // reserve 预留字段填 nullptr
    aclError ret = aclrtReduceAsync(dst, src, count, kind, type, streamPtr, nullptr);
    HCCL_INFO("Call rtReduceAsync, return value[%d]. para: dst[%p] destMax[%llu] src[%p] count[%llu] rtReduceOp[%d] "
               "rtDataType[%d] streamPtr[%p].",
               ret, dst, destMax, src, count, kind, type, streamPtr);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("[rt][ReduceAsync]errNo[0x%016llx] rt reduce async fail, "
                   "return[%d]. para: dst[%p] destMax[%llu] src[%p] count[%llu] rtReduceOp[%d] rtDataType[%d] streamPtr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, src, count, kind, type, streamPtr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtRDMASend(u32 qpn, u32 wqeIndex, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtRDMASend] qpn[%u], wqeIndex[%u], streamPtr[%p].", qpn, wqeIndex, streamPtr);
    CHECK_NULLPTR(streamPtr, "[HrtRDMASend] streamPtr is nullptr!");
    rtError_t ret = rtRDMASend(qpn, wqeIndex, streamPtr);
    HCCL_INFO("Call rtRDMASend, return value[%d]. Params: qpn[%u] wqeIndex[%u] streamPtr[%p].", ret, qpn, wqeIndex, streamPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[rt][RdmaSend]errNo[0x%016llx] rt rdma send fail, "
                   "return[%d]. para: qpn[%u] wqeIndex[%u] streamPtr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, qpn, wqeIndex, streamPtr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtRDMADBSend(uint32_t dbindex, uint64_t dbinfo, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtRDMADBSend] dbindex[%u], dbinfo[%llu], streamPtr[%p].", dbindex, dbinfo, streamPtr);
    CHECK_NULLPTR(streamPtr, "[HrtRDMADBSend] streamPtr is nullptr!");
    rtError_t ret = rtRDMADBSend(dbindex, dbinfo, streamPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[rtRDMADBSend]errNo[0x%016llx] rt rdma send fail, "
                   "return[%d]. para: dbindex[%u] dbinfo[%llu] streamPtr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dbindex, dbinfo, streamPtr);
        MACRO_THROW(RuntimeApiException, msg);
    }
}
// 兜底extern, 正式适配待todo
void HrtKernelLaunchWithFlagV2(const void *stubFunc, uint32_t numBlocks, rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc,
    rtStream_t stream, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo)
{
    HCCL_INFO("[hrtKernelLaunchWithFlagV2]numBlocks: [%u]", numBlocks);
    rtError_t ret = rtKernelLaunchWithFlagV2(stubFunc, numBlocks, argsInfo, smDesc, stream, flags, cfgInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[hrtKernelLaunchWithFlagV2]execute kernel launch v2 failed, "
                   "stubFunc[%p], numBlocks[%u], argsInfo[%p], smDesc[%p], stream[%p], flags[%u], cfgInfo[%p], return[%d].", 
                    stubFunc, numBlocks, argsInfo, smDesc, stream, flags, cfgInfo, ret);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtGetTaskIdAndStreamID(u32 &taskId, u32 &streamId)
{
    HCCL_INFO("[HrtGetTaskIdAndStreamID] taskId[%u], streamId[%u].", taskId, streamId);
    rtError_t ret = rtGetTaskIdAndStreamID(&taskId, &streamId);
    HCCL_INFO("Call rtGetTaskIdAndStreamId, return value[%d], para: taskId[%u], streamId[%u].", ret, taskId, streamId);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("[Get][TaskIdAndStreamID]errNo[0x%016llx] "
                   "rt get task ID and stream ID fail. return[%d], taskId[%u], streamId[%u].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, taskId, streamId);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtUbDbSend(const HrtUbDbInfo &info, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtUbDbSend] info: dbNum=%u funcId=%u dieId=%u rsv=%u jettyId=%u piValue=%u wrCqe=%u, streamPtr.",
        info.dbNum, info.info[0].functionId, info.info[0].dieId, info.info[0].rsv,
        info.info[0].jettyId, info.info[0].piValue, info.wrCqe, streamPtr);
    CHECK_NULLPTR(streamPtr, "[HrtUbDbSend] streamPtr is nullptr!");
    // 调用RTS接口，通过doorbell方式下发UB任务
    rtUbDbInfo_t dbSendInfo{};
    dbSendInfo.dbNum              = info.dbNum;
    dbSendInfo.info[0].functionId = info.info[0].functionId;
    dbSendInfo.info[0].dieId      = info.info[0].dieId;
    dbSendInfo.info[0].rsv        = info.info[0].rsv;
    dbSendInfo.info[0].jettyId    = info.info[0].jettyId;
    dbSendInfo.info[0].piValue    = info.info[0].piValue;
    dbSendInfo.wrCqe              = info.wrCqe;
    rtError_t ret = rtUbDbSend(&dbSendInfo, streamPtr);

    HCCL_INFO("Call rtUbDbSend, return value[%d]. Params: dbNum[%u] wrCqe[%u].", ret, info.dbNum, info.wrCqe);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("call rtUbDbSend failed, return[%d], "
                "info: dbNum=%u, funcId=%u, dieId=%u, rsv=%u, jettyId=%u, piValue=%u, wrCqe=%u, streamPtr=%p, "
                "dbSendInfo: dbNum=%u, funcId=%u, dieId=%u, rsv=%u, jettyId=%u, piValue=%u, wrCqe=%u.",
                ret, info.dbNum, info.info[0].functionId, info.info[0].dieId, info.info[0].rsv,
                info.info[0].jettyId, info.info[0].piValue, info.wrCqe, streamPtr, 
                dbSendInfo.dbNum, dbSendInfo.info[0].functionId, dbSendInfo.info[0].dieId, dbSendInfo.info[0].rsv,
                dbSendInfo.info[0].jettyId, dbSendInfo.info[0].piValue, dbSendInfo.wrCqe);
        MACRO_THROW(RuntimeApiException, msg);
    }
}

void HrtUbDirectSend(const HrtUbWqeInfo &info, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtUbDirectSend] info: dieId[%u], jettyId=[%u], funcId=[%u], wqeSize=[%u], wqePtrLen=[%u], "
                "wrCqe=[%u], wqe[%u], streamPtr.",
                info.dieId, info.jettyId, info.functionId, info.wqeSize, info.wqePtrLen, 
                info.wrCqe, info.wqe, streamPtr);
    CHECK_NULLPTR(streamPtr, "[HrtUbDirectSend] streamPtr is nullptr!");
    rtUbWqeInfo_t dwqeInfo{};
    dwqeInfo.wrCqe      = info.wrCqe;
    dwqeInfo.functionId = info.functionId;
    dwqeInfo.dieId      = info.dieId;
    dwqeInfo.wqeSize    = info.wqeSize;
    dwqeInfo.wqe        = info.wqe;
    dwqeInfo.wqePtrLen  = info.wqePtrLen;
    dwqeInfo.jettyId    = info.jettyId;
    rtError_t ret       = rtUbDirectSend(&dwqeInfo, streamPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("call rtUbDirectSend failed, dieId[%u], jettyId=[%u], funcId=[%u], wqeSize=[%u], "
                                  "wqePtrLen=[%u], wrCqe=[%u], returm=[%d].",
                                  info.dieId, info.jettyId, info.functionId, info.wqeSize, info.wqePtrLen, info.wrCqe, ret);
        THROW<RuntimeApiException>(msg);
    }
}

rtCntNotify_t HrtCntNotifyCreate(u32 deviceId)
{ 
    HCCL_INFO("[HrtCntNotifyCreate] deviceId[%u].", deviceId);
    rtCntNotify_t handle;
    aclError     ret = rtCntNotifyCreateServer(&handle, RT_NOTIFY_FLAG_DEFAULT);
    HCCL_INFO("Call rtGetTaskIdAndStreamId, return value[%d] devId[%d] handle[%p].", ret, deviceId, handle);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtCntNotifyCreate failed, return[%d], deviceId[%u], handle[%p].", 
                                    ret, deviceId, handle);
        THROW<RuntimeApiException>(msg);
    }
    return handle;
}

u32 HrtGetCntNotifyId(const rtCntNotify_t inCntNotify)
{
    HCCL_INFO("[HrtGetCntNotifyId] inCntNotify[%p].", inCntNotify);
    CHECK_NULLPTR(inCntNotify, "[HrtGetCntNotifyId] inCntNotify is nullptr!");
    u32       notifyId = 0; // 待接口rtGetCntNotifyId(inCntNotify, notifyId)上库，目前打桩;
    aclError ret      = rtsCntNotifyGetId(inCntNotify, &notifyId);
    HCCL_INFO("Call rtGetCntNotifyId, return value[%d], inCntNotify[%p], notifyId[%u].", ret, inCntNotify, notifyId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtGetCntNotifyId failed, return[%d], inCntNotify[%p], notifyId[%u].", 
            ret, inCntNotify, notifyId);
        THROW<RuntimeApiException>(msg);
    }
    return notifyId;
}

void HrtCntNotifyDestroy(const rtCntNotify_t inCntNotify)
{
    HCCL_INFO("[HrtCntNotifyDestroy] inCntNotify[%p].", inCntNotify);
    CHECK_NULLPTR(inCntNotify, "[HrtCntNotifyDestroy] inCntNotify is nullptr!");
    aclError ret = rtCntNotifyDestroy(inCntNotify);
    HCCL_INFO("Call rtCntNotifyDestroy, return value[%d], inCntNotify[%p].", ret, inCntNotify);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtCntNotifyDestroy failed, return[%d], inCntNotify[%p].", 
            ret, inCntNotify);
        THROW<RuntimeApiException>(msg);
    }
}

const std::map<HrtCntNotifyRecordMode, rtCntNotifyRecordMode> HRT_CNT_NOTIFY_RECORD_MODE_MAP
    = {{HrtCntNotifyRecordMode::WRITE_BIT, rtCntNotifyRecordMode::RT_CNT_NOTIFY_RECORD_BIT_OR_MODE},
       {HrtCntNotifyRecordMode::STORE, rtCntNotifyRecordMode::RT_CNT_NOTIFY_RECORD_SET_VALUE_MODE}};
void HrtCntNotifyRecord(const rtCntNotify_t inCntNotify, const aclrtStream streamPtr, 
                        HrtCntNotifyRecordMode mode, u32 value)
{
    HCCL_INFO("[HrtCntNotifyRecord] inCntNotify[%p], streamPtr[%p], mode[%d], value[%u].", 
                inCntNotify, streamPtr, mode, value);
    CHECK_NULLPTR(inCntNotify, "[HrtCntNotifyRecord] inCntNotify is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtCntNotifyRecord] streamPtr is nullptr!");
    rtCntNotifyRecordInfo_t recordInfo{};
    recordInfo.mode  = HRT_CNT_NOTIFY_RECORD_MODE_MAP.at(mode);
    recordInfo.value = value;
    aclError ret    = rtsCntNotifyRecord(inCntNotify, streamPtr, &recordInfo);
    HCCL_INFO("Call rtCntNotifyRecord, return value[%d], inCntNotify[%p], streamPtr[%p], mode[%d], value[%u].", 
                ret, inCntNotify, streamPtr, mode, value);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtCntNotifyRecord failed, return[%d], inCntNotify[%p], streamPtr[%p], mode[%d], value[%u].", 
                ret, inCntNotify, streamPtr, mode, value);
        THROW<RuntimeApiException>(msg);
    }
}
// david接口 包间接口
const std::map<HrtCntNotifyWaitMode, rtCntNotifyWaitMode> HRT_CNT_NOTIFY_WAIT_MODE_MAP
    = {{HrtCntNotifyWaitMode::EQUAL, rtCntNotifyWaitMode::RT_CNT_NOTIFY_WAIT_EQUAL_MODE},
       {HrtCntNotifyWaitMode::BITMAP, rtCntNotifyWaitMode::RT_CNT_NOTIFY_WAIT_EQUAL_WITH_BITMASK_MODE}};
void HrtCntNotifyWaitWithTimeOut(const rtCntNotify_t inCntNotify, const aclrtStream streamPtr, 
                                HrtCntNotifyWaitMode mode, u32 value, u32 timeout, bool isClear)
{
    HCCL_INFO("[HrtCntNotifyRecord] inCntNotify[%p], streamPtr[%p], mode[%d], value[%u], timeout[%u], isClear[%s].", 
                inCntNotify, streamPtr, mode, value, timeout, isClear);
    CHECK_NULLPTR(inCntNotify, "[HrtCntNotifyRecord] inCntNotify is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtCntNotifyRecord] streamPtr is nullptr!");
    rtCntNotifyWaitInfo_t waitInfo{};
    waitInfo.mode    = HRT_CNT_NOTIFY_WAIT_MODE_MAP.at(mode);
    waitInfo.value   = value;
    waitInfo.isClear = isClear;
    waitInfo.timeout = timeout;
    aclError ret    = rtsCntNotifyWaitWithTimeout(inCntNotify, streamPtr, &waitInfo);
    HCCL_INFO("Call rtCntNotifyWaitWithTimeout, return value[%d], inCntNotify[%p]", ret, inCntNotify);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtCntNotifyWaitWithTimeout failed, return[%d], inCntNotify[%p], streamPtr[%p], "
                    "mode[%d], value[%u], timeout[%u], isClear[%s].", 
                    inCntNotify, streamPtr, mode, value, timeout, isClear);
        THROW<RuntimeApiException>(msg);
    }
}

aclrtNotify HrtNotifyCreateWithFlag(u32 devId, u32 flag)
{
    HCCL_INFO("[HrtNotifyCreateWithFlag] devId[%u], flag[%u].", devId, flag);
    aclrtNotify ptr = nullptr;
    aclError  ret = aclrtCreateNotify(&ptr, flag);
    HCCL_INFO("Call HrtNotifyCreateWithFlag, return value[%d], flag[%u] devid[%u].", ret, flag, devId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtNotifyCreateWithFlag failed, return[%d], devId[%u], flag[%u].", 
                    ret, devId, flag);
        THROW<RuntimeApiException>(msg);
    }
    return ptr;
}

RtNotify_t HrtIpcOpenNotifyWithFlag(const char_t *name, uint32_t flags)
{
    HCCL_INFO("[HrtIpcOpenNotifyWithFlag] name[%s], flags[%u].", name, flags);
    CHECK_NULLPTR(name, "[HrtIpcOpenNotifyWithFlag] name is nullptr!");
    RtNotify_t ptr = nullptr;
    aclError ret = aclrtNotifyImportByKey(&ptr, name, static_cast<uint64_t>(flags));
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtIpcOpenNotifyWithFlag failed, return[%d], name[%p], flags[%u], ptr[%p].", 
                    ret, name, flags, ptr);
        THROW<RuntimeApiException>(msg);
    }
    return ptr;
}
// 兜底extern形式
void HrtAicpuKernelLaunchExWithArgs(uint32_t kernelType, const char *opName, uint32_t numBlocks,
                                    const rtAicpuArgsEx_t *argsInfo, rtSmDesc_t * const smDesc, const rtStream_t stream,
                                    uint32_t flags)
{
    HCCL_INFO("[HrtAicpuKernelLaunchExWithArgs] kernelType[%u], opName[%s], numBlocks[%u], argsInfo[%p], "
                "smDesc[%p], stream[%p], flags[%u].", 
                kernelType, opName, numBlocks, argsInfo, smDesc, stream, flags);
    CHECK_NULLPTR(argsInfo, "[HrtAicpuKernelLaunchExWithArgs] argsInfo is nullptr!");
    CHECK_NULLPTR(smDesc, "[HrtAicpuKernelLaunchExWithArgs] smDesc is nullptr!");
    CHECK_NULLPTR(stream, "[HrtAicpuKernelLaunchExWithArgs] stream is nullptr!");
    auto tmp = reinterpret_cast<const char *>(argsInfo->args) + argsInfo->soNameAddrOffset;
    HCCL_INFO("HrtAicpuKernelLaunchExWithArgs: args.soName = %s", tmp);
    tmp = reinterpret_cast<const char *>(argsInfo->args) + argsInfo->kernelNameAddrOffset;
    HCCL_INFO("HrtAicpuKernelLaunchExWithArgs: args.kernelName = %s", tmp);

    rtError_t ret = rtAicpuKernelLaunchExWithArgs(kernelType, opName, numBlocks, argsInfo, smDesc, stream, flags);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtAicpuKernelLaunchExWithArgs failed, return[%d], kernelType[%u], opName[%s], "
                    "numBlocks[%u], argsInfo[%p], smDesc[%p], stream[%p], flags[%u], tmp[%s].", 
                ret, kernelType, opName, numBlocks, argsInfo, smDesc, stream, flags, tmp);
        THROW<RuntimeApiException>(msg);
    }
}

void HrtRegTaskFailCallbackByModule(aclrtExceptionInfoCallback callback)
{
    HCCL_INFO("[HrtRegTaskFailCallbackByModule] callback[%p].", callback);
    CHECK_NULLPTR(callback, "[HrtRegTaskFailCallbackByModule] callback is nullptr!");
    aclError ret = aclrtSetExceptionInfoCallback(callback);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtSetExceptionInfoCallback failed, return[%d], callback[%p].", 
                                    ret, callback);
        THROW<RuntimeApiException>(msg);
    }
}

u32 HrtStreamGetSqId(const aclrtStream ptr)
{
    HCCL_INFO("[HrtStreamGetSqId] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtStreamGetSqId] ptr is nullptr!");
    u32       sqId;
    rtError_t ret = rtStreamGetSqid(ptr, &sqId);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtStreamGetSqid failed, return[%d], ptr[%p], sqId[%u].", ret, ptr, sqId);
        THROW<RuntimeApiException>(msg);
    }
    return sqId;
}

u32 HrtStreamGetCqId(const aclrtStream ptr)
{
    HCCL_INFO("[HrtStreamGetCqId] ptr[%p].", ptr);
    CHECK_NULLPTR(ptr, "[HrtStreamGetCqId] ptr is nullptr!");
    u32 cqId;
    u32 logicCqId;
    rtError_t ret = rtStreamGetCqid(ptr, &cqId, &logicCqId);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtStreamGetCqid failed, return[%d], ptr[%p], cqId[%u], logicCqId[%u].", 
                    ret, ptr, cqId, logicCqId);
        THROW<RuntimeApiException>(msg);
    }
    return logicCqId;
}

void HrtCcuLaunch(rtCcuTaskInfo_t &taskInfo, aclrtStream const streamPtr)
{
    HCCL_INFO("[HrtCcuLaunch] taskInfo[%d], streamPtr[%p].", taskInfo, streamPtr);
    CHECK_NULLPTR(streamPtr, "[HrtCcuLaunch] streamPtr is nullptr!");
    auto ret = rtCCULaunch(&taskInfo, streamPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtCCULaunch failed, return[%d], taskInfo[%d], streamPtr[%p].", 
                                ret, taskInfo, streamPtr);
        THROW<RuntimeApiException>(msg);
    }
}

void HrtUbDevQueryInfo(rtUbDevQueryCmd cmd, void *devInfo)
{
    HCCL_INFO("[HrtUbDevQueryInfo] cmd[%d], devInfo[%p].", cmd, devInfo);
    CHECK_NULLPTR(devInfo, "[HrtUbDevQueryInfo] devInfo is nullptr!");
    auto ret = rtUbDevQueryInfo(cmd, devInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtUbDevQueryInfo failed, return[%d], cmd[%d], devInfo[%p].", 
                                ret, cmd, devInfo);
        THROW<RuntimeApiException>(msg);
    }
    if (cmd == QUERY_PROCESS_TOKEN) {
        rtMemUbTokenInfo *info = static_cast<rtMemUbTokenInfo *>(devInfo);
        info->tokenId = info->tokenId >> TOKEN_ID_RIGHT_SHIF;
    }
}
// pair<tokendId, tokenValue>
std::pair<u32, u32> HrtUbDevQueryToken(u64 addr, u64 size)
{
    HCCL_INFO("[HrtUbDevQueryToken] addr[%llu], size[%llu].", addr, size);
    rtMemUbTokenInfo info;
    info.va   = addr;
    info.size = size;
    auto ret  = rtUbDevQueryInfo(QUERY_PROCESS_TOKEN, &info);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[UbDev][QueryToken]errNo[0x%016llx] query(va=0x%llx, size=0x%llx) token failed, ret=%d",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), addr, size, ret);
        return std::make_pair(0, 0);
    }

    return {info.tokenId >> TOKEN_ID_RIGHT_SHIF, info.tokenValue};
}

const std::map<HrtDevResProcType, rtDevResProcType_t> HRT_DEV_RES_PROC_TYPE_MAP
    = {{HrtDevResProcType::PROCESS_CP1, RT_PROCESS_CP1}, {HrtDevResProcType::PROCESS_HCCP, RT_PROCESS_HCCP}};

const std::map<HrtDevResType, rtDevResType_t> HRT_DEV_RES_TYPE_MAP
    = {{HrtDevResType::RES_TYPE_STARS_NOTIFY_RECORD, RT_RES_TYPE_STARS_NOTIFY_RECORD},
       {HrtDevResType::RES_TYPE_CCU_CKE, RT_RES_TYPE_CCU_CKE},
       {HrtDevResType::RES_TYPE_CCU_XN, RT_RES_TYPE_CCU_XN},
       {HrtDevResType::RES_TYPE_STARS_CNT_NOTIFY_BIT_WR, RT_RES_TYPE_STARS_CNT_NOTIFY_BIT_WR}};
HrtDevResAddrInfo HrtGetDevResAddress(const HrtDevResInfo &devResInfo)
{
    HCCL_INFO("[HrtGetDevResAddress] devResInfo.dieId[%u], devResInfo.procType[%u], "
                "devResInfo.resType[%u], devResInfo.flag[%u], devResInfo.resId[%u].", 
                devResInfo.dieId, devResInfo.procType, devResInfo.resType, devResInfo.flag, devResInfo.resId);
    rtDevResInfo resInfo;
    resInfo.dieId    = devResInfo.dieId;
    resInfo.procType = HRT_DEV_RES_PROC_TYPE_MAP.at(devResInfo.procType);
    resInfo.resType  = HRT_DEV_RES_TYPE_MAP.at(devResInfo.resType);
    resInfo.flag     = devResInfo.flag;
    resInfo.resId    = devResInfo.resId;

    uint64_t         addr = 0;
    u32              len  = 0;
    rtDevResAddrInfo addrInfo;
    addrInfo.resAddress = &addr;
    addrInfo.len        = &len;
    auto ret           = rtGetDevResAddress(&resInfo, &addrInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtGetDevResAddress failed, return[%d], devResInfo.dieId[%u], "
                                "devResInfo.procType[%u], devResInfo.resType[%u], devResInfo.flag[%u], "
                                "devResInfo.resId[%u].", 
                                ret, devResInfo.dieId, devResInfo.procType, devResInfo.resType, devResInfo.flag, 
                                devResInfo.resId);
        THROW<RuntimeApiException>(msg);
    }
    HrtDevResAddrInfo devResAddrInfo;
    devResAddrInfo.address = addr;
    devResAddrInfo.len     = len;
    HCCL_INFO("devResAddrInfo.address[%llu], devResAddrInfo.len[%u]", devResAddrInfo.address, devResAddrInfo.len);
    return devResAddrInfo;
}
void HrtReleaseDevResAddress(const HrtDevResInfo &devResInfo)
{
    HCCL_INFO("[HrtReleaseDevResAddress] devResInfo.dieId[%u], devResInfo.procType[%u], "
                "devResInfo.resType[%u], devResInfo.flag[%u], devResInfo.resId[%u].", 
                devResInfo.dieId, devResInfo.procType, devResInfo.resType, devResInfo.flag, devResInfo.resId);
    rtDevResInfo resInfo;
    resInfo.dieId    = devResInfo.dieId;
    resInfo.procType = HRT_DEV_RES_PROC_TYPE_MAP.at(devResInfo.procType);
    resInfo.resType  = HRT_DEV_RES_TYPE_MAP.at(devResInfo.resType);
    resInfo.flag     = devResInfo.flag;
    resInfo.resId    = devResInfo.resId;

    rtError_t ret = rtReleaseDevResAddress(&resInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtReleaseDevResAddress failed, return[%d], devResInfo.dieId[%u], "
                                    "devResInfo.procType[%u],devResInfo.resType[%u], devResInfo.flag[%u], "
                                    "devResInfo.resId[%u].", 
                                    ret, devResInfo.dieId, devResInfo.procType, devResInfo.resType, 
                                    devResInfo.flag, devResInfo.resId);
        THROW<RuntimeApiException>(msg);
    }
}

aclrtEvent HrtEventCreateWithFlag(u32 flag)
{
    HCCL_INFO("[HrtEventCreateWithFlag] flag[%u].", flag);
    aclrtEvent ptr = nullptr;
    aclError ret = aclrtCreateEventWithFlag(&ptr, flag);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtEventCreateWithFlag failed, return[%d], flag[%u], ptr[%p].", ret, flag, ptr);
        THROW<RuntimeApiException>(msg);
    }
    return ptr;
}

void HrtEventDestroy(RtEvent_t eventPtr)
{
    HCCL_INFO("[HrtEventDestroy] eventPtr[%p].", eventPtr);
    CHECK_NULLPTR(eventPtr, "[HrtEventDestroy] eventPtr is nullptr!");
    aclError ret = aclrtDestroyEvent(eventPtr);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtDestroyEvent failed, return[%d], eventPtr[%p].", ret, eventPtr);
        THROW<RuntimeApiException>(msg);
    }
}

void HrtEventRecord(RtEvent_t eventPtr, aclrtStream streamPtr)
{
    HCCL_INFO("[HrtEventRecord] eventPtr[%p], streamPtr[%p].", eventPtr, streamPtr);
    CHECK_NULLPTR(eventPtr, "[HrtEventRecord] eventPtr is nullptr!");
    CHECK_NULLPTR(streamPtr, "[HrtEventRecord] streamPtr is nullptr!");
    aclError ret = aclrtRecordEvent(eventPtr, streamPtr);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtRecordEvent failed, return[%d], eventPtr[%p], streamPtr[%p].", 
                    ret, eventPtr, streamPtr);
        THROW<RuntimeApiException>(msg);
    }
}

const std::map<aclrtEventWaitStatus, HrtEventStatus> HRT_EVENT_STATUS_MAP{
    {ACL_EVENT_WAIT_STATUS_NOT_READY, HrtEventStatus::EVENT_INIT},
    {ACL_EVENT_WAIT_STATUS_COMPLETE, HrtEventStatus::EVENT_RECORDED},
};

HrtEventStatus HrtEventQueryStatus(RtEvent_t eventPtr)
{
    HCCL_INFO("[HrtEventQueryStatus] eventPtr[%p].", eventPtr);
    CHECK_NULLPTR(eventPtr, "[HrtEventQueryStatus] eventPtr is nullptr!");
    aclrtEventWaitStatus status = ACL_EVENT_WAIT_STATUS_NOT_READY;
    aclError ret = aclrtQueryEventWaitStatus(eventPtr, &status);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtQueryEventWaitStatus failed, return[%d], eventPtr[%p].", ret, eventPtr);
        THROW<RuntimeApiException>(msg);
    }
    if (HRT_EVENT_STATUS_MAP.find(status) == HRT_EVENT_STATUS_MAP.end()) {
        THROW<InvalidParamsException>(
            StringFormat("event status[%u] not in HRT_EVENT_STATUS_MAP", static_cast<u32>(status)));
    }
    return HRT_EVENT_STATUS_MAP.at(status);
}

void HrtWriteValue(u64 addr, u32 piVal, const aclrtStream streamPtr)
{
    HCCL_INFO("[HrtWriteValue] addr[%llu], piVal[%u], streamPtr[%p].", addr, piVal, streamPtr);
    CHECK_NULLPTR(streamPtr, "[HrtWriteValue] streamPtr is nullptr!");
    rtWriteValueInfo_t writeValueInfo;
    writeValueInfo.addr  = addr;
    writeValueInfo.size  = WRITE_VALUE_SIZE_TYPE_32BIT;
    writeValueInfo.value = reinterpret_cast<u8 *>(&piVal);

    HCCL_INFO("addr=[0x%llx]", writeValueInfo.addr);
    HCCL_INFO("size=[%u]", writeValueInfo.size);
    HCCL_INFO("piVal=[%u]", piVal);

    rtError_t ret = rtWriteValue(&writeValueInfo, streamPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("call rtWriteValue failed, return[%d], addr[%llu], piVal[%u], streamPtr[%p].", 
            ret, addr, piVal, streamPtr);
        THROW<RuntimeApiException>(msg);
    }
}

void HrtDeviceAbortRegCallBack(rtsDeviceTaskAbortCallback callback, void *args)
{
    aclError ret = rtsSetDeviceTaskAbortCallback("HCCL", callback, args);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("call rtSetTaskAbortCallBack failed, ret=[%d]", ret);
        THROW<RuntimeApiException>(msg);
    }
}

HcclResult HrtThreadExchangeCaptureMode(aclmdlRICaptureMode *mode)
{
    HCCL_INFO("[HrtThreadExchangeCaptureMode] mode[%p].", mode);
    CHECK_NULLPTR(mode, "[HrtThreadExchangeCaptureMode] mode is nullptr!");
    aclError ret = aclmdlRICaptureThreadExchangeMode(mode);
    if (ret == ACL_ERROR_RT_FEATURE_NOT_SUPPORT) {
        HCCL_ERROR("[HrtThreadExchangeCaptureMode] errNo[0x%016llx] "
            "rtThreadExchangeCaptureMode not support!, ret=%d, mode=%p.",
            HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, mode);
        return HCCL_E_NOT_SUPPORT;
    } else {
        CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[HrtThreadExchangeCaptureMode]rtThreadExchangeCaptureMode "
            "failed mode:%d, return value[%d].", *mode, ret), HCCL_E_RUNTIME);
    }
    return HCCL_SUCCESS;
}

HcclResult HrtMemPrefetchToDevice(void *devPtr, uint64_t len)
{
    HCCL_INFO("[HrtMemPrefetchToDevice] devPtr[%p], len[%llu].", devPtr, len);
    CHECK_NULLPTR(devPtr, "[HrtMemPrefetchToDevice] devPtr is nullptr!");
    int ret = rtMemPrefetchToDevice(devPtr, len, HrtGetDevice());
    if (ret != 0) {
        HCCL_ERROR("rtMemPrefetchToDevice fail ret=%d, devPtr=%p, len=%llu", ret, devPtr, len);
        return HCCL_E_RUNTIME;
    }
    return HCCL_SUCCESS;
}
} // namespace Hccl