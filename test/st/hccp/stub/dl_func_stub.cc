/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string.h>
extern "C"{
#include "dl_hal_function.h"

drvError_t drvGetDevNum(unsigned int *numDev)
{
    return DRV_ERROR_NONE;
}

int drvGetLocalDevIdByHostDevId(unsigned int devId, unsigned int* chipId)
{
    return 0;
}

drvError_t drvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex)
{
    return DRV_ERROR_NONE;
}

int drvGetDevIdByLocalDevId(unsigned int localDevId, unsigned int *devId)
{
    return 0;
}

drvError_t drvDeviceGetPhyIdByIndex(unsigned int devIndex, unsigned int *phyId)
{
    return DRV_ERROR_NONE;
}

drvError_t halQueryDevPid(struct halQueryDevpidInfo info, pid_t *devPid)
{
    return DRV_ERROR_NONE;
}

drvError_t halMemBindSibling(int hostPid, int aicpuPid, unsigned int vfid, unsigned int devId,
    unsigned int flag)
{
    return DRV_ERROR_NONE;
}

drvError_t drvQueryProcessHostPid(int pid, unsigned int *chipId, unsigned int *vfid, unsigned int *hostPid,
    unsigned int *cpType)
{
    return DRV_ERROR_NONE;
}

drvError_t halMemGetInfoEx(unsigned int devId, unsigned int type, struct MemInfo *info)
{
    return DRV_ERROR_NONE;
}

int halGrpQuery(GroupQueryCmdType cmd, void *inBuff, unsigned int inLen, void *outBuff, unsigned int *outLen)
{
    return 0;
}

drvError_t halHdcGetSessionAttr(HDC_SESSION session, int attr, int *value)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcGetCapacity(struct drvHdcCapacity *capacity)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcClientCreate(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcClientDestroy(HDC_CLIENT client)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSessionConnect(int peerNode, int peerDevid, HDC_CLIENT client, HDC_SESSION *session)
{
    return DRV_ERROR_NONE;
}

hdcError_t halHdcSessionConnectEx(int peerNode, int peerDevid, int peerPid, HDC_CLIENT client,
    HDC_SESSION *pSession)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcServerCreate(int devid, int serviceType, HDC_SERVER *pServer)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcServerDestroy(HDC_SERVER server)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSessionAccept(HDC_SERVER server, HDC_SESSION *session)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSessionClose(HDC_SESSION session)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcFreeMsg(struct drvHdcMsg *msg)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcReuseMsg(struct drvHdcMsg *msg)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcAddMsgBuffer(struct drvHdcMsg *msg, char *pBuf, int len)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcGetMsgBuffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    return DRV_ERROR_NONE;
}

hdcError_t halHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout)
{
    return DRV_ERROR_NONE;
}

hdcError_t halHdcSend(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSetSessionReference(HDC_SESSION session)
{
    return DRV_ERROR_NONE;
}

drvError_t drvGetProcessSign(struct process_sign *sign)
{
    return DRV_ERROR_NONE;
}

pid_t drvDeviceGetBareTgid(void)
{
    return DRV_ERROR_NONE;
}

drvError_t halNotifyGetInfo(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val)
{
    return DRV_ERROR_NONE;
}

drvError_t halMemAlloc(void **pp, unsigned long long size, unsigned long long flag)
{
    return DRV_ERROR_NONE;
}

drvError_t halMemFree(void *pp)
{
    return DRV_ERROR_NONE;
}

drvError_t halEschedSubmitEvent(uint32_t devId, struct event_summary *event)
{
    return DRV_ERROR_NONE;
}

drvError_t halMemCtl(int type, void *paramValue, size_t paramValueSize, void *outValue, size_t *outSizeRet)
{
    return DRV_ERROR_NONE;
}

int halBuffAllocAlignEx(uint64_t size, unsigned int align, unsigned long flag, int grpId, void **buff)
{
    return 0;
}

int halBuffFree(void *buff)
{
    return 0;
}

drvError_t halBindCgroup(BIND_CGROUP_TYPE bindType)
{
    return DRV_ERROR_NONE;
}

drvError_t drvGetPlatformInfo(uint32_t* info)
{
    return DRV_ERROR_NONE;
}

drvError_t halGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    return DRV_ERROR_NONE;
}

drvError_t halGetChipInfo(unsigned int devId, halChipInfo *chipInfo)
{
    return DRV_ERROR_NONE;
}

drvError_t halSensorNodeRegister(uint32_t devid, struct halSensorNodeCfg *cfg, uint64_t *handle)
{
    return DRV_ERROR_NONE;
}

drvError_t halSensorNodeUnregister(uint32_t devid, uint64_t handle)
{
    return DRV_ERROR_NONE;
}

drvError_t halSensorNodeUpdateState(uint32_t devid, uint64_t handle, int val, halGeneralEventType_t assertion)
{
    return DRV_ERROR_NONE;
}

drvError_t halEschedAttachDevice(uint32_t devId)
{
    return DRV_ERROR_NONE;
}

drvError_t halEschedCreateGrp(uint32_t devId, uint32_t grpId, GROUP_TYPE type)
{
    return DRV_ERROR_NONE;
}

drvError_t halEschedSubscribeEvent(unsigned int devId, unsigned int grpId,
    unsigned int threadId, unsigned long long eventBitmap)
{
    return DRV_ERROR_NONE;
}

