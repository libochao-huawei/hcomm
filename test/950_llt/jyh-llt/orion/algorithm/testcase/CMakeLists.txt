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