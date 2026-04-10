set(aicpu_kfc_utest_aicpu_list
    main.cc
    ut_aicpu_kfc_interface_utest.cc
    ut_aicpu_kfc_trace_utest.cc
    ut_aicpu_kfc_unfold_utest.cc
)
add_executable(aicpu_kfc_utest_aicpu  ${aicpu_kfc_utest_aicpu_list})

target_include_directories(aicpu_kfc_utest_aicpu PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub
)

target_compile_definitions(aicpu_kfc_utest_aicpu PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(aicpu_kfc_utest_aicpu PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
    -fno-var-tracking-assignments
)

target_link_libraries(aicpu_kfc_utest_aicpu  PRIVATE
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

add_dependencies(aicpu_kfc_utest_aicpu hccl_llt)
run_llt_test(
   TARGET aicpu_kfc_utest_aicpu
   TASK_NUM 1
)
