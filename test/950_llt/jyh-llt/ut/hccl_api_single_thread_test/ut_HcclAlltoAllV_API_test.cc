#include "hccl_api_base_test.h"

class HcclAlltoAllVTest : public BaseInit {
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
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AlltoAllV)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AlltoAllVOutPlace)
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
    u64* sendCounts = nullptr;
    u64* sendDispls = nullptr;
    s8* recvBuf = nullptr;
    u64* recvCounts = nullptr;
    u64* recvDispls = nullptr;
};

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_SendBufIsRecvBuf_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,
                                HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    free(sendBuf);
    sendBuf = recvBuf;

    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);
    sendBuf = nullptr;  // 防止被多次释放内存
    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_SendBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(0, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0, 
                                HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_SendCountsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                0, 0, 
                                1, 0, 
                                HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_SendDisplsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                0, 0, 
                                HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_SendCountsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, 0, 
                                1, 0, 
                                HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_RecvBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,  
                                0,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_RecvCountsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,   
                                HCCL_COM_DATA_SIZE,
                                0, 0, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_RecvDisplsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,   
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                0, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_RecvCountsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,  
                                HCCL_COM_DATA_SIZE,
                                1, 0, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,  
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_DataSize1KB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,  
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_DataSize300MB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_BIG_DATA_SIZE, 
                                1, HCCL_COM_BIG_DATA_SIZE, 
                                1, 0,  
                                HCCL_COM_BIG_DATA_SIZE,
                                1, HCCL_COM_BIG_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_Exec20times_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUFV_RECVBUFV(HCCL_COM_DATA_SIZE, 
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0,  
                                HCCL_COM_DATA_SIZE,
                                1, HCCL_COM_DATA_SIZE, 
                                1, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    for(u64 k = 0;k < 20;k ++) {
        HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Stream_Synchronize(stream);
    }

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM(comm, stream);
}

TEST_F(HcclAlltoAllVTest, Ut_HcclAlltoAllV_When_2Server4Rank_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_2SERVER_4RANK;
    UT_SET_SENDBUFV_RECVBUFV(8 * HCCL_COM_DATA_SIZE,
                                8, HCCL_COM_DATA_SIZE, 
                                8, HCCL_COM_DATA_SIZE,
                                8 * HCCL_COM_DATA_SIZE,
                                8, HCCL_COM_DATA_SIZE, 
                                8, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAllV(sendBuf, sendCounts, sendDispls, HCCL_DATA_TYPE_INT8, recvBuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}