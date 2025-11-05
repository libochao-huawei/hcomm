/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include "errno.h"
#include "ascend_hal_dl.h"
#include "dl_hal_function.h"

#define DL_API_IS_NULL_CHECK(handle, ptr, str) do { \
    if ((handle) == NULL) { \
        roce_err("g_hal_api_handle is NULL!"); \
        return (-EINVAL); \
    } \
    if ((ptr) == NULL) { \
        roce_err("[%s] is NULL!", (str)); \
        return (-EINVAL); \
    } \
} while (0)

static pthread_mutex_t g_hal_api_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_hal_api_handle = NULL;
static struct dl_hal_ops g_hal_ops;
static int g_hal_api_refcnt = 0;

static void dl_hal_api_init(void)
{
    g_hal_ops.dl_devdrv_get_board_id = (int (*)(unsigned int dev_id, unsigned int *board_id))
        ascend_hal_dlsym(g_hal_api_handle, "devdrv_get_board_id");

    g_hal_ops.dl_devdrv_get_vnic_ip = (int (*)(unsigned int dev_id, unsigned int *ip_addr))
        ascend_hal_dlsym(g_hal_api_handle, "devdrv_get_vnic_ip");

    g_hal_ops.dl_drv_get_dev_num = (int (*)(unsigned int *num_dev))
        ascend_hal_dlsym(g_hal_api_handle, "drvGetDevNum");

    g_hal_ops.dl_drv_get_local_dev_id_by_host_dev_id = (int (*)(uint32_t dev_id, uint32_t* chip_id))
        ascend_hal_dlsym(g_hal_api_handle, "drvGetLocalDevIDByHostDevID");

    g_hal_ops.dl_drv_get_dev_id_by_local_dev_id = (int (*)(uint32_t localDevId, uint32_t *devId))
        ascend_hal_dlsym(g_hal_api_handle, "drvGetDevIDByLocalDevID");

    g_hal_ops.dl_drv_device_get_index_by_phy_id = (int (*)(uint32_t phyId, uint32_t *devIndex))
        ascend_hal_dlsym(g_hal_api_handle, "drvDeviceGetIndexByPhyId");

    g_hal_ops.dl_drv_device_get_phy_id_by_index = (int (*)(unsigned int devIndex, unsigned int *phyId))
        ascend_hal_dlsym(g_hal_api_handle, "drvDeviceGetPhyIdByIndex");

    g_hal_ops.dl_hal_hdc_get_session_attr = (int (*)(HDC_SESSION session, int attr, int *value))
        ascend_hal_dlsym(g_hal_api_handle, "halHdcGetSessionAttr");

    g_hal_ops.dl_drv_hdc_get_capacity = (hdcError_t (*)(struct drvHdcCapacity *capacity))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcGetCapacity");

    g_hal_ops.dl_drv_hdc_client_create = (hdcError_t (*)(HDC_CLIENT *client, int maxSessionNum,
        int serviceType, int flag))ascend_hal_dlsym(g_hal_api_handle, "drvHdcClientCreate");

    g_hal_ops.dl_drv_hdc_client_destroy = (hdcError_t (*)(HDC_CLIENT client))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcClientDestroy");

    g_hal_ops.dl_drv_hdc_session_connect =
        (hdcError_t (*)(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session))
            ascend_hal_dlsym(g_hal_api_handle, "drvHdcSessionConnect");

    g_hal_ops.dl_drv_hdc_server_create = (hdcError_t (*)(int devid, int serviceType, HDC_SERVER *pServer))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcServerCreate");

    g_hal_ops.dl_drv_hdc_server_destroy = (hdcError_t (*)(HDC_SERVER server))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcServerDestroy");

    g_hal_ops.dl_drv_hdc_session_accept = (hdcError_t (*)(HDC_SERVER server, HDC_SESSION *session))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcSessionAccept");

    g_hal_ops.dl_drv_hdc_session_close = (hdcError_t (*)(HDC_SESSION session))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcSessionClose");

    g_hal_ops.dl_drv_hdc_free_msg = (hdcError_t (*)(struct drvHdcMsg *msg))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcFreeMsg");

    g_hal_ops.dl_drv_hdc_reuse_msg = (hdcError_t (*)(struct drvHdcMsg *msg))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcReuseMsg");

    g_hal_ops.dl_drv_hdc_add_msg_buffer = (hdcError_t (*)(struct drvHdcMsg *msg, char *pBuf, int len))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcAddMsgBuffer");

    g_hal_ops.dl_drv_hdc_get_msg_buffer = (hdcError_t (*)(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcGetMsgBuffer");

    g_hal_ops.dl_hal_hdc_recv =
        (hdcError_t (*)(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag, int *recvBufCount,
            UINT32 timeout))ascend_hal_dlsym(g_hal_api_handle, "halHdcRecv");

    g_hal_ops.dl_hal_hdc_send = (hdcError_t (*)(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag,
        UINT32 timeout))ascend_hal_dlsym(g_hal_api_handle, "halHdcSend");

    g_hal_ops.dl_drv_hdc_alloc_msg = (hdcError_t (*)(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcAllocMsg");

    g_hal_ops.dl_drv_hdc_set_session_reference = (hdcError_t (*)(HDC_SESSION session))
        ascend_hal_dlsym(g_hal_api_handle, "drvHdcSetSessionReference");

    g_hal_ops.dl_drv_get_process_sign = (int (*)(struct process_sign *sign))
        ascend_hal_dlsym(g_hal_api_handle, "drvGetProcessSign");

    g_hal_ops.dl_drv_device_get_bare_tgid = (pid_t (*)(void))
        ascend_hal_dlsym(g_hal_api_handle, "drvDeviceGetBareTgid");

    g_hal_ops.dl_hal_notify_get_info = (int (*)(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val))
        ascend_hal_dlsym(g_hal_api_handle, "halNotifyGetInfo");

    g_hal_ops.dl_hal_mem_alloc = (int (*)(void **pp, unsigned long long size, unsigned long long flag))
        ascend_hal_dlsym(g_hal_api_handle, "halMemAlloc");

    g_hal_ops.dl_hal_mem_free = (int (*)(void *pp))
        ascend_hal_dlsym(g_hal_api_handle, "halMemFree");

    g_hal_ops.dl_hal_esched_submit_event = (int (*)(uint32_t devId, struct event_summary *event))
        ascend_hal_dlsym(g_hal_api_handle, "halEschedSubmitEvent");

    g_hal_ops.dl_devdrv_set_user_config = (int (*)(uint32_t devid, const char *name, uint8_t *buf, uint32_t buf_size))
        ascend_hal_dlsym(g_hal_api_handle, "devdrv_set_user_config");

    g_hal_ops.dl_devdrv_clear_user_config = (int (*)(uint32_t devid, const char *name))
        ascend_hal_dlsym(g_hal_api_handle, "devdrv_clear_user_config");

    g_hal_ops.dl_hal_get_device_info = (int (*)(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value))
        ascend_hal_dlsym(g_hal_api_handle, "halGetDeviceInfo");

    g_hal_ops.dl_hal_bind_cgroup = (int (*)(BIND_CGROUP_TYPE bindType))
        ascend_hal_dlsym(g_hal_api_handle, "halBindCgroup");

    g_hal_ops.dl_drv_get_platform_info = (int (*)(uint32_t* info))
        ascend_hal_dlsym(g_hal_api_handle, "drvGetPlatformInfo");

    g_hal_ops.dl_hal_get_chip_info = (int (*)(unsigned int dev_id, halChipInfo *chip_info))
        ascend_hal_dlsym(g_hal_api_handle, "halGetChipInfo");

#ifndef HNS_ROCE_LLT
    g_hal_ops.dl_hal_mem_ctl =
        (int (*)(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret))
            ascend_hal_dlsym(g_hal_api_handle, "halMemCtl");
#endif

    g_hal_ops.dl_hal_query_dev_pid = (drvError_t (*)(struct halQueryDevpidInfo info, pid_t *dev_pid))
        ascend_hal_dlsym(g_hal_api_handle, "halQueryDevpid");

    g_hal_ops.dl_hal_hdc_session_connect_ex =
        (hdcError_t (*)(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client, HDC_SESSION *pSession))
            ascend_hal_dlsym(g_hal_api_handle, "halHdcSessionConnectEx");

    g_hal_ops.dl_devdrv_get_vnic_ip_by_sdid = (int (*)(unsigned int sdid, unsigned int *ip_addr))
        ascend_hal_dlsym(g_hal_api_handle, "devdrv_get_vnic_ip_by_sdid");

    g_hal_ops.dl_hal_mem_bind_sibling =
        (drvError_t (*)(int host_pid, int aicpu_pid, unsigned int vfid, unsigned int dev_id, unsigned int flag))
            ascend_hal_dlsym(g_hal_api_handle, "halMemBindSibling");

    g_hal_ops.dl_drv_query_process_host_pid = (drvError_t (*)(int pid, unsigned int *chip_id, unsigned int *vfid,
        unsigned int *host_pid, unsigned int *cp_type))
            ascend_hal_dlsym(g_hal_api_handle, "drvQueryProcessHostPid");

    g_hal_ops.dl_hal_mem_get_info_ex = (drvError_t (*)(unsigned int dev_id, unsigned int type, struct MemInfo *info))
            ascend_hal_dlsym(g_hal_api_handle, "halMemGetInfoEx");

    g_hal_ops.dl_hal_grp_query = (int (*)(GroupQueryCmdType cmd, void *in_buff, unsigned int in_len, void *out_buff,
        unsigned int *out_len))
            ascend_hal_dlsym(g_hal_api_handle, "halGrpQuery");

    g_hal_ops.dl_hal_sensor_node_register =
        (drvError_t (*)(uint32_t devid, struct halSensorNodeCfg *cfg, uint64_t *handle))
        ascend_hal_dlsym(g_hal_api_handle, "halSensorNodeRegister");

    g_hal_ops.dl_hal_sensor_node_unregister = (drvError_t (*)(uint32_t devid, uint64_t handle))
        ascend_hal_dlsym(g_hal_api_handle, "halSensorNodeUnregister");

    g_hal_ops.dl_hal_sensor_node_update_state =
        (drvError_t (*)(uint32_t devid, uint64_t handle, int val, halGeneralEventType_t assertion))
        ascend_hal_dlsym(g_hal_api_handle, "halSensorNodeUpdateState");

    g_hal_ops.dl_hal_buff_alloc_align_ex = (int (*)(uint64_t size, unsigned int align, unsigned long flag, int grp_id,
        void **buff))
            ascend_hal_dlsym(g_hal_api_handle, "halBuffAllocAlignEx");

    g_hal_ops.dl_hal_buff_free = (int (*)(void *buff))
            ascend_hal_dlsym(g_hal_api_handle, "halBuffFree");

    g_hal_ops.dl_hal_esched_attach_device = (int (*)(uint32_t devId))
        ascend_hal_dlsym(g_hal_api_handle, "halEschedAttachDevice");

    g_hal_ops.dl_hal_esched_create_grp = (int (*)(uint32_t devId, uint32_t grpId, GROUP_TYPE type))
        ascend_hal_dlsym(g_hal_api_handle, "halEschedCreateGrp");

    g_hal_ops.dl_hal_esched_subscribe_event = (int (*)(uint32_t devId, uint32_t grpId, uint32_t threadId,
        uint64_t eventBitmap))ascend_hal_dlsym(g_hal_api_handle, "halEschedSubscribeEvent");

    g_hal_ops.dl_hal_esched_wait_event = (int (*)(uint32_t devId, uint32_t grpId, uint32_t threadId, int32_t timeout,
        struct event_info *event))ascend_hal_dlsym(g_hal_api_handle, "halEschedWaitEvent");

    return;
}

void dl_hal_deinit(void)
{
    pthread_mutex_lock(&g_hal_api_lock);
    if (g_hal_api_handle != NULL) {
        g_hal_api_refcnt--;
        if (g_hal_api_refcnt > 0) {
            pthread_mutex_unlock(&g_hal_api_lock);
            roce_info("dl_hal_deinit success, no need to dlclose libascend_hal.so!");
            return;
        }

        (void)ascend_hal_dlclose(g_hal_api_handle);
        g_hal_api_handle = NULL;
    }

    pthread_mutex_unlock(&g_hal_api_lock);
    roce_info("dl_hal_deinit success!");
    return;
}

int dl_hal_init(void)
{
    pthread_mutex_lock(&g_hal_api_lock);
    if (g_hal_api_handle != NULL) {
        g_hal_api_refcnt++;
        pthread_mutex_unlock(&g_hal_api_lock);
        roce_info("dl_hal_init success, no need to dlopen libascend_hal.so!");
        return 0;
    }

    g_hal_api_handle = ascend_hal_dlopen("libascend_hal.so", RTLD_NOW);
    if (g_hal_api_handle == NULL) {
        pthread_mutex_unlock(&g_hal_api_lock);
        roce_err("dlopen libascend_hal.so fail! error_no=[%d]", errno);
        return -EINVAL;
    }

    dl_hal_api_init();
    g_hal_api_refcnt++;

    pthread_mutex_unlock(&g_hal_api_lock);
    roce_info("dl_hal_init success!");
    return 0;
}

int dl_devdrv_get_board_id(unsigned int dev_id, unsigned int *board_id)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_devdrv_get_board_id, "dl_devdrv_get_board_id");

    return g_hal_ops.dl_devdrv_get_board_id(dev_id, board_id);
}

int dl_devdrv_get_vnic_ip(unsigned int dev_id, unsigned int *ip_addr)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_devdrv_get_vnic_ip, "dl_devdrv_get_vnic_ip");

    return g_hal_ops.dl_devdrv_get_vnic_ip(dev_id, ip_addr);
}

