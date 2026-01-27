#include "hccl_api_base_test.h"

class HcclBatchSendRecvTest : public BaseInit {
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
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::BatchSendRecv)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_InfoIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclSendRecvItem* sendRecvInfo = nullptr;
    int itemNum = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_itemNumIsZero_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclSendRecvItem* sendRecvInfo = (HcclSendRecvItem*)malloc(sizeof(HcclSendRecvItem));
    sendRecvInfo->buf = sal_malloc(HCCL_COM_DATA_SIZE * sizeof(s8));
    sendRecvInfo->count = HCCL_COM_DATA_SIZE;
    sendRecvInfo->remoteRank = 1;
    sendRecvInfo->dataType = HCCL_DATA_TYPE_INT8;
    sendRecvInfo->sendRecvType = HCCL_SEND;
    int itemNum = 0;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(sendRecvInfo->buf);
    free(sendRecvInfo);
    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclSendRecvItem* sendRecvInfo = (HcclSendRecvItem*)malloc(sizeof(HcclSendRecvItem));
    sendRecvInfo->buf = sal_malloc(HCCL_COM_DATA_SIZE * sizeof(s8));
    sendRecvInfo->count = HCCL_COM_DATA_SIZE;
    sendRecvInfo->remoteRank = 1;
    sendRecvInfo->dataType = HCCL_DATA_TYPE_INT8;
    sendRecvInfo->sendRecvType = HCCL_SEND;
    int itemNum = 1;
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    sal_free(sendRecvInfo->buf);
    free(sendRecvInfo);
    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_DataSize1KB_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclSendRecvItem* sendRecvInfo = (HcclSendRecvItem*)malloc(sizeof(HcclSendRecvItem));
    sendRecvInfo->buf = sal_malloc(HCCL_COM_DATA_SIZE * sizeof(s8));
    sendRecvInfo->count = HCCL_COM_DATA_SIZE;
    sendRecvInfo->remoteRank = 1;
    sendRecvInfo->dataType = HCCL_DATA_TYPE_INT8;
    sendRecvInfo->sendRecvType = HCCL_SEND;
    int itemNum = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(sendRecvInfo->buf);
    free(sendRecvInfo);
    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_DataSize300MB_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclSendRecvItem* sendRecvInfo = (HcclSendRecvItem*)malloc(sizeof(HcclSendRecvItem));
    sendRecvInfo->buf = sal_malloc(HCCL_COM_BIG_DATA_SIZE * sizeof(s8));
    sendRecvInfo->count = HCCL_COM_BIG_DATA_SIZE;
    sendRecvInfo->remoteRank = 1;
    sendRecvInfo->dataType = HCCL_DATA_TYPE_INT8;
    sendRecvInfo->sendRecvType = HCCL_SEND;
    int itemNum = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(sendRecvInfo->buf);
    free(sendRecvInfo);
    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_Exec20times_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclSendRecvItem* sendRecvInfo = (HcclSendRecvItem*)malloc(sizeof(HcclSendRecvItem));
    int itemNum = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    for(int i = 0;i < 20;i ++) {
        sendRecvInfo->buf = sal_malloc(HCCL_COM_DATA_SIZE * sizeof(s8));
        sendRecvInfo->count = HCCL_COM_DATA_SIZE;
        sendRecvInfo->remoteRank = 1;
        sendRecvInfo->dataType = HCCL_DATA_TYPE_INT8;
        if(i % 2)
            sendRecvInfo->sendRecvType = HCCL_SEND;
        else 
            sendRecvInfo->sendRecvType = HCCL_RECV;
        
        HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_free(sendRecvInfo->buf);
    }

    free(sendRecvInfo);
    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}

TEST_F(HcclBatchSendRecvTest, Ut_HcclBatchSendRecv_When_2Server4Rank_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_2SERVER_4RANK;
    HcclSendRecvItem* sendRecvInfo = (HcclSendRecvItem*)malloc(sizeof(HcclSendRecvItem));
    sendRecvInfo->buf = sal_malloc(HCCL_COM_DATA_SIZE * sizeof(s8));
    sendRecvInfo->count = HCCL_COM_DATA_SIZE;
    sendRecvInfo->remoteRank = 7;
    sendRecvInfo->dataType = HCCL_DATA_TYPE_INT8;
    sendRecvInfo->sendRecvType = HCCL_SEND;
    int itemNum = 1;
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBatchSendRecv(sendRecvInfo, itemNum, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(sendRecvInfo->buf);
    free(sendRecvInfo);
    UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, stream);
}