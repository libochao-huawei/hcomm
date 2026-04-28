/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"
#include "hccl_group_utils.h"

class HcclOpInfoTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        MOCKER(GetExternalInputHcclEnableEntryLog).stubs().with(any()).will(returnValue(true));
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclOpInfoTest, Ut_HcclOpInfo_DefaultConstructor_Expect_AllMembersInitialized)
{
    hcclOpInfo info;

    EXPECT_EQ(info.coll, static_cast<HcclCMDType>(0));
    EXPECT_EQ(info.sendbuff, nullptr);
    EXPECT_EQ(info.recvbuff, nullptr);
    EXPECT_EQ(info.sendCount, 0);
    EXPECT_EQ(info.recvCount, 0);
    EXPECT_EQ(info.sendCounts, nullptr);
    EXPECT_EQ(info.recvCounts, nullptr);
    EXPECT_EQ(info.sdispls, nullptr);
    EXPECT_EQ(info.rdispls, nullptr);
    EXPECT_EQ(info.sendType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.recvType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.op, static_cast<HcclReduceOp>(0));
    EXPECT_EQ(info.root, 0);
    EXPECT_EQ(info.comm, nullptr);
    EXPECT_EQ(info.stream, nullptr);
}

TEST_F(HcclOpInfoTest, Ut_HcclOpInfo_BraceInitialization_Expect_SameAsDefaultConstructor)
{
    hcclOpInfo info = {};

    EXPECT_EQ(info.coll, static_cast<HcclCMDType>(0));
    EXPECT_EQ(info.sendbuff, nullptr);
    EXPECT_EQ(info.recvbuff, nullptr);
    EXPECT_EQ(info.sendCount, 0);
    EXPECT_EQ(info.recvCount, 0);
    EXPECT_EQ(info.sendCounts, nullptr);
    EXPECT_EQ(info.recvCounts, nullptr);
    EXPECT_EQ(info.sdispls, nullptr);
    EXPECT_EQ(info.rdispls, nullptr);
    EXPECT_EQ(info.sendType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.recvType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.op, static_cast<HcclReduceOp>(0));
    EXPECT_EQ(info.root, 0);
    EXPECT_EQ(info.comm, nullptr);
    EXPECT_EQ(info.stream, nullptr);
}

TEST_F(HcclOpInfoTest, Ut_HcclOpInfo_SetFields_Expect_FieldsSetCorrectly)
{
    hcclOpInfo info;

    info.coll = HcclCMDType::HCCL_CMD_BROADCAST;
    info.sendbuff = reinterpret_cast<const void *>(0x1000);
    info.recvbuff = reinterpret_cast<void *>(0x2000);
    info.sendCount = 100;
    info.recvCount = 100;
    info.root = 1;

    EXPECT_EQ(info.coll, HcclCMDType::HCCL_CMD_BROADCAST);
    EXPECT_EQ(info.sendbuff, reinterpret_cast<const void *>(0x1000));
    EXPECT_EQ(info.recvbuff, reinterpret_cast<void *>(0x2000));
    EXPECT_EQ(info.sendCount, 100);
    EXPECT_EQ(info.recvCount, 100);
    EXPECT_EQ(info.root, 1);
}

class HcclOpInfoInnerTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        MOCKER(GetExternalInputHcclEnableEntryLog).stubs().with(any()).will(returnValue(true));
        HcclCommunicator commun_mock;
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::BroadcastOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::ReduceScatterOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AllGatherOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::ReduceOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::ScatterOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AlltoAllVOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AlltoAllOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::Send)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::Recv)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        while (hcclGroupDepth > 0) {
            HcclGroupEnd();
        }
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
protected:
    s8* sendBuf = nullptr;
    s8* recvBuf = nullptr;
    u64* sendCounts = nullptr;
    u64* sendDispls = nullptr;
    u64* recvCounts = nullptr;
    u64* recvDispls = nullptr;
    u64 sendCount = 0;
    u64 recvCount = 0;
    u64 count = 0;
};

TEST_F(HcclOpInfoInnerTest, Ut_HcclBroadcast_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int root = 0;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclBroadcastInner(sendBuf, count, HCCL_DATA_TYPE_INT8, root, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclReduceScatter_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclReduceScatterInner(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclReduce_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int root = 0;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclReduceInner(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, root, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclAllGather_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclAllGatherInner(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclScatter_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int root = 0;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclScatterInner(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, root, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclAlltoAll_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclAlltoAllInner(sendBuf, count, HCCL_DATA_TYPE_INT8, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclReduceScatterV_GroupDepthPositive_Expect_Success)
{
    u64 sendCounts = 1;
    u64 sendDispls = 0;
    UT_SET_SENDBUFV_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, 1, sendCounts, 1, sendDispls, HCCL_COM_DATA_SIZE, 1);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclReduceScatterVInner(sendBuf, &sendCounts, &sendDispls, recvBuf, recvCount,
        HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUFV_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclAllGatherV_GroupDepthPositive_Expect_Success)
{
    u64 recvCounts = 1;
    u64 recvDispls = 0;
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE, 1, HCCL_COM_DATA_SIZE, 1, recvCounts, 1, recvDispls);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclAllGatherVInner(sendBuf, sendCount, recvBuf, &recvCounts, &recvDispls,
        HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUFV_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclRecv_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    uint32_t srcRank = 0;

    HcclGroupStart();
    HcclResult ret = HcclRecvInner(recvBuf, count, HCCL_DATA_TYPE_INT8, srcRank, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclAlltoAllV_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 1, 1, 1, 0, HCCL_COM_DATA_SIZE, 1, 1, 1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclGroupStart();
    HcclResult ret = HcclAlltoAllVInner(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8,
        recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclOpInfoInnerTest, Ut_HcclSend_GroupDepthPositive_Expect_Success)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    uint32_t destRank = 0;

    HcclGroupStart();
    HcclResult ret = HcclSendInner(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclGroupEnd();

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}
