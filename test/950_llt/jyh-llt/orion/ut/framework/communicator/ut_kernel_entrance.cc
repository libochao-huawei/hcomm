#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "kernel_entrance.h"
#include "communicator_impl_lite.h"

using namespace Hccl;

TEST(KernelEntranceTest, test_hccl_kernel_entrance_with_nullptr)
{
    auto res = HcclKernelEntrance(nullptr);
    EXPECT_EQ(1, res);
}

TEST(KernelEntranceTest, test_hccl_kernel_entrance_with_valid_param)
{
    HcclKernelParamLite param;

    MOCKER_CPP(&CommunicatorImplLite::LoadWithOpBasedMode).stubs().with(any()).will(returnValue(1));

    auto res = HcclKernelEntrance(&param);
    EXPECT_EQ(1, res);
    GlobalMockObject::verify();
}

TEST(UpdateCommKernelEntranceTest, test_hccl_update_comm_kernel_entrance_with_nullptr)
{
    auto res = HcclUpdateCommKernelEntrance(nullptr);
    EXPECT_EQ(1, res);
}

TEST(UpdateCommKernelEntranceTest, test_hccl_update_comm_kernel_entrance_with_valid_param)
{
    HcclKernelParamLite param;

    MOCKER_CPP(&CommunicatorImplLite::UpdateComm).stubs().with(any()).will(returnValue(1));

    auto res = HcclUpdateCommKernelEntrance(&param);
    EXPECT_EQ(0, res);
    GlobalMockObject::verify();
}