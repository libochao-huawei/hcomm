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
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/uio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "ascend_hal.h"
#include "ascend_hal_stub.h"

// ==================== 核心接口 ====================
/**
 * @brief 初始化共享内存（父进程创建/子进程挂载）
 * @return 0成功，负数失败
 */
__attribute__((constructor(101))) static int ShmInitHostPid(void)
{
    // 1. 创建/获取共享内存段
    g_shmid = shmget(SHM_KEY, SHM_SIZE, 0666 | IPC_CREAT);
    if (g_shmid == -1) {
        printf("shmget failed, errno:%d, msg:%s\n", errno, strerror(errno));
        return -1;
    }

    // 2. 挂载共享内存到进程虚拟地址空间
    g_h2dInfo = (h2d_info_t *)shmat(g_shmid, NULL, 0);
    if (g_h2dInfo == (void *)-1) {
        printf("shmat failed, errno:%d\n", errno);
        g_h2dInfo = NULL;
        return -1;
    }

    printf("Shared memory initialized: hostPid=%d, devPid=%d\n", g_h2dInfo->hostPid, g_h2dInfo->devPid);
    return 0;
}

/**
 * @brief 设置共享的HostPid（父进程调用）
 * @param pid 要共享的进程ID
 */
void setHostPid(pid_t pid)
{
    printf("Setting Host PID in shared memory: %d\n", pid);
    if (g_h2dInfo == NULL && ShmInitHostPid() != 0) {
        printf("setHostPid init failed\n");
        return;
    }
    g_h2dInfo->hostPid = pid;  // 写入共享内存，所有进程可见
}

void setDevPid(pid_t pid)
{
    printf("Setting Dev PID in shared memory: %d\n", pid);
    if (g_h2dInfo == NULL && ShmInitHostPid() != 0) {
        printf("setDevPid init failed\n");
        return;
    }
    g_h2dInfo->devPid = pid;  // 写入共享内存，所有进程可见
}

void InitHdcId()
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        g_h2dInfo[i].hostPhyId = i;
        g_h2dInfo[i].devPhyId = 0;
        g_h2dInfo[i].hostLogicId = i;
        g_h2dInfo[i].devLogicId = 0;
    }
}

drvError_t drvGetDevNum(unsigned int *numDev)
{
    return DRV_ERROR_NONE;
}

