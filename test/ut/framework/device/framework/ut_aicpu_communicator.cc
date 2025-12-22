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

#ifndef private
#define private public
#define protected public
#endif
#include "aicpu_hccl_sqcq.h"
#include "aicpu_communicator.h"
#undef private
#undef protected
#include "llt_hccl_stub_pub.h"

using namespace std;
using namespace hccl;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef enum tagRtClearStep {
    RT_STREAM_STOP = 0,
    RT_STREAM_CLEAR,
} rtClearStep_t;

rtError_t rtStreamClear(rtStream_t stm, rtClearStep_t step)
{
    return 0;
}

#ifdef __cplusplus
}
#endif // __cplusplus

class AicpuCommunicatorTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuCommunicatorTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AicpuCommunicatorTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "AicpuCommunicatorTest Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "AicpuCommunicatorTest Test TearDown" << std::endl;
    }
};

drvError_t drvQueryProcessHostPidStub(int pid, unsigned int *chip_id, unsigned int *vfid,
    unsigned int *host_pid, unsigned int *cp_type)
{
    if (chip_id != NULL) {
        *chip_id = 0;
    }

    if (vfid != NULL) {
        *vfid = 0;
    }

    if (host_pid != NULL) {
        *host_pid = 0;
    }

    if (cp_type != NULL) {
        *cp_type = 0;
    }

    return DRV_ERROR_NONE;
}

void MockForExecutorTracerUt()
{
    MOCKER(&CqReportRecv).stubs().will(returnValue(dfx::CqeStatus::kCqeException));
    MOCKER(&drvQueryProcessHostPid).stubs().will(invoke(drvQueryProcessHostPidStub));
    MOCKER(&getpid).stubs().will(returnValue(0));
    MOCKER(&halSqCqQuery).stubs().will(returnValue(0));
    MOCKER_CPP(&LocalNotify::GetNotifyData).stubs().will(returnValue(HCCL_SUCCESS));
}

void SetRetryEnableAndErrorCode(HcclCommAicpu *hcclCommAicpu, ErrCqeContext &cqeCtx,
    bool retryEnable, u8 sqeType, u8 errorCode)
{
    hcclCommAicpu->retryEnable_ = retryEnable;
    hcclCommAicpu->errMessageReport_ = true;
    cqeCtx.cqeStatus = 1; // CqeStatus::kCqeException
    cqeCtx.sqeType = sqeType;
    cqeCtx.errorCode = errorCode;
}

TEST_F(AicpuCommunicatorTest, Ut_HandleCqeException_When_CqeExceptionOccured_Expect_RunSuccess)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    Stream stream;
    MockForExecutorTracerUt();

    hcclCommAicpu->HandleCqeException(stream, false);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuCommunicatorTest, Ut_ReportErrCqe_When_SdmaErrorOccuredAndRetryEnableIsTrue_Expect_RunSuccess)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    Stream stream;
    ErrCqeContext cqeCtx;
    MockForExecutorTracerUt();

    // 开启重执行，发生SDMA除8 9 a以外的错误才上报
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).expects(once()).will(returnValue(HCCL_SUCCESS));
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, true, RT_STARS_SQE_TYPE_SDMA, RT_SDMA_COMPDATAERR);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, true, RT_STARS_SQE_TYPE_SDMA, 2U);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuCommunicatorTest, Ut_ReportErrCqe_When_SdmaErrorOccuredAndRetryEnableIsFalse_Expect_RunSuccess)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    Stream stream;
    ErrCqeContext cqeCtx;
    MockForExecutorTracerUt();

    // 关闭重执行，所有SDMA错误都上报
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).expects(exactly(3)).will(returnValue(HCCL_SUCCESS));
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, false, RT_STARS_SQE_TYPE_SDMA, RT_SDMA_COMPDATAERR);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, false, RT_STARS_SQE_TYPE_SDMA, RT_SDMA_COMPERR);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, false, RT_STARS_SQE_TYPE_SDMA, RT_SDMA_DATAERR);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuCommunicatorTest, Ut_HandleCqeException_When_OtherErrorOccured_Expect_RunSuccess)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    Stream stream;
    ErrCqeContext cqeCtx;
    MockForExecutorTracerUt();

    // 其他错误上报类型
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).expects(exactly(3)).will(returnValue(HCCL_SUCCESS));
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, false, RT_STARS_SQE_TYPE_NOTIFY_WAIT, 0U);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, false, RT_STARS_SQE_TYPE_PLACE_HOLDER, 0U);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    SetRetryEnableAndErrorCode(hcclCommAicpu, cqeCtx, false, RT_STARS_SQE_TYPE_WRITE_VALUE, 0U);
    hcclCommAicpu->ReportErrCqe(stream, cqeCtx);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuCommunicatorTest, Ut_PrepareSymmetricMemory_When_OpTransportResponseIsEmpty_Expect_ReturnIsHCCL_E_PARA) {
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    OpParam opParam;
    OpCommTransport opTransportResponse;
    HcclResult ret = hcclCommAicpu->PrepareSymmetricMemory(opParam, opTransportResponse);
    EXPECT_EQ(ret, HCCL_E_PARA);
    delete hcclCommAicpu;
}

