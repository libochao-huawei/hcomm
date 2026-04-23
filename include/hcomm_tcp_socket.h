/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_TCP_SOCKET_H
#define HCOMM_TCP_SOCKET_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "hcomm_res_defs.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void *HcommTcpSocket;
typedef void *HcommTcpSocketEpoll;

struct HcommTcpSocketEpollEvent {
    uint32_t                events;     // 事件类型（EPOLLIN，EPOLLOUT，EPOLLET...)
    HcommTcpSocketEpollData data;       // 用户数据（常放fd或指针）
};

union HcommTcpSocketEpollData {
    void           *ptr;
    HcommTcpSocket socket;
    uint32_t       u32;
    uint64_t       u64;
};

const uint32_t HCOMM_SOCK_CONN_TAG_MAX_SIZE = 192; ///< 握手标识最大长度（含终止符）

// 要求：set device
// 接口内部：get device查询phyId
/**
 * @brief 获取本端VNIC IP
 * @param[in] location 网卡部署位置（只支持device）
 * @param[out] addr 输出的ipv4 地址
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketGetDeviceVnicIp(EndpointLoc location, struct in_addr *addr);

/**
 * @brief 创建Socket通信实例
 * @param[in] endpoint endpoint句柄
 * @param[in] type 类型（当前仅支持传0，默认为非阻塞socket，后续考虑扩展HCOMM_SOCK_BLOCK）
 * @param[out] socket 输出的socket句柄
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketCreate(EndpointHandle endpoint, int32_t type, HcommTcpSocket *socket);

/**
 * @brief 关闭Socket句柄并释放资源
 * @param[in] socket 要关闭的socket句柄
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketClose(HcommTcpSocket socket);

/**
 * @brief 设置监听模式（服务端用）
 * @param[in] socket 已创建的socket句柄
 * @param[in] backlog 最大挂起连接数（参考系统SOMAXCONN）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketListen(HcommTcpSocket socket, int32_t backlog);

struct HcommTcpSocketAcceptInfo {
    char *handShakeTag; // handShakeTag 握手标识
    uint32_t tagLen; // 标识长度（必须<=HCOMM_SOCK_CONN_TAG_MAX_SIZE）
};

/**
 * @brief 批量接受客户端连接（非阻塞操作）
 * @param[in] serverSocket 处于监听状态的服务器socket句柄
 * @param[in] acceptInfos 批量的握手标识
 * @param[in] acceptNum 接受的数量
 * @param[in/out] sockets 输出的新连接句柄（传入指针数组，出参非空代表成功，空指针代表失败）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketBatchAccept(HcommTcpSocket serverSocket,
    HcommTcpSocketAcceptInfo *acceptInfos, uint32_t acceptNum, HcommTcpSocket *sockets);

struct HcommTcpSocketConnectInfo {
    HcommTcpSocket socket;
    const struct sockaddr *addr; // 目标服务器地址
    socklen_t addrlen; // 地址结构体长度
    char *handShakeTag; // 握手标识
    uint32_t tagLen; // 标识长度（必须<=HCOMM_SOCK_CONN_TAG_MAX_SIZE）
};

/**
 * @brief 客户端批量发起连接请求（非阻塞操作）
 * @param[in] connectInfos 批量的连接信息
 * @param[in] connectNum 连接数量
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketBatchConnect(HcommTcpSocketConnectInfo *connectInfos, uint32_t connectNum);

/**
 * @brief 批量获取Socket连接状态
 * @param[in] sockets 目标socket句柄数组
 * @param[out] status 状态码输出（0表示正常）
 * @param[in] socketNum socket数量
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketBatchGetStatus(HcommTcpSocket *sockets, int32_t *status, uint32_t socketNum);

/**
 * @brief 发送数据（非阻塞）
 * @param[in] socket 已连接的socket句柄（考虑增加性能约束）
 * @param[in] data 待发送数据缓冲区
 * @param[in] len 待发送数据长度（字节）
 * @param[in] flags 控制发送行为的标记位（当前仅支持传0，默认为非阻塞，后续考虑扩展MSG_BLOCK）
 * @param[out] sendLen 实际发送数据长度（可能部分发送）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketSend(HcommTcpSocket socket, void *data, uint64_t len, int32_t flags, uint64_t *sendLen);

/**
 * @brief 接收数据（非阻塞）
 * @param[in] socket 已连接的socket句柄（考虑增加性能约束）
 * @param[out] recvBuf 接收数据缓冲区
 * @param[in] len 缓冲区最大容量
 * @param[in] flags 控制接收行为的标记位（当前仅支持传0，默认为非阻塞，后续考虑扩展MSG_BLOCK）
 * @param[out] recvLen 实际接收数据长度
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketRecv(HcommTcpSocket socket, void *recvBuf, uint64_t len, int32_t flags, uint64_t *recvLen);

/**
 * @brief 获取套接字选项
 * @param[in] socket 已连接的socket句柄
 * @param[in] level 选项所在的协议层（SOL_SOCKET, IPPROTO_TCP 等）
 * @param[in] optName 选项名，如 SO_REUSEADDR、TCP_NODELAY
 * @param[out] optVal 指向选项值的指针
 * @param[out] optLen 选项值的长度
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketGetOpt(HcommTcpSocket socket, int32_t level, int32_t optName, void *optVal, socklen_t *optLen);

/**
 * @brief 设置套接字选项
 * @param[in] socket 已连接的socket句柄
 * @param[in] level 选项所在的协议层（SOL_SOCKET, IPPROTO_TCP 等）
 * @param[in] optName 选项名，如 SO_REUSEADDR、TCP_NODELAY
 * @param[in] optVal 指向选项值的指针
 * @param[in] optLen 选项值的长度
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketSetOpt(HcommTcpSocket socket, int32_t level, int32_t optName, void *optVal, socklen_t optLen);

/**
 * @brief 创建Epoll（仅支持HcommTcpSocket的事件）
 * @param[out] epoll 输出的epoll句柄
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketEpollCreate(HcommTcpSocketEpoll *epoll);

/**
 * @brief 销毁Epoll（仅支持HcommTcpSocket的事件）
 * @param[in] epoll epoll句柄
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketEpollClose(HcommTcpSocketEpoll epoll);

/**
 * @brief 控制epoll事件
 * @param[in] epoll 已创建的epoll句柄
 * @param[in] op 操作类型（EPOLL_CTL_ADD, EPOLL_CTL_MOD 等）
 * @param[in] socket 已连接的socket句柄（当前仅支持host网卡socket）
 * @param[in] event epoll事件
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketEpollCtl(HcommTcpSocketEpoll epoll, int32_t op, HcommTcpSocket socket, HcommTcpSocketEpollEvent *event);

/**
 * @brief 等待epoll事件
 * @param[in] epoll 已创建的epoll句柄
 * @param[out] events 事件数组
 * @param[in] maxEvents 事件数组大小
 * @param[in] timeout 超时时间（毫秒）
 * @param[in] eventNum 输出的事件数量
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketEpollWait(HcommTcpSocketEpoll epoll, HcommTcpSocketEpollEvent *events, int32_t maxEvents, int32_t timeout, uint32_t *eventNum);

/**
 * @struct SocketWlistInfo
 * @brief 白名单配置信息结构体
 * @var addr         - 允许连接的远端地址
 * @var addrLen      - 地址结构体长度
 * @var connMaxNum   - 最大允许连接数（0表示无限制）
 * @var handShakeTag - 握手标识字符串（必须以'\0'结尾）
 */
