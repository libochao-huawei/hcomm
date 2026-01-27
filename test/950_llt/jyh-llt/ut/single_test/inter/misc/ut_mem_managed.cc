#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "mem_managed_pub.h"
#include "sal.h"

using namespace std;
using namespace hccl;

class ManagedMemTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--ManagedMemTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--ManagedMemTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(ManagedMemTest, constructor_00)
{
    s32 ret = HCCL_SUCCESS;

    ManagedMem mem =  ManagedMem::alloc(8);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ManagedMemTest, constructor_01)
{
    s32 ret = HCCL_SUCCESS;

    ManagedMem mem1 =  ManagedMem::alloc(8);
    ManagedMem mem2(mem1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ManagedMemTest, operate_equal)
{
    s32 ret = HCCL_SUCCESS;

    ManagedMem mem1 =  ManagedMem::alloc(8);
    ManagedMem mem2 = mem1;
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ManagedMemTest, create)
{
    s32 ret = HCCL_SUCCESS;
    void *ptr = NULL;
    ManagedMem mem;
    mem.create(ptr, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ManagedMemTest, range)
{
    s32 ret = HCCL_SUCCESS;
    ManagedMem test;
    ManagedMem mem1 =  ManagedMem::alloc(8);
    test = mem1.range(0, 4);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ManagedMemTest, operate_equal_copy_assignment)
{
    s32 ret = HCCL_SUCCESS;

    ManagedMem mem1 ;
    ManagedMem mem0 = ManagedMem::alloc(8);
    mem1 = mem0;

    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ManagedMemTest, alloc_fail)
{
    s32 ret = HCCL_SUCCESS;

    /*构造rt_malloc异常*/
    .expects(atMost(1))
    .will(returnValue(1));
    ManagedMem mem =  ManagedMem::alloc(8);
    GlobalMockObject::verify();
}

TEST_F(ManagedMemTest, free_fail)
{
    s32 ret = HCCL_SUCCESS;
    ManagedMem mem =  ManagedMem::alloc(8);

    /*构造rt_malloc异常*/
    .expects(atMost(1))
    .will(returnValue(1));
    mem.~ ManagedMem();
    GlobalMockObject::verify();
}

