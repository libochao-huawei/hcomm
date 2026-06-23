/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <atomic>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "acl/acl_base.h"
#include "acl/acl_rt.h"
#include "dtype_common.h"
#include "sim_common_macro.h"
#include "sim_log.h"
#include "runtime/base.h"
#include "db_sim_runner_common.h"
#include "db_sim_runner_ops.h"

// current host id
uint64_t g_host_id;
extern uint64_t g_cur_server_key;

extern pid_t g_devicePid;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

int rtModelFake = 0;
aclError aclmdlRICaptureGetInfo(aclrtStream stream, aclmdlRICaptureStatus *status, aclmdlRI *modelRI)
{
    (void) stream;
    (void) status;
    *modelRI = &rtModelFake;
    return ACL_SUCCESS;
}

HcclResult hrtGetDeviceIndexByPhyId(uint32_t devicePhyId, uint32_t &deviceLogicId)
{
    try {
        auto ret = RunnerDB::GetOneByPred<sim::Device>([devicePhyId](const sim::Device& d) {
            return d.server_id == g_cur_server_key && d.physical_id  == (uint32_t)devicePhyId;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[hrtGetDeviceIndexByPhyId] cannot find device by physical id {:d}", devicePhyId);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        deviceLogicId = ret.first.logic_id;
        return HCCL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[hrtGetDeviceIndexByPhyId] exception: {}", e.what());
        return HcclResult::HCCL_E_INTERNAL;
    }
}

aclError aclrtSetDevice(int32_t deviceId)
{
    try {
        HCCL_VM_DEBUG("[aclrtSetDevice] start set device {:d}", deviceId);
        uint32_t rankId;
        if (!sim::GetRankIdByMPI(rankId)) {
            HCCL_VM_ERROR("[aclrtSetDevice] cannot get rank id by MPI.");
            return ACL_ERROR_INVALID_PARAM;
        }

        sim::Device device{};
        auto devRet = sim::GetDeviceByRankId(rankId, device);
        if (devRet != ACL_SUCCESS) {
            HCCL_VM_ERROR("[aclrtSetDevice] cannot find device by rank id {:d}", rankId);
            return devRet;
        }
        HCCL_VM_DEBUG("[aclrtSetDevice] device logic id: {:d}, rank id: {:d}, device key: {:d}", device.logic_id, rankId, device.id);

        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(device.server_id, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto curRunnerId = runner.id;

        uint64_t currCtxId = 0;
        uint64_t deviceKey = device.id;
        auto ret = RunnerDB::GetOneByPred<sim::Context>([deviceKey](const sim::Context &ctx) {
            return ctx.device_id == deviceKey && ctx.is_default == 1;
        });
        if (!ret.second) {
            sim::Context context{};
            context.device_id = device.id;
            context.run_id = curRunnerId;
            context.is_default = 1;
            context.ref_cnt = 1;
            currCtxId = RunnerDB::Add<sim::Context>(context);

            sim::Stream stream{};
            stream.ctx_id = currCtxId;
            stream.activated = 1;
            stream.is_primary_default = 1;
            RunnerDB::Add<sim::Stream>(stream);
        } else {
            currCtxId = ret.first.id;
            RunnerDB::Update<sim::Context>(currCtxId, [](sim::Context &ctx) { ctx.ref_cnt++;});
            currCtxId = ret.first.id;
        }

        sim::SetCurrCtxTls(currCtxId);
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtSetDevice] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtResetDevice(int32_t deviceId)
{
    try {
        HCCL_VM_INFO("[aclrtResetDevice] Enter. deviceId= {:d}", deviceId);
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto curCtxId = runner.current_ctx_id;
        if (curCtxId == 0) {
            return ACL_SUCCESS;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(curCtxId);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtResetDevice] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto streamRet = RunnerDB::GetOneByPred<sim::Stream>([curCtxId](const sim::Stream& stream) {
            return stream.ctx_id == curCtxId && stream.is_primary_default == 1;
        });
        if (!streamRet.second) {
            HCCL_VM_ERROR("[aclrtResetDevice] can not find stream of context {:d}", curCtxId);
            return ACL_ERROR_INVALID_PARAM;
        }

        if (currCtx->ref_cnt > 1) {
            RunnerDB::Update<sim::Context>(curCtxId, [](sim::Context &ctx) { ctx.ref_cnt--;});
        } else {
            RunnerDB::Delete<sim::Stream>(streamRet.first.id);
            RunnerDB::Delete<sim::Context>(curCtxId);
        }

        curCtxId = 0;
        sim::SetCurrCtxTls(curCtxId);
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtResetDevice] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtResetDeviceForce(int32_t deviceId)
{
    try {
        HCCL_VM_DEBUG("[aclrtResetDeviceForce] can not find stream of deviceId {:d}", deviceId);
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto curCtxId = runner.current_ctx_id;
        if (curCtxId == 0) {
            return ACL_SUCCESS;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(curCtxId);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtResetDevice] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto streamRet = RunnerDB::GetOneByPred<sim::Stream>([curCtxId](const sim::Stream& stream) {
            return stream.ctx_id == curCtxId && stream.is_primary_default == 1;
        });
        if (!streamRet.second) {
            HCCL_VM_ERROR("[aclrtResetDevice] can not find stream of context {:d}", curCtxId);
            return ACL_ERROR_INVALID_PARAM;
        }

        RunnerDB::Delete<sim::Stream>(streamRet.first.id);
        RunnerDB::Delete<sim::Context>(curCtxId);
        curCtxId = 0;
        sim::SetCurrCtxTls(curCtxId);
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtResetDeviceForce] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtGetDevice(int32_t* device)
{
    try {
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtGetDevice] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto devRes = RunnerDB::GetById<sim::Device>(currCtx->device_id);
        if (!devRes.has_value()) {
            HCCL_VM_ERROR("[aclrtGetDevice] can not find current device id:{:d}", currCtx->device_id);
            return ACL_ERROR_INVALID_PARAM;
        }
        *device = devRes->logic_id;
        HCCL_VM_DEBUG("[aclstub][aclrtGetDevice]device: {:d}", devRes->logic_id);
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetDevice] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtGetRunMode(aclrtRunMode *runMode)
{
    *runMode = ACL_DEVICE;
    return ACL_SUCCESS;
}

aclError aclrtSetTsDevice(aclrtTsId tsId)
{
    try {
        if (tsId == ACL_TS_ID_AICORE) {
            sim::SetTsDevice(tsId);
        }

        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtSetTsDevice] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto devRes = RunnerDB::GetById<sim::Device>(currCtx->device_id);
        if (!devRes.has_value()) {
            HCCL_VM_ERROR("[aclrtSetTsDevice] can not find current device id:{:d}", currCtx->device_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        uint32_t vectoCount = sim::GetVectorCoreCount(devRes->logic_id);
        if (vectoCount != 0) {
            sim::SetTsDevice(tsId);
        }

        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtSetTsDevice] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtGetDeviceCount(uint32_t *count)
{
    try {
        auto devs = RunnerDB::GetByPred<sim::Device>([](const sim::Device& d) {
            return d.status == 0;
        });

        if (devs.empty()) {
            HCCL_VM_ERROR("[aclstub][aclrtGetDeviceCount]devices failed");
            return ACL_ERROR_INVALID_PARAM;
        }
        *count = devs.size();
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetDeviceCount] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtGetDeviceUtilizationRate(int32_t deviceId, aclrtUtilizationInfo *utilizationInfo)
{
    try {
        auto ret = RunnerDB::GetOneByPred<sim::Device>([deviceId](const sim::Device &d) {
            return d.logic_id == deviceId;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[aclrtGetDeviceUtilizationRate] cannot find device by physical id {:d}", deviceId);
            return 0;
        }

        utilizationInfo->cubeUtilization    = 20;
        utilizationInfo->vectorUtilization = 20;
        utilizationInfo->aicpuUtilization  = 20;
        utilizationInfo->memoryUtilization  = 20;
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetDeviceUtilizationRate] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtQueryDeviceStatus(int32_t deviceId, aclrtDeviceStatus *deviceStatus)
{
    try {
        auto ret = RunnerDB::GetOneByPred<sim::Device>([deviceId](const sim::Device& d) {
            return d.logic_id  == (uint32_t)deviceId;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[aclrtQueryDeviceStatus] cannot find device by logic id {:d}", deviceId);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        *deviceStatus = (aclrtDeviceStatus)ret.first.status;
        return HcclVmResult::HCCL_SIM_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtQueryDeviceStatus] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

const char *aclrtGetSocName()
{
    try {
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return "invalid param";
        }
        auto hostId = runner.host_id;
        if (hostId == 0) {
            HCCL_VM_ERROR("[aclrtGetSocName] wrong host id: {:d}", hostId);
            return "invalid param";
        }

        auto host = RunnerDB::GetOneByPred<sim::Host>([hostId](const sim::Host& h) {
            return h.id == hostId;
        });
        if (!host.second) {
            HCCL_VM_ERROR("[aclrtResetDevice] can not find host by key {:d}", hostId);
            return "";
        }

        auto serverId = host.first.server_id;
        auto devRes = RunnerDB::GetOneByPred<sim::Device>([serverId](const sim::Device& d) {
            return d.server_id == serverId;
        });
        if (!devRes.second) {
            HCCL_VM_ERROR("[aclrtResetDevice] can not find device by server id {:d}", serverId);
            return "";
        }

        thread_local static char SocName[128] = {0};
        memcpy(SocName, devRes.first.soc_version, strlen(devRes.first.soc_version));
        SocName[strlen(devRes.first.soc_version)] = '\0';
        HCCL_VM_DEBUG("[aclstub][aclrtGetSocName]device: {}", devRes.first.soc_version);
        return SocName;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetSocName] exception: {}", e.what());
        static thread_local char s_errMsg[] = "";
        return s_errMsg;
    }
}

aclError aclrtSetDeviceSatMode(aclrtFloatOverflowMode mode)
{
    try {
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtGetDevice] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto curDevId = currCtx->device_id;
        RunnerDB::Update<sim::Device>(curDevId, [curDevId, mode](sim::Device &dev) { dev.overflow_mode = mode; });
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtSetDeviceSatMode] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtGetDeviceSatMode(aclrtFloatOverflowMode *mode)
{
    try {
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtGetDeviceSatMode] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev = RunnerDB::GetById<sim::Device>(currCtx->device_id);
        if (!dev.has_value()) {
            HCCL_VM_ERROR("[aclrtGetDeviceSatMode] can not find current device:{:d}", currCtx->device_id);
            return ACL_ERROR_INVALID_PARAM;
        }
        *mode = (aclrtFloatOverflowMode)dev->overflow_mode;
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetDeviceSatMode] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtDeviceCanAccessPeer(int32_t *canAccessPeer, int32_t deviceId, int32_t peerDeviceId)
{
    try {
        auto dev1 = RunnerDB::GetOneByPred<sim::Device>([deviceId](const sim::Device &d) {
            return d.logic_id == deviceId;
        });
        if (!dev1.second) {
            HCCL_VM_ERROR("[aclrtDeviceCanAccessPeer] cannot find device by logic id {:d}", deviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev2 = RunnerDB::GetOneByPred<sim::Device>([peerDeviceId](const sim::Device &d) {
            return d.logic_id == peerDeviceId;
        });
        if (!dev2.second) {
            HCCL_VM_ERROR("[aclrtDeviceCanAccessPeer] cannot find device by logic id {:d}", peerDeviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev1Id = dev1.first.id;
        auto dev2Id = dev2.first.id;

        auto ret = RunnerDB::GetOneByPred<sim::DeviceConnection>([dev1Id, dev2Id](const sim::DeviceConnection& devConn) {
            return devConn.src_dev_id  == dev1Id && devConn.dst_dev_id  == dev2Id;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[aclrtDeviceCanAccessPeer] get device connection failed srcdev:{:d} dstdev:{:d}", dev1Id, dev2Id);
            return HcclResult::HCCL_E_NOT_FOUND;
        }

        *canAccessPeer = (int32_t)ret.first.access_by_remote;
        return HcclVmResult::HCCL_SIM_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtDeviceCanAccessPeer] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtDeviceEnablePeerAccess(int32_t peerDeviceId, uint32_t flags)
{
    (void) flags;
    try {
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev1 = RunnerDB::GetById<sim::Device>(currCtx->device_id);
        if (!dev1.has_value()) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] can not find current device id:{:d}", currCtx->device_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        if (dev1->logic_id == peerDeviceId) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] invalid device id:{:d}", peerDeviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev2 = RunnerDB::GetOneByPred<sim::Device>([peerDeviceId](const sim::Device &d) {
            return d.logic_id == peerDeviceId;
        });
        if (!dev2.second) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] cannot find device by logic id {:d}", peerDeviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev1Id = dev1->id;
        auto dev2Id = dev2.first.id;
        auto ret = RunnerDB::GetOneByPred<sim::DeviceConnection>([dev1Id, dev2Id](const sim::DeviceConnection& devConn) {
            return devConn.src_dev_id  == dev1Id && devConn.dst_dev_id  == dev2Id;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[aclrtDeviceCanAccessPeer] get device connection failed srcdev:{:d} dstdev:{:d}", dev1Id, dev2Id);
            return HcclResult::HCCL_E_NOT_FOUND;
        }

        RunnerDB::Update<sim::DeviceConnection>(ret.first.id, [](sim::DeviceConnection &devConn) { devConn.access_by_remote = 1;});
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtDeviceDisablePeerAccess(int32_t peerDeviceId)
{
    try {
        sim::Runner runner{};
        if (!sim::GetCurrRunnerTls(0, runner)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
        if (!currCtx.has_value()) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] can not find current context:{:d}", runner.current_ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev1 = RunnerDB::GetById<sim::Device>(currCtx->device_id);
        if (!dev1.has_value()) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] can not find current device id:{:d}", currCtx->device_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        if (dev1->logic_id == peerDeviceId) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] invalid device id:{:d}", peerDeviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev2 = RunnerDB::GetOneByPred<sim::Device>([peerDeviceId](const sim::Device &d) {
            return d.logic_id == peerDeviceId;
        });
        if (!dev2.second) {
            HCCL_VM_ERROR("[aclrtDeviceEnablePeerAccess] cannot find device by logic id {:d}", peerDeviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev1Id = dev1->id;
        auto dev2Id = dev2.first.id;
        auto ret = RunnerDB::GetOneByPred<sim::DeviceConnection>([dev1Id, dev2Id](const sim::DeviceConnection& devConn) {
            return devConn.src_dev_id  == dev1Id && devConn.dst_dev_id  == dev2Id;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[aclrtDeviceCanAccessPeer] get device connection failed srcdev:{:d} dstdev:{:d}", dev1Id, dev2Id);
            return ACL_ERROR_INVALID_PARAM;
        }

        RunnerDB::Update<sim::DeviceConnection>(ret.first.id, [](sim::DeviceConnection &devConn) { devConn.access_by_remote = 0;});
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtDeviceDisablePeerAccess] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtGetOverflowStatus(void *outputAddr, size_t outputSize, aclrtStream stream)
{
    (void) outputSize;
    try {
        uint64_t streamIdx = (uint64_t)(uintptr_t)stream;
        auto stmRes = RunnerDB::GetById<sim::Stream>(streamIdx);
        if (!stmRes.has_value()) {
            HCCL_VM_ERROR("can not get stream:{:d}", streamIdx);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto ctxRes = RunnerDB::GetById<sim::Context>(stmRes->ctx_id);
        if (!ctxRes.has_value()) {
            HCCL_VM_ERROR("can not get context:{:d}", stmRes->ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto deviceIdx = ctxRes->device_id;
        auto devStatusRes = RunnerDB::GetOneByPred<sim::DeviceStatus>([deviceIdx](const sim::DeviceStatus& dev) {
            return dev.device_id  == deviceIdx;
        });
        if (!devStatusRes.second) {
            HCCL_VM_ERROR("[aclrtGetOverflowStatus] get device :{:d} failed", deviceIdx);
            return ACL_ERROR_INVALID_PARAM;
        }

        uint8_t* tmp = (uint8_t *)outputAddr;
        *tmp = devStatusRes.first.overflow_status;

        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetOverflowStatus] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtResetOverflowStatus(aclrtStream stream)
{
    try {
        uint64_t streamIdx = (uint64_t)(uintptr_t)stream;
        auto stmRes = RunnerDB::GetById<sim::Stream>(streamIdx);
        if (!stmRes.has_value()) {
            HCCL_VM_ERROR("can not get stream:{:d}", streamIdx);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto ctxRes = RunnerDB::GetById<sim::Context>(stmRes->ctx_id);
        if (!ctxRes.has_value()) {
            HCCL_VM_ERROR("can not get Context:{:d}", stmRes->ctx_id);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto deviceIdx = ctxRes->device_id;
        auto devStatusRes = RunnerDB::GetOneByPred<sim::DeviceStatus>(
            [deviceIdx](const sim::DeviceStatus &dev) { return dev.device_id == deviceIdx; });
        if (!devStatusRes.second) {
            HCCL_VM_ERROR("[aclrtResetOverflowStatus] get device :{:d} failed", deviceIdx);
            return ACL_ERROR_INVALID_PARAM;
        }
        RunnerDB::Update<sim::DeviceStatus>(devStatusRes.first.id, [](sim::DeviceStatus &devStatus) { devStatus.overflow_status = 0;});
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtResetOverflowStatus] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtSynchronizeDevice(void)
{
    return ACL_SUCCESS;
}

aclError aclrtSynchronizeDeviceWithTimeout(int32_t timeout)
{
    (void) timeout;
    return ACL_SUCCESS;
}

aclError aclrtGetDeviceInfo(uint32_t deviceId, aclrtDevAttr attr, int64_t *value)
{
    uint32_t count = 0;
    if (attr == ACL_DEV_ATTR_AICPU_CORE_NUM) {
        count = sim::GetAICpuCount(deviceId);
    } else if (attr == ACL_DEV_ATTR_AICORE_CORE_NUM) {
        count = sim::GetAICoreCount(deviceId);
    } else if (attr == ACL_DEV_ATTR_VECTOR_CORE_NUM) {
        count = sim::GetVectorCoreCount(deviceId);
    }
    *value = static_cast<int64_t>(count);
    return ACL_SUCCESS;
}

aclError aclrtDeviceGetStreamPriorityRange(int32_t *leastPriority, int32_t *greatestPriority)
{
    (void) leastPriority;
    (void) greatestPriority;
    return ACL_SUCCESS;
}

aclError aclrtGetDeviceCapability(int32_t deviceId, aclrtDevFeatureType devFeatureType, int32_t *value)
{
    (void) deviceId;
    (void) devFeatureType;
    (void) value;
    return ACL_SUCCESS;
}

aclError aclrtGetDevicesTopo(uint32_t deviceId, uint32_t otherDeviceId, uint64_t *value)
{
    try {
        auto dev1 = RunnerDB::GetOneByPred<sim::Device>([deviceId](const sim::Device &d) {
            return d.logic_id == deviceId;
        });
        if (!dev1.second) {
            HCCL_VM_ERROR("[aclrtGetDevicesTopo] cannot find device by logic id {:d}", deviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev2 = RunnerDB::GetOneByPred<sim::Device>([otherDeviceId](const sim::Device &d) {
            return d.logic_id == otherDeviceId;
        });
        if (!dev2.second) {
            HCCL_VM_ERROR("[aclrtGetDevicesTopo] cannot find device by logic id {:d}", otherDeviceId);
            return ACL_ERROR_INVALID_PARAM;
        }

        auto dev1Id = dev1.first.id;
        auto dev2Id = dev2.first.id;

        auto ret = RunnerDB::GetOneByPred<sim::DeviceConnection>([dev1Id, dev2Id](const sim::DeviceConnection& devConn) {
            return devConn.src_dev_id  == dev1Id && devConn.dst_dev_id  == dev2Id;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[aclrtGetDevicesTopo] get device connection failed srcdev:{:d} dstdev:{:d}", dev1Id, dev2Id);
            return HcclResult::HCCL_E_NOT_FOUND;
        }

        *value = (uint64_t)ret.first.link_type;
        return HcclVmResult::HCCL_SIM_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[aclrtGetDevicesTopo] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

aclError aclrtDevicePeerAccessStatus(int32_t deviceId, int32_t peerDeviceId, int32_t *status)
{
    return aclrtDeviceCanAccessPeer(status, deviceId, peerDeviceId);
}

aclError aclInit(const char *configPath)
{
    (void) configPath;
    HCCL_VM_DEBUG("[{0}] Success", __func__);
    return ACL_SUCCESS;
}

aclError aclFinalize()
{
    if (g_devicePid != 0) {
        kill(g_devicePid, SIGKILL);
    }
    HCCL_VM_DEBUG("[{0}] Success", __func__);
    return ACL_SUCCESS;
}

aclError aclrtGetPhyDevIdByLogicDevId(int32_t logicDevId, int32_t *const phyDevId)
{
    HCCL_VM_DEBUG("[aclstub][aclrtGetPhyDevIdByLogicDevId]Success");
    sim::Device device{};
    auto devRet = sim::GetDeviceByLogicId((uint32_t)logicDevId, device);
    if (devRet != ACL_SUCCESS) {
        return devRet;
    }

    *phyDevId = (int32_t)device.physical_id;
    HCCL_VM_DEBUG("[aclstub][aclrtGetPhyDevIdByLogicDevId] result. server: {:d}, logicId: {:d}, phyId: {:d} ",g_cur_server_key, logicDevId, *phyDevId);
    return ACL_SUCCESS;
}

aclError aclrtGetLogicDevIdByPhyDevId(const int32_t phyDevId, int32_t *const logicDevId)
{
    HCCL_VM_DEBUG("[aclstub][aclrtGetLogicDevIdByPhyDevId]Success");
    sim::Device device{};
    auto devRet = sim::GetDeviceByPhysicalId((uint32_t)phyDevId, device);
    if (devRet != ACL_SUCCESS) {
        return devRet;
    }

    *logicDevId = (int32_t)device.logic_id;
    return ACL_SUCCESS;
}

aclError aclrtSetDeviceTaskAbortCallback(const char *regName, aclrtDeviceTaskAbortCallback callback, void *args)
{
    (void) regName;
    (void) callback;
    (void) args;
    return ACL_SUCCESS;
}

rtError_t rtGetDevicePhyIdByIndex(uint32_t devIndex, uint32_t *phyId)
{
    sim::Device device{};
    auto devRet = sim::GetDeviceByLogicId((uint32_t)devIndex, device);
    if (devRet != ACL_SUCCESS) {
        return devRet;
    }
    *phyId = device.physical_id;
    return ACL_SUCCESS;
}

rtError_t rtGetPhyDeviceInfo(uint32_t phyId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    (void) phyId;
    (void) moduleType;
    (void) infoType;
    (void) val;
    return ACL_SUCCESS;
}

rtError_t rtGetDeviceIndexByPhyId(uint32_t phyId, uint32_t *devIndex)
{
    try {
        auto ret = RunnerDB::GetOneByPred<sim::Device>([phyId](const sim::Device& d) {
            return d.physical_id  == phyId;
        });
        if (!ret.second) {
            HCCL_VM_ERROR("[rtGetDeviceIndexByPhyId] cannot find device by physical id {:d}", phyId);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        *devIndex = ret.first.logic_id;
        return ACL_SUCCESS;
    } catch (const std::exception &e) {
        HCCL_VM_ERROR("[rtGetDeviceIndexByPhyId] exception: {}", e.what());
        return ACL_ERROR_INTERNAL_ERROR;
    }
}

rtError_t rtSetDevice(int32_t devId)
{
    return aclrtSetDevice(devId);
}

rtError_t rtGetPairPhyDevicesInfo(uint32_t devId, uint32_t otherDevId, int32_t infoType, int64_t *val)
{
    (void) devId;
    (void) otherDevId;
    (void) infoType;
    HCCL_VM_DEBUG("[aclstub][rtGetPairPhyDevicesInfo]Success");
    *val = 1;
    return ACL_SUCCESS;
}

rtError_t rtsGetLogicDevIdByPhyDevId(int32_t phyDevId, int32_t * const logicDevId)
{
    return aclrtGetLogicDevIdByPhyDevId(phyDevId, logicDevId);
}

struct rtDevResInfo;
rtError_t rtReleaseDevResAddress(rtDevResInfo * const resInfo)
{
    (void) resInfo;
    HCCL_VM_DEBUG("[aclstub][rtReleaseDevResAddress]Success");
    return ACL_SUCCESS;
}

#ifdef __cplusplus
}
#endif  // __cplusplus
