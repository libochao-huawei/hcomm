/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "orion_adapter_rts.h"
#include "runtime_api_exception.h"
#include "exception_util.h"
#include "invalid_params_exception.h"
#include "log.h"
#include "acl/acl_rt.h"
#include "driver/ascend_hal.h"
#include "not_support_exception.h"
#include "adapter_error_manager_pub.h"
#include "dlrts_function_v2.h"

using namespace std;
namespace Hccl {

static constexpr int32_t RT_NOT_SUPPORT = 207000;
HcclResult HrtThreadExchangeCaptureMode(aclmdlRICaptureMode *mode);
constexpr u32 TOKEN_ID_RIGHT_SHIF = 8; // URMA_TOKEN_ID_RIGHT_SHIF，因URMA配置原因需要右移8位
namespace {
    constexpr char RT_SET_XPU_DEVICE[] = "rtSetXpuDevice";
    constexpr char RT_RESET_XPU_DEVICE[] = "rtResetXpuDevice";
}
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
        HCCL_INFO("[HrtGetDeviceType]DeviceType = DevType::DEV_TYPE_950.");
        return DevType::DEV_TYPE_950;
    }

    auto iter = SOC_VER_CONVERT.find(targetChipVerStr);
    if (iter == SOC_VER_CONVERT.end()) {
        HCCL_ERROR("[Get][DeviceType]errNo[0x%016llx] rtGetSocVersion get "
                   "illegal chipver, chip_ver[%s].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), targetChipVerStr.c_str());

        throw RuntimeApiException("call HrtGetSocVer failed.");
    }
    return iter->second;
}

DevId HrtGetDevicePhyIdByIndex(s32 deviceLogicId)
{
    DevType deviceType = HrtGetDeviceType();
    if (deviceType == DevType::DEV_TYPE_NOSOC) {
        return 0;
    }

    s32 devicePhyId = 0;
    aclError ret = aclrtGetPhyDevIdByLogicDevId(deviceLogicId, &devicePhyId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Get][DevicePhyId]errNo[0x%016llx] rtGet device PhyId by "
                   "index failed, return[%d], "
                   "para: devIndex[%d], phyId[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_DRV), ret, deviceLogicId, devicePhyId);
        throw RuntimeApiException(StringFormat("call aclrtGetPhyDevIdByLogicDevId failed, deviceLogicId=%d", deviceLogicId));
    }
    return static_cast<DevId>(devicePhyId);
}

s32 HrtDeviceGetBareTgid()
{
    s32       pid = 0;
    aclError ret = aclrtDeviceGetBareTgid(&pid);
    HCCL_INFO("Call rtDeviceGetBareTgid, return value[%d], rtGet pid[%d].", ret, pid);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Get][BareTgid]errNo[0x%016llx] rtGet pid fail, "
                   "return[%d], rtGet pid[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, pid);
        throw RuntimeApiException("call rtDeviceGetBareTgid failed. ");
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
        HCCL_WARNING("[Get][Device]errNo[0x%016llx] rtGet device fail, "
                     "please make sure that device is set. return[%d], para:deviceLogicId[%d]",
                     HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);
        throw RuntimeApiException("call aclrtGetDevice failed. ");
    }
    return deviceLogicId;
}

void HrtSetDevice(s32 deviceLogicId)
{
    aclError ret = aclrtSetDevice(deviceLogicId);
    HCCL_INFO("Call rtSetDevice, return value[%d], para: device_id[%d].", ret, deviceLogicId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Set][Device]errNo[0x%016llx] rtSet device fail, return[%d], "
                   "para:deviceLogicId[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);
        throw RuntimeApiException(StringFormat("call rtSetDevice failed, deviceLogicId=%d", deviceLogicId));
    }
}

void HrtResetDevice(s32 deviceLogicId)
{
    aclError ret = aclrtResetDevice(deviceLogicId);
    HCCL_INFO("Call aclrtResetDevice, return value[%d], para: device_id[%d].", ret, deviceLogicId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Reset][Device]errNo[0x%016llx] rtReset device fail, return[%d], "
                   "para: deviceLogicId[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);
        throw RuntimeApiException(StringFormat("call aclrtResetDevice failed, deviceLogicId=%d", deviceLogicId));
    }
}

u32 HrtGetDeviceCount()
{
    u32       count = 0;
    aclError ret   = aclrtGetDeviceCount(&count);
    HCCL_INFO("Call rtGetDeviceCount, return value[%d], para: count[%u].", ret, count);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Get][DeviceCount]errNo[0x%016llx] rtGet device count fail, "
                   "return[%d], para:count[%u]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, count);
        throw RuntimeApiException("call rtDeviceReset failed. ");
    }
    return count;
}

constexpr char RTS_SO_NAME[] = "libruntime.so";
DlRtsFunctionV2<RTS_SO_NAME> g_dlRts;
HcclResult HrtResetXpuDevice(uint32_t devType, const uint32_t devId)
{
    static auto funcPtr = reinterpret_cast<rtError_t(*)(uint32_t, const uint32_t)>(g_dlRts.Handle<RT_RESET_XPU_DEVICE>());
    CHK_PTR_NULL(funcPtr);
    rtError_t ret = funcPtr(devType, devId);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[%s] reset xpu device failed, devType[%u],devId[%u],return[%d]", __func__, devType, devId, ret);
        return HCCL_E_RUNTIME;
    }
    return HCCL_SUCCESS;
}

HcclResult HrtSetXpuDevice(uint32_t devType, const uint32_t devId)
{
    static auto funcPtr = reinterpret_cast<rtError_t(*)(uint32_t, const uint32_t)>(g_dlRts.Handle<RT_SET_XPU_DEVICE>());
    CHK_PTR_NULL(funcPtr);
    rtError_t ret = funcPtr(devType, devId);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[%s] set xpu device failed, devType[%u],devId[%u],return[%d]", __func__, devType, devId, ret);
        return HCCL_E_RUNTIME;
    }
    return HCCL_SUCCESS;
}

