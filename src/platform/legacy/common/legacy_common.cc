/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "legacy_common.h"
#include "legacy_log.h"
#include "ascend_hal.h"
#include "mmpa_api.h"

using namespace std;

static std::unordered_map<s32, s64> g_devChipIdMap; // 记录 devLogID 和 chipID 的关系，避免重复查询
static thread_local u64 g_debugConfig = UINT64_MAX;

const char *GetSocVer()
{
    return aclrtGetSocName();
}

HcclResult GetDeviceType(LegacyDevType &deviceType)
{
    const char *socName = GetSocVer();
    CHK_PRT_RET(socName == nullptr, HCCL_ERROR("[GetDeviceType]get soc name is nullptr"), HCCL_E_RUNTIME);
    std::string targetChipVerStr(socName);
    HCCL_INFO("get soc name[%s] [%s]", socName, targetChipVerStr.c_str());
    if (targetChipVerStr.find("Ascend950") != std::string::npos) {
        deviceType = LegacyDevType::DEV_TYPE_910_95;
        return HCCL_SUCCESS;
    }
    auto iter = LEGACY_SOC_VER_CONVERT.find(targetChipVerStr);
    if (iter == LEGACY_SOC_VER_CONVERT.end()) {
        HCCL_ERROR("[Get][DeviceType]errNo[0x%016llx] rtGetSocVersion get illegal chipver, chip_ver[%s].", \
            HCCL_ERROR_CODE(HCCL_E_RUNTIME), socName);
        return HCCL_E_RUNTIME;
    }
    deviceType = iter->second;

    return HCCL_SUCCESS;
}

HcclResult Malloc(void **devPtr, u64 size)
{
    CHK_PTR_NULL(devPtr);
    CHK_PRT_RET((size == 0), HCCL_ERROR("Malloc size[%llu Byte] should be greater than 0.", size), HCCL_E_PARA);
    LegacyDevType devType;
    aclrtMallocConfig mallocConfig;
    aclrtMallocAttribute attriBute;
    s32 mallocPolicy = 0;
    CHK_RET(GetDeviceType(devType));
    if (devType == LegacyDevType::DEV_TYPE_310P3) {
        mallocPolicy = static_cast<int>(ACL_MEM_TYPE_LOW_BAND_WIDTH) | static_cast<int>(ACL_MEM_MALLOC_NORMAL_ONLY_P2P);
    } else if (devType == LegacyDevType::DEV_TYPE_310P1) {
        mallocPolicy = static_cast<int>(ACL_MEM_TYPE_LOW_BAND_WIDTH);
    } else {
        mallocPolicy = static_cast<int>(ACL_MEM_TYPE_HIGH_BAND_WIDTH) | static_cast<int>(ACL_MEM_MALLOC_HUGE_FIRST);
    }
    attriBute.attr = ACL_RT_MEM_ATTR_MODULE_ID;
    attriBute.value.moduleId = HCCL;
    mallocConfig.attrs = &attriBute;
    mallocConfig.numAttrs = 1;
    aclError ret = aclrtMallocWithCfg(devPtr, size, static_cast<aclrtMemMallocPolicy>(mallocPolicy), &mallocConfig);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[Malloc][Mem]errNo[0x%016llx] aclrtMallocWithCfg failed, "\
        "return[%d], para: devPtrAddr[%p], size[%llu Byte].", HCCL_ERROR_CODE(HCCL_E_RUNTIME), ret, *devPtr, size),
        HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult Free(void *devPtr)
{
    CHK_PTR_NULL(devPtr);
    aclError ret = aclrtFree(devPtr);
    CHK_PRT_RET((ret != ACL_SUCCESS), HCCL_ERROR("[Free][Mem]errNo[0x%016llx] aclrtFree failed, "\
        "return[%d], para: devPtrAddr[%p].", HCCL_ERROR_CODE(HCCL_E_RUNTIME), ret, devPtr), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult TbeMemcpy(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtMemcpyKind kind)
{
    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    aclmdlRICaptureMode mode = ACL_MODEL_RI_CAPTURE_MODE_RELAXED;
    aclError ret = aclmdlRICaptureThreadExchangeMode(&mode);
    CHK_PRT_CONT(ret != ACL_SUCCESS && ret != ACL_ERROR_RT_FEATURE_NOT_SUPPORT,
        HCCL_ERROR("[TbeMemcpy] aclmdlRICaptureThreadExchangeMode return [%d]", ret));
    ret = aclrtMemcpy(dst, destMax, src, count, kind);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtMemcpy] rt data memcpy host to device failed,ret[%d]"
        "size[%llu Byte] src[%p] kind[%u] dst[%p] count[%llu]", ret, destMax, src, kind, dst, count), HCCL_E_RUNTIME);
    ret = aclmdlRICaptureThreadExchangeMode(&mode);
    CHK_PRT_CONT(ret != ACL_SUCCESS && ret != ACL_ERROR_RT_FEATURE_NOT_SUPPORT,
        HCCL_ERROR("[TbeMemcpy] aclmdlRICaptureThreadExchangeMode return [%d]", ret));
    return HCCL_SUCCESS;
}

