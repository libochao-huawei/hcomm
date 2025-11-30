/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <mpi.h>
#include <vector>
#include <algorithm>
#include <thread>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#include "hccl_network_pub.h"
#include "local_rdma_rma_buffer.h"
#include "local_ipc_rma_buffer.h"
#include "remote_rdma_rma_buffer.h"
#include "remote_ipc_rma_buffer.h"
#include "adapter_hccp_common.h"
#include "adapter_rts_common.h"
#include "hccl_socket.h"
#include "sal_pub.h"
#include "mem_device_pub.h"
#include "p2p_mgmt_pub.h"
#include "es_private.h"
#include "stream_pub.h"
#include "llt_hccl_stub_pub.h"
#include "mem_mapping_manager.h"
#define private public
#define protected public
#include "dlhal_function.h"
#undef protected
#undef private

using namespace std;
using namespace hccl;

class RmaBufferTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--RmaBufferTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RmaBufferTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

HcclResult Work(s32 deviceId, std::string ip, std::string dstIp, HcclResult &ret)
{
    // 指定集合通信操作使用的设备
    ret = hrtSetDevice(deviceId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 使能p2p
    std::vector<u32> enableP2PDevices;
    if (deviceId == 0) {
        enableP2PDevices.push_back(1);
    } else {
        enableP2PDevices.push_back(0);
    }
    ret = P2PMgmtPub::EnableP2P(enableP2PDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmtPub::WaitP2PEnabled(enableP2PDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 初始化网卡
    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, deviceId, deviceId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclIpAddress ipAddr(ip);
    HcclIpAddress dstIpAddr(dstIp);

    HcclNetDevCtx netDevCtx = nullptr;
    ret = HcclNetOpenDev(&netDevCtx, NicType::DEVICE_NIC_TYPE, deviceId, deviceId, ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string tag = "HCCL";
    HcclSocket serverSocket(netDevCtx, 16666);
    ret = serverSocket.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = serverSocket.Listen();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<SocketWlistInfo> wlistInfosVec;
    SocketWlistInfo wlistInfo;
    wlistInfo.connLimit = HOST_SOCKET_CONN_LIMIT;
    wlistInfo.remoteIp.addr = dstIpAddr.GetBinaryAddress().addr;
    wlistInfo.remoteIp.addr6 = dstIpAddr.GetBinaryAddress().addr6;
    s32 sRet = memcpy_s(&wlistInfo.tag[0], sizeof(wlistInfo.tag), tag.c_str(), tag.size() + 1);
    if (sRet != EOK) {
        HCCL_ERROR("[Add][SocketWhiteList]memory copy failed. errorno[%d]", sRet);
        return HCCL_E_MEMORY;
    }
    wlistInfosVec.push_back(wlistInfo);
    ret = serverSocket.AddWhiteList(wlistInfosVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclSocket clientSocket(tag, netDevCtx, dstIpAddr, 16666, HcclSocketRole::SOCKET_ROLE_CLIENT);
    ret = clientSocket.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // server connect socket
    std::shared_ptr<HcclSocket> connectSocket;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(20);
#if 1
    if (deviceId == 0) {
        ret = clientSocket.Connect();
        EXPECT_EQ(ret, HCCL_SUCCESS);
        while (true) {
           if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                HCCL_ERROR("[Get][Connection]topo exchange agent get socket timeout! timeout[%lld]", timeout);
                ret = HCCL_E_TIMEOUT;
                return HCCL_E_TIMEOUT;
            }
            HcclSocketStatus status = clientSocket.GetStatus();
            if (status == HcclSocketStatus::SOCKET_CONNECTING) {
                SaluSleep(ONE_MILLISECOND_OF_USLEEP);
            } else if (status != HcclSocketStatus::SOCKET_OK) {
                HCCL_ERROR("[Get][Connection]server: get socket failed ret[%d]", status);
                ret = HCCL_E_TCP_CONNECT;
                return HCCL_E_TCP_CONNECT;
            } else {
                printf("TopoInfoExchangeAgent get socket success.\n");
                break;
            }
        }
    } else {
        while (true) {
           if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                HCCL_ERROR("[Get][Connection]topo exchange agent get socket timeout! timeout[%lld]", timeout);
                ret = HCCL_E_TIMEOUT;
                return HCCL_E_TIMEOUT;
            }
            ret = serverSocket.Accept(tag, connectSocket);
            if (ret == HCCL_SUCCESS) {
                HCCL_RUN_INFO("listenSocket_->Accept completed.");
                break;
            }
        }

    }
#endif

    //申请HOST内存
    unsigned long long int SMALL_PAGE_SIZE = 4096;
    unsigned long long int size = 4096;
    char *hostPtr = new char[size + SMALL_PAGE_SIZE];
    void *ptr = reinterpret_cast<void *>(Align<unsigned long long int>(reinterpret_cast<u64>(hostPtr), SMALL_PAGE_SIZE));

    // 申请device内存
    DeviceMem devicePtr = DeviceMem::alloc(size);
    std::string test = "HCCL TESTxx";
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = hrtMemAsyncCopy(devicePtr.ptr(), test.length(), test.data(), test.length(),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 构造rmabuff
    std::shared_ptr<LocalRdmaRmaBuffer> localRmaBuffer =
        std::make_shared<LocalRdmaRmaBuffer>(netDevCtx, ptr, size, RmaMemType::HOST);
    ret = localRmaBuffer->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<LocalRdmaRmaBuffer> localRmaBufferDev =
        std::make_shared<LocalRdmaRmaBuffer>(netDevCtx, devicePtr.ptr(), size);
    ret = localRmaBufferDev->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<RemoteRdmaRmaBuffer> remoteRmaBuffer = std::make_shared<RemoteRdmaRmaBuffer>();
    std::shared_ptr<RemoteRdmaRmaBuffer> remoteDevRmaBuffer = std::make_shared<RemoteRdmaRmaBuffer>();
#if 1
    if (deviceId == 0) {
        std::string data = localRmaBuffer->Serialize();
        int length = 256 - data.length();
        std::string padd(length, '0');
        data += padd;
        ret = clientSocket.Send(&data[0], data.size());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        std::string devData = localRmaBufferDev->Serialize();
        length = 256 - devData.length();
        std::string padd1(length, '0');
        devData += padd1;
        ret = clientSocket.Send(&devData[0], devData.size());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        printf("dev[%d]:host ptr[%p], size[%llu], RmaType[%d], devPtr[%p], memType[%d], key[%u]\n", deviceId,
            localRmaBuffer->GetAddr(), localRmaBuffer->GetSize(), localRmaBuffer->GetRmaType(),
            localRmaBuffer->GetDevAddr(), localRmaBuffer->GetMemType(), localRmaBuffer->GetKey());
        printf("dev[%d]:device ptr[%p], size[%llu], RmaType[%d], devPtr[%p], memType[%d], key[%u]\n", deviceId,
            localRmaBufferDev->GetAddr(), localRmaBufferDev->GetSize(), localRmaBufferDev->GetRmaType(),
            localRmaBufferDev->GetDevAddr(), localRmaBufferDev->GetMemType(), localRmaBufferDev->GetKey());
    } else {
        std::string data(256, '\0');
        ret = connectSocket->Recv(&data[0], data.size());
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteRmaBuffer->Deserialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        std::string devData(256, '\0');
        ret = connectSocket->Recv(&devData[0], devData.size());
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteDevRmaBuffer->Deserialize(devData);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        printf("dev[%d]:host ptr[%p], size[%llu], RmaType[%d], devPtr[%p], memType[%d], key[%u]\n", deviceId,
            remoteRmaBuffer->GetAddr(), remoteRmaBuffer->GetSize(), remoteRmaBuffer->GetRmaType(),
            remoteRmaBuffer->GetDevAddr(), remoteRmaBuffer->GetMemType(), remoteRmaBuffer->GetKey());
        printf("dev[%d]:device ptr[%p], size[%llu], RmaType[%d], devPtr[%p], memType[%d], key[%u]\n", deviceId,
            remoteDevRmaBuffer->GetAddr(), remoteDevRmaBuffer->GetSize(), remoteDevRmaBuffer->GetRmaType(),
            remoteDevRmaBuffer->GetDevAddr(), remoteDevRmaBuffer->GetMemType(), remoteDevRmaBuffer->GetKey());
    }
#endif
    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    localRmaBuffer = nullptr;
    localRmaBufferDev = nullptr;
    remoteRmaBuffer = nullptr;
    remoteDevRmaBuffer = nullptr;

    // 销毁网卡资源
    connectSocket = nullptr;
    ret = clientSocket.DeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = serverSocket.DeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetCloseDev(netDevCtx);
    ret = HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, deviceId, deviceId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 资源释放
    devicePtr = DeviceMem();
    delete[] hostPtr;
    ret = hrtResetDevice(deviceId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    return HCCL_SUCCESS;
}

TEST_F(RmaBufferTest, ut_rma_buffer_multi_process)
{
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::vector<s32> ids;
    ids.push_back(0);
    ids.push_back(1);
    std::vector<std::string> ips;
    ips.push_back("192.168.99.121");
    ips.push_back("192.168.99.122");
    std::vector<std::string> dstIps;
    dstIps.push_back("192.168.99.122");
    dstIps.push_back("192.168.99.121");

    MPI_Barrier(MPI_COMM_WORLD);
    HcclResult execRet;
    HcclResult ret = Work(ids[rank], ips[rank], dstIps[rank], execRet);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Barrier(MPI_COMM_WORLD);
}

TEST_F(RmaBufferTest, ut_rma_buffer_single_process)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::vector<s32> ids;
    ids.push_back(0);
    ids.push_back(1);
    std::vector<std::string> ips;
    ips.push_back("192.168.99.121");
    ips.push_back("192.168.99.122");
    std::vector<std::string> dstIps;
    dstIps.push_back("192.168.99.122");
    dstIps.push_back("192.168.99.121");

    if (rank == 0) {
        HcclResult execRet[2];
        std::vector<std::unique_ptr<std::thread>> threads(2);
        for (unsigned int i = 0; i < 2; ++i) {
            threads[i].reset(new (std::nothrow) std::thread(&Work, ids[i], ips[i], dstIps[i], std::ref(execRet[i])));
            EXPECT_NE(nullptr, threads[i]);
        }
        for (auto& thread : threads) {
            thread->join();
        }
        for (unsigned int i = 0; i < 2; ++i) {
            EXPECT_EQ(HCCL_SUCCESS, execRet[i]);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

TEST_F(RmaBufferTest, ut_mem_mapping_error)
{
    DlHalFunction::GetInstance().DlHalFunctionInit();
    DlHalFunction::GetInstance().handle_ = nullptr;
    unsigned long long int SMALL_PAGE_SIZE = 4096;
    unsigned long long int size = 4096;
    char *hostPtr = new char[size + SMALL_PAGE_SIZE];
    void *ptr = reinterpret_cast<void *>(Align<unsigned long long int>(reinterpret_cast<u64>(hostPtr), SMALL_PAGE_SIZE));
    void *devPtr = nullptr;

    MemMappingManager::GetInstance(0).GetDevVA(0, ptr, size, devPtr);
    MemMappingManager::GetInstance(0).ReleaseDevVA(0, ptr, size);
    MemMappingManager::GetInstance(HOST_DEVICE_ID).GetDevVA(0, ptr, size, devPtr);
    MemMappingManager::GetInstance(HOST_DEVICE_ID).ReleaseDevVA(0, ptr, size);
    delete[] hostPtr;
    GlobalMockObject::verify();
}

TEST_F(RmaBufferTest, ut_getDevVA_isBackup)
{
    DlHalFunction::GetInstance().DlHalFunctionInit();
    DlHalFunction::GetInstance().handle_ = nullptr;
    unsigned long long int SMALL_PAGE_SIZE = 4096;
    unsigned long long int size = 4096;
    char *hostPtr = new char[size + SMALL_PAGE_SIZE];
    void *ptr = reinterpret_cast<void *>(Align<unsigned long long int>(reinterpret_cast<u64>(hostPtr), SMALL_PAGE_SIZE));
    void *devPtr = nullptr;

    MemMappingManager::GetInstance(0).GetDevVA(0, ptr, size, devPtr);
    MemMappingManager::GetInstance(0).ReleaseDevVA(0, ptr, size);
    MemMappingManager::GetInstance(HOST_DEVICE_ID).GetDevVA(0, ptr, size, devPtr);
    MemMappingManager::GetInstance(HOST_DEVICE_ID).ReleaseDevVA(0, ptr, size);
    delete[] hostPtr;
    GlobalMockObject::verify();
}