s32 HrtGetStreamId(aclrtStream ptr)
{
    s32       streamId;
    aclError ret = aclrtStreamGetId(ptr, &streamId);
    HCCL_INFO("Call aclrtStreamGetId, return value[%d] streamId[%d].", ret, streamId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Get][StreamId]errNo[0x%016llx] "
                   "rt get stream ID fail. return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException(StringFormat("call aclrtStreamGetId failed, ptr=%p", ptr));
    }

    return streamId;
}

u64 HrtStreamGetMode(HcclRtStream const ptr)
{
    if (ptr == nullptr) {
        throw RuntimeApiException(StringFormat("ptr is null, call aclrtGetStreamAttribute failed, ptr=%p", ptr));
    }
    u64 stmMode  =  0;
    s32 streamId = -1;
    aclError ret = aclrtStreamGetId(ptr, &streamId);
    HCCL_DEBUG("Call aclrtStreamGetId, return value[%d].", ret);
    aclrtStreamAttrValue value;
    ret = aclrtGetStreamAttribute(ptr, ACL_STREAM_ATTR_FAILURE_MODE, &value);
    stmMode = value.failureMode;
    HCCL_INFO("Call rtStreamGetMode return value[%d]. stmMode[%llu].", ret, stmMode);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Stream][GetMode]errNo[0x%016llx] rtStreamGetMode error, "
                   "rtRet[%d], stmMode[%llu]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, stmMode);
        throw RuntimeApiException(StringFormat("call aclrtGetStreamAttribute failed, ptr=%p", ptr));
    }
    return static_cast<u64>(stmMode);
}

void HrtStreamSetMode(HcclRtStream streamPtr, const uint64_t stmMode)
{
    if (streamPtr == nullptr) {
        throw RuntimeApiException(StringFormat("ptr is null, call aclrtSetStreamAttribute failed, ptr=%p", streamPtr));
    }
    s32 streamId = -1;
    aclError ret = aclrtStreamGetId(streamPtr, &streamId);
    HCCL_DEBUG("Call aclrtStreamGetId, return value[%d].", ret);
    aclrtStreamAttrValue value;
    value.failureMode = stmMode;
    ret = aclrtSetStreamAttribute(streamPtr, ACL_STREAM_ATTR_FAILURE_MODE, &value);
    HCCL_INFO("Call rtStreamSetMode return value[%d]. stmMode[%llu].", ret, stmMode);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Stream][SetMode]errNo[0x%016llx] rtStreamSetMode error, "
                   "rtRet[%d], stmMode[%llu]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, stmMode);
        throw RuntimeApiException(
            StringFormat("call aclrtSetStreamAttribute failed, ptr=%p, flags=0x%llx", streamPtr, stmMode));
    }
}

HcclResult HrtGetDeviceInfo(uint32_t deviceLogicId, int32_t moduleType, aclrtDevAttr infoType, int64_t &val)
{
    if(moduleType != DEV_MODULE_TYPE::MODULE_TYPE_SYSTEM)
    {
        THROW<NotSupportException>(StringFormat("[hrtGetDeviceInfo]Unsupported moduleType[%d].", moduleType));
    }
    aclError ret = aclrtGetDeviceInfo(deviceLogicId, infoType, reinterpret_cast<int64_t *>(&val));
    HCCL_INFO("Call HrtGetDeviceInfo return[%d]. val[%ld].", ret, val);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[HrtGetDeviceInfo]errNo[0x%016llx] rt get device info failed, "
                   "deviceLogicId=%u, moduleType=%d, infoType=%d",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), deviceLogicId, moduleType, infoType);
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
    constexpr int32_t moduleType = DEV_MODULE_TYPE::MODULE_TYPE_SYSTEM;
    constexpr aclrtDevAttr infoType = aclrtDevAttr::ACL_DEV_ATTR_MAINBOARD_ID;
    constexpr uint64_t BITS_5 = 5;
    constexpr uint64_t MASK_7 = 0x7;
    int64_t val = 0;
    CHK_RET(HrtGetDeviceInfo(deviceLogicId, moduleType, infoType, val));
    HCCL_INFO("[HrtGetMainboardId] deviceLogicId[%u] val[%ld].", deviceLogicId, val);
    CHK_PRT_RET(val < 0, HCCL_ERROR("[HrtGetMainboardId]val[%lld] < 0", val), HCCL_E_RUNTIME);
    uint64_t mainboardId = (static_cast<uint64_t>(val) >> BITS_5) & MASK_7; // 提取val的5-7位，判断整机形态
    auto it = rtMainboardIdToHcclMainboardId.find(mainboardId);
    if (it != rtMainboardIdToHcclMainboardId.end()) {
        hcclMainboardId = it->second;
    } else {
        hcclMainboardId = HcclMainboardId::MAINBOARD_OTHERS;
    }
    HCCL_INFO("[HrtGetMainboardId] deviceLogicId[%u] mainboardId[%llu] hcclMainboardId[%s].",
              deviceLogicId, mainboardId, hcclMainboardId.Describe().c_str());
    return HcclResult::HCCL_SUCCESS;
}

aclrtStream HrtStreamCreateWithFlags(uint32_t priority, uint32_t flag)
{
    aclrtStream ptr = nullptr;
    aclError ret = aclrtCreateStreamWithConfig(&ptr, priority, flag);
    HCCL_INFO("Call rtGetStreamId return value[%d]. Params: flags[%u].", ret, flag);

    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Stream][CreateWithFlags]errNo[0x%016llx] rtStreamCreate error, "
                   "rtRet[%d], flags[%u]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, flag);
        throw RuntimeApiException(
                StringFormat("call aclrtCreateStreamWithConfig failed, priority=%p, flags=%u", priority, flag));
    }

    return ptr;
}

