#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include "rma_buf_slice_lite.h"
#include "rma_buffer_lite.h"
#include "rmt_rma_buf_slice_lite.h"
#include "rmt_rma_buffer_lite.h"

using namespace Hccl;
class AicpuBufferTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuBuffer tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AicpuBuffer tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AicpuBuffer SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AicpuBuffer TearDown" << std::endl;
    }

};

TEST_F(AicpuBufferTest, test_rma_buffer_lite)
{
    RmaBufferLite ipc(1, 1);
    EXPECT_EQ(1, ipc.GetAddr());
    EXPECT_EQ(1, ipc.GetSize());

    RmaBufferLite rdma(1, 1, 1);
    EXPECT_EQ(1, rdma.GetAddr());
    EXPECT_EQ(1, rdma.GetSize());
    EXPECT_EQ(1, rdma.GetLkey());

    RmaBufferLite ub(1, 1, 1, 1);
    EXPECT_EQ(1, ub.GetAddr());
    EXPECT_EQ(1, ub.GetSize());
    EXPECT_EQ(1, ub.GetTokenId());
    EXPECT_EQ(1, ub.GetTokenValue());

    EXPECT_EQ(1, ub.GetRmaBufSliceLite(0, 1).GetTokenId());
    std::cout << ub.Describe() << std::endl;
}

TEST_F(AicpuBufferTest, test_rmt_rma_buffer_lite)
{
    RmtRmaBufferLite ipc(1, 1);
    EXPECT_EQ(1, ipc.GetAddr());
    EXPECT_EQ(1, ipc.GetSize());

    RmtRmaBufferLite rdma(1, 1, 1);
    EXPECT_EQ(1, rdma.GetAddr());
    EXPECT_EQ(1, rdma.GetSize());
    EXPECT_EQ(1, rdma.GetRkey());

    RmtRmaBufferLite ub(1, 1, 1, 1);
    EXPECT_EQ(1, ub.GetAddr());
    EXPECT_EQ(1, ub.GetSize());
    EXPECT_EQ(1, ub.GetTokenId());
    EXPECT_EQ(1, ub.GetTokenValue());

    EXPECT_EQ(1, ub.GetRmtRmaBufSliceLite(0, 1).GetTokenId());
    std::cout << ub.Describe() << std::endl;
}

TEST_F(AicpuBufferTest, test_RmaBufSliceLite)
{
    RmaBufSliceLite lite(1, 1, 1, 1);
    EXPECT_EQ(1, lite.GetAddr());
    EXPECT_EQ(1, lite.GetSize());
    EXPECT_EQ(1, lite.GetLkey());
    EXPECT_EQ(1, lite.GetTokenId());

    std::cout << lite.Describe() << std::endl;
}

TEST_F(AicpuBufferTest, test_RmtRmaBufSliceLite)
{
    RmtRmaBufSliceLite lite(1, 1, 1, 1, 1);
    EXPECT_EQ(1, lite.GetAddr());
    EXPECT_EQ(1, lite.GetSize());
    EXPECT_EQ(1, lite.GetRkey());
    EXPECT_EQ(1, lite.GetTokenId());
    EXPECT_EQ(1, lite.GetTokenValue());

    std::cout << lite.Describe() << std::endl;
}
