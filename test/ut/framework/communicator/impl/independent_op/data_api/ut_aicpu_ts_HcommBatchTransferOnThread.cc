/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_aicpu_ts_base.h"
#include "transport_pub.h"
#include "transport_device_ibverbs_pub.h"
#include "dispatcher_pub.h"
#include "dispatcher.h"
#include "dlhns_function.h"

#define private public
#define protected public
#include "aicpu_ts_thread.h"
#undef protected
#undef private

using namespace hccl;

class UtAicpuTsHcommBatchTransferOnThread : public UtAicpuTsBase
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "UtAicpuTsHcommBatchTransferOnThread tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UtAicpuTsHcommBatchTransferOnThread tests tear down." << std::endl;
    }

    virtual void SetUp() override
    {
        std::cout << "A Test case in UtAicpuTsHcommBatchTransferOnThread SetUp" << std::endl;
        UtAicpuTsBase::SetUp();

        mainDevThread->devType_ = DevType::DEV_TYPE_910B;
        mainDevThread->stream_.reset(new Stream());

        HcclResult ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherHandle_);
        ASSERT_EQ(ret, HCCL_SUCCESS);
        dispatcher_ = reinterpret_cast<DispatcherPub *>(dispatcherHandle_);

        TransportDeviceIbverbsData d;
        FillMinimalMemDetailsQpData(d);

        MOCKER_CPP(&DlHnsFunction::DlHnsFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));

        MachinePara machinePara{};
        machinePara.deviceLogicId = 0;
        auto *impl = new TransportDeviceIbverbs(
            dispatcher_, nullptr, machinePara, std::chrono::milliseconds(1), d);
        ret = impl->Init();
        ASSERT_EQ(ret, HCCL_SUCCESS);

        transportWrapper_.reset(new Transport(impl));
        devHandle = reinterpret_cast<ChannelHandle>(transportWrapper_.get());
    }

    virtual void TearDown() override
    {
        transportWrapper_.reset();
        if (dispatcherHandle_ != nullptr) {
            HcclDispatcherDestroy(dispatcherHandle_);
            dispatcherHandle_ = nullptr;
            dispatcher_ = nullptr;
        }
        std::cout << "A Test case in UtAicpuTsHcommBatchTransferOnThread TearDown" << std::endl;
        UtAicpuTsBase::TearDown();
    }

    void FillMinimalMemDetailsQpData(TransportDeviceIbverbsData &d)
    {
        d.useMemDetailsMgr = true;
        d.qpsPerConnection = 1U;
        d.qpInfo.resize(1U);
        d.qpInfo[0].qpPtr = 0x5000ULL;
        d.qpInfo[0].sqIndex = 1U;
        d.qpInfo[0].dbIndex = 2U;
        d.multiQpThreshold = HCCL_MULTI_QP_THRESHOLD_DEFAULT;
        d.useAtomicWrite = false;

        RoceMemDetails loc{};
        loc.addr = 0x20000ULL;
        loc.devAddr = 0xC20000ULL;
        loc.size = 4096ULL;
        loc.key = 11U;
        d.localRoceMemDetailsList = { loc };

        RoceMemDetails rem{};
        rem.addr = 0x10000ULL;
        rem.devAddr = 0xC10000ULL;
        rem.size = 4096ULL;
        rem.key = 22U;
        d.remoteRoceMemDetailsList = { rem };
    }

    HcclDispatcher dispatcherHandle_ = nullptr;
    DispatcherPub *dispatcher_ = nullptr;
    std::unique_ptr<Transport> transportWrapper_;
    ChannelHandle devHandle = 0;

    void *dst = reinterpret_cast<void *>(0x10000);
    void *src = reinterpret_cast<void *>(0x20000);
    uint64_t len = 64;
    int32_t res{HCCL_E_RESERVED};
    HcommBatchTransferDesc transferDescs[2];
};

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_A5Path_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    mainDevThread->devType_ = DevType::DEV_TYPE_950;
    mainDevThread->stream_.release();

    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = dst;
    transferDescs[0].transferInfo.write.src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_ThreadIsNull_Expect_ReturnHCCL_E_PTR)
{
    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = dst;
    transferDescs[0].transferInfo.write.src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;

    res = HcommBatchTransferOnThread(0, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DescsIsNull_Expect_ReturnHCCL_E_PTR)
{
    res = HcommBatchTransferOnThread(thread, devHandle, nullptr, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DescNumIsZero_Expect_ReturnHCCL_E_PARA)
{
    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = dst;
    transferDescs[0].transferInfo.write.src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 0);
    EXPECT_EQ(res, HCCL_E_PARA);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_InvalidTransType_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = dst;
    transferDescs[0].transferInfo.write.src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_INVALID;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DstIsNull_Expect_ReturnHCCL_E_PTR)
{
    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = nullptr;
    transferDescs[0].transferInfo.write.src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_SrcIsNull_Expect_ReturnHCCL_E_PTR)
{
    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = dst;
    transferDescs[0].transferInfo.write.src = nullptr;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_MultipleDescs_Expect_ReturnHCCL_SUCCESS)
{
    MOCKER_CPP(&TransportDeviceIbverbs::UseMultiQp).stubs().will(returnValue(false));
    MOCKER_CPP(&TransportDeviceIbverbs::TxSendDataAndNotifyWithSingleQP,
        HcclResult(TransportDeviceIbverbs::*)(std::vector<WrInformation> &, Stream &, bool))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    transferDescs[0].transferInfo.write.len = len;
    transferDescs[0].transferInfo.write.dst = dst;
    transferDescs[0].transferInfo.write.src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;

    transferDescs[1].transferInfo.read.len = len;
    transferDescs[1].transferInfo.read.dst = src;
    transferDescs[1].transferInfo.read.src = dst;
    transferDescs[1].transType = HCOMM_TRANSFER_TYPE_READ;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 2);
    EXPECT_EQ(res, HCCL_SUCCESS);
}