void HrtStreamDestroy(aclrtStream ptr)
{
    aclError ret = aclrtDestroyStreamForce(ptr);
    HCCL_INFO("Call aclrtDestroyStreamForce, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Stream][Destroy]errNo[0x%016llx] rt stream Destroy fail, "
                   "return[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException(StringFormat("call aclrtDestroyStreamForce failed, ptr=%p", ptr));
    }
}

void HrtStreamActive(aclrtStream activeStream, aclrtStream stream)
{
    aclError ret = aclrtActiveStream(activeStream, stream);
    HCCL_INFO("Call aclrtActiveStream, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Activate][Stream]errNo[0x%016llx] "
                   "rt stream active fail. return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException(
            StringFormat("call aclrtActiveStream failed, active_stream=%p, stream=%p", activeStream, stream));
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
    if (ptr == nullptr) {
        throw RuntimeApiException(StringFormat("ptr is null, call aclrtSynchronizeStreamWithTimeout failed, ptr=%p", ptr));
    }
    s32       timeout = GetMsTimeFromExecTimeout();
    aclError  ret     = aclrtSynchronizeStreamWithTimeout(ptr, timeout);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Synchronize][Stream]errNo[0x%016llx] rt "
                   "streamsynchronizewithtimeout fail. return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException(StringFormat("call HcclStreamSynchronize failed, stream=%p", ptr));
    }
}

/*
 *RT_MEMORY_TS：aclrtMallocForTaskScheduler
 *RT_MEMORY_DDR：ACL_MEM_TYPE_LOW_BAND_WIDTH
 *RT_MEMORY_HBM：ACL_MEM_TYPE_HIGH_BAND_WIDTH
*/
void *HrtMalloc(u64 size, aclrtMemType_t memType)
{
    aclError ret = ACL_SUCCESS;
    void     *devPtr = nullptr;
    aclrtMallocAttrValue moduleIdValue;
    moduleIdValue.moduleId = HCCL;
    aclrtMallocAttribute attrs{.attr = ACL_RT_MEM_ATTR_MODULE_ID, .value = moduleIdValue};
    aclrtMallocConfig cfg{.attrs = &attrs, .numAttrs = 1};
    ret = aclrtMallocWithCfg(&devPtr, size, static_cast<aclrtMemMallocPolicy>(memType), &cfg);
    HCCL_INFO("Call aclrtMallocWithCfg, return value[%d] size[%llu] devPtr[%p], moudleId: HCCL.", ret, size, devPtr);
    if (ret == ACL_ERROR_RT_MEMORY_ALLOCATION) {
        RPT_INPUT_ERR(true, "EI0007", std::vector<std::string>({"resource_type", "resource_info"}),
                            std::vector<std::string>({"DeviceMemory", std::string("size:") + std::to_string(size)}));
        HCCL_ERROR("[Malloc][Mem]errNo[0x%016llx] aclrtMallocWithCfg failed, "
                   "Reason: out of memory, return[%d], para: devPtrAddr[%p], size[%llu]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, devPtr, size);
        throw RuntimeApiException(StringFormat("call HrtMalloc failed, size=0x%llu", size));
    }
    if (ret != ACL_SUCCESS) {
        RPT_INPUT_ERR(true, "EI0007", std::vector<std::string>({"resource_type", "resource_info"}),
                            std::vector<std::string>({"DeviceMemory", std::string("size:") + std::to_string(size)}));
        HCCL_ERROR("[Malloc][Mem]errNo[0x%016llx] aclrtMallocWithCfg failed, "
                   "return[%d], para: devPtrAddr[%p], size[%llu]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, devPtr, size);
        throw RuntimeApiException(StringFormat("call HrtMalloc failed, size=0x%llu", size));
    }
    return devPtr;
}

void HrtFree(void *devPtr)
{
    aclError ret = aclrtFree(devPtr);
    HCCL_INFO("Call aclrtFree, return value[%d], para: dev_ptr[%p].", ret, devPtr);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[Free][Mem]errNo[0x%016llx] aclrtFree failed, "
                   "return[%d], para: devPtrAddr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, devPtr);
        throw RuntimeApiException(StringFormat("call aclrtFree failed, devPtr=%p", devPtr));
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
        HCCL_WARNING("[hrtMemcpy] HrtThreadExchangeCaptureMode return [%d]", hcclRet));
    aclrtMemcpyKind rtKind = ACL_MEMCPY_DEFAULT;
    hcclRet = MemcpyKindTranslate(kind, &rtKind);
    aclError ret = aclrtMemcpy(dst, destMax, src, count, rtKind);
    HCCL_INFO("Call rtMemcpy, return value[%d]", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[SyncCopy][Mem]errNo[0x%016llx] rtMemcpy failed, "
                   "return[%d], para: dstAddr[%p], destMax[%llu], srcAddr[%p], count[%llu], kind[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, src, count, kind);
        throw RuntimeApiException(StringFormat(
            "call rtMemcpy failed, dst=%p, destMax=0x%llx, src=%p, count=0x%llx, kind=%d",
            dst, destMax, src, count, kind));
    }
    hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_WARNING("[hrtMemcpy] HrtThreadExchangeCaptureMode return [%d]", hcclRet));
}

