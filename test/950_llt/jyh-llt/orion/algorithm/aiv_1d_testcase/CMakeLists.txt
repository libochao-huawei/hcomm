target_sources(orion_alg_aiv_1d_testcase
        PUBLIC
        main.cc
        testcase_utils.cc
        testcase_all_reduce_aiv_1d_twoshot.cc
        testcase_all_reduce_aiv_1d_oneshot.cc
        testcase_all_gather_aiv_1d.cc
        testcase_scatter_aiv_1d.cc
        testcase_broadcast_aiv_1d.cc
        testcase_all_to_all_aiv_1d.cc
        testcase_all_to_all_v_aiv_1d.cc
        testcase_reduce_aiv_1d.cc
        testcase_reduce_scatter_aiv_1d.cc
        ${TOP_DIR}/llt/ace/comop/hccl/orion/common/virtual_topo_stub/virtual_topo_stub.cc
)

run_llt_test(
   TARGET  orion_alg_aiv_1d_testcase
   TASK_NUM 1
)