int dl_devdrv_get_vnic_ip_by_sdid(unsigned int sdid, unsigned int *ip_addr)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_devdrv_get_vnic_ip_by_sdid, "dl_devdrv_get_vnic_ip_by_sdid");

    return g_hal_ops.dl_devdrv_get_vnic_ip_by_sdid(sdid, ip_addr);
}

int dl_drv_get_dev_num(unsigned int *num_dev)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_get_dev_num, "dl_drv_get_dev_num");

    return g_hal_ops.dl_drv_get_dev_num(num_dev);
}

int dl_drv_get_local_dev_id_by_host_dev_id(unsigned int dev_id, unsigned int* chip_id)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_get_local_dev_id_by_host_dev_id,
        "dl_drv_get_local_dev_id_by_host_dev_id");

    return g_hal_ops.dl_drv_get_local_dev_id_by_host_dev_id(dev_id, chip_id);
}

int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_device_get_index_by_phy_id,
        "dl_drv_device_get_index_by_phy_id");

    return g_hal_ops.dl_drv_device_get_index_by_phy_id(phyId, devIndex);
}

int dl_drv_get_dev_id_by_local_dev_id(unsigned int localDevId, unsigned int *devId)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_get_dev_id_by_local_dev_id,
        "dl_drv_get_dev_id_by_local_dev_id");

    return g_hal_ops.dl_drv_get_dev_id_by_local_dev_id(localDevId, devId);
}

