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