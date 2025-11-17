/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>
#include <fcntl.h>

#include "sal.h"
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_comm_pub.h"
#include "llt_hccl_stub_pub.h"

using namespace std;
using namespace hccl;

class MPI_HCCL_CPU_KERNEL_CPU_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_HCCL_CPU_KERNEL_CPU_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_HCCL_CPU_KERNEL_CPU_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};