int dl_drv_device_get_phy_id_by_index(unsigned int devIndex, unsigned int *phyId)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_device_get_phy_id_by_index,
        "dl_drv_device_get_phy_id_by_index");

    return g_hal_ops.dl_drv_device_get_phy_id_by_index(devIndex, phyId);
}

drvError_t dl_hal_query_dev_pid(struct halQueryDevpidInfo info, pid_t *dev_pid)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_query_dev_pid, "dl_hal_query_dev_pid");

    return g_hal_ops.dl_hal_query_dev_pid(info, dev_pid);
}

drvError_t dl_hal_mem_bind_sibling(int host_pid, int aicpu_pid, unsigned int vfid, unsigned int dev_id,
    unsigned int flag)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_mem_bind_sibling, "dl_hal_mem_bind_sibling");

    return g_hal_ops.dl_hal_mem_bind_sibling(host_pid, aicpu_pid, vfid, dev_id, flag);
}

drvError_t dl_drv_query_process_host_pid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
    unsigned int *cp_type)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_query_process_host_pid, "dl_drv_query_process_host_pid");

    return g_hal_ops.dl_drv_query_process_host_pid(pid, chip_id, vfid, host_pid, cp_type);
}

drvError_t dl_hal_mem_get_info_ex(unsigned int dev_id, unsigned int type, struct MemInfo *info)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_mem_get_info_ex, "dl_hal_mem_get_info_ex");

    return g_hal_ops.dl_hal_mem_get_info_ex(dev_id, type, info);
}

