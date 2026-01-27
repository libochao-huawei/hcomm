#include "hccl_api_base_test.h"

class HcclAlltoAllTest : public BaseInit {
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
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AlltoAll)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
protected:
    s8 *sendBuf = nullptr;
    s8 *recvBuf = nullptr;
    u64 sendCount = 0;
    u64 recvCount = 0;
};

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_SendBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(0, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_RecvBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, 0, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_CountIsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, 0, HCCL_COM_DATA_SIZE, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_CountIsNotEqual_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE + 1, HCCL_COM_DATA_SIZE + 1, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);
    
    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_TypeIsNotEqual_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT32, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);
    
    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_DataSize1KB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_DataSize300MB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_BIG_DATA_SIZE, HCCL_COM_BIG_DATA_SIZE, HCCL_COM_BIG_DATA_SIZE, HCCL_COM_BIG_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_Exec20times_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    for(int k = 0;k < 20;k ++) {
        HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Stream_Synchronize(stream);
    }

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM(comm, stream);
}

TEST_F(HcclAlltoAllTest, Ut_HcclAlltoAll_When_2Server4Rank_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_2SERVER_4RANK;
    UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAlltoAll(sendBuf, sendCount, HCCL_DATA_TYPE_INT8, recvBuf, recvCount, HCCL_DATA_TYPE_INT8, comm, stream);;
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}