TEST_F(AicpuCommunicatorTest, Ut_PrepareSymmetricMemory_When_LinkIsNull_Expect_ReturnIsHCCL_SUCCESS) {
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    OpParam opParam;
    OpCommTransport opTransportResponse;
    // 构造一个 level0 单元（遵循代码中使用 COMM_LEVEL0 索引）
    opTransportResponse.resize(COMM_LEVEL0 + 1);
    SingleSubCommTransport single;
    // push a nullptr link and corresponding transportRequest with isValid = true
    single.links.push_back(nullptr);
    TransportRequest req{};
    req.isValid = true;
    single.transportRequests.push_back(req);
    opTransportResponse[COMM_LEVEL0].push_back(single);

    HcclResult ret = hcclCommAicpu->PrepareSymmetricMemory(opParam, opTransportResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete hcclCommAicpu;
}

TEST_F(AicpuCommunicatorTest, Ut_ExecOp_When_SymMemEnabled_Expect_ReturnIsHCCL_SUCCESS)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = stream;
    hcclCommAicpu->retryEnable_ = true;
    hcclCommAicpu->printTaskExceptionForErr_ = true;
    hcclCommAicpu->identifier_ = "1";
    MOCKER(QuerySqStatusByType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    hccl::HcclCommAicpu *hcclCommAicpu2 = new hccl::HcclCommAicpu;
    hcclCommAicpu2->mainStream_ = stream;
    hcclCommAicpu2->retryEnable_ = true;
    hcclCommAicpu2->printTaskExceptionForErr_ = true;
    hcclCommAicpu2->identifier_ = "2";

    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> aicpuCommInfo;
    aicpuCommInfo.push_back({hcclCommAicpu->identifier_, hcclCommAicpu});
    aicpuCommInfo.push_back({hcclCommAicpu2->identifier_, hcclCommAicpu2});

    MOCKER_CPP(&HcclCommAicpu::GetAlgResponseRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateOpRingBufferIdx).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    MOCKER(&AicpuHcclProcess::AicpuGetCommAll).stubs().with(outBound(aicpuCommInfo)).will(returnValue(HCCL_SUCCESS));

    std::string newTag = "tag_test_taskException";
    std::string algName = "algName_test_taskException";
    OpParam opParam;
    HcclOpResParam commParam;
    commParam.localUsrRankId = 0;
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);

    hcclCommAicpu->isZeroCopy_ = true;
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);

    hcclCommAicpu->isZeroCopy_ = true;
    MOCKER_CPP(&AicpuZeroCopyExchanger::BatchSetLocalAddrToRemote).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuZeroCopyExchanger::GetRemoteAddr).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);
    delete hcclCommAicpu;
    delete hcclCommAicpu2;
}