int dl_hal_grp_query(GroupQueryCmdType cmd, void *in_buff, unsigned int in_len, void *out_buff, unsigned int *out_len)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_grp_query, "dl_hal_grp_query");

    return g_hal_ops.dl_hal_grp_query(cmd, in_buff, in_len, out_buff, out_len);
}

int dl_hal_hdc_get_session_attr(HDC_SESSION session, int attr, int *value)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_hdc_get_session_attr, "dl_hal_hdc_get_session_attr");

    return g_hal_ops.dl_hal_hdc_get_session_attr(session, attr, value);
}

hdcError_t dl_drv_hdc_get_capacity(struct drvHdcCapacity *capacity)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_get_capacity, "dl_drv_hdc_get_capacity");

    return g_hal_ops.dl_drv_hdc_get_capacity(capacity);
}

hdcError_t dl_drv_hdc_client_create(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_client_create, "dl_drv_hdc_client_create");

    return g_hal_ops.dl_drv_hdc_client_create(client, maxSessionNum, serviceType, flag);
}

hdcError_t dl_drv_hdc_client_destroy(HDC_CLIENT client)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_client_destroy, "dl_drv_hdc_client_destroy");

    return g_hal_ops.dl_drv_hdc_client_destroy(client);
}

hdcError_t dl_drv_hdc_session_connect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_session_connect, "dl_drv_hdc_session_connect");

    return g_hal_ops.dl_drv_hdc_session_connect(peer_node, peer_devid, client, session);
}