void HrtMemset(void *dst, uint64_t destMax, uint64_t count)
{
    aclmdlRICaptureMode mode = aclmdlRICaptureMode::ACL_MODEL_RI_CAPTURE_MODE_RELAXED;
    HcclResult hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_WARNING("[hrtMemSet] HrtThreadExchangeCaptureMode return [%d]", hcclRet));
    aclError ret = aclrtMemset(dst, destMax, 0, count);

    HCCL_INFO("Call aclrtMemset, return value[%d]", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[SyncSet][Mem]errNo[0x%016llx] aclrtMemset failed, "
                   "return[%d], para: dstAddr[%p], destMax[%llu], count[%llu].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, count);
        throw RuntimeApiException(StringFormat(
            "call aclrtMemset failed, dst=%p, destMax=0x%llx, count=0x%llx", dst, destMax, count));
    }
    hcclRet = HrtThreadExchangeCaptureMode(&mode);
    CHK_PRT_CONT(hcclRet != HCCL_SUCCESS && hcclRet != HCCL_E_NOT_SUPPORT,
        HCCL_WARNING("[hrtMemSet] HrtThreadExchangeCaptureMode return [%d]", hcclRet));
}

void HrtIpcSetMemoryName(void *ptr, char_t *name, u64 ptrMaxLen, u32 nameMaxLen)
{
    aclError ret = aclrtIpcMemGetExportKey(ptr, ptrMaxLen, name, nameMaxLen, 1UL);
    HCCL_INFO("Call aclrtIpcMemGetExportKey, return value[%d], para: ptr[%p], name[%s], byteCount[%llu], nameLen[%u]",
              ret, ptr, name, ptrMaxLen, nameMaxLen);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Set][IpcMemoryName]errNo[0x%016llx] rtSet Ipc Memory Name, "
                   "return[%d], para: ptr[%p] byteCount[%llu].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, ptrMaxLen);
        throw RuntimeApiException(StringFormat("call aclrtIpcMemGetExportKey failed, ptr=%p, ptrMaxLen=0x%llx, name=%s",
                                               ptr, ptrMaxLen, name));
    }
}

void HrtIpcDestroyMemoryName(const char_t *name)
{
    aclError ret = aclrtIpcMemClose(reinterpret_cast<const char *>(name));
    HCCL_INFO("Call aclrtIpcMemClose, return[%d], para: name[%s]", ret, reinterpret_cast<const char *>(name));
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Destroy][IpcMemoryName]errNo[0x%016llx] "
                   "rtDestroy Ipc memory name fail. return[%d], para: name[%s]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, name);
        throw RuntimeApiException(StringFormat("call aclrtIpcMemClose failed, name=%s", name));
    }
}

void *HrtIpcOpenMemory(const char_t *name)
{
    void     *ptr = nullptr;
    aclError ret = aclrtIpcMemImportByKey(&ptr, name, 0UL);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Open][IpcMemory]errNo[0x%016llx] "
                   "rtOpen ipc memory fail. return[%d], para: ptr[%p], name[%s]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, name);
        throw RuntimeApiException(StringFormat("call aclrtIpcMemImportByKey failed, name=%s", name));
    }
    return ptr;
}

void HrtIpcCloseMemory(const void *ptr)
{
    aclError ret = aclrtIpcMemClose(reinterpret_cast<const char *>(ptr));
    HCCL_INFO("Call aclrtIpcMemClose, return[%d], para: name[%s]", ret, reinterpret_cast<const char *>(ptr));
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Close][IpcMemory]errNo[0x%016llx] "
                   "rtClose ipc memory fail, return[%d]. para: ptr[%p]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr);
        throw RuntimeApiException(StringFormat("call aclrtIpcMemClose failed, ptr=%p", ptr));
    }
}

void HrtIpcSetMemoryPid(const char_t *name, int pid)
{
    aclError ret = aclrtIpcMemSetImportPid(name, &pid, 1);
    HCCL_INFO("Call aclrtIpcMemSetImportPid, return value[%d], pid[%d], name[%s].", ret, pid, name);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Set][IpcMemoryPid]errNo[0x%016llx] "
                   "rtSet ipc memory pid fail. return[%d], pid[%d], name[%s]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, pid, name);
        throw RuntimeApiException(StringFormat("call aclrtIpcMemSetImportPid failed, name=%s, pid=%d", name, pid));
    }
}

aclrtPtrAttributes  HrtPointerGetAttributes(const void *ptr)
{
    aclrtPtrAttributes  ptrAttr;
    aclError             ret = aclrtPointerGetAttributes(ptr, reinterpret_cast<aclrtPtrAttributes *>(&ptrAttr));
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Get][PointAttr]errNo[0x%016llx] rt get point attr failed, "
                   "return[%d], para: ptrAddr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr);
        throw RuntimeApiException(StringFormat("call aclrtPointerGetAttributes failed, ptr=%p", ptr));
    }
    return ptrAttr;
}

void PrintMemoryAttr(const void *memAddr)
{
    if (LIKELY(!CheckInfoLogLevel())) {
        return;
    }
    aclrtPtrAttributes memAttr = HrtPointerGetAttributes(memAddr);
    HCCL_INFO("memory attributes: address[%p], page size[%u], type[%d]", memAddr, memAttr.pageSize,
              memAttr.location.type);
}

void HrtDevMemAlignWithPage(void *ptr, u64 size, void *&ipcPtr, u64 &ipcSize, u64 &ipcOff)
{
    aclrtPtrAttributes memAttr = HrtPointerGetAttributes(ptr);

    HCCL_INFO("[HrtDevMemAlignWithPage]get pageSize[%u]", memAttr.pageSize);
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
    void *hostPtr = nullptr;
    aclrtMallocAttrValue moduleIdValue;
    moduleIdValue.moduleId = HCCL;
    aclrtMallocAttribute attrs{.attr = ACL_RT_MEM_ATTR_MODULE_ID, .value = moduleIdValue};
    aclrtMallocConfig cfg{.attrs = &attrs, .numAttrs = 1};
    aclError ret = aclrtMallocHostWithCfg(&hostPtr, size, &cfg);
    HCCL_INFO("Call aclrtMallocHostWithCfg, return value[%d], para: hostPtr[%p], size[%llu], moudleId: HCCL.", ret,
              hostPtr, size);
    if (ret != ACL_SUCCESS) {
        RPT_INPUT_ERR(true, "EI0007", std::vector<std::string>({"resource_type", "resource_info"}),
                            std::vector<std::string>({"HostMemory", std::string("size:") + std::to_string(size)}));
        HCCL_ERROR("[Malloc][Host]errNo[0x%016llx] rt malloc host fail. return[%d], "
                   "para: size[%llu].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, size);
        throw RuntimeApiException(StringFormat("call HrtMallocHost failed, moudleId: HCCL size=0x%llx", size));
    }
    return hostPtr;
}

void HrtFreeHost(void *hostPtr)
{
    aclError ret = aclrtFreeHost(hostPtr);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Free][Host]errNo[0x%016llx] rt free host fail. return[%d], "
                   "para: hostPtr[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, hostPtr);
        throw RuntimeApiException(StringFormat("call aclrtFreeHost failed, ptr=%p", hostPtr));
    }
}

