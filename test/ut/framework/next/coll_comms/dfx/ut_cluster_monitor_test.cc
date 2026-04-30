/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FIT FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include <memory>
#include <string>
#include <cstring>
#include "hccl_types.h"
#define private public
#include "dfx/cluster_monitor/cluster_monitor.h"
#undef private
#include "hccl_comm_socket_c_adpt.h"

using namespace hcomm;

class TestClusterMonitor : public ::testing::Test {
public:
    void SetUp() override {
        testing::Test::SetUp();
    }
    void TearDown() override {
        testing::Test::TearDown();
        GlobalMockObject::verify();
    }

    static ClusterMonitor g_monitor;
};

ClusterMonitor TestClusterMonitor::g_monitor;

TEST_F(TestClusterMonitor, CreateMonitorLinksAsync)
{
    s32 ret;

    HcclCommSocketDesc socketDesc;
    std::string testTag = "monitor_test_tag";
    struct in_addr localAddr;
    struct in_addr remoteAddr;
    inet_pton(AF_INET, "127.0.0.1", &localAddr);
    socketDesc.localEndpoint.commAddr.addr = localAddr;
    socketDesc.localEndpoint.commAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    socketDesc.localEndpoint.loc.locType = EndpointLocType::ENDPOINT_LOC_TYPE_DEVICE;
    inet_pton(AF_INET, "192.186.0.1", &remoteAddr);
    socketDesc.remoteEndpoint.commAddr.addr = remoteAddr;
    socketDesc.remoteEndpoint.commAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    socketDesc.remoteEndpoint.loc.locType = EndpointLocType::ENDPOINT_LOC_TYPE_DEVICE;
    ret = memcpy_s(socketDesc.tag, HCCL_SOCKET_TAG_LEN, testTag.c_str(), testTag.length() + 1);
    EXPECT_EQ(ret, EOK);
    socketDesc.role = HCOMM_SOCKET_ROLE_CLIENT;
    socketDesc.listenPort = 8080;

    HcclCommSocketDesc socketDesc1;
    testTag = "monitor_test_tag_1";
    inet_pton(AF_INET, "127.0.0.1", &localAddr);
    socketDesc1.localEndpoint.commAddr.addr = localAddr;
    socketDesc1.localEndpoint.commAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    socketDesc1.localEndpoint.loc.locType = EndpointLocType::ENDPOINT_LOC_TYPE_HOST;
    inet_pton(AF_INET, "192.186.0.2", &remoteAddr);
    socketDesc1.remoteEndpoint.commAddr.addr = remoteAddr;
    socketDesc1.remoteEndpoint.commAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    socketDesc1.remoteEndpoint.loc.locType = EndpointLocType::ENDPOINT_LOC_TYPE_DEVICE;
    ret = memcpy_s(socketDesc1.tag, HCCL_SOCKET_TAG_LEN, testTag.c_str(), testTag.length() + 1);
    EXPECT_EQ(ret, EOK);
    socketDesc1.role = HCOMM_SOCKET_ROLE_CLIENT;
    socketDesc1.listenPort = 8080;

    HcclCommSocketDesc socketDesc2;
    testTag = "monitor_test_tag_2";
    inet_pton(AF_INET, "127.0.0.1", &localAddr);
    socketDesc2.localEndpoint.commAddr.addr = localAddr;
    socketDesc2.localEndpoint.commAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    socketDesc2.localEndpoint.loc.locType = EndpointLocType::ENDPOINT_LOC_TYPE_HOST;
    inet_pton(AF_INET, "192.186.0.3", &remoteAddr);
    socketDesc2.remoteEndpoint.commAddr.addr = remoteAddr;
    socketDesc2.remoteEndpoint.commAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    socketDesc2.remoteEndpoint.loc.locType = EndpointLocType::ENDPOINT_LOC_TYPE_HOST;
    ret = memcpy_s(socketDesc2.tag, HCCL_SOCKET_TAG_LEN, testTag.c_str(), testTag.length() + 1);
    EXPECT_EQ(ret, EOK);
    socketDesc2.role = HCOMM_SOCKET_ROLE_CLIENT;
    socketDesc2.listenPort = 8080;

    ClusterMonitorSocketCtx context;
    context.socketDesc = socketDesc;
    context.socketHandler = nullptr;

    ClusterMonitorSocketCtx context1;
    context1.socketDesc = socketDesc1;
    context1.socketHandler = nullptr;

    ClusterMonitorSocketCtx context2;
    context2.socketDesc = socketDesc2;
    context2.socketHandler = nullptr;

    ClusterUIDType uid;
    ret = memcpy_s(uid.id, sizeof(uid.id), "test_uid", sizeof("test_uid") + 1);
    EXPECT_EQ(ret, EOK);

    ClusterUIDType uid1;
    ret = memcpy_s(uid1.id, sizeof(uid1.id), "test_uid1", sizeof("test_uid1") + 1);
    EXPECT_EQ(ret, EOK);

    ClusterUIDType uid2;
    ret = memcpy_s(uid2.id, sizeof(uid2.id), "test_uid2", sizeof("test_uid2") + 1);
    EXPECT_EQ(ret, EOK);
    
    g_monitor.commIdMap_["comm1"].insert(std::make_pair(uid, false));
    g_monitor.commIdMap_["comm1"].insert(std::make_pair(uid1, false));
    g_monitor.commIdMap_["comm1"].insert(std::make_pair(uid2, false));

    std::unique_lock<std::mutex> linksLock(g_monitor.clusertMonitorLinkMtx_);
    g_monitor.clusterLinkContext_["comm1"].push(std::make_pair(uid, context));
    g_monitor.clusterLinkContext_["comm1"].push(std::make_pair(uid1, context1));
    g_monitor.clusterLinkContext_["comm2"].push(std::make_pair(uid2, context2));
    linksLock.unlock();

    g_monitor.CreateHBLinksAsync();

    u32 uncompletedCount = 3;
    auto linkThreadIt = g_monitor.monitorLinkStatusMap_.begin();
    const auto CREATE_LINK_TIMEOUT = std::chrono::seconds(10);
    auto startTime = std::chrono::steady_clock::now();
    while((std::chrono::steady_clock::now() - startTime) <= CREATE_LINK_TIMEOUT) {
        HCCL_ERROR("Waiting for monitor links to complete, uncompletedCount = %u", uncompletedCount);
        if (linkThreadIt == g_monitor.monitorLinkStatusMap_.end()) {
            linkThreadIt = g_monitor.monitorLinkStatusMap_.begin();
        } 
        if (linkThreadIt->second == ClusterMonitor::MonitorLinkStatus::MONITOR_LINK_COMPLETED) {
            uncompletedCount--;
        }
        if (uncompletedCount == 0) {
            break;
        } else {
            linkThreadIt++;
        }
    }

    g_monitor.linkRunningStatus_ = false;
    // 在心跳进程结束之前join所有的建链线程
    for (auto &pair : g_monitor.linkThreadMap_) {
        if (pair.second != nullptr && pair.second->joinable()) {
            pair.second->join();
            HCCL_INFO("[HeartbeatStatusMonitor] thread has joined. Remote uid is [%s]", g_monitor.GetUID(pair.first).c_str());
        }
    }

    EXPECT_EQ(uncompletedCount, 0);
}

