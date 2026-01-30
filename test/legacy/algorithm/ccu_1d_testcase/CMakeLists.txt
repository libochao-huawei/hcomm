# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
target_sources(orion_alg_ccu_1d_testcase
        PUBLIC
        main.cc
        testcase_utils.cc
        testcase_all_gather_ccu.cc
        testcase_all2all_ccu.cc
        testcase_all2allv_ccu.cc
        testcase_all_reduce_ccu.cc
        # testcase_all_gather_ccu_detour.cc
        # testcase_all_reduce_ccu_detour.cc
        testcase_broadcast_ccu.cc
        testcase_reduce_scatter_ccu.cc
        testcase_reduce_ccu.cc
        testcase_scatter_ccu.cc
        testcase_all_gather_v_ccu.cc
        testcase_reduce_scatter_v_ccu.cc
        ${TOP_DIR}/llt/ace/comop/hccl/orion/common/virtual_topo_stub/virtual_topo_stub.cc
)

run_llt_test(
   TARGET  orion_alg_ccu_1d_testcase
   TASK_NUM 1
)