aclrtNotify HrtNotifyCreate(s32 deviceLogicId)
{
    aclrtNotify ptr = nullptr;
    // aclrtCreateNotify 中通过 aclrtGetDevice 获取 deviceId，所以要求当前线程设置过 setDevice
    aclError  ret = aclrtCreateNotify(&ptr, ACL_NOTIFY_DEFAULT);
    HCCL_INFO("[HrtNotifyCreate] deviceId[%d]", deviceLogicId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Notify][Create]errNo[0x%016llx] rtNotifyCreate error, "
                   "return[%d], deviceId[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, deviceLogicId);
        throw RuntimeApiException(StringFormat("call rtNotifyCreate failed, deviceLogidId=%d", deviceLogicId));
    }
    HCCL_INFO("[HrtNotifyCreate] deviceId[%d]", deviceLogicId);
    return ptr;
}

void HrtNotifyDestroy(RtNotify_t ptr)
{
    aclError ret = aclrtDestroyNotify(ptr);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Notify][Destroy]errNo[0x%016llx] aclrtDestroyNotify error, "
                   "return[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException("call aclrtDestroyNotify failed. ");
    }
}

void HrtIpcSetNotifyName(RtNotify_t ptr, char_t *name, uint32_t len)
{
    if (HrtGetDeviceType() == DevType::DEV_TYPE_950) {
        return;
    }
    aclError ret = aclrtNotifyGetExportKey(ptr, name, len, 0UL);
    HCCL_INFO("Call aclrtNotifyGetExportKey, return value[%d].", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Set][IPCNotify]errNo[0x%016llx] IPC set notify name fail.  "
                   "return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException("call aclrtNotifyGetExportKey failed. ");
    }
    HCCL_INFO("[HrtIpcSetNotifyName] name[%s] len[%u]", name, len);
}

u32 HrtGetNotifyID(RtNotify_t notifyHandle)
{
    u32       notifyID = 0;
    aclError  ret      = aclrtGetNotifyId(notifyHandle, &notifyID);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[HrtGetNotifyID]rt get notify id failed.");
        throw RuntimeApiException("call aclrtGetNotifyId failed. ");
    }
    return notifyID;
}

u64 HrtNotifyGetAddr(RtNotify_t notifyHandle)
{
    uint64_t  addr;
    rtError_t ret = rtGetNotifyAddress(notifyHandle, &addr);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[rtGetNotifyAddress]rt get notify address failed.");
        throw RuntimeApiException(StringFormat("call rtGetNotifyAddress failed, ptr=%p", notifyHandle));
    }
    return addr;
}

void HrtSetIpcNotifyPid(aclrtNotify notify, int32_t pid)
{
    aclError ret = aclrtNotifySetImportPid(notify, &pid, 1);
    HCCL_INFO("Call rtSetIpcNotifyPid, return value[%d]. Params: pid[%d].", ret, pid);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[Set][IpcNotifyPid]errNo[0x%016llx] "
                   "rtSet ipc Notify pid fail. return[%d], pid[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, pid);
        throw RuntimeApiException(StringFormat("call rtSetIpcNotifyPid failed,pid=%d", pid));
    }
}

RtNotify_t HrtIpcOpenNotify(const char_t *name)
{
    RtNotify_t ptr = nullptr;
    uint64_t flags = 0;
    aclrtNotify* notify = nullptr;
    aclError ret = aclrtNotifyImportByKey(notify, name, static_cast<uint64_t>(flags));
    HCCL_INFO("Call rtIpcOpenNotify, return value[%d] para: notify[%p], name[%s].", ret, ptr, name);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[rt][IpcOpenNotify]errNo[0x%016llx] rt ipc notify open fail,"
                   "return[%d]. para: notify[%p], name[%s]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, ptr, name);
        throw RuntimeApiException(StringFormat("call rtIpcOpenNotify failed, name=%s", name));
    }
    return ptr;
}

u32 HrtNotifyGetOffset(RtNotify_t ptr)
{
    uint32_t  offset = 0;
    aclError ret = aclrtGetNotifyId(ptr, &offset);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[rt][NotifyGetOffset]errNo[0x%016llx] rt ipc notify open fail,"
                   "return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException(StringFormat("call rtNotifyGetAddrOffset failed, ptr=%p", ptr));
    }
    return offset;
}

void HrtNotifyWaitWithTimeOut(RtNotify_t notifyPtr, aclrtStream streamPtr, uint32_t timeOut)
{
    aclError ret = aclrtWaitAndResetNotify(notifyPtr, streamPtr, timeOut);
    HCCL_INFO("Call aclrtWaitAndResetNotify, return value[%d]", ret);
    if (ret != ACL_SUCCESS) {
        throw RuntimeApiException(
            StringFormat("call aclrtWaitAndResetNotify failed, notifyPtr=%p, streamPtr=%p", notifyPtr, streamPtr));
        ;
    }
}

