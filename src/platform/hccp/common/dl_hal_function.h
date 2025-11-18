/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DL_HAL_FUNCTION_H__
#define __DL_HAL_FUNCTION_H__

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include "user_log.h"
#include "ascend_hal.h"
#include "ascend_hal_error.h"
#include "ascend_hal_define.h"

// device info, see devdrv_hardware_version
#define VER_BIN5            5
#define VER_BIN8            8
#define GET_CHIP_OFFSET     8
#define CHIP_TYPE_910A      1
#define CHIP_TYPE_310P      4
#define CHIP_TYPE_910B_910_93 5

#define RDMA_CQE_ERR_SENSOR_TYPE 0xC3
#define RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE 0x8
/* bit position is 1:event enable; bit position is 0:event disable */
#define RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_MASK (1U << RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE)
/* bit position is 1:fault event;  bit position is 0:notify event(one time) */
#define RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE_MASK (0U << RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE)

struct dl_hal_ops {
    int (*dl_devdrv_get_board_id)(unsigned int dev_id, unsigned int *board_id);
    int (*dl_devdrv_get_vnic_ip)(unsigned int dev_id, unsigned int *ip_addr);
    int (*dl_devdrv_get_vnic_ip_by_sdid)(unsigned int sdid, unsigned int *ip_addr);