typedef struct {
    struct sockaddr *addr;                          ///< 允许的远端地址
    socklen_t addrLen;
    uint32_t connMaxNum;                            ///< 最大连接数限制
    char handShakeTag[HCOMM_SOCK_CONN_TAG_MAX_SIZE]; ///< 握手标识
} HcommTcpSocketWlistInfo;

/**
 * @brief 启用白名单认证功能（进程级，仅部署在host的网卡生效）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketHostWhiteListEnable();

/**
 * @brief 禁用白名单认证功能（进程级，仅部署在host的网卡生效）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketHostWhiteListDisable();

/**
 * @brief 添加白名单规则
 * @param[in] endpoint endpoint句柄
 * @param[in] whitelists 规则数组（支持批量添加）
 * @param[in] num 规则数量（数组长度）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketWhiteListAdd(EndpointHandle endpoint, HcommTcpSocketWlistInfo *whitelists, uint32_t num);

/**
 * @brief 删除白名单规则
 * @param[in] endpoint endpoint句柄
 * @param[in] whitelists 待删除规则数组
 * @param[in] num 规则数量（数组长度）
 * @return 执行状态码 HcommResult
 */
extern HcommResult HcommTcpSocketWhiteListDel(EndpointHandle endpoint, HcommTcpSocketWlistInfo *whitelists, uint32_t num);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif