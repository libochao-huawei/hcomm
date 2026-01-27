#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "op_base_stream_manager_pub.h"
#include "stream_pub.h"
#include "sal.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

using namespace std;
using namespace hccl;

class OpBaseStreamManagerTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--OpBaseStreamManagerTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--OpBaseStreamManagerTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        master = Stream(StreamType::STREAM_TYPE_OFFLINE);
        master.SetMode(1);
        manager.RegisterMaster(master);
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }

    Stream master;
    OpBaseStreamManager manager;
};

TEST_F(OpBaseStreamManagerTest, ut_register_alloc_and_get_master)
{
    s32 ret = HCCL_SUCCESS;
    Stream master(StreamType::STREAM_TYPE_OFFLINE);
    Stream outStream;

    ret = manager.AllocMaster(StreamType::STREAM_TYPE_OFFLINE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = manager.RegisterMaster(master);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    outStream = manager.GetMaster();
    EXPECT_EQ(outStream.ptr(), master.ptr());
}

TEST_F(OpBaseStreamManagerTest, ut_register_master_by_rtStream)
{
    s32 ret = HCCL_SUCCESS;
    rtStream_t rtStream;
    Stream outStream;
    rtStreamCreate(&rtStream, 5);

    ret = manager.RegisterMaster(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    outStream = manager.GetMaster();
    EXPECT_EQ(outStream.ptr(), rtStream);
    rtStreamDestroy(rtStream);
}

TEST_F(OpBaseStreamManagerTest, ut_get_one_slave)
{
    std::vector<Stream> outStream = manager.AllocSlaves(StreamType::STREAM_TYPE_OFFLINE, 1);
    EXPECT_EQ(outStream.size(), 1);

    /*  释放资源 */
    HcclResult ret = manager.ClearSlaves();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpBaseStreamManagerTest, ut_get_muti_slaves)
{
    std::vector<Stream> outStream = manager.AllocSlaves(StreamType::STREAM_TYPE_OFFLINE, 8);
    EXPECT_EQ(outStream.size(), 8);

    /*  释放资源 */
    HcclResult ret = manager.ClearSlaves();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpBaseStreamManagerTest, ut_get_zero_slave)
{
    std::vector<Stream> outStream = manager.AllocSlaves(StreamType::STREAM_TYPE_OFFLINE, 0);
    EXPECT_EQ(outStream.size(), 0);

    /*  释放资源 */
    HcclResult ret = manager.ClearSlaves();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpBaseStreamManagerTest, ut_get_slave_exceed_fail)
{
    std::vector<Stream> outStream = manager.AllocSlaves(StreamType::STREAM_TYPE_OFFLINE, 80);
    EXPECT_EQ(outStream.size(), 0);

    /*  释放资源 */
    HcclResult ret = manager.ClearSlaves();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpBaseStreamManagerTest, ut_get_slave_stream_alloc_fail)
{
    MOCKER(rtStreamCreate)
    .stubs()
    .will(returnValue(1));

    std::vector<Stream> outStream = manager.AllocSlaves(StreamType::STREAM_TYPE_OFFLINE, 5);
    EXPECT_EQ(outStream.size(), 0);

    /*  释放资源 */
    HcclResult ret = manager.ClearSlaves();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
