set(aicpu_kfc_utest_misc_list
    main.cc
    ut_aicpu_kfc_allgather_utest.cc
    ut_aicpu_kfc_allreduce_utest.cc
    ut_aicpu_kfc_alltoall_utest.cc
    ut_aicpu_kfc_reducescatter_utest.cc
    ut_aicpu_kfc_retry_utest.cc
    ut_aicpu_kfc_sqe_context_utest.cc
    ut_aicpu_kfc_task_dispatcher_utest.cc
)
add_executable(aicpu_kfc_utest_misc ${aicpu_kfc_utest_misc_list})

target_include_directories(aicpu_kfc_utest_misc PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub
)

target_compile_definitions(aicpu_kfc_utest_misc PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(aicpu_kfc_utest_misc PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(aicpu_kfc_utest_misc PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    -pthread
    hccl_llt
    -Wl,--whole-archive
    -Wl,--no-whole-archi
    -lgcov
    -lrt
    -ldl
)

add_dependencies(aicpu_kfc_utest_misc hccl_llt)
run_llt_test(
   TARGET  aicpu_kfc_utest_misc
   TASK_NUM 1
)
