#include "hccl_api_base_test.h"

class HcclAllGatherVTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
        HcclCommunicator commun_mock;
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AllGatherVOutPlace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
protected:
    s8* sendBuf = nullptr;
    u64 sendCount = 0;
    s8* recvBuf = nullptr;
    u64* recvCounts = nullptr;
    u64* recvDispls = nullptr;
};

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_SendBufIsNullAndSendCountGEZero_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(0,
        HCCL_COM_DATA_SIZE,
        HCCL_COM_DATA_SIZE,
        1, HCCL_COM_DATA_SIZE,
        1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls,
        HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_SendBufIsNullAndSendCountIsZero_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(0,
                                0,
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_SendCountIsTooLarge_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                SYS_MAX_COUNT + 1, 
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_RecvBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                0,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_RecvCountsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE,
                                0, 0, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_RecvDisplsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                0, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_RecvCountsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE,
                                1, 0, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_RankDataNotEqual_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE + 1,
                                1, HCCL_COM_DATA_SIZE + 1, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_DataSize1KB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_DataSize300MB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_BIG_DATA_SIZE,
                                HCCL_COM_BIG_DATA_SIZE, 
                                HCCL_COM_BIG_DATA_SIZE,
                                1, HCCL_COM_BIG_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_Exec20times_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
                                HCCL_COM_DATA_SIZE, 
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    for(u64 k = 0;k < 20;k ++) {
        HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Stream_Synchronize(stream);
    }

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM(comm, stream);
}

TEST_F(HcclAllGatherVTest, Ut_HcclAllGatherV_When_2Server4Rank_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_2SERVER_4RANK;
    UT_SET_SENDBUF_COUNT_RECVBUFV(HCCL_COM_DATA_SIZE,
        HCCL_COM_DATA_SIZE,
        8 * HCCL_COM_DATA_SIZE,
        8, HCCL_COM_DATA_SIZE, 
        8, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGatherV(sendBuf, sendCount, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}