hdcError_t dl_hal_hdc_session_connect_ex(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client,
    HDC_SESSION *pSession)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_hdc_session_connect_ex, "dl_hal_hdc_session_connect_ex");

    return g_hal_ops.dl_hal_hdc_session_connect_ex(peer_node, peer_devid, peer_pid, client, pSession);
}

hdcError_t dl_drv_hdc_server_create(int devid, int serviceType, HDC_SERVER *pServer)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_server_create, "dl_drv_hdc_server_create");

    return g_hal_ops.dl_drv_hdc_server_create(devid, serviceType, pServer);
}

hdcError_t dl_drv_hdc_server_destroy(HDC_SERVER server)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_server_destroy, "dl_drv_hdc_server_destroy");

    return g_hal_ops.dl_drv_hdc_server_destroy(server);
}

hdcError_t dl_drv_hdc_session_accept(HDC_SERVER server, HDC_SESSION *session)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_session_accept, "dl_drv_hdc_session_accept");

    return g_hal_ops.dl_drv_hdc_session_accept(server, session);
}

hdcError_t dl_drv_hdc_session_close(HDC_SESSION session)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_session_close, "dl_drv_hdc_session_close");

    return g_hal_ops.dl_drv_hdc_session_close(session);
}

hdcError_t dl_drv_hdc_free_msg(struct drvHdcMsg *msg)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_free_msg, "dl_drv_hdc_free_msg");

    return g_hal_ops.dl_drv_hdc_free_msg(msg);
}

