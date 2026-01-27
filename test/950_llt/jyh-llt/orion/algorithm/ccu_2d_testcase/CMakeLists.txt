target_sources(orion_alg_ccu_2d_testcase
        PUBLIC
        main.cc
        testcase_utils.cc
        testcase_all_gather_ccu_2d.cc
        testcase_all_reduce_ccu_2d.cc
        testcase_broadcast_ccu_2d.cc
        testcase_reduce_scatter_ccu_2d.cc
        testcase_reduce_ccu_2d.cc
        testcase_scatter_ccu_2d.cc
        testcase_alltoall_ccu_2d.cc
        testcase_alltoallv_ccu_2d.cc
)

run_llt_test(
   TARGET  orion_alg_ccu_2d_testcase
   TASK_NUM 1
)