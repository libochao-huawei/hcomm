target_sources(orion_alg_aicpu_2d_testcase
        PUBLIC
        main.cc
        testcase_all_reduce_aicpu_2d.cc
        testcase_utils.cc
        testcase_scatter_aicpu_2d.cc
        testcase_all_gather_aicpu_2d.cc
        testcast_alltoall_aicpu_2d.cc
        testcase_broadcast.cc
        testcase_reduce_aicpu_2d.cc
        testcase_reduce_scatter_aicpu_2d.cc
)

run_llt_test(
   TARGET  orion_alg_aicpu_2d_testcase
   TASK_NUM 1
)