    int (*dl_drv_get_dev_num)(unsigned int *num_dev);
    int (*dl_drv_get_local_dev_id_by_host_dev_id)(unsigned int dev_id, unsigned int* chip_id);
    int (*dl_drv_get_dev_id_by_local_dev_id)(unsigned int localDevId, unsigned int *devId);
    int (*dl_drv_device_get_index_by_phy_id)(uint32_t phyId, uint32_t *devIndex);
    int (*dl_drv_device_get_phy_id_by_index)(unsigned int devIndex, unsigned int *phyId);
    drvError_t (*dl_hal_query_dev_pid)(struct halQueryDevpidInfo info, pid_t *dev_pid);
    drvError_t (*dl_hal_mem_bind_sibling)(int host_pid, int aicpu_pid, unsigned int vfid, unsigned int dev_id,
        unsigned int flag);
    drvError_t (*dl_drv_query_process_host_pid)(int pid, unsigned int *chip_id, unsigned int *vfid,
        unsigned int *host_pid, unsigned int *cp_type);
    drvError_t (*dl_hal_mem_get_info_ex)(unsigned int dev_id, unsigned int type, struct MemInfo *info);
    int (*dl_hal_grp_query)(GroupQueryCmdType cmd, void *in_buff, unsigned int in_len, void *out_buff,
        unsigned int *out_len);
    // HDC
    int (*dl_hal_hdc_get_session_attr)(HDC_SESSION session, int attr, int *value);
    hdcError_t (*dl_drv_hdc_get_capacity)(struct drvHdcCapacity *capacity);
    hdcError_t (*dl_drv_hdc_client_create)(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag);
    hdcError_t (*dl_drv_hdc_client_destroy)(HDC_CLIENT client);
    hdcError_t (*dl_drv_hdc_session_connect)(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session);
    hdcError_t (*dl_hal_hdc_session_connect_ex)(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client,
        HDC_SESSION *session);
    hdcError_t (*dl_drv_hdc_server_create)(int devid, int serviceType, HDC_SERVER *pServer);
    hdcError_t (*dl_drv_hdc_server_destroy)(HDC_SERVER server);
    hdcError_t (*dl_drv_hdc_session_accept)(HDC_SERVER server, HDC_SESSION *session);
    hdcError_t (*dl_drv_hdc_session_close)(HDC_SESSION session);
    hdcError_t (*dl_drv_hdc_free_msg)(struct drvHdcMsg *msg);
    hdcError_t (*dl_drv_hdc_reuse_msg)(struct drvHdcMsg *msg);
    hdcError_t (*dl_drv_hdc_add_msg_buffer)(struct drvHdcMsg *msg, char *pBuf, int len);
    hdcError_t (*dl_drv_hdc_get_msg_buffer)(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen);
    hdcError_t (*dl_hal_hdc_recv)(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
        int *recvBufCount, UINT32 timeout);
    hdcError_t (*dl_hal_hdc_send)(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout);
    hdcError_t (*dl_drv_hdc_alloc_msg)(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
    hdcError_t (*dl_drv_hdc_set_session_reference)(HDC_SESSION session);

    int (*dl_drv_get_process_sign)(struct process_sign *sign);

    pid_t (*dl_drv_device_get_bare_tgid)(void);

    int (*dl_hal_notify_get_info)(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val);
    int (*dl_hal_mem_alloc)(void **pp, unsigned long long size, unsigned long long flag);
    int (*dl_hal_mem_free)(void *pp);
    int (*dl_hal_esched_submit_event)(uint32_t devId, struct event_summary *event);

    int (*dl_devdrv_set_user_config)(uint32_t devid, const char *name, uint8_t *buf, uint32_t buf_size);
    int (*dl_devdrv_clear_user_config)(uint32_t devid, const char *name);
    int (*dl_hal_get_device_info)(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);

    int (*dl_hal_mem_ctl)(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);

    int (*dl_hal_bind_cgroup)(BIND_CGROUP_TYPE bindType);
    int (*dl_drv_get_platform_info)(uint32_t* info);

    int (*dl_hal_get_chip_info)(unsigned int dev_id, halChipInfo *chip_info);

    drvError_t (*dl_hal_sensor_node_register)(uint32_t devid, struct halSensorNodeCfg *cfg, uint64_t *handle);
    drvError_t (*dl_hal_sensor_node_unregister)(uint32_t devid, uint64_t handle);
    drvError_t (*dl_hal_sensor_node_update_state)(uint32_t devid, uint64_t handle, int val,
        halGeneralEventType_t assertion);

    int (*dl_hal_buff_alloc_align_ex)(uint64_t size, unsigned int align, unsigned long flag, int grp_id, void **buff);
    int (*dl_hal_buff_free)(void *buff);

    int (*dl_hal_esched_attach_device)(uint32_t devId);
    int (*dl_hal_esched_create_grp)(uint32_t devId, uint32_t grpId, GROUP_TYPE type);
    int (*dl_hal_esched_subscribe_event)(uint32_t devId, uint32_t grpId, uint32_t threadId, uint64_t eventBitmap);
    int (*dl_hal_esched_wait_event)(uint32_t devId, uint32_t grpId,uint32_t threadId, int32_t timeout,
        struct event_info *event);
};

int dl_hal_init(void);
void dl_hal_deinit(void);

int dl_devdrv_get_board_id(unsigned int dev_id, unsigned int *board_id);
int dl_devdrv_get_vnic_ip(unsigned int dev_id, unsigned int *ip_addr);
int dl_devdrv_get_vnic_ip_by_sdid(unsigned int sdid, unsigned int *ip_addr);

int dl_drv_get_dev_num(unsigned int *num_dev);
int dl_drv_get_local_dev_id_by_host_dev_id(unsigned int dev_id, unsigned int* chip_id);
int dl_drv_get_dev_id_by_local_dev_id(unsigned int localDevId, unsigned int *devId);
int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
int dl_drv_device_get_phy_id_by_index(unsigned int devIndex, unsigned int *phyId);
drvError_t dl_hal_query_dev_pid(struct halQueryDevpidInfo info, pid_t *dev_pid);
drvError_t dl_hal_mem_bind_sibling(int host_pid, int aicpu_pid, unsigned int vfid, unsigned int dev_id,
    unsigned int flag);
drvError_t dl_drv_query_process_host_pid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
    unsigned int *cp_type);