drvError_t drvGetLocalDevIDByHostDevID(unsigned int devId, unsigned int *chipId)
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (g_h2dInfo[i].hostPhyId == devId) {
            *chipId = g_h2dInfo[i].hostLogicId;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t drvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex)
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (g_h2dInfo[i].hostPhyId == phyId) {
            *devIndex = g_h2dInfo[i].devLogicId;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t drvGetDevIdByLocalDevId(unsigned int localDevId, unsigned int *devId)
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (g_h2dInfo[i].hostLogicId == localDevId) {
            *devId = g_h2dInfo[i].devLogicId;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_NONE;
}

//host phy to host logic
drvError_t drvDeviceGetPhyIdByIndex(unsigned int devIndex, unsigned int *phyId)
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (g_h2dInfo[i].hostLogicId == devIndex) {
            *phyId = g_h2dInfo[i].hostPhyId;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t halQueryDevPid(struct halQueryDevpidInfo info, pid_t *devPid)
{
    if (g_h2dInfo == NULL && ShmInitHostPid() != 0) {
        printf("halQueryDevPid init failed\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    *devPid = g_h2dInfo->devPid;
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
    if (g_h2dInfo == NULL && ShmInitHostPid() != 0) {
        printf("drvQueryProcessHostPid init failed\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    *hostPid = g_h2dInfo->hostPid;
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
    HdcSessionT *pSession = (HdcSessionT *)session;
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if(g_hdcMgr[i] == NULL) {
            continue;
        }
        if (pSession->sessionId == g_hdcMgr[i]->sessionId) {
            switch(attr) {
                case HDC_SESSION_ATTR_PEER_CREATE_PID:
                    *value = 0; // 模拟返回对端创建进程ID
                    break;
                default:
                    return DRV_ERROR_INVALID_VALUE;
            }
            return DRV_ERROR_NONE;
        }
    }
    return DRV_ERROR_INVALID_VALUE;
}

hdcError_t drvHdcGetCapacity(struct drvHdcCapacity *capacity)
{
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcClientCreate(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag)
{
    *client = calloc(1, 1);
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcClientDestroy(HDC_CLIENT client)
{
    free(client);
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSessionConnect(int peerNode, int peerDevid, HDC_CLIENT client, HDC_SESSION *session)
{
    *session = calloc(1, sizeof(HdcSessionT));
    HdcSessionT *pSession = (HdcSessionT *)(*session);
    pSession->hostPid = getpid();
    return DRV_ERROR_NONE;
}

hdcError_t halHdcSessionConnectEx(int peerNode, int peerDevid, int peerPid, HDC_CLIENT client,
    HDC_SESSION *pSession)
{
    // 1. 入参合法性检查
    if (pSession == NULL) {
        printf("[ERR][ConnectEx] 输出会话指针为空\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (peerDevid < 0) {
        printf("[ERR][ConnectEx] 对端设备ID非法\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    // 2. 分配会话内存
    HdcSessionT *pSessionNode = (HdcSessionT *)calloc(1, sizeof(HdcSessionT));
    if (pSessionNode == NULL) {
        printf("[ERR][ConnectEx] 会话内存分配失败\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    // ==================== 核心：创建TCP客户端Socket ====================
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[ERR][ConnectEx] Socket创建失败");
        free(pSessionNode);
        *pSession = NULL;
        return DRV_ERROR_INVALID_VALUE;
    }

    // ==================== 配置服务端地址并发起连接 ====================
    int listen_port = GetPeerPortByDevid(peerDevid);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(listen_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // 本地连接（可改对端IP）

retry:
    int ret = connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("[WARNING][HOST] connect failed,IP: %s, Port: %d , ret=%d\n", "127.0.0.1", listen_port, ret);
        usleep(10000); //100ms
        goto retry;
    }

    // ==================== 填充会话所有字段（驱动标准格式） ====================
    pSessionNode->sessionId      = AllocSessionId();    // 分配唯一会话ID
    pSessionNode->conn_fd        = sock_fd;             // 客户端通信FD

    pSessionNode->lastRecvStatus = 0;                   // 初始状态正常
    pSessionNode->isUsed         = 1;                   // 标记会话已启用

    // 填充对端信息（入参传入）
    pSessionNode->peerNode       = peerNode;


    // 3. 返回创建好的会话
    *pSession = pSessionNode;

    printf("[HOST]conn success sessionId=%d, conn_fd=%d, local PID=%d, remote devID=%d, peerPid=%d, peerNode=%d\n",
           pSessionNode->sessionId, sock_fd, getpid(), peerDevid, peerPid, peerNode);

    return DRV_ERROR_NONE;
}

hdcError_t drvHdcServerCreate(int devid, int serviceType, HDC_SERVER *pServer)
{
    ServerInfoT *server = NULL;
    // 1. 入参合法性检查
    if (devid < 0 || devid >= MAX_DEV_ID) {
        printf("[ERR] devid(%d) invalid 0~%d\n", devid, MAX_DEV_ID-1);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (pServer == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }
    // 4. 分配对外Server句柄
    server = (ServerInfoT *)calloc(1, sizeof(ServerInfoT));
    if (server == NULL) {
        printf("[ERR] ServerInfo 内存分配失败\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    // ==================== 核心：创建TCP Server + 启动监听 ====================
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("[ERR] socket 创建失败");
        free(server);
        server = NULL;
        return DRV_ERROR_INVALID_VALUE;
    }

    // 设置端口复用（驱动必备）
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 获取监听端口
    int listen_port = GetPortByServiceType(serviceType);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 监听所有网卡

    // 绑定端口
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ERR] bind 绑定失败");
        close(listen_fd);
        free(server);
        server = NULL;
        return DRV_ERROR_INVALID_VALUE;
    }
    printf("[SUCCESS] Server created and bound to port %d\n", listen_port);
    // 启动监听（驱动创建即监听）
    if (listen(listen_fd, LISTEN_BACKLOG) < 0) {
        perror("[ERR] listen 启动失败");
        close(listen_fd);
        free(server);
        server = NULL;
        return DRV_ERROR_INVALID_VALUE;
    }

    printf("[SUCCESS] Server is listening on port %d\n", listen_port);
    // ==================== 填充对外Server句柄 ====================
    server->listen_fd = listen_fd;
    server->phyId     = devid;

    *pServer = (void *)server;
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcServerDestroy(HDC_SERVER server)
{
    free(server);
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSessionAccept(HDC_SERVER server, HDC_SESSION *session)
{
    ServerInfoT *pServer = (ServerInfoT *)server;
    // 1. 入参合法性检查
    if (pServer == NULL || session == NULL) {
        printf("[ERR] 空指针\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    // 2. 分配会话内存
    HdcSessionT *pSession = (HdcSessionT *)calloc(1, sizeof(HdcSessionT));
    if (pSession == NULL) {
        printf("[ERR] 会话内存分配失败\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    // ==================== 核心：接受TCP连接 ====================
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    // accept阻塞等待客户端连接，返回通信fd
    int conn_fd = accept(pServer->listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (conn_fd < 0) {
        perror("[ERR] accept 接受连接失败");
        free(pSession);
        return DRV_ERROR_INVALID_VALUE;
    }

    // ==================== 填充会话所有字段 ====================
    pSession->sessionId      = AllocSessionId();    // 分配唯一ID
    pSession->conn_fd        = conn_fd;             // 客户端通信fd
    pSession->hostPhyId          = pServer->phyId;       // 所属物理通道
    pSession->lastRecvStatus = 0;                   // 初始状态正常
    pSession->isUsed         = 1;                   // 标记已使用

    // 输出会话句柄
    *session = pSession;
    g_hdcMgr[pServer->phyId] = pSession;
    printf("[SUCCESS] 接受连接成功 → hostPhyId=%u, sessionId=%d, conn_fd=%d\n",
           pSession->hostPhyId, pSession->sessionId, pSession->conn_fd);

    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSessionClose(HDC_SESSION session)
{
    // free(session);
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcFreeMsg(struct drvHdcMsg *msg)
{
    free(msg);
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcReuseMsg(struct drvHdcMsg *msg)
{
    for(int i = 0; i < msg->count; i++) {
        msg->bufList[i].pBuf = NULL;
        msg->bufList[i].len = 0;
    }

    return DRV_ERROR_NONE;
}

hdcError_t drvHdcAddMsgBuffer(struct drvHdcMsg *msg, char *pBuf, int len)
{
    for(int i = 0; i < msg->count; i++) {
        if (msg->bufList[i].pBuf == NULL) {
            msg->bufList[i].pBuf = pBuf;
            msg->bufList[i].len = len;
            return DRV_ERROR_NONE;
        }
    }
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcGetMsgBuffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    if (index < 0 || index >= msg->count) {
        return DRV_ERROR_INVALID_VALUE;
    }
    *pBuf = msg->bufList[index].pBuf;
    *pLen = msg->bufList[index].len;
    return DRV_ERROR_NONE;
}

static hdcError_t safe_read(int fd, void *buf, size_t len) {
    size_t has_read = 0;
    while (has_read < len) {
        ssize_t r = read(fd, (char*)buf + has_read, len - has_read);
        if (r == 0) {
            return DRV_ERROR_INVALID_VALUE;
        }
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return DRV_ERROR_INVALID_VALUE;
            }
            return DRV_ERROR_INVALID_VALUE;
        }
        has_read += r;
    }
    return DRV_ERROR_NONE;
}

hdcError_t halHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout)
{
    (void)bufLen; // 要求2：忽略 bufLen
    (void)flag;

    HdcSessionT *pSession = (HdcSessionT *)session;
    hdcError_t ret = DRV_ERROR_NONE;
    int expected_count = 0; // 上层期望的 count
    int received_count = 0; // 网络实际收到的 count
    int *tmp_lens = NULL;   // 临时存储接收的长度列表
    int allocated_count = 0;// 记录已分配了多少段，用于错误回滚

    // ===================== 1. 基础入参校验 =====================
    if (pMsg == NULL || recvBufCount == NULL || pSession == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }
    if (pSession->conn_fd < 0) {
        return DRV_ERROR_INVALID_VALUE;
    }

    // 要求4：校验上层传入的 count（先保存期望值）
    expected_count = pMsg->count;
    if (expected_count <= 0 || expected_count > 128) {
        fprintf(stderr, "[DEVICE] Invalid expected count=%d from caller\n", expected_count);
        return DRV_ERROR_INVALID_VALUE;
    }

    *recvBufCount = 0;

    // ===================== 2. 超时设置（要求1：保持超时逻辑） =====================
    struct timeval old_tv, new_tv;
    socklen_t old_tv_len = sizeof(old_tv);
    if (timeout > 0) {
        getsockopt(pSession->conn_fd, SOL_SOCKET, SO_RCVTIMEO, &old_tv, &old_tv_len);
        new_tv.tv_sec = timeout / 1000;
        new_tv.tv_usec = (timeout % 1000) * 1000;
        setsockopt(pSession->conn_fd, SOL_SOCKET, SO_RCVTIMEO, &new_tv, sizeof(new_tv));
    }

    // ===================== 3. 接收消息头 & 要求4：校验 Count =====================
    ret = safe_read(pSession->conn_fd, &received_count, sizeof(int));
    if (ret != DRV_ERROR_NONE) {
        goto EXIT_RESTORE_TV;
    }

    // 【核心校验】期望 vs 实际
    if (received_count != expected_count) {
        fprintf(stderr, "[DEVICE] Count Mismatch! Expected=%d, Received=%d\n", 
                expected_count, received_count);
        ret = DRV_ERROR_INVALID_VALUE;
        goto EXIT_RESTORE_TV;
    }

    // ===================== 4. 接收分段长度列表 =====================
    tmp_lens = (int*)malloc(sizeof(int) * received_count);
    if (tmp_lens == NULL) {
        ret = DRV_ERROR_INVALID_VALUE;
        goto EXIT_RESTORE_TV;
    }

    ret = safe_read(pSession->conn_fd, tmp_lens, sizeof(int) * received_count);
    if (ret != DRV_ERROR_NONE) {
        goto EXIT_FREE_TMP;
    }

    // ===================== 5. 要求3：在 recv 中分配 buff 并接收数据 =====================
    for (int i = 0; i < received_count; i++) {
        // 5.1 校验长度合法性
        if (tmp_lens[i] <= 0) {
            ret = DRV_ERROR_INVALID_VALUE;
            goto EXIT_FREE_ALLOCATED;
        }

        // 5.2 分配内存（要求3）
        pMsg->bufList[i].len = tmp_lens[i];
        pMsg->bufList[i].pBuf = (char*)malloc(tmp_lens[i]);
        if (pMsg->bufList[i].pBuf == NULL) {
            ret = DRV_ERROR_INVALID_VALUE;
            goto EXIT_FREE_ALLOCATED;
        }
        allocated_count = i + 1;

        // 5.3 接收数据
        ret = safe_read(pSession->conn_fd, pMsg->bufList[i].pBuf, tmp_lens[i]);
        if (ret != DRV_ERROR_NONE) {
            goto EXIT_FREE_ALLOCATED;
        }

        *recvBufCount += tmp_lens[i];
    }

    // ===================== 6. 成功路径 =====================
    ret = DRV_ERROR_NONE;
    goto EXIT_FREE_TMP;

    // ===================== 7. 错误处理与内存回滚 =====================
EXIT_FREE_ALLOCATED:
    // 回滚：释放已经分配的 buffer
    for (int i = 0; i < allocated_count; i++) {
        if (pMsg->bufList[i].pBuf != NULL) {
            free(pMsg->bufList[i].pBuf);
            pMsg->bufList[i].pBuf = NULL;
        }
    }
    pMsg->count = 0; // 标记消息无效

EXIT_FREE_TMP:
    if (tmp_lens != NULL) {
        free(tmp_lens);
    }

EXIT_RESTORE_TV:
    // 恢复超时设置
    if (timeout > 0) {
        setsockopt(pSession->conn_fd, SOL_SOCKET, SO_RCVTIMEO, &old_tv, sizeof(old_tv));
    }

    return ret;
}

hdcError_t halHdcSend(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout)
{
    (void)flag;
    hdcError_t ret = DRV_ERROR_NONE;
    struct timeval old_tv, new_tv;
    socklen_t old_tv_len = sizeof(old_tv);
    int *tmp_lens = NULL;
    struct iovec *iov = NULL;
    int iov_cnt = 0;
    size_t total_len = 0;
    size_t sent = 0;

    // ===================== 1. 基础入参校验 =====================
    if (session == NULL || pMsg == NULL) {
        fprintf(stderr, "[HOST] Null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    HdcSessionT *pSession = (HdcSessionT *)session;
    if (pSession->conn_fd < 0 || !pSession->isUsed) {
        fprintf(stderr, "[HOST] Invalid session\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (pMsg->count <= 0 || pMsg->count > 128) {
        fprintf(stderr, "[HOST] Invalid count=%d\n", pMsg->count);
        return DRV_ERROR_INVALID_VALUE;
    }

    // ===================== 2. 超时设置（保存并恢复） =====================
    if (timeout > 0) {
        getsockopt(pSession->conn_fd, SOL_SOCKET, SO_SNDTIMEO, &old_tv, &old_tv_len);
        new_tv.tv_sec = timeout / 1000;
        new_tv.tv_usec = (timeout % 1000) * 1000;
        setsockopt(pSession->conn_fd, SOL_SOCKET, SO_SNDTIMEO, &new_tv, sizeof(new_tv));
    }

    // ===================== 3. 准备发送数据（Header + Payload） =====================
    
    // 3.1 准备长度列表
    tmp_lens = (int*)malloc(sizeof(int) * pMsg->count);
    if (tmp_lens == NULL) {
        ret = DRV_ERROR_INVALID_VALUE;
        goto EXIT_RESTORE_TV;
    }
    for (int i = 0; i < pMsg->count; i++) {
        if (pMsg->bufList[i].pBuf == NULL || pMsg->bufList[i].len <= 0) {
            fprintf(stderr, "[HOST] Invalid buffer %d\n", i);
            ret = DRV_ERROR_INVALID_VALUE;
            goto EXIT_FREE_TMP;
        }
        tmp_lens[i] = pMsg->bufList[i].len;
    }

    // 3.2 构建完整的 iovec (Count + Lens + Payload)
    iov_cnt = 1 + 1 + pMsg->count; // 1个count + 1个lens数组 + N个payload
    iov = (struct iovec*)malloc(sizeof(struct iovec) * iov_cnt);
    if (iov == NULL) {
        ret = DRV_ERROR_INVALID_VALUE;
        goto EXIT_FREE_TMP;
    }

    iov[0].iov_base = &pMsg->count;  // 第1部分：Count
    iov[0].iov_len  = sizeof(int);
    
    iov[1].iov_base = tmp_lens;       // 第2部分：Lengths 列表
    iov[1].iov_len  = sizeof(int) * pMsg->count;

    for (int i = 0; i < pMsg->count; i++) { // 第3部分：Payload
        iov[2 + i].iov_base = pMsg->bufList[i].pBuf;
        iov[2 + i].iov_len  = pMsg->bufList[i].len;
    }

    // 计算总长度
    total_len = 0;
    for (int i = 0; i < iov_cnt; i++) total_len += iov[i].iov_len;

    // ===================== 4. 循环发送（处理部分写入） =====================
    sent = 0;
    int iov_idx = 0;
    size_t iov_offset = 0;
    
    while (sent < total_len) {
        // 调整 iovec 指针以跳过已发送的数据
        struct iovec *cur_iov = &iov[iov_idx];
        cur_iov->iov_base = (char*)cur_iov->iov_base + iov_offset;
        cur_iov->iov_len -= iov_offset;

        ssize_t w_ret = writev(pSession->conn_fd, cur_iov, iov_cnt - iov_idx);
        if (w_ret < 0) {
            if (errno == EINTR) continue; // 被信号中断，重试
            fprintf(stderr, "[HOST] Send failed, errno=%d\n", errno);
            ret = DRV_ERROR_INVALID_VALUE;
            goto EXIT_FREE_IOV;
        }

        sent += w_ret;

        // 更新 iov 索引和偏移量
        size_t remaining = w_ret;
        while (iov_idx < iov_cnt && remaining >= iov[iov_idx].iov_len) {
            remaining -= iov[iov_idx].iov_len;
            iov_idx++;
            iov_offset = 0;
        }
        if (iov_idx < iov_cnt) {
            iov_offset = remaining;
        }
    }

    fprintf(stderr, "[HOST] Send success: total=%zd\n", sent);
    ret = DRV_ERROR_NONE;

    // ===================== 5. 资源清理 =====================
EXIT_FREE_IOV:
    if (iov != NULL) free(iov);
EXIT_FREE_TMP:
    if (tmp_lens != NULL) free(tmp_lens);
EXIT_RESTORE_TV:
    if (timeout > 0) {
        setsockopt(pSession->conn_fd, SOL_SOCKET, SO_SNDTIMEO, &old_tv, sizeof(old_tv));
    }
    return ret;
}

hdcError_t drvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count)
{
    *ppMsg = calloc(1, sizeof(struct drvHdcMsg) + count * sizeof(struct drvHdcMsgBuf));
    if (*ppMsg == NULL) {
        printf("[ERR] drvHdcAllocMsg 内存分配失败\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    (*ppMsg)->count = count;
    printf("[HOST] AllocMsg success: session=%d, count=%d\n", ((HdcSessionT *)session)->sessionId, count);
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
