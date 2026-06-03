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
#include <iostream>

#include "hccl/base.h"
#include "hccl/hccl_ex.h"
#include <hccl/hccl_types.h>

#define private public
#define protected public
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "hccd_impl_pml.h"
#include "hccd_comm.h"
#include "mr_manager.h"
#include "transport_heterog_event_roce_pub.h"
#undef protected
#undef private

#include "comm.h"
#include "sal.h"
#include <vector>
#include "llt_hccl_stub_pub.h"
#include "dlra_function.h"
#include "llt_hccl_stub_gdr.h"
#include "externalinput.h"
#include <fstream>
using namespace std;
using namespace hccl;


class HeterogSendRecvTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--HeterogSendRecvTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--HeterogSendRecvTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(0, "UT_TEST");
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        ResetInitState();
        std::cout << "A Test TearDown" << std::endl;
    }
};

#if 1
TEST_F(HeterogSendRecvTest, ut_hccd_mr_manager)
{
    nlohmann::json rank_table =
    {
	    {"collective_id", "192.168.0.101-9527-0001"},
        {"master_ip", "192.168.0.11"},
        {"master_port", "18000"},
        {"status", "completed"},
	    {"version","1.1"},
        {"node_list", {
            {
                {"node_addr", "192.168.0.11"},
                {"ranks", {
                    {
                        {"rank_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.1.11"},
                {"ranks", {
                    {
                        {"rank_id", "1"},
                        {"device_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.2.11"},
                {"ranks", {
                    {
                        {"rank_id", "2"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.3.11"},
                {"ranks", {
                    {
                        {"rank_id", "3"}
                    }
                }}
            }
        }
        }
    };
    MOCKER(hrtIbvPostSrqRecv)
    .stubs()
    .with(any())
    .will(returnValue(0));
    MOCKER_CPP(&TransportHeterogEventRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(0));

    HcclResult ret;
    void *buff, *buff1, *buff2;
    void *rdmaHandle = (void *)0xabcd;
    void *qpHandle = (void *)0xabce;
    u32 devId = 0;
    bool IsHostMem = true;
    map<MrMapKey, MrInfo> unRegMrMap;
    u64 size = 10;
    u32 lkey;
    buff = sal_malloc(size);
    sal_memset(buff, size, 0, size);
    buff1 = sal_malloc(size);
    sal_memset(buff1, size, 0, size);
    buff2 = buff + 100;

    HcclComm comm;
    u32 rank_ID = 0;
    std::string rank_table_string = rank_table.dump();
    CommAttr attr;
    attr.deviceId = 0;
    attr.mode = HCCL_MODE_NORMAL;

    ret = HcclInitComm(rank_table_string.c_str(), rank_ID, &attr, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MrManager::GetInstance().Init(rdmaHandle);

    HccdComm *hccdcomm = static_cast<HccdComm *>(comm);
    hccdcomm->impl_->mrManager_->rdmaHandle_ = rdmaHandle;

    // 注册全局内存
    ret = HcclRegisterMemory(comm, buff, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = MrManager::GetInstance().GetKey(buff, size, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
   // 注册0长内存
    ret = HcclRegisterMemory(comm, buff2, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclUnregisterMemory(comm, buff2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 解注册未注册的全局内存，报错
    ret = HcclUnregisterMemory(comm, buff1);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = HcclUnregisterMemory(comm, buff);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 注册临时内存
    ret = MrManager::GetInstance().GetKey(buff, size, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 重复注册临时内存
    ret = MrManager::GetInstance().GetKey(buff, size, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 注册地址相同大小不同的临时内存
    ret = MrManager::GetInstance().GetKey(buff, size+1, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 释放临时内存
    ret = MrManager::GetInstance().ReleaseKey(buff, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = MrManager::GetInstance().ReleaseKey(buff, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 释放地址相同大小不同的临时内存
    ret = MrManager::GetInstance().ReleaseKey(buff, size+1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 注册全局内存
    ret = HcclRegisterMemory(comm, buff, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 解注册未注册的全局内存，报错
    ret = HcclUnregisterMemory(comm, buff1);
    EXPECT_EQ(ret, HCCL_E_PARA);
    // 使用全局内存
    ret = MrManager::GetInstance().GetKey(buff, size, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 使用小于全局内存的地址
    ret = MrManager::GetInstance().GetKey(buff-1, size, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 释放未注册的全局内存，报错
    ret = MrManager::GetInstance().ReleaseKey(buff1, size);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    // 注册临时内存
    ret = MrManager::GetInstance().GetKey(buff1, size, lkey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 注册和临时内存地址相同的全局内存
    ret = HcclRegisterMemory(comm, buff1, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 解注册临时内存
    ret = MrManager::GetInstance().ReleaseKey(buff1, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 释放向全局内存申请的内存
    ret = MrManager::GetInstance().ReleaseKey(buff, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 重复释放向全局内存申请的内存
    ret = MrManager::GetInstance().ReleaseKey(buff - 1, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MrManager::GetInstance().DeInit(rdmaHandle);
    ret = HcclFinalizeComm(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclRegisterGlobalMemory(buff, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclUnregisterGlobalMemory(buff);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MrManager::GetInstance().Init(qpHandle, devId, IsHostMem, unRegMrMap);

    unRegMrMap = MrManager::GetInstance().GetUnregMap();
    HcclMrInfo mrinfo;
    MrManager::GetInstance().TransMrInfo(qpHandle, size, mrinfo);

    MOCKER(hrtHalHostRegister)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaMrDereg)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaMrReg)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtHalHostUnregister)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    void *stub;
    MrInfo MrInformation;
    MrInformation.addr = buff;
    MrInformation.size = size;
    MrManager::GetInstance().isUseQPHandle_ = true;
    MrManager::GetInstance().IsHostMem_ = true;
    ret = MrManager::GetInstance().RegMrImpl(buff, size, mrinfo, stub, stub);
    ret = MrManager::GetInstance().DeRegMrImpl(MrInformation);

    MrManager::GetInstance().isUseQPHandle_ = false;
    ret = MrManager::GetInstance().RegMrImpl(buff, size, mrinfo, stub, stub);
    ret = MrManager::GetInstance().DeRegMrImpl(MrInformation);

    MrManager::mappedHostToDevMap_ = {};
    MrManager::GetInstance().IsRequireMapping(buff, size, stub);
    u64 userAddr = reinterpret_cast<uintptr_t>(buff);
    HostMappingKey key(userAddr, size, 0);
    MrManager::GetInstance().curDevId_ = 0;
    MrManager::mappedHostToDevMap_[key].devVirAddr = stub;
    MrManager::GetInstance().IsRequireMapping(buff, size - 1, stub);
    MOCKER_CPP(&MrManager::GetMrInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    u64 dva = reinterpret_cast<uintptr_t>(stub);
    ret = MrManager::GetInstance().GetDevVirAddr(buff, size, dva);

    sal_free(buff);
    sal_free(buff1);
}


#endif