hdcError_t dl_drv_hdc_reuse_msg(struct drvHdcMsg *msg)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_reuse_msg, "dl_drv_hdc_reuse_msg");

    return g_hal_ops.dl_drv_hdc_reuse_msg(msg);
}

hdcError_t dl_drv_hdc_add_msg_buffer(struct drvHdcMsg *msg, char *pBuf, int len)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_add_msg_buffer, "dl_drv_hdc_add_msg_buffer");

    return g_hal_ops.dl_drv_hdc_add_msg_buffer(msg, pBuf, len);
}

hdcError_t dl_drv_hdc_get_msg_buffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_get_msg_buffer, "dl_drv_hdc_get_msg_buffer");

    return g_hal_ops.dl_drv_hdc_get_msg_buffer(msg, index, pBuf, pLen);
}

hdcError_t dl_hal_hdc_recv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_hdc_recv, "dl_hal_hdc_recv");

    return g_hal_ops.dl_hal_hdc_recv(session, pMsg, bufLen, flag, recvBufCount, timeout);
}

hdcError_t dl_hal_hdc_send(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_hdc_send, "dl_hal_hdc_send");

    return g_hal_ops.dl_hal_hdc_send(session, pMsg, flag, timeout);
}

hdcError_t dl_drv_hdc_alloc_msg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_alloc_msg, "dl_drv_hdc_alloc_msg");

    return g_hal_ops.dl_drv_hdc_alloc_msg(session, ppMsg, count);
}

hdcError_t dl_drv_hdc_set_session_reference(HDC_SESSION session)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_hdc_set_session_reference,
        "dl_drv_hdc_set_session_reference");

    return g_hal_ops.dl_drv_hdc_set_session_reference(session);
}

int dl_drv_get_process_sign(struct process_sign *sign)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_get_process_sign, "dl_drv_get_process_sign");

    return g_hal_ops.dl_drv_get_process_sign(sign);
}

pid_t dl_drv_device_get_bare_tgid(void)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_device_get_bare_tgid, "dl_drv_device_get_bare_tgid");

    return g_hal_ops.dl_drv_device_get_bare_tgid();
}

int dl_hal_notify_get_info(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_notify_get_info, "dl_hal_notify_get_info");

    return g_hal_ops.dl_hal_notify_get_info(devId, tsId, type, val);
}

int dl_hal_mem_alloc(void **pp, unsigned long long size, unsigned long long flag)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_mem_alloc, "dl_hal_mem_alloc");

    return g_hal_ops.dl_hal_mem_alloc(pp, size, flag);
}

int dl_hal_mem_free(void *pp)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_mem_free, "dl_hal_mem_free");

    return g_hal_ops.dl_hal_mem_free(pp);
}

int dl_hal_esched_submit_event(uint32_t devId, struct event_summary *event)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_esched_submit_event, "dl_hal_esched_submit_event");

    return g_hal_ops.dl_hal_esched_submit_event(devId, event);
}

int dl_devdrv_set_user_config(uint32_t devid, const char *name, uint8_t *buf, uint32_t buf_size)
{
    int ret;

    ret = dl_hal_init();
    if (ret) {
        roce_err("dl_hal_init failed, ret:%d", ret);
        return ret;
    }
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_devdrv_set_user_config, "dl_devdrv_set_user_config");

    ret = g_hal_ops.dl_devdrv_set_user_config(devid, name, buf, buf_size);
    dl_hal_deinit();

    return ret;
}

int dl_devdrv_clear_user_config(uint32_t devid, const char *name)
{
    int ret;

    ret = dl_hal_init();
    if (ret) {
        roce_err("dl_hal_init failed, ret:%d", ret);
        return ret;
    }
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_devdrv_clear_user_config, "dl_devdrv_clear_user_config");

    ret = g_hal_ops.dl_devdrv_clear_user_config(devid, name);
    dl_hal_deinit();

    return ret;
}

