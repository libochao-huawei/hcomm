/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "dispatcher_aicpu.h"
#include "aicpu_zero_copy_exchanger.h"
#undef private
#undef protected
#include "ascend_hal.h"
#include "transport_pub.h"
#include "transport_device_p2p_pub.h"

using namespace std;
using namespace hccl;
class AicpuZeroCopyExchanger_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--AicpuZeroCopyExchanger_UT SetUP--\033[0m" << std::endl;
        // 初始化notifyPool
        notifyPool.reset(new (std::nothrow) NotifyPool());
        machinePara.deviceLogicId = 0;
        machinePara.localDeviceId = 0;
        machinePara.remoteDeviceId = 1;
        machinePara.localUserrank = 0;
        machinePara.localWorldRank = 0;
        machinePara.remoteWorldRank = 1;
        machinePara.remoteUserrank = 1;
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum).stubs().with(any(), outBound(portNum)).will(returnValue(HCCL_SUCCESS));
        // 初始化dispatcher

        DispatcherPub disPatcherTemp(s32(1));
        dispatcher = &disPatcherTemp;
        dispatcher = new (std::nothrow) DispatcherAiCpu(0);
    }
    static void TearDownTestCase()
    {
        delete dispatcher;
        notifyPool = nullptr;
        std::cout << "\033[36m--AicpuZeroCopyExchanger_UT TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        GlobalMockObject::verify();
    }
public:
    static DispatcherPub *dispatcher;
    static std::unique_ptr<NotifyPool> notifyPool;
    static MachinePara machinePara;
};

DispatcherPub * AicpuZeroCopyExchanger_UT::dispatcher = nullptr;
std::unique_ptr<NotifyPool> AicpuZeroCopyExchanger_UT::notifyPool = nullptr;
MachinePara AicpuZeroCopyExchanger_UT::machinePara{};
void gen_levelNSubCommTransport_stub(OpCommTransport &opTransportResponse)
{
    
    u32 rankId = 0;
    constexpr int SINGAL_SUB_COMM_NUM = 4;
    constexpr int LEVEL_SUB_COMM_NUM = 2;
    constexpr int OP_COM_NUM = 1;
    opTransportResponse.resize(OP_COM_NUM);
    for (int opIdx = 0; opIdx < OP_COM_NUM; opIdx++) {
        LevelNSubCommTransport levelNSubCommTransport;
        levelNSubCommTransport.resize(LEVEL_SUB_COMM_NUM);
        for (int levelIdx = 0; levelIdx < LEVEL_SUB_COMM_NUM; levelIdx++) {
            SingleSubCommTransport singleSubCommTransport;
            singleSubCommTransport.transportRequests.resize(SINGAL_SUB_COMM_NUM);
            singleSubCommTransport.links.resize(SINGAL_SUB_COMM_NUM);
            for (int i = 0; i < SINGAL_SUB_COMM_NUM; i++) {
                singleSubCommTransport.transportRequests[i].isValid = true;
                singleSubCommTransport.transportRequests[i].remoteUserRank = rankId;
                singleSubCommTransport.transportRequests[i].inputMemType = TransportMemType::CCL_INPUT;
                singleSubCommTransport.transportRequests[i].outputMemType = TransportMemType::CCL_OUTPUT;

                // 初始化MachinePara
                // 初始化TransportDeviceP2pData
                HcclSignalInfo notifyInfo;
                notifyInfo.addr = 100;
                notifyInfo.devId = 1;
                notifyInfo.rankId = 2;
                notifyInfo.resId = 3;
                notifyInfo.tsId = 4;
                notifyInfo.flag = 1;
                TransportDeviceP2pData transDevP2pData;
                transDevP2pData.inputBufferPtr = nullptr;
                transDevP2pData.outputBufferPtr = nullptr;
                transDevP2pData.ipcPreWaitNotify = std::make_shared<LocalIpcNotify>();
                transDevP2pData.ipcPreWaitNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
                transDevP2pData.ipcPostWaitNotify = std::make_shared<LocalIpcNotify>();
                transDevP2pData.ipcPostWaitNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
                transDevP2pData.ipcPreRecordNotify = std::make_shared<RemoteNotify>();
                transDevP2pData.ipcPreRecordNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
                transDevP2pData.ipcPostRecordNotify = std::make_shared<RemoteNotify>();
                transDevP2pData.ipcPostRecordNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
                DeviceMem signalBuff;
                signalBuff = DeviceMem::alloc(4);
                transDevP2pData.transportAttr.signalRecordBuff.length = 4;
                transDevP2pData.transportAttr.signalRecordBuff.address = reinterpret_cast<u64>(signalBuff.ptr());
                transDevP2pData.transportAttr.linkType = LinkType::LINK_HCCS_SW;
                transDevP2pData.transportAttr.relationship |= HCCL_TRANSPORT_RELATIONSHIP_SAME_SERVER;
                transDevP2pData.transportAttr.relationship |= HCCL_TRANSPORT_RELATIONSHIP_SAME_SUPERPOD;
                TransportPara para{};
                std::shared_ptr<Transport> transDevP2p;
                transDevP2p.reset(new (std::nothrow) Transport(TransportType::TRANS_TYPE_DEVICE_P2P,
                    para,
                    AicpuZeroCopyExchanger_UT::dispatcher,
                    AicpuZeroCopyExchanger_UT::notifyPool,
                    AicpuZeroCopyExchanger_UT::machinePara,
                    transDevP2pData));

                EXPECT_EQ(transDevP2p->Init(), HCCL_SUCCESS);
                std::shared_ptr<TransportDeviceP2p> tmpLink = std::dynamic_pointer_cast<TransportDeviceP2p>(transDevP2p);
                singleSubCommTransport.links[i] = transDevP2p;
                rankId++;
            }
            levelNSubCommTransport[levelIdx] = singleSubCommTransport;
        }
        opTransportResponse[opIdx] = levelNSubCommTransport;
    }
}

