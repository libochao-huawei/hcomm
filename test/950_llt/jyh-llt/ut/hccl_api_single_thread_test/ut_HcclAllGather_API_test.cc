#include "hccl_api_base_test.h"

namespace hccl_api_single_thread_test {

class HcclAllGatherTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        // 目的
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
        // 目的
        HcclCommunicator commun_mock;
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::AllGatherOutPlace)
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
    u64 count = 0;
};

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_SendBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(0, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_RecvBufIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, 0, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_CountIsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(0, 0, 0);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_CountIsTooLarge_Expect_ReturnIsHCCL_E_PARA)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, SYS_MAX_COUNT + 1);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PARA);
    
    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_DataSize1KB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_DataSize300MB_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_BIG_DATA_SIZE, HCCL_COM_BIG_DATA_SIZE, HCCL_COM_BIG_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_Exec20times_Expect_ReturnIsHCCL_SUCCESS)
{
    constexpr int LOOP_TIMES = 20;
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    for (int k = 0; k < LOOP_TIMES; k++) {
        HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Stream_Synchronize(stream);
    }

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM(comm, stream);
}

TEST_F(HcclAllGatherTest, Ut_HcclAllGather_When_2Server4Rank_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_2SERVER_4RANK;
    UT_SET_SENDBUF_RECVBUF_COUNT(HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE, HCCL_COM_DATA_SIZE);
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclAllGather(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, comm, stream);;
    EXPECT_EQ(ret, HCCL_SUCCESS);

    UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

} /* hccl_api_single_thread_test */