TEST_F(TestClusterMonitor, SendMonitorFrame_test)
{
    for (auto iter = g_monitor.uid2SocketRefMap_.begin(); iter != g_monitor.uid2SocketRefMap_.end(); iter++) {
        ClusterUIDType rem = iter->first;
        g_monitor.uid2SocketRefMap_[rem].lostNum++;
        g_monitor.SendFrame(rem, g_monitor.myRankUid_, g_monitor.myRankUid_, ClusterMonitorStatus::CLUSTER_MONITOR_STUCK);
        EXPECT_EQ(g_monitor.uid2SocketRefMap_[rem].sendBuffer.size(), 0);
    }
}

TEST_F(TestClusterMonitor, RecvMonitorFrame_test)
{
    MOCKER_CPP(&ClusterMonitor::ParseFrame)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    uint64_t compSize = 0;
    MOCKER(HcclCommSocketRecvNb)
    .stubs()
    .with(any(), any(), any(), outBoundP(&compSize, sizeof(uint64_t *)))
    .will(invoke(HcclCommSocketRecvNb))    // 第1次走真实函数
    .then(returnValue(HCCL_E_AGAIN));  // 第2次返回特定值

    for (auto iter = g_monitor.uid2SocketRefMap_.begin(); iter != g_monitor.uid2SocketRefMap_.end(); iter++) {
        ClusterUIDType rem = iter->first;
        HcclResult ret = g_monitor.RecvFrame(rem);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}