HcclResult GetDevice(s32 *deviceLogicId)
{
    CHK_PTR_NULL(deviceLogicId);
    aclError ret = aclrtGetDevice(deviceLogicId);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[Get][Device]rtGet device fail, "\
        "please make sure that device is set. return[%d], para:deviceLogicId[%d]",
        ret, *deviceLogicId), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult GetDeviceInfo(u32 deviceId, aclrtDevAttr attr, int64_t *value)
{
    CHK_PTR_NULL(value);
    aclError ret = aclrtGetDeviceInfo(deviceId, attr, value);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtGetDeviceInfo]rt get device info failed.ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult BinaryGetFunction(const aclrtBinHandle binHandle, const char *kernelName,
    aclrtFuncHandle *funcHandle)
{
    CHK_PTR_NULL(binHandle);
    CHK_PTR_NULL(kernelName);
    CHK_PTR_NULL(funcHandle);
    aclError ret = aclrtBinaryGetFunction(binHandle, kernelName, funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtBinaryGetFunction]binary get function failed,ret[%d]",
        ret), HCCL_E_RUNTIME);
    CHK_PRT_RET(*funcHandle == nullptr, HCCL_ERROR("[aclrtBinaryGetFunction]funcHandle is nullptr[%p]",
        *funcHandle), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult KernelArgsInit(aclrtFuncHandle funcHandle, aclrtArgsHandle *argsHandle)
 
{
    CHK_PTR_NULL(funcHandle);
    aclError ret = aclrtKernelArgsInit(funcHandle, argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsInit]kernel args init failed,ret[%d]",
        ret), HCCL_E_RUNTIME);
    CHK_PRT_RET(argsHandle == nullptr, HCCL_ERROR("[aclrtKernelArgsInit]argsHandle is nullptr[%p]",
        argsHandle), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult KernelArgsFinalize(aclrtArgsHandle argsHandle)
{
    CHK_PTR_NULL(argsHandle);
    aclError ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsFinalize]kernel args finalize failed,ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult KernelArgsAppend(aclrtArgsHandle argsHandle, void *param, size_t paramSize,
    aclrtParamHandle *paramHandle)
{
    CHK_PTR_NULL(argsHandle);
    CHK_PTR_NULL(param);
    aclError ret = aclrtKernelArgsAppend(argsHandle, param, paramSize, paramHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsAppend]kernel args append failed,ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult GetFunctionAddr(aclrtFuncHandle funcHandle, void **aicAddr, void **aivAddr)
{
    CHK_PTR_NULL(funcHandle);
    aclError ret = aclrtGetFunctionAddr(funcHandle, aicAddr, aivAddr);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtGetFunctionAddr]get function Addr failed.ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult BinaryUnLoad(aclrtBinHandle binHandle)
{
    CHK_PTR_NULL(binHandle);
    aclError ret = aclrtBinaryUnLoad(binHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtBinaryUnLoad]rt binary unload failed.ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult BinaryLoadFromData(const void *data, size_t length,
    const aclrtBinaryLoadOptions *options, aclrtBinHandle *binHandle)
{
    CHK_PTR_NULL(data);
    CHK_PTR_NULL(options);
    CHK_PTR_NULL(binHandle);
    aclError ret = aclrtBinaryLoadFromData(data, length, options, binHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[BinaryLoadFromData]register opBinary failed.ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult LaunchKernelWithConfig(aclrtFuncHandle funcHandle, uint32_t numBlocks,
    aclrtStream stream, aclrtLaunchKernelCfg *cfg,
    aclrtArgsHandle argsHandle, void *reserve)
{
    CHK_PTR_NULL(funcHandle);
    CHK_PTR_NULL(stream);
    CHK_PTR_NULL(cfg);
    CHK_PTR_NULL(argsHandle);
    aclError ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, stream, cfg, argsHandle, reserve);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[LaunchKernelWithConfig]execute kernel launch failed"), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult GetDevArgsAddr(rtStream_t stream, rtArgsEx_t *argsInfo, void **devArgsAddr,
    void **argsHandle)
{
    CHK_PTR_NULL(stream);
    CHK_PTR_NULL(argsInfo);
    CHK_PTR_NULL(devArgsAddr);
    CHK_PTR_NULL(argsHandle);
    rtError_t ret = rtGetDevArgsAddr(stream, argsInfo, devArgsAddr, argsHandle);
    CHK_PRT_RET(ret != RT_ERROR_NONE, HCCL_ERROR("[GetDevArgsAddr]rtKernel Get Addr failed.ret[%d]",
        ret), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult NotifyGetAddr(void *signal, u64 *notifyAddr)
{
    uint64_t* const addr = reinterpret_cast<uint64_t*>(notifyAddr);
    rtError_t ret = rtGetNotifyAddress(signal, addr);
    HCCL_DEBUG("Call rtGetNotifyAddress, signal[%p], notifyAddr[%016llx]", signal, *notifyAddr);
    CHK_PRT_RET(ret != RT_ERROR_NONE, HCCL_ERROR("[rtGetNotifyAddress]rt get notify address failed."), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult GetNotifyID(void *signal, u32 *notifyID)
{
    CHK_PTR_NULL(signal);
    CHK_PTR_NULL(notifyID);
    aclError ret = aclrtGetNotifyId(signal, notifyID);
    HCCL_INFO("Call aclrtGetNotifyId signal:%p, notifyID:%u.", signal, *notifyID);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtGetNotifyId]rt get notify id failed."), HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult FftsPlusTaskLaunch(rtFftsPlusTaskInfo_t *fftsPlusTaskInfo, rtStream_t stm)
{
    CHK_PTR_NULL(fftsPlusTaskInfo);
    CHK_PTR_NULL(stm);
    uintptr_t input[2];    // fftsplus task下发时需要输入2个参数，0：task info，1：stream handle
    input[0] = reinterpret_cast<uintptr_t>(fftsPlusTaskInfo);
    input[1] = reinterpret_cast<uintptr_t>(stm);

    rtError_t ret = rtGeneralCtrl(input, 2, RT_GNL_CTRL_TYPE_FFTS_PLUS); // 2: fftsplus task有2个input

    CHK_PRT_RET(ret != RT_ERROR_NONE, HCCL_ERROR("[FftsPlusTaskLaunchWithFlag]rt ffts launch failed, ret[%d]", ret),
        HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult GetRdmaDoorbellAddr(u32 dbIndex, u64 &dbAddr)
{
    s32 devLogID = 0;
    int64_t chipID = 0;
    static std::mutex devChipIdMapSpinMutex; // devLogID 和 chipID 关系表的读写互斥锁

    CHK_RET(GetDevice(&devLogID));

    // 读写表前先加锁
    std::unique_lock<std::mutex> lockDevChipIdMap(devChipIdMapSpinMutex);

    if (g_devChipIdMap.find(devLogID) != g_devChipIdMap.end()) {
        // 若已有记录，则直接获取chipID
        chipID = g_devChipIdMap[devLogID];
    } else {
        CHK_RET(GetDeviceInfo(devLogID, ACL_DEV_ATTR_PHY_CHIP_ID, &chipID));
        g_devChipIdMap[devLogID] = chipID;
    }
    // 解锁
    lockDevChipIdMap.unlock();

    u64 roceBaseAddr;
    u64 roceVfDbCfg0Reg;
    u64 chipAddrOffset;
    u64 dieAddrOffset;
    u32 dbDieIdMask;
    u32 dbDieIdShift;

    LegacyDevType deviceType;
    CHK_RET(GetDeviceType(deviceType));
    if (deviceType == LegacyDevType::DEV_TYPE_910_93) {
        HCCL_DEBUG("[GetRdmaDoorbellAddr] The roceBaseAddr and dieAddrOffset has changed, when deviceType is 910_93.");
        // 910_93 HCCS_SW 组网
        roceBaseAddr = 0x202000000000ULL;
        roceVfDbCfg0Reg = 0x230ULL;
        chipAddrOffset = 0x20000000000ULL;
        dieAddrOffset = 0x10000000000ULL;
        dbDieIdMask = 0x00ff0000;
        dbDieIdShift = 16; // 16 is dbDieIdShift
    } else {
        roceBaseAddr = 0x2000000000ULL;
        roceVfDbCfg0Reg = 0x230ULL;
        chipAddrOffset = 0x80000000000ULL;
        dieAddrOffset = 0x10000000000ULL;
        dbDieIdMask = 0x00ff0000;
        dbDieIdShift = 16; // 16 is dbDieIdShift
    }

    dbAddr = roceBaseAddr + roceVfDbCfg0Reg + chipAddrOffset * chipID +
        dieAddrOffset * ((dbIndex & dbDieIdMask) >> dbDieIdShift);
    return HCCL_SUCCESS;
}

HcclResult LegacyParseDebugConfig()
{
    g_debugConfig = 0;
    char* env = nullptr; // 环境变量值
    MM_SYS_GET_ENV(MM_ENV_HCCL_DEBUG_CONFIG, env);
    if (env == nullptr) {
        HCCL_INFO("HCCL_DEBUG_CONFIG is not set, debugConfig set by default to 0x%llx", g_debugConfig);
        return HCCL_SUCCESS;
    }

    bool invert = (env[0] == '^');
    g_debugConfig = invert ? ~0ULL : 0ULL; // 第一个字符是'^', 使用取反模式，用户配置的项关闭，未配置的项打开
    char* configValue = (env[0] == '^') ? env + 1 : env; // 去掉'^'符号
    char* configDup = strdup(configValue); // 需要使用strdup避免修改字符串常量
    CHK_PTR_NULL(configDup);

    char* left = nullptr;
    char* subConfig = strtok_r(configDup, ",", &left); // 按逗号分割
    while (subConfig != nullptr) {
        u64 mask = 0;
        if (strcasecmp(subConfig, "ALG") == 0) {
            mask = PLF_ALG;
        } else if (strcasecmp(subConfig, "TASK") == 0) {
            mask = PLF_TASK;
        } else if (strcasecmp(subConfig, "RESOURCE") == 0) {
            mask = PLF_RES;
        } else if (strcasecmp(subConfig, "AIV_OPS_EXC") == 0) {
            mask = PLF_AIV_OPS_EXC;
        } else {
            HCCL_ERROR("HCCL_DEBUG_CONFIG:%s is invalid, subConfig:%s is not supported", env, subConfig);
            free(configDup);
            return HCCL_E_PARA;
        }
        g_debugConfig = invert ? (g_debugConfig & (~mask)) : (g_debugConfig | mask);
        subConfig = strtok_r(nullptr, ",", &left);
    }
    free(configDup);
    HCCL_INFO("HCCL_DEBUG_CONFIG[%s], set debugConfig[0x%llx]", env, g_debugConfig);
    return HCCL_SUCCESS;
}

const u64& GetExterInputDebugConfigLegacy()
{
    return g_debugConfig;
}

std::string LegacyGetDataTypeEnumStr(HcclDataType dataType)
{
    auto iter = LEGACY_DATA_TYPE_STR_MAP.find(dataType);
    if (iter == LEGACY_DATA_TYPE_STR_MAP.end()) {
        return "HcclDataType(" + std::to_string(dataType) + ")";
    } else {
        return iter->second;
    }
}

std::string LegacyGetReduceOpEnumStr(HcclReduceOp reduceOp)
{
    auto iter = LEGACY_REDUCE_OP_STR_MAP.find(reduceOp);
    if (iter == LEGACY_REDUCE_OP_STR_MAP.end()) {
        return "HcclReduceOp(" + std::to_string(reduceOp) + ")";
    } else {
        return iter->second;
    }
}