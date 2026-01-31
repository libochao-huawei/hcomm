# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
target_sources(orion_alg_testcase
        PUBLIC
        main.cc
        testcase_all_reduce_aicpu_mesh1d_nhr.cc
        testcase_all_reduce_aicpu_mesh2d_nhr.cc
        testcase_reduce_scatter_aicpu_mesh2d_nhr.cc
        testcase_reduce_scatter_aicpu_mesh1d_nhr.cc
        testcase_all_reduce_aicpu_1d.cc
        testcase_utils.cc
        testcase_all_gather.cc
        testcase_all_gather_2d.cc
        testcase_all_reduce.cc
        testcase_alltoall.cc
        testcase_alltoallv.cc
        testcase_alltoallvc.cc
        testcase_reduce_scatter.cc
        testcase_scatter.cc
        testcase_reduce.cc
        testcase_reduce_aicpu_mesh1d_nhr.cc
        testcase_broadcast.cc
        testcase_selector.cc
        testcase_all_reduce_aicpu_1d_mesh_chunk.cc
        testcase_all_gather_aicpu_mesh2d_nhr.cc
        ${TOP_DIR}/llt/ace/comop/hccl/orion/common/virtual_topo_stub/virtual_topo_stub.cc
)

run_llt_test(
   TARGET  orion_alg_testcase
   TASK_NUM 1
)