int dl_hal_mem_ctl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_mem_ctl, "dl_hal_mem_ctl");

    return g_hal_ops.dl_hal_mem_ctl(type, param_value, param_value_size, out_value, out_size_ret);
}

int dl_hal_buff_alloc_align_ex(uint64_t size, unsigned int align, unsigned long flag, int grp_id, void **buff)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_buff_alloc_align_ex, "dl_hal_buff_alloc_align_ex");

    return g_hal_ops.dl_hal_buff_alloc_align_ex(size, align, flag, grp_id, buff);
}

int dl_hal_buff_free(void *buff)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_buff_free, "dl_hal_buff_free");

    return g_hal_ops.dl_hal_buff_free(buff);
}

int dl_hal_bind_cgroup(BIND_CGROUP_TYPE bindType)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_bind_cgroup, "dl_hal_bind_cgroup");

    return g_hal_ops.dl_hal_bind_cgroup(bindType);
}

int dl_drv_get_platform_info(uint32_t* info)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_drv_get_platform_info, "dl_drv_get_platform_info");

    return g_hal_ops.dl_drv_get_platform_info(info);
}

int dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_get_device_info, "dl_hal_get_device_info");

    return g_hal_ops.dl_hal_get_device_info(devId, moduleType, infoType, value);
}

int dl_hal_get_chip_info(unsigned int dev_id, halChipInfo *chip_info)
{
    if (g_hal_api_handle == NULL) {
        roce_err("g_hal_api_handle is NULL!");
        return -EINVAL;
    }

    if (g_hal_ops.dl_hal_get_chip_info == NULL) {
        roce_warn("dl_hal_get_chip_info is NULL!");
        return -EINVAL;
    }

    return g_hal_ops.dl_hal_get_chip_info(dev_id, chip_info);
}

int dl_hal_sensor_node_register(uint32_t devid, struct halSensorNodeCfg *cfg, uint64_t *handle)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_sensor_node_register, "dl_hal_sensor_node_register");

    return g_hal_ops.dl_hal_sensor_node_register(devid, cfg, handle);
}

int dl_hal_sensor_node_unregister(uint32_t devid, uint64_t handle)
{
    /* sensor may not support, handle is 0 */
    if (handle == 0) {
        return 0;
    }

    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_sensor_node_unregister, "dl_hal_sensor_node_unregister");

    return g_hal_ops.dl_hal_sensor_node_unregister(devid, handle);
}

int dl_hal_sensor_node_update_state(uint32_t devid, uint64_t handle, int val, halGeneralEventType_t assertion)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_sensor_node_update_state,
        "dl_hal_sensor_node_update_state");

    return g_hal_ops.dl_hal_sensor_node_update_state(devid, handle, val, assertion);
}

int dl_hal_esched_attach_device(uint32_t devId)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_esched_attach_device, "dl_hal_esched_attach_device");

    return g_hal_ops.dl_hal_esched_attach_device(devId);
}

int dl_hal_esched_create_grp(uint32_t devId, uint32_t grpId, GROUP_TYPE type)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_esched_create_grp, "dl_hal_esched_create_grp");

    return g_hal_ops.dl_hal_esched_create_grp(devId, grpId, type);
}

int dl_hal_esched_subscribe_event(uint32_t devId, uint32_t grpId, uint32_t threadId, uint64_t eventBitmap)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_esched_subscribe_event, "dl_hal_esched_subscribe_event");

    return g_hal_ops.dl_hal_esched_subscribe_event(devId, grpId, threadId, eventBitmap);
}

int dl_hal_esched_wait_event(uint32_t devId, uint32_t grpId, uint32_t threadId, int32_t timeout,
    struct event_info *event)
{
    DL_API_IS_NULL_CHECK(g_hal_api_handle, g_hal_ops.dl_hal_esched_wait_event, "dl_hal_esched_wait_event");

    return g_hal_ops.dl_hal_esched_wait_event(devId, grpId, threadId, timeout, event);
}
