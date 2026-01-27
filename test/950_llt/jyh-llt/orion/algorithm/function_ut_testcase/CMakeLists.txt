target_sources(orion_alg_function_ut_testcase
        PUBLIC
        main.cc
        ${TOP_DIR}/llt/ace/comop/hccl/orion/common/virtual_topo_stub/virtual_topo_stub.cc
        testcase_utils.cc
        gen_ccu_task_node_utils.cc
        testcase_ccu_revamp_parallel.cc
        testcase_ccu_revamp_bilateral.cc
        testcase_calc_res.cc
)

run_llt_test(
   TARGET  orion_alg_function_ut_testcase
   TASK_NUM 1
)