void HrtNotifyRecord(RtNotify_t notifyPtr, aclrtStream streamPtr)
{
    aclError ret = aclrtRecordNotify(notifyPtr, streamPtr);
    HCCL_INFO("Call aclrtRecordNotify, return value[%d]", ret);
    if (ret != ACL_SUCCESS) {
        throw RuntimeApiException(
            StringFormat("call HrtNotifyRecord failed, notifyPtr=%p, streamPtr=%p", notifyPtr, streamPtr));
    }
}

void HrtMemAsyncCopy(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtMemcpyKind kind,
                     aclrtStream streamPtr)
{
    aclError ret = aclrtMemcpyAsync(dst, destMax, src, count, kind, streamPtr);
    HCCL_DEBUG("Call aclrtMemcpyAsync, return value[%d], para: dstAddr[%p], destMax[%llu], "
               "srcAddr[%p], count[%llu], rtKind[%d]", ret, dst, destMax, src, count, kind);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[AsyncCopy][Mem]errNo[0x%016llx] rt memory async copy failed, "
                   "return[%d], para: dstAddr[%p], destMax[%llu], srcAddr[%p], count[%llu], kind[%d], stream[%p].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, src, count, kind, streamPtr);
        throw RuntimeApiException(StringFormat("call HrtMemAsyncCopy failed, dst=%p, destMax=0x%llx, src=%p, "
                                               "count=0x%llx, kind=%d, streamPtr=%p",
                                               dst, destMax, src, count, kind, streamPtr));
    }
}

void HrtReduceAsync(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtReduceKind kind,
                    aclDataType type, aclrtStream streamPtr)
{
    // reserve 预留字段填 nullptr
    aclError ret = aclrtReduceAsync(dst, src, count, kind, type, streamPtr, nullptr);
    HCCL_INFO("Call rtReduceAsync, return value[%d]. para: dst[%p] destMax[%llu] src[%p] count[%llu] rtReduceOp[%d] "
               "rtDataType[%d].",
               ret, dst, destMax, src, count, kind, type);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[rt][ReduceAsync]errNo[0x%016llx] rt reduce async fail,"
                   "return[%d]. para: dst[%p] destMax[%llu] src[%p] count[%llu] rtReduceOp[%d] rtDataType[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dst, destMax, src, count, kind, type);
        throw RuntimeApiException(StringFormat("call rtReduceAsync failed, dst=%p, destMax=0x%llx, src=%p, "
                                               "count=0x%llx, kind=%d, dataType=%d, streamPtr=%p",
                                               dst, destMax, src, count, kind, type, streamPtr));
    }
}

void HrtRDMASend(u32 qpn, u32 wqeIndex, aclrtStream streamPtr)
{
    rtError_t ret = rtRDMASend(qpn, wqeIndex, streamPtr);
    HCCL_INFO("Call rtRDMASend, return value[%d]. Params: qpn[%u] wqeIndex[%u].", ret, qpn, wqeIndex);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[rt][RdmaSend]errNo[0x%016llx] rt rdma send fail, "
                   "return[%d]. para: qpn[%u] wqeIndex[%u].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, qpn, wqeIndex);
        throw RuntimeApiException(StringFormat("call rtRDMASend failed, qpn[%d], wqeIndex[%d]", qpn, wqeIndex));
    }
}

void HrtRDMADBSend(uint32_t dbindex, uint64_t dbinfo, aclrtStream streamPtr)
{
    rtError_t ret = rtRDMADBSend(dbindex, dbinfo, streamPtr);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[rtRDMADBSend]errNo[0x%016llx] rt rdma send fail, "
                   "return[%d]. para: dbindex[%u]dbinfo[%llu].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret, dbindex, dbinfo);
        throw RuntimeApiException(StringFormat("call rtRDMASend failed, dbindex[%u]dbinfo[%llu]", dbindex, dbinfo));
    }
}

void HrtGetTaskIdAndStreamID(u32 &taskId, u32 &streamId)
{
    rtError_t ret = rtGetTaskIdAndStreamID(&taskId, &streamId);
    HCCL_INFO("Call rtGetTaskIdAndStreamId, return value[%d], para: taskId[%u], streamId[%u].", ret, taskId, streamId);
    if (ret != RT_ERROR_NONE) {
        HCCL_ERROR("[Get][TaskIdAndStreamID]errNo[0x%016llx] "
                   "rt get task ID and stream ID fail. return[%d].",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_RUNTIME), ret);
        throw RuntimeApiException("call HrtGetTaskIdAndStreamID failed. ");
    }
}

void HrtUbDbSend(const HrtUbDbInfo &info, aclrtStream streamPtr)
{
    THROW<NotSupportException>(StringFormat("Unsupported rtUbDbSend"));
}

void HrtUbDirectSend(const HrtUbWqeInfo &info, aclrtStream streamPtr)
{
    THROW<NotSupportException>(StringFormat("Unsupported rtUbDirectSend"));
}

aclrtCntNotify HrtCntNotifyCreate(u32 deviceId)
{
	aclrtCntNotify handle;
    aclError     ret = aclrtCntNotifyCreate(&handle, RT_NOTIFY_FLAG_DEFAULT);
    HCCL_INFO("Call aclrtCntNotifyCreate, return value[%d] devId[%u].", ret, deviceId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtCntNotifyCreate failed");
        THROW<RuntimeApiException>(msg);
    }
    return handle;
}

u32 HrtGetCntNotifyId(const aclrtCntNotify inCntNotify)
{
    u32       notifyId = 0; // 待接口rtGetCntNotifyId(inCntNotify, notifyId)上库，目前打桩;
    aclError ret      = aclrtCntNotifyGetId(inCntNotify, &notifyId);
    HCCL_INFO("Call rtGetCntNotifyId, return value[%d], inCntNotify[%p], notifyId[%u]", ret, inCntNotify, notifyId);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtGetCntNotifyId failed");
        THROW<RuntimeApiException>(msg);
    }
    return notifyId;
}

