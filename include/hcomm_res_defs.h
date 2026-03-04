/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCOMM_RES_DEFS_H
#define HCOMM_RES_DEFS_H
 
#include <stdint.h>
#include <cstddef>
#include <functional>
#include "hccl/hccl_res.h"

#define RAWS_SIZE 128

 
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
 
 
/* 网络设备句柄 */
typedef void* EndpointHandle;

/**
 * @enum HcclMemType
 * @brief 内存类型枚举定义
 */
typedef enum {
    HCCL_MEM_TYPE_DEVICE, ///< 设备侧内存（如NPU等）
    HCCL_MEM_TYPE_HOST,   ///< 主机侧内存
    HCCL_MEM_TYPE_NUM     ///< 内存类型数量
} HcclMemType;

typedef struct{
    int32_t devPhyId;
    uint32_t superPodId;
} HcommDevId;

typedef struct {
    HcclMemType type; ///< 内存物理位置类型，参见HcclMemType
    void *addr;       ///< 内存地址
    uint64_t size;    ///< 内存区域字节数
} HcommMem;
 
/**
 * @struct HcommBuf
 * @brief 内存缓冲区描述结构体
 * @var addr   - 虚拟地址指针
 * @var len    - 内存长度（单位字节）
 */
typedef struct {
    void *addr;
    uint64_t len;
} HcommBuf;
 
typedef struct {
    uint32_t sdid;
    int32_t pid;
} HcommMemGrantInfo;
 
typedef void* MemHandle;

typedef void* HcommSocket;
 
typedef struct {
    EndpointDesc remoteEndpoint;
    uint32_t notifyNum;

    // exchangeAllMems = True 就不需要 memHandle 了
    bool exchangeAllMems;
    void **memHandles;
    uint32_t memHandleNum;
 
    union {
        uint8_t raws[RAWS_SIZE];
        struct {
            uint32_t queueNum;
            uint32_t retryCnt;
            uint32_t retryInterval;
            uint32_t tc;
            uint32_t sl;
        } roceAttr;
        struct
        {
            uint32_t qos;
        } hccsAttr;
    };

    HcommSocket socket;
    // socket 监听指定端口号（源/目的端口号）
    uint16_t port;
} HcommChannelDesc;
 
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCOMM_RES_DEFS_H