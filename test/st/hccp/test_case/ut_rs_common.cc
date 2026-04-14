/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_system_test_base.h"
#include "hccp.h"
#include <unordered_map>
class HccpSystemTest : public HccpSystemTestBase {

};

TEST_F(HccpSystemTest, HelloWord)
{
    const std::vector<int> test_phyids = {60};

    RunTests(test_phyids, [](int phyid)->int {
        TEST_LOG_INFO(phyid, "hello world!");
        return 0;
    });
}

TEST_F(HccpSystemTest, MultiHostTlsTest)
{
    const std::vector<int> test_phyids = {1,5};

    RunTests(test_phyids, [](int phyid)->int {
        TEST_LOG_INFO(phyid, "Running RaInit...");
        RaInitConfig config;
        config.phyId = phyid;
        config.nicPosition = 1;
        config.hdcType = 18;
        config.enableHdcAsync = false;
        int ret = RaInit(&config);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaInit failed ret=%d", ret);
            return ret;
        }
        bool tls_enable = false;
        RaInfo info{.mode = 1, .phyId = phyid};

        TEST_LOG_INFO(phyid, "Running RaGetTlsEnable...");
        ret = RaGetTlsEnable(&info, &tls_enable);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaGetTlsEnable failed ret=%d", ret);
            RaDeinit(&config);
        }
        ret = RaDeinit(&config);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaDeinit failed ret=%d", ret);
            return ret;
        }

        TEST_LOG_INFO(phyid, "TLS enable result: %d", tls_enable);
        return ret;
    });
}


TEST_F(HccpSystemTest, d2dSocketSendRecv)
{
    struct TestConfig {
        struct RaInitConfig config;
        struct SocketInitInfoT socketInit;
    };

    std::unordered_map<int, TestConfig> config_map = {
        std::pair<int, TestConfig>{1, {{1, 1, 18, false},{{1, AF_INET, 16777343}, 4}}},
        std::pair<int, TestConfig>{5, {{5, 1, 18, false},{{5, AF_INET, 16777343}, 4}}}
    };

    const std::vector<int> test_phyids = {1,5};

    RunTests(test_phyids, [&](int phyid)->int {
        TEST_LOG_INFO(phyid, "Running RaInit...");
        char sendBufServer[] = "Hello HCCP sever!";
        char sendBufClient[] = "Hello HCCP client!";
        char recvBuf[256] = {0};  // 接收缓冲区清零（关键）

        unsigned long long bufLenServer = sizeof(sendBufServer);
        unsigned long long bufLenClient = sizeof(sendBufClient);
        unsigned long long leftSize = 0;
        unsigned long long sentSize = 0;
        unsigned long long recvSize = 0;
                unsigned int num = 0;
        char *buf = NULL;
        int ret = RaInit(&(config_map[phyid].config));
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaInit failed ret=%d", ret);
            return ret;
        }
        void *socketHandle = NULL;

        ret = RaSocketInitV1(config_map[phyid].config.nicPosition, config_map[phyid].socketInit, &socketHandle);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaSocketInitV1 failed ret=%d", ret);
            goto err;
        }

        struct SocketListenInfoT serverSocket;
        serverSocket.socketHandle = socketHandle;
        serverSocket.port = 15666;
        if(phyid == 1) {
            ret = RaSocketListenStart(&serverSocket, 1);
            if (ret != 0) {
                TEST_LOG_ERROR(phyid, "RaSocketInitV1 failed ret=%d", ret);
                goto err;
            }
        }

        struct SocketWlistInfoT whiteList;
        whiteList.remoteIp.addr.s_addr = 16777343;
        whiteList.connLimit = 1;
        sprintf(whiteList.tag, "HELLO WORD!");
        ret = RaSocketWhiteListAdd(socketHandle, &whiteList, 1);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaSocketWhiteListAdd failed ret=%d", ret);
            goto err;
        }

        struct SocketConnectInfoT conn;
        conn.socketHandle = socketHandle;
        conn.port = 15666;
        sprintf(conn.tag, "HELLO WORD!");
        conn.remoteIp.addr.s_addr = 16777343;
        

        struct SocketInfoT sock;
        sock.socketHandle = socketHandle;
        sock.remoteIp.addr.s_addr = 16777343;
        sock.fdHandle = NULL;
        sprintf(sock.tag, "HELLO WORD!");
        
        if(phyid == 5) {
            ret = RaSocketBatchConnect(&conn, 1);
            if (ret != 0) {
                TEST_LOG_ERROR(phyid, "RaSocketBatchConnect failed ret=%d", ret);
                goto err;
            }
            while(num<=0) {
                ret = RaGetSockets(1, &sock, 1, &num);
            }
            if (ret != 0) {
                TEST_LOG_ERROR(phyid, "RaGetSockets failed ret=%d", ret);
                goto err;
            }
        }
        if(phyid==1) {
            while(num<=0) {
                ret = RaGetSockets(0, &sock, 1, &num);
                usleep(100000);
            }
            if (ret != 0) {
                TEST_LOG_ERROR(phyid, "RaGetSockets failed ret=%d", ret);
                goto err;
            }
        }

        if(phyid == 1) { // ======== 服务端逻辑 ========
            // 1. 循环发送：把 sendBuf 完整发出去
            leftSize = bufLenServer;
            buf = sendBufServer;
            if(sock.fdHandle == NULL) {
            }
            while(leftSize > 0) {
                RaSocketSend(sock.fdHandle, buf, leftSize, &sentSize);
                leftSize -= sentSize;
                buf += sentSize;
            }
            printf("[Server] 发送数据：%s\n", sendBufServer);
            // 2. 循环接收：接收客户端发回来的数据（存入 recvBuf）
            leftSize = bufLenClient;  // 期望接收和发送一样长的数据
            buf = recvBuf;      // 指向接收缓冲区
            while(leftSize > 0) {
                RaSocketRecv(sock.fdHandle, buf, leftSize, &recvSize);
                leftSize -= recvSize;
                buf += recvSize;
            }
            // 打印收到的内容
            printf("[Server] 收到客户端数据：%s\n", recvBuf);
        }

        if(phyid == 5) { // ======== 客户端逻辑 ========
            // 1. 循环接收：先接收服务端发来的数据
            leftSize = bufLenServer;
            buf = recvBuf;
            if(sock.fdHandle == NULL) {
            }
            while(leftSize > 0) {
                RaSocketRecv(sock.fdHandle, buf, leftSize, &recvSize);
                leftSize -= recvSize;
                buf += recvSize;
            }
            printf("[Client] 收到服务端数据：%s\n", recvBuf);

            // 2. 循环发送：把自己的 sendBuf 发回给服务端（互相收发）
            leftSize = bufLenClient;
            buf = sendBufClient;
            while(leftSize > 0) {
                RaSocketSend(sock.fdHandle, buf, leftSize, &sentSize);
                leftSize -= sentSize;
                buf += sentSize;
            }
            printf("[Client] 发送数据：%s\n", sendBufClient);
        }
        ret = RaDeinit(&(config_map[phyid].config));
        return ret;

    err:
        RaDeinit(&(config_map[phyid].config));
        return ret;
    });
}