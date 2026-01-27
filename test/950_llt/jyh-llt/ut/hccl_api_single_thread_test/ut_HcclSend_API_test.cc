#include "hccl_api_base_test.h"

class HcclSendTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_2RANK;
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
        HcclCommunicator commun_mock;
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::SendOutPlace)
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
    u64 count = 0;
};

TEST_F(HcclSendTest, Ut_HcclSend_When_DataIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT(0, HCCL_COM_DATA_SIZE);
    int destRank = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_CountIsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, 0);
    int destRank = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_RootIsInvaild_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int destRank = 0xFFFFFFFF;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int destRank = 1;  
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_DataSize1KB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int destRank = 1;  
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_DataSize300MB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_BIG_DATA_SIZE, HCCL_COM_BIG_DATA_SIZE);
    int destRank = 1;  
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_Exec20times_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int destRank = 1;  
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    for(int k = 0; k < 20; k++) {

        HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        Ut_Stream_Synchronize(stream);
    }
    UT_UNSET_SENDBUF_COMM_STREAM(comm, stream);
}

TEST_F(HcclSendTest, Ut_HcclSend_When_2Server4Rank_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_2SERVER_4RANK;
    UT_SET_SENDBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    int destRank = 7;  
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclSend(sendBuf, count, HCCL_DATA_TYPE_INT8, destRank, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}