TEST_F(AicpuZeroCopyExchanger_UT, ExchangeAddress)
{
    MOCKER(halSdmaCopy).stubs().will(returnValue(0));
    MOCKER(halSdmaBatchCopy).stubs().will(returnValue(0));
    HcclOpResParam *resParam = new HcclOpResParam();
    int rankId_ = 0;
    int rankSize = 2;
    LocalIpc2RemoteAddr ipc2RemoteAddr;
    ipc2RemoteAddr.remoteAddr = 0x1;
    ipc2RemoteAddr.localIpcAddr = 0x1;
    LocalIpc2RemoteAddr ipc2RemoteAddr1;  
    ipc2RemoteAddr1.remoteAddr = 0x2;
    ipc2RemoteAddr1.localIpcAddr = 0x2;
    u64 bufferLen = 4096;
    AicpuZeroCopyExchanger::FlagData zeroCopyLocalAddr1[8];
    u64 buffer[bufferLen];
    memset_s(buffer, bufferLen, 0x1, bufferLen);
    std::string tag = "tag";
    for (u32 i = 0; i < 8; i++) {
        zeroCopyLocalAddr1[i].flag = 1;
        zeroCopyLocalAddr1[i].outAddr = reinterpret_cast<std::uintptr_t>(&buffer);
        zeroCopyLocalAddr1[i].inAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    }
    resParam->zeroCopyIpcPtrs[0] = reinterpret_cast<std::uintptr_t>(&zeroCopyLocalAddr1);
    resParam->zeroCopyIpcPtrs[1] = reinterpret_cast<std::uintptr_t>(&zeroCopyLocalAddr1);
    resParam->zeroCopyIpcPtrs[2] = reinterpret_cast<std::uintptr_t>(&zeroCopyLocalAddr1);
    resParam->zeroCopyIpcPtrs[3] = reinterpret_cast<std::uintptr_t>(&zeroCopyLocalAddr1);
    u32 head = 0;
    u32 tail = 3;
    ZeroCopyRingBufferItem zeroCopyRingBufferItem[8];
    LocalIpc2RemoteAddr remote_addr[8];
    remote_addr[0].devicePhyId = 1;
    remote_addr[0].length = 1;
    remote_addr[0].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[0].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[1].devicePhyId = 1;
    remote_addr[1].length = 1;
    remote_addr[1].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[1].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[2].devicePhyId = 0;
    remote_addr[2].length = 1;
    remote_addr[2].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[2].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[3].devicePhyId = 0;
    remote_addr[3].length = 1;
    remote_addr[3].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[3].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[4].devicePhyId = 1;
    remote_addr[4].length = 1;
    remote_addr[4].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[4].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[5].devicePhyId = 1;
    remote_addr[5].length = 1;
    remote_addr[5].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[5].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[6].devicePhyId = 0;
    remote_addr[6].length = 1;
    remote_addr[6].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[6].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[7].devicePhyId = 0;
    remote_addr[7].length = 1;
    remote_addr[7].localIpcAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    remote_addr[7].remoteAddr = reinterpret_cast<std::uintptr_t>(&buffer);
    resParam->zeroCopyHeadPtr = reinterpret_cast<std::uintptr_t>(&head);
    resParam->zeroCopyTailPtr = reinterpret_cast<std::uintptr_t>(&tail);
    zeroCopyRingBufferItem[0].addr = remote_addr[0];
    zeroCopyRingBufferItem[1].addr = remote_addr[1];
    zeroCopyRingBufferItem[2].addr = remote_addr[2];
    zeroCopyRingBufferItem[3].addr = remote_addr[3];
    zeroCopyRingBufferItem[4].addr = remote_addr[4];
    zeroCopyRingBufferItem[5].addr = remote_addr[5];
    zeroCopyRingBufferItem[6].addr = remote_addr[6];
    zeroCopyRingBufferItem[7].addr = remote_addr[7];
    zeroCopyRingBufferItem[0].type = ZeroCopyItemType::SET_MEMORY;
    zeroCopyRingBufferItem[1].type = ZeroCopyItemType::ACTIVATE_MEMORY;
    zeroCopyRingBufferItem[2].type = ZeroCopyItemType::SET_MEMORY;
    zeroCopyRingBufferItem[3].type = ZeroCopyItemType::ACTIVATE_MEMORY;
    zeroCopyRingBufferItem[4].type = ZeroCopyItemType::DEACTIVATE_MEMORY;
    zeroCopyRingBufferItem[5].type = ZeroCopyItemType::UNSET_MEMORY;
    zeroCopyRingBufferItem[6].type = ZeroCopyItemType::DEACTIVATE_MEMORY;
    zeroCopyRingBufferItem[7].type = ZeroCopyItemType::UNSET_MEMORY;
    resParam->zeroCopyRingBuffer = reinterpret_cast<std::uintptr_t>(&zeroCopyRingBufferItem);

    AicpuZeroCopyExchanger aicpuZeroCopyExchanger(rankId_, rankSize, resParam, [](){return false;});
    void *localInput = nullptr;
    void *localOutput = nullptr;
    AlgResourceResponse *algResResponse = nullptr;
    EXPECT_EQ(aicpuZeroCopyExchanger.ExchangeAddress(tag,localInput, localOutput, algResResponse), HCCL_E_PARA);
    localInput = new char[10];
    localOutput = new char[10];
    algResResponse = new AlgResourceResponse();
    aicpuZeroCopyExchanger.rankId_ = MAX_MODULE_DEVICE_NUM;
    aicpuZeroCopyExchanger.rankSize_ = MAX_MODULE_DEVICE_NUM;
    EXPECT_EQ(aicpuZeroCopyExchanger.ExchangeAddress(tag, localInput, localOutput, algResResponse), HCCL_E_PARA);
    aicpuZeroCopyExchanger.rankId_ = 0;
    aicpuZeroCopyExchanger.rankSize_ = 2;
    gen_levelNSubCommTransport_stub(algResResponse->opTransportResponse);
    EXPECT_EQ(aicpuZeroCopyExchanger.ExchangeAddress(tag, localInput, localOutput, algResResponse), HCCL_SUCCESS);
    head = 4;
    tail = 5;
    EXPECT_EQ(AicpuZeroCopyExchanger::globalAddrMgr_.ProcessRingBuffer(zeroCopyRingBufferItem, &head, &tail), HCCL_SUCCESS);
    delete[] reinterpret_cast<char *>(localInput);
    delete[] reinterpret_cast<char *>(localOutput);
    delete algResResponse;
    delete resParam;
}