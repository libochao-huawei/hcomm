target_sources(orion_alg_ccu_1d_hf16p_testcase
        PUBLIC
        main.cc
        testcase_utils.cc
        testcase_all_gather_hf.cc
        testcase_all2all_hf.cc
        testcase_all2allv_hf.cc
        testcase_reduce_scatter_hf.cc
        ${TOP_DIR}/llt/ace/comop/hccl/orion/common/virtual_topo_stub/virtual_topo_stub.cc
)

run_llt_test(
   TARGET  orion_alg_ccu_1d_hf16p_testcase
   TASK_NUM 1
)