drvError_t dl_hal_mem_get_info_ex(unsigned int dev_id, unsigned int type, struct MemInfo *info);
int dl_hal_grp_query(GroupQueryCmdType cmd, void *in_buff, unsigned int in_len, void *out_buff, unsigned int *out_len);
// HDC
int dl_hal_hdc_get_session_attr(HDC_SESSION session, int attr, int *value);
hdcError_t dl_drv_hdc_get_capacity(struct drvHdcCapacity *capacity);
hdcError_t dl_drv_hdc_client_create(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag);
hdcError_t dl_drv_hdc_client_destroy(HDC_CLIENT client);
hdcError_t dl_drv_hdc_session_connect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session);
hdcError_t dl_hal_hdc_session_connect_ex(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client,
    HDC_SESSION *pSession);
hdcError_t dl_drv_hdc_server_create(int devid, int serviceType, HDC_SERVER *pServer);
hdcError_t dl_drv_hdc_server_destroy(HDC_SERVER server);
hdcError_t dl_drv_hdc_session_accept(HDC_SERVER server, HDC_SESSION *session);
hdcError_t dl_drv_hdc_session_close(HDC_SESSION session);
hdcError_t dl_drv_hdc_free_msg(struct drvHdcMsg *msg);
hdcError_t dl_drv_hdc_reuse_msg(struct drvHdcMsg *msg);
hdcError_t dl_drv_hdc_add_msg_buffer(struct drvHdcMsg *msg, char *pBuf, int len);
hdcError_t dl_drv_hdc_get_msg_buffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen);
hdcError_t dl_hal_hdc_recv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
hdcError_t dl_hal_hdc_send(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout);
hdcError_t dl_drv_hdc_alloc_msg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
hdcError_t dl_drv_hdc_set_session_reference(HDC_SESSION session);

int dl_drv_get_process_sign(struct process_sign *sign);

pid_t dl_drv_device_get_bare_tgid(void);

int dl_hal_notify_get_info(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val);
int dl_hal_mem_alloc(void **pp, unsigned long long size, unsigned long long flag);
int dl_hal_mem_free(void *pp);
int dl_hal_esched_submit_event(uint32_t devId, struct event_summary *event);

int dl_devdrv_set_user_config(uint32_t devid, const char *name, uint8_t *buf, uint32_t buf_size);
int dl_devdrv_clear_user_config(uint32_t devid, const char *name);

// device info, see devdrv_hardware_version
int dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);

static inline uint32_t dl_hal_plat_get_ver(uint64_t device_info)
{
    return (uint32_t)(device_info & 0xff);
}

static inline uint32_t dl_hal_plat_get_chip(uint64_t device_info)
{
    return (uint32_t)((device_info >> GET_CHIP_OFFSET) & 0xff);
}

int dl_hal_bind_cgroup(BIND_CGROUP_TYPE bindType);
int dl_drv_get_platform_info(uint32_t* info);

int dl_hal_mem_ctl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);
int dl_hal_buff_alloc_align_ex(uint64_t size, unsigned int align, unsigned long flag, int grp_id, void **buff);
int dl_hal_buff_free(void *buff);

int dl_hal_get_chip_info(unsigned int dev_id, halChipInfo *chip_info);

int dl_hal_sensor_node_register(uint32_t devid, struct halSensorNodeCfg *cfg, uint64_t *handle);
int dl_hal_sensor_node_unregister(uint32_t devid, uint64_t handle);
int dl_hal_sensor_node_update_state(uint32_t devid, uint64_t handle, int val, halGeneralEventType_t assertion);

int dl_hal_esched_attach_device(uint32_t devId);
int dl_hal_esched_create_grp(uint32_t devId, uint32_t grpId, GROUP_TYPE type);
int dl_hal_esched_subscribe_event(uint32_t devId, uint32_t grpId, uint32_t threadId, uint64_t eventBitmap);
int dl_hal_esched_wait_event(uint32_t devId, uint32_t grpId,uint32_t threadId, int32_t timeout,
    struct event_info *event);

#endif  // __DL_HAL_FUNCTION_H__