drvError_t halEschedWaitEvent(uint32_t devId, uint32_t grpId, uint32_t threadId, int32_t timeout,
    struct event_info *event)
{
    return DRV_ERROR_NONE;
}

drvError_t halQueryDevpid(struct halQueryDevpidInfo info, pid_t *dev_pid)
{
    return DRV_ERROR_NONE;
}

#define ASCEND_HAL_HANDLE_ADDR ((void *)0xabcd)
void *dlopen (const char *__file, int __mode)
{
    static void *ascend_hal_handle = ASCEND_HAL_HANDLE_ADDR;
    if(strcmp(__file ,"libascend_hal.so") == 0) {
        return ascend_hal_handle;
    }
    return NULL;
}

int dlclose (void *__handle)
{
    return 0;
}

char *dlerror (void)
{
    return NULL;
}

static struct {
    const char *name;
    void *func;
} func_map[] = {
    {"drvGetDevNum", (void*)drvGetDevNum},
    {"drvGetLocalDevIdByHostDevId", (void*)drvGetLocalDevIdByHostDevId},
    {"drvGetDevIdByLocalDevId", (void*)drvGetDevIdByLocalDevId},
    {"drvDeviceGetIndexByPhyId", (void*)drvDeviceGetIndexByPhyId},
    {"drvDeviceGetPhyIdByIndex", (void*)drvDeviceGetPhyIdByIndex},
    {"halQueryDevPid", (void*)halQueryDevPid},
    {"halMemBindSibling", (void*)halMemBindSibling},
    {"drvQueryProcessHostPid", (void*)drvQueryProcessHostPid},
    {"halMemGetInfoEx", (void*)halMemGetInfoEx},
    {"halGrpQuery", (void*)halGrpQuery},
    {"halHdcGetSessionAttr", (void*)halHdcGetSessionAttr},
    {"drvHdcGetCapacity", (void*)drvHdcGetCapacity},
    {"drvHdcClientCreate", (void*)drvHdcClientCreate},
    {"drvHdcClientDestroy", (void*)drvHdcClientDestroy},
    {"drvHdcSessionConnect", (void*)drvHdcSessionConnect},
    {"halHdcSessionConnectEx", (void*)halHdcSessionConnectEx},
    {"drvHdcServerCreate", (void*)drvHdcServerCreate},
    {"drvHdcServerDestroy", (void*)drvHdcServerDestroy},
    {"drvHdcSessionAccept", (void*)drvHdcSessionAccept},
    {"drvHdcSessionClose", (void*)drvHdcSessionClose},
    {"drvHdcFreeMsg", (void*)drvHdcFreeMsg},
    {"drvHdcReuseMsg", (void*)drvHdcReuseMsg},
    {"drvHdcAddMsgBuffer", (void*)drvHdcAddMsgBuffer},
    {"drvHdcGetMsgBuffer", (void*)drvHdcGetMsgBuffer},
    {"halHdcRecv", (void*)halHdcRecv},
    {"halHdcSend", (void*)halHdcSend},
    {"drvHdcAllocMsg", (void*)drvHdcAllocMsg},
    {"drvHdcSetSessionReference", (void*)drvHdcSetSessionReference},
    {"drvGetProcessSign", (void*)drvGetProcessSign},
    {"drvDeviceGetBareTgid", (void*)drvDeviceGetBareTgid},
    {"halNotifyGetInfo", (void*)halNotifyGetInfo},
    {"halMemAlloc", (void*)halMemAlloc},
    {"halMemFree", (void*)halMemFree},
    {"halEschedSubmitEvent", (void*)halEschedSubmitEvent},
    {"halGetDeviceInfo", (void*)halGetDeviceInfo},
    {"halMemCtl", (void*)halMemCtl},
    {"halBindCgroup", (void*)halBindCgroup},
    {"drvGetPlatformInfo", (void*)drvGetPlatformInfo},
    {"halGetChipInfo", (void*)halGetChipInfo},
    {"halSensorNodeRegister", (void*)halSensorNodeRegister},
    {"halSensorNodeUnregister", (void*)halSensorNodeUnregister},
    {"halSensorNodeUpdateState", (void*)halSensorNodeUpdateState},
    {"halBuffAllocAlignEx", (void*)halBuffAllocAlignEx},
    {"halBuffFree", (void*)halBuffFree},
    {"halEschedAttachDevice", (void*)halEschedAttachDevice},
    {"halEschedCreateGrp", (void*)halEschedCreateGrp},
    {"halEschedSubscribeEvent", (void*)halEschedSubscribeEvent},
    {"halEschedWaitEvent", (void*)halEschedWaitEvent},
    {"halQueryDevpid", (void*)halQueryDevpid},
    {NULL, NULL},
};

static void *find_func_by_name(const char *func_name)
{
    void *ret = NULL;
    int index = 0;

    for(; func_map[index].name != NULL; index++) {
        if(strcmp(func_map[index].name, func_name) == 0) {
            ret = func_map[index].func;
            break;
        }
    }

    return ret;
}

void *dlsym(void *__restrict __handle, const char *__restrict __name)
{
    if(__handle == ASCEND_HAL_HANDLE_ADDR) {
        return find_func_by_name(__name);
    }
    return NULL;
}

} // extern "C"