void HrtCntNotifyDestroy(const aclrtCntNotify inCntNotify)
{
    aclError ret = aclrtCntNotifyDestroy(inCntNotify);
    HCCL_INFO("Call aclrtCntNotifyDestroy, return value[%d], inCntNotify[%p]", ret, inCntNotify);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtCntNotifyDestroy failed");
        THROW<RuntimeApiException>(msg);
    }
}

const std::map<HrtCntNotifyRecordMode, aclrtCntNotifyRecordMode> HRT_CNT_NOTIFY_RECORD_MODE_MAP
    = {{HrtCntNotifyRecordMode::WRITE_BIT, aclrtCntNotifyRecordMode::ACL_RT_CNT_NOTIFY_RECORD_BIT_OR_MODE},
       {HrtCntNotifyRecordMode::STORE, aclrtCntNotifyRecordMode::ACL_RT_CNT_NOTIFY_RECORD_SET_VALUE_MODE}};
void HrtCntNotifyRecord(const aclrtCntNotify inCntNotify, const aclrtStream streamPtr, HrtCntNotifyRecordMode mode, u32 value)
{
    aclrtCntNotifyRecordInfo recordInfo{};
    recordInfo.mode  = HRT_CNT_NOTIFY_RECORD_MODE_MAP.at(mode);
    recordInfo.value = value;
    aclError ret    = aclrtCntNotifyRecord(inCntNotify, streamPtr, &recordInfo);
    HCCL_INFO("Call aclrtCntNotifyRecord, return valuee[%d], inCntNotify[%p]", ret, inCntNotify);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call aclrtCntNotifyRecord failed");
        THROW<RuntimeApiException>(msg);
    }
}
// david接口 包间接口
const std::map<HrtCntNotifyWaitMode, aclrtCntNotifyWaitMode> HRT_CNT_NOTIFY_WAIT_MODE_MAP
    = {{HrtCntNotifyWaitMode::EQUAL, aclrtCntNotifyWaitMode::ACL_RT_CNT_NOTIFY_WAIT_EQUAL_MODE},
       {HrtCntNotifyWaitMode::BITMAP, aclrtCntNotifyWaitMode::ACL_RT_CNT_NOTIFY_WAIT_EQUAL_WITH_BITMASK_MODE}};
void HrtCntNotifyWaitWithTimeOut(const aclrtCntNotify inCntNotify, const aclrtStream streamPtr, HrtCntNotifyWaitMode
                    mode, u32 value, u32 timeout, bool isClear)
{
    aclrtCntNotifyWaitInfo waitInfo{};
    waitInfo.mode    = HRT_CNT_NOTIFY_WAIT_MODE_MAP.at(mode);
    waitInfo.value   = value;
    waitInfo.isClear = isClear;
    waitInfo.timeout = timeout;
    aclError ret    = aclrtCntNotifyWaitWithTimeout(inCntNotify, streamPtr, &waitInfo);
    HCCL_INFO("Call aclrtCntNotifyWaitWithTimeout, return value[%d], inCntNotify[%p]", ret, inCntNotify);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("Call rtCntNotifyWaitWithTimeout failed");
        THROW<RuntimeApiException>(msg);
    }
}

aclrtNotify HrtNotifyCreateWithFlag(u32 devId, u32 flag)
{
    aclrtNotify ptr = nullptr;
    aclError  ret = aclrtCreateNotify(&ptr, flag);
    HCCL_INFO("Call HrtNotifyCreateWithFlag, return value[%d], flag[%u] devid[%u]", ret, flag, devId);
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call rtNotifyCreateWithFlag failed, with ret[%d]", ret));
    }
    return ptr;
}

RtNotify_t HrtIpcOpenNotifyWithFlag(const char_t *name, uint32_t flags)
{
    RtNotify_t ptr = nullptr;
    aclError ret = aclrtNotifyImportByKey(&ptr, name, static_cast<uint64_t>(flags));
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call rtIpcOpenNotifyWithFlag failed, with ret[%d].", ret));
    }
    return ptr;
}

// 兜底extern形式
void HrtAicpuLaunchKernelWithHostArgs(aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream,
                                      aclrtLaunchKernelCfg *cfg, void *hostArgs, size_t argsSize,
                                      aclrtPlaceHolderInfo *placeHolderArray, size_t placeHolderNum)
{
    rtError_t ret = aclrtLaunchKernelWithHostArgs(funcHandle, numBlocks, stream, cfg, hostArgs, argsSize,
                                                  placeHolderArray, placeHolderNum);
    if (ret != RT_ERROR_NONE) {
        THROW<RuntimeApiException>(StringFormat("Call aclrtLaunchKernelWithHostArgs failed, with ret[%d]", ret));
    }
}

void HrtRegTaskFailCallbackByModule(aclrtExceptionInfoCallback callback)
{
    aclError ret = aclrtSetExceptionInfoCallback(callback);
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call aclrtSetExceptionInfoCallback failed, with ret[%d]", ret));
    }
}

u32 HrtStreamGetSqId(const aclrtStream ptr)
{
    u32       sqId;
    rtError_t ret = rtStreamGetSqid(ptr, &sqId);
    if (ret != RT_ERROR_NONE) {
        THROW<RuntimeApiException>(StringFormat("Call rtStreamGetSqid failed, with ret[%d]", ret));
    }
    return sqId;
}

u32 HrtStreamGetCqId(const aclrtStream ptr)
{
    u32 cqId;
    u32 logicCqId;
    rtError_t ret = rtStreamGetCqid(ptr, &cqId, &logicCqId);
    if (ret != RT_ERROR_NONE) {
        THROW<RuntimeApiException>(StringFormat("Call rtStreamGetCqid failed, with ret[%d]", ret));
    }
    return logicCqId;
}

