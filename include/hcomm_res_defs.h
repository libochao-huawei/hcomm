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
 
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @enum HcommResult
 * @brief 错误码类型枚举定义
 */
typedef enum {
    HCOMM_SUCCESS = 0,          ///< 成功
} HcommResult;
 
/* 网络设备句柄 */
typedef void *EndpointHandle;

/**
 * @brief 内存句柄类型（不透明结构）
 */
typedef void *HcommMemHandle;

typedef void *HcommSocket;

/**
 * @enum CommMemType
 * @brief 内存类型枚举定义
 */
typedef enum {
    COMM_MEM_TYPE_INVALID = -1, ///< 无效的内存类别
    COMM_MEM_TYPE_DEVICE = 0, ///< 设备侧内存（如NPU等）
    COMM_MEM_TYPE_HOST,   ///< 主机侧内存
} CommMemType;

/**
 * @brief 内存段元数据描述结构体
 */
typedef struct {
    CommMemType type; ///< 内存物理位置类型，参见CommMemType
    void *addr;       ///< 内存地址
    uint64_t size;    ///< 内存区域字节数
} CommMem;

/**
 * @brief 兼容Abi字段结构体
 */
typedef struct {
    uint32_t version;
    uint32_t magicWord;
    uint32_t size;
    uint32_t reserved;
} CommAbiHeader;

/**
 * @brief 套接字角色
 */
typedef enum {
    HCOMM_SOCKET_ROLE_RESERVED = -1, ///< 保留的套接字角色
    HCOMM_SOCKET_ROLE_CLIENT = 0, ///< 客户端角色，用于发起连接
    HCOMM_SOCKET_ROLE_SERVER = 1, ///< 服务器角色，用于监听连接
} HcommSocketRole;

/**
 * @brief 通道描述参数
 */
typedef struct {
    CommAbiHeader header; ///< ABI头部，包含版本等信息
    EndpointDesc remoteEndpoint; ///< 远端网络设备端侧描述
    uint32_t notifyNum; ///< channel上使用的通知消息数量
    bool exchangeAllMems; ///< exchangeAllMems = True 就不需要 memHandle 了
    HcommMemHandle *memHandles;
    uint32_t memHandleNum;
    HcommSocket socket;
    HcommSocketRole role; ///< 本端角色(SERVER或CLIENT)
    uint16_t port; ///< 端口号。当HcommSocketRole为SERVER时，表示本端监听端口；当为CLIENT时，表示远端目标端口
    union {
        uint8_t raws[128]; ///< 通用缓存
        struct {
            uint32_t queueNum;      ///< QP数量
            uint32_t retryCnt;      ///< 最大重传次数
            uint32_t retryInterval; ///< 重传间隔（ms)(对应协议计算公式)
            uint8_t tc;            ///< 流量类别（QoS)
            uint8_t sl;            ///< 服务等级（QoS)
        } roceAttr;
        struct {
            uint32_t qos;
        } hccsAttr;
    };
} HcommChannelDesc;

/**
 * @brief 初始化HcommChannelDesc结构体
 *
 * @param[inout] channelDesc 返回的通道描述参数
 * @param[in] descNum 描述数量
 * @return HcommResult 执行结果状态码
 */
inline HcommResult HcommChannelDescInit(HcommChannelDesc *channelDesc, uint32_t descNum)
{
    for (uint32_t idx = 0; idx < descNum; idx++) {
        if (channelDesc != nullptr) {
            // 先用0xFF填充整个结构体
            (void)memset_s(channelDesc, sizeof(HcommChannelDesc), 0xFF, sizeof(HcommChannelDesc));
            
            // 初始化ABI头信息
            channelDesc->header.version     = HCOMM_CHANNEL_VERSION;
            channelDesc->header.magicWord   = HCOMM_CHANNEL_MAGIC_WORD;
            channelDesc->header.size        = sizeof(HcommChannelDesc);
            channelDesc->header.reserved    = 0;

            // 初始化关键字段
            

            // 显式设置EndpointDesc相关字段为无效值
            if (UNLIKELY(EndpointDescInit(&channelDesc->localEndpoint, 1) != 0) ||
                UNLIKELY(EndpointDescInit(&channelDesc->remoteEndpoint, 1) != 0)) {
                return 4; // INTERNAL
            }
            channelDesc++;  // 移动到下一个描述符
        } else {
            return 2; // E_PTR
        }
    }
    return 0;
}
 
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCOMM_RES_DEFS_H