/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"
#include "comm.h"
#include "llt_hccl_stub_pub.h"
#include <mpi.h>
#include "env_config.h"

GTEST_API_ int main(int argc, char **argv) 
{
    int mpi_ret,gtest_ret;
    printf("Running main() from gtest_main.cc\n");
    testing::InitGoogleTest(&argc, argv);

    setenv("LD_LIBRARY_PATH", "./hcomm/test/llt/stub/workspace/fwkacllib/lib64", 1);
    setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
    setTargetPort(16451, 31111);
    InitEnvParam();
    mpi_ret = MPI_Init(&argc, &argv);
    if (mpi_ret)
    {
        printf("MPI_Init failed[%d]\n", mpi_ret);
        return mpi_ret;
    }

    gtest_ret = RUN_ALL_TESTS();
    
    mpi_ret = MPI_Finalize();
    if (mpi_ret)
    {
        printf("MPI_Finalize failed[%d]\n", mpi_ret);
        return mpi_ret;
    }
  return gtest_ret;
}