void HrtCcuLaunch(rtCcuTaskInfo_t &taskInfo, aclrtStream const streamPtr)
{
    auto ret = rtCCULaunch(&taskInfo, streamPtr);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtCCULaunch failed.");
        THROW<RuntimeApiException>(msg);
    }
}

void HrtUbDevQueryInfo(rtUbDevQueryCmd cmd, void *devInfo)
{
    auto ret = rtUbDevQueryInfo(cmd, devInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtUbDevQueryInfo failed.");
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
    rtMemUbTokenInfo info;
    info.va   = addr;
    info.size = size;
    auto ret  = rtUbDevQueryInfo(QUERY_PROCESS_TOKEN, &info);
    if (ret != RT_ERROR_NONE) {
        HCCL_WARNING("query(va=0x%llx, size=0x%llx) token failed, ret=%d", addr, size, ret);
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
    auto ret            = rtGetDevResAddress(&resInfo, &addrInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtGetDevResAddress failed.");
        THROW<RuntimeApiException>(msg);
    }
    HrtDevResAddrInfo devResAddrInfo;
    devResAddrInfo.address = addr;
    devResAddrInfo.len     = len;
    return devResAddrInfo;
}
void HrtReleaseDevResAddress(const HrtDevResInfo &devResInfo)
{
    rtDevResInfo resInfo;
    resInfo.dieId    = devResInfo.dieId;
    resInfo.procType = HRT_DEV_RES_PROC_TYPE_MAP.at(devResInfo.procType);
    resInfo.resType  = HRT_DEV_RES_TYPE_MAP.at(devResInfo.resType);
    resInfo.flag     = devResInfo.flag;
    resInfo.resId    = devResInfo.resId;

    rtError_t ret = rtReleaseDevResAddress(&resInfo);
    if (ret != RT_ERROR_NONE) {
        string msg = StringFormat("Call rtReleaseDevResAddress failed.");
        THROW<RuntimeApiException>(msg);
    }
}

aclrtEvent HrtEventCreateWithFlag(u32 flag)
{
    aclrtEvent ptr = nullptr;
    aclError ret = aclrtCreateEventWithFlag(&ptr, flag);
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call rtEventCreateWithFlag failed, with ret[%d]", ret));
    }
    return ptr;
}

void HrtEventDestroy(RtEvent_t eventPtr)
{
    aclError ret = aclrtDestroyEvent(eventPtr);
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call aclrtDestroyEvent failed, with ret[%d]", ret));
    }
}

void HrtEventRecord(RtEvent_t eventPtr, aclrtStream streamPtr)
{
    aclError ret = aclrtRecordEvent(eventPtr, streamPtr);
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call aclrtRecordEvent failed, with ret[%d]", ret));
    }
}

const std::map<aclrtEventWaitStatus, HrtEventStatus> HRT_EVENT_STATUS_MAP{
    {ACL_EVENT_WAIT_STATUS_NOT_READY, HrtEventStatus::EVENT_INIT},
    {ACL_EVENT_WAIT_STATUS_COMPLETE, HrtEventStatus::EVENT_RECORDED},
};

HrtEventStatus HrtEventQueryStatus(RtEvent_t eventPtr)
{
    aclrtEventWaitStatus status = ACL_EVENT_WAIT_STATUS_NOT_READY;
    aclError ret = aclrtQueryEventWaitStatus(eventPtr, &status);
    if (ret != ACL_SUCCESS) {
        THROW<RuntimeApiException>(StringFormat("Call aclrtQueryEventWaitStatus failed, with ret[%d]", ret));
    }
    if (HRT_EVENT_STATUS_MAP.find(status) == HRT_EVENT_STATUS_MAP.end()) {
        THROW<InvalidParamsException>(
            StringFormat("event status[%u] not in HRT_EVENT_STATUS_MAP", static_cast<u32>(status)));
    }
    return HRT_EVENT_STATUS_MAP.at(status);
}

void HrtWriteValue(u64 addr, u32 piVal, const aclrtStream streamPtr)
{
    THROW<NotSupportException>(StringFormat("Unsupported rtWriteValue"));
}

void HrtDeviceAbortRegCallBack(aclrtDeviceTaskAbortCallback callback, void *args)
{
    aclError ret = aclrtSetDeviceTaskAbortCallback("HCCL", callback, args);
    if (ret != ACL_SUCCESS) {
        string msg = StringFormat("call rtSetTaskAbortCallBack failed, ret=[%d]", ret);
        THROW<RuntimeApiException>(msg);
    }
}

HcclResult HrtThreadExchangeCaptureMode(aclmdlRICaptureMode *mode)
{
    aclError ret = aclmdlRICaptureThreadExchangeMode(mode);
    if (ret == ACL_ERROR_RT_FEATURE_NOT_SUPPORT) {
        HCCL_WARNING("[HrtThreadExchangeCaptureMode]rtThreadExchangeCaptureMode not support!");
        return HCCL_E_NOT_SUPPORT;
    } else {
        CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[HrtThreadExchangeCaptureMode]rtThreadExchangeCaptureMode "
            "failed mode:%d, return value[%d].", *mode, ret), HCCL_E_RUNTIME);
    }
    return HCCL_SUCCESS;
}

HcclResult HrtMemPrefetchToDevice(void *devPtr, uint64_t len)
{
    CHK_PRT_RET(aclrtMemP2PMap == nullptr, HCCL_ERROR("aclrtMemP2PMap is nullptr, "
            "Does not support this interface."), HCCL_E_RUNTIME);
	aclError ret = aclrtMemP2PMap(devPtr, static_cast<size_t>(len), HrtGetDevice(), 0);
    HCCL_INFO("Call [HrtMemPrefetchToDevice]aclrtMemP2PMap ret = %d", ret);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("aclrtMemP2PMap fail ret = %d", ret);
        return HCCL_E_RUNTIME;
    }
    return HCCL_SUCCESS;
}
} // namespace Hccl