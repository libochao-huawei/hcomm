/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <iostream>
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <stdarg.h>
#include <string>
#include <exception>
#include <sys/socket.h>
#include <vector>
#include <climits>
#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <list>
#include <vector>
#include <mutex>
#include "mmpa_api.h"
#include "nlohmann/json.hpp"

#include <driver/ascend_hal.h>
#include "rt_external.h"
#define private public
#define protected public
#include "eletwise_v3.h"
#include "op_json_info.h"
#include "op_tiling.h"
#include "vector_tiling.h"
#include "tbe_vector_reduce.h"
#include "tbe_crack_cleared.h"
#include "tbe_gatherv2_aicore.h"
#include "tbe_unsorted_segment_sum_aicore.h"

#undef protected
#undef private

using namespace std;
using namespace TbeReduce;

class TbeVectorReduceTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TbeVectorReduceTